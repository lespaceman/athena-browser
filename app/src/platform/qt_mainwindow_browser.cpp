/**
 * QtMainWindow Browser Control Implementation
 *
 * Handles browser control methods: JavaScript execution, HTML retrieval,
 * screenshots, and CEF client access.
 */

#include "browser/cef_client.h"
#include "include/cef_app.h"
#include "platform/qt_mainwindow.h"
#include "rendering/gl_renderer.h"
#include "utils/logging.h"

#include <atomic>
#include <chrono>
#include <QCoreApplication>
#include <QEventLoop>
#include <thread>

namespace athena {
namespace platform {

using namespace browser;
using namespace rendering;
using namespace utils;

static Logger logger("QtMainWindow::Browser");

// ============================================================================
// CEF Client Accessors
// ============================================================================

CefClient* QtMainWindow::GetCefClient() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tabs_.empty()) {
    return nullptr;
  }

  return tabs_[active_tab_index_].cef_client;
}

CefClient* QtMainWindow::GetCefClientForTab(size_t tab_index) const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tab_index >= tabs_.size()) {
    return nullptr;
  }

  return tabs_[tab_index].cef_client;
}

GLRenderer* QtMainWindow::GetGLRenderer() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tabs_.empty()) {
    return nullptr;
  }

  return tabs_[active_tab_index_].renderer.get();
}

// ============================================================================
// Browser Content Access
// ============================================================================

QString QtMainWindow::GetPageHTML() const {
  QtTab* tab = nullptr;
  CefRefPtr<CefBrowser> browser;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    tab = const_cast<QtMainWindow*>(this)->GetActiveTab();
    if (!tab || !tab->cef_client || !tab->cef_client->GetBrowser()) {
      return QString();
    }
    browser = tab->cef_client->GetBrowser();
  }

  auto main_frame = browser->GetMainFrame();
  if (!main_frame) {
    logger.Error("No main frame available");
    return QString();
  }

  // Helper class for synchronous HTML retrieval
  class HtmlVisitor : public CefStringVisitor {
   public:
    HtmlVisitor() : complete_(false) {}

    void Visit(const CefString& string) override {
      std::lock_guard<std::mutex> lock(mutex_);
      html_ = string.ToString();
      complete_.store(true, std::memory_order_release);
    }

    bool WaitForHtml(std::string& html, int timeout_ms = 5000) {
      auto start = std::chrono::steady_clock::now();

      // Poll and pump CEF message loop instead of blocking
      while (!complete_.load(std::memory_order_acquire)) {
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        if (elapsed >= timeout_ms) {
          return false;
        }

        // Pump CEF message loop to allow callbacks to run
        CefDoMessageLoopWork();

        // Small sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      std::lock_guard<std::mutex> lock(mutex_);
      html = html_;
      return true;
    }

   private:
    std::mutex mutex_;
    std::string html_;
    std::atomic<bool> complete_;
    IMPLEMENT_REFCOUNTING(HtmlVisitor);
  };

  // Request HTML source
  CefRefPtr<HtmlVisitor> visitor = new HtmlVisitor();
  main_frame->GetSource(visitor);

  // Wait for result
  std::string html;
  if (visitor->WaitForHtml(html, 5000)) {
    logger.Info("Retrieved HTML (" + std::to_string(html.length()) + " bytes)");
    return QString::fromStdString(html);
  } else {
    logger.Error("Timeout waiting for HTML");
    return QString();
  }
}

QString QtMainWindow::ExecuteJavaScript(const QString& code) const {
  browser::CefClient* cef_client = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);

    QtTab* tab = const_cast<QtMainWindow*>(this)->GetActiveTab();
    if (!tab || !tab->cef_client || !tab->cef_client->GetBrowser()) {
      logger.Error("ExecuteJavaScript: No active CEF client or browser");
      return QString(R"({"success":false,"error":{"message":"No active browser"}})");
    }

    cef_client = tab->cef_client;
  }

  auto request_id_opt = cef_client->RequestJavaScriptEvaluation(code.toStdString());
  if (!request_id_opt.has_value()) {
    logger.Error("ExecuteJavaScript: Failed to dispatch request");
    return QString(
        R"({"success":false,"error":{"message":"Failed to dispatch JavaScript to renderer"}})");
  }

  const std::string request_id = request_id_opt.value();
  const int timeout_ms = 5000;
  auto start = std::chrono::steady_clock::now();

  while (true) {
    auto result = cef_client->TryConsumeJavaScriptResult(request_id);
    if (result.has_value()) {
      logger.Info("JavaScript executed ({} bytes)", result->size());
      return QString::fromStdString(result.value());
    }

    if (closed_) {
      logger.Error("ExecuteJavaScript aborted: window closed while waiting");
      cef_client->CancelJavaScriptEvaluation(request_id);
      return QString(
          R"({"success":false,"error":{"message":"Window closed while waiting for result"}})");
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    if (elapsed >= timeout_ms) {
      logger.Error("ExecuteJavaScript timed out after {}ms", timeout_ms);
      cef_client->CancelJavaScriptEvaluation(request_id);
      return QString(
          R"({"success":false,"error":{"message":"Timeout waiting for JavaScript result"},"type":"timeout"})");
    }

    CefDoMessageLoopWork();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

QString QtMainWindow::TakeScreenshot() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  QtTab* tab = const_cast<QtMainWindow*>(this)->GetActiveTab();
  if (!tab || !tab->renderer) {
    logger.Error("TakeScreenshot: No active tab or renderer");
    return QString();
  }

  // Call the GLRenderer's TakeScreenshot method (automatically scaled to 0.5 for AI analysis)
  std::string base64_png = tab->renderer->TakeScreenshot();
  if (base64_png.empty()) {
    logger.Error("TakeScreenshot: Failed to capture screenshot");
    return QString();
  }

  logger.Info("Screenshot captured successfully");
  return QString::fromStdString(base64_png);
}

// ============================================================================
// Wait Utilities
// ============================================================================

bool QtMainWindow::WaitForLoadToComplete(size_t tab_index, int timeout_ms) const {
  auto start = std::chrono::steady_clock::now();

  while (true) {
    bool ready = false;
    {
      std::lock_guard<std::mutex> lock(tabs_mutex_);
      if (tab_index >= tabs_.size()) {
        logger.Warn("WaitForLoadToComplete: invalid tab index " + std::to_string(tab_index));
        return false;
      }

      const QtTab& tab = tabs_[tab_index];
      ready = (tab.cef_client != nullptr) && !tab.is_loading;
    }

    if (ready) {
      return true;
    }

    if (closed_) {
      logger.Warn("WaitForLoadToComplete aborted because window is closed");
      return false;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    if (elapsed >= timeout_ms) {
      logger.Warn("WaitForLoadToComplete timed out after " + std::to_string(timeout_ms) +
                  "ms for tab " + std::to_string(tab_index));
      return false;
    }

    CefDoMessageLoopWork();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace platform
}  // namespace athena

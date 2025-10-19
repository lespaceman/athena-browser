/**
 * Browser Control Server - Content Handlers
 *
 * Handlers for basic content operations: HTML retrieval, JavaScript execution, screenshots.
 */

#include "runtime/browser_control_server.h"
#include "runtime/browser_control_server_internal.h"
#include "runtime/js_execution_utils.h"
#include "platform/qt_mainwindow.h"
#include "utils/logging.h"
#include <nlohmann/json.hpp>

namespace athena {
namespace runtime {

static utils::Logger logger("BrowserControlServer");

// ============================================================================
// Content Handlers
// ============================================================================

std::string BrowserControlServer::HandleGetPageHtml(std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"},
          {"tabIndex", static_cast<int>(target_tab)}}.dump();
    }

    QString html = window->GetPageHTML();
    if (html.isEmpty()) {
      return nlohmann::json{
          {"success", false},
          {"error", "Failed to retrieve HTML"}}.dump();
    }

    nlohmann::json response = {
        {"success", true},
        {"html", html.toStdString()},
        {"tabIndex", static_cast<int>(window->GetActiveTabIndex())}};
    return response.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleExecuteJavaScript(const std::string& code,
                                                          std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    bool ready = window->WaitForLoadToComplete(target_tab, 2000);
    if (!ready) {
      logger.Warn("HandleExecuteJavaScript: page still reporting loading state, executing anyway");
    }

    QString result = window->ExecuteJavaScript(QString::fromStdString(code));
    std::string parse_error;
    auto exec = ParseJsExecutionResultString(result.toStdString(), parse_error);
    if (!exec.has_value()) {
      return nlohmann::json{
          {"success", false},
          {"error", parse_error.empty() ? "Failed to parse JavaScript response" : parse_error}}.dump();
    }

    if (!exec->success) {
      nlohmann::json error_json = {
          {"success", false},
          {"error", exec->error_message.empty() ? "JavaScript execution failed" : exec->error_message}};
      if (!exec->error_stack.empty()) {
        error_json["stack"] = exec->error_stack;
      }
      return error_json.dump();
    }

    nlohmann::json response = {
        {"success", true},
        {"type", exec->type},
        {"result", exec->value},
        {"tabIndex", static_cast<int>(target_tab)},
        {"loadWaitTimedOut", !ready}};

    if (!exec->string_value.empty()) {
      response["stringResult"] = exec->string_value;
    }

    return response.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleTakeScreenshot(std::optional<size_t> tab_index,
                                                       std::optional<bool> full_page) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    bool ready = window->WaitForLoadToComplete(target_tab, 2000);
    if (!ready) {
      logger.Warn("HandleTakeScreenshot: page still reporting loading state, capturing anyway");
    }

    if (full_page.value_or(false)) {
      logger.Warn("Full page screenshot requested but not supported; capturing viewport only");
    }

    QString base64_png = window->TakeScreenshot();
    if (base64_png.isEmpty()) {
      return nlohmann::json{
          {"success", false},
          {"error", "Failed to capture screenshot"}}.dump();
    }

    return nlohmann::json{
        {"success", true},
        {"screenshot", base64_png.toStdString()},
        {"tabIndex", static_cast<int>(target_tab)},
        {"loadWaitTimedOut", !ready}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

}  // namespace runtime
}  // namespace athena

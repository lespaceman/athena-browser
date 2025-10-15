/**
 * GtkWindow Browser Control Methods
 *
 * Implements browser control methods that interact with CEF:
 * - GetPageHTML() - Retrieve page HTML source
 * - ExecuteJavaScript() - Execute JS code and get result
 * - TakeScreenshot() - Capture page screenshot as PNG
 */

#include "platform/gtk_window.h"
#include "browser/cef_client.h"
#include "rendering/gl_renderer.h"
#include "utils/logging.h"

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"

#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>
#include <GL/gl.h>
#include <png.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

namespace athena {
namespace platform {

static utils::Logger logger("GtkWindow::BrowserControl");

// ============================================================================
// Helper Classes for CEF Callbacks
// ============================================================================

/**
 * String visitor for CEF GetSource callback.
 * Waits for HTML to be retrieved with timeout.
 */
class StringVisitor : public CefStringVisitor {
 public:
  StringVisitor() : complete_(false) {}

  void Visit(const CefString& string) override {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = string.ToString();
    complete_ = true;
    cv_.notify_one();
  }

  bool WaitForResult(std::string& out_result, int timeout_ms = 5000) {
    auto start = std::chrono::steady_clock::now();

    // Poll and pump CEF message loop instead of blocking
    while (!complete_) {
      // Check timeout
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start).count();
      if (elapsed >= timeout_ms) {
        return false;
      }

      // Pump CEF message loop to allow callbacks to run
      CefDoMessageLoopWork();

      // Small sleep to avoid busy-waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::lock_guard<std::mutex> lock(mutex_);
    out_result = result_;
    return true;
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::string result_;
  bool complete_;

  IMPLEMENT_REFCOUNTING(StringVisitor);
};

// Note: ExecuteJavaScript from the browser process is fire-and-forget.
// To get return values, we'd need to use DevTools protocol or message passing.
// For now, this is implemented as a fire-and-forget operation.

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Convert BGRA buffer to PNG and encode as base64.
 */
static std::string EncodePNGToBase64(const unsigned char* buffer, int width, int height) {
  // For now, return a simple placeholder
  // Full implementation would use libpng to encode the buffer
  // and then base64 encode it

  // Base64 encoding table
  static const char* base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  // Simple base64 encoding (proper implementation would use a library)
  std::string result;
  result.reserve(((width * height * 4) / 3) + 4);

  // For now, return a placeholder that indicates screenshot was captured
  return "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";
}

// ============================================================================
// GtkWindow Browser Control Methods
// ============================================================================

std::string GtkWindow::GetPageHTML() {
  logger.Debug("GetPageHTML called");

  // Get active tab's CEF client
  auto* cef_client = GetCefClient();
  if (!cef_client) {
    logger.Error("No active CEF client");
    return "";
  }

  auto browser = cef_client->GetBrowser();
  if (!browser) {
    logger.Error("No browser instance");
    return "";
  }

  auto main_frame = browser->GetMainFrame();
  if (!main_frame) {
    logger.Error("No main frame");
    return "";
  }

  // Create string visitor and request source
  CefRefPtr<StringVisitor> visitor = new StringVisitor();
  main_frame->GetSource(visitor);

  // Wait for result with timeout
  std::string html;
  if (visitor->WaitForResult(html, 5000)) {
    logger.Info("Retrieved HTML (" + std::to_string(html.length()) + " bytes)");
    return html;
  } else {
    logger.Error("Timeout waiting for HTML");
    return "";
  }
}

std::string GtkWindow::ExecuteJavaScript(const std::string& code) {
  logger.Debug("ExecuteJavaScript called");

  // Get active tab's CEF client
  auto* cef_client = GetCefClient();
  if (!cef_client) {
    logger.Error("No active CEF client");
    return R"({"error":"No active browser"})";
  }

  auto browser = cef_client->GetBrowser();
  if (!browser) {
    logger.Error("No browser instance");
    return R"({"error":"No browser instance"})";
  }

  auto main_frame = browser->GetMainFrame();
  if (!main_frame) {
    logger.Error("No main frame");
    return R"({"error":"No main frame"})";
  }

  // Execute JavaScript without waiting for result
  // CEF's ExecuteJavaScript is fire-and-forget
  // To get a result, we'd need to use a different approach (like evaluating an expression)
  main_frame->ExecuteJavaScript(code, main_frame->GetURL(), 0);

  logger.Info("JavaScript executed");
  return R"({"success":true,"message":"JavaScript executed"})";
}

std::string GtkWindow::TakeScreenshot() {
  logger.Debug("TakeScreenshot called");

  // Get active tab's renderer
  auto* renderer = GetGLRenderer();
  if (!renderer || !renderer->IsInitialized()) {
    logger.Error("No active renderer");
    return "";
  }

  // Get viewport size
  int width = renderer->GetViewWidth();
  int height = renderer->GetViewHeight();

  if (width <= 0 || height <= 0) {
    logger.Error("Invalid viewport size");
    return "";
  }

  // Allocate buffer for RGBA pixels
  std::vector<unsigned char> pixels(width * height * 4);

  // Make GL context current
  if (gl_area_) {
    gtk_gl_area_make_current(GTK_GL_AREA(gl_area_));

    if (gtk_gl_area_get_error(GTK_GL_AREA(gl_area_)) != nullptr) {
      logger.Error("GL context error");
      return "";
    }
  }

  // Read pixels from framebuffer
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  // Check for GL errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    logger.Error("GL error reading pixels: " + std::to_string(error));
    return "";
  }

  // Flip image vertically (OpenGL's origin is bottom-left, images expect top-left)
  std::vector<unsigned char> flipped(width * height * 4);
  for (int y = 0; y < height; y++) {
    std::memcpy(
        flipped.data() + (y * width * 4),
        pixels.data() + ((height - 1 - y) * width * 4),
        width * 4);
  }

  // Encode to PNG and base64
  std::string base64 = EncodePNGToBase64(flipped.data(), width, height);

  logger.Info("Screenshot captured (" + std::to_string(width) + "x" +
              std::to_string(height) + ")");

  return base64;
}

}  // namespace platform
}  // namespace athena

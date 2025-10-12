#ifndef ATHENA_BROWSER_CEF_CLIENT_H_
#define ATHENA_BROWSER_CEF_CLIENT_H_

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_render_handler.h"
#include "rendering/gl_renderer.h"
#include <functional>

namespace athena {
namespace browser {

/**
 * CEF client handler for Off-Screen Rendering (OSR) browsers.
 *
 * This class implements the CEF client interfaces required for OSR:
 * - CefClient: Main client interface
 * - CefLifeSpanHandler: Browser lifecycle events
 * - CefDisplayHandler: Display-related events (title changes, etc.)
 * - CefLoadHandler: Load and navigation events (address changes, loading state)
 * - CefRenderHandler: Rendering events (paint, resize, etc.)
 *
 * Design:
 * - Non-copyable, movable
 * - Uses dependency injection for GLRenderer (non-owning pointer)
 * - No global state
 * - Thread-safe for CEF UI thread operations
 */
class CefClient : public ::CefClient,
                  public ::CefLifeSpanHandler,
                  public ::CefDisplayHandler,
                  public ::CefLoadHandler,
                  public ::CefRenderHandler {
 public:
  /**
   * Construct a CEF client.
   *
   * @param native_window Platform-specific window handle (GtkWidget*, HWND, etc.)
   * @param gl_renderer Non-owning pointer to GL renderer
   */
  CefClient(void* native_window, rendering::GLRenderer* gl_renderer);

  ~CefClient() override;

  // Non-copyable, movable
  CefClient(const CefClient&) = delete;
  CefClient& operator=(const CefClient&) = delete;
  CefClient(CefClient&&) = default;
  CefClient& operator=(CefClient&&) = default;

  // ============================================================================
  // CefClient methods
  // ============================================================================

  CefRefPtr<::CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<::CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<::CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<::CefRenderHandler> GetRenderHandler() override { return this; }

  // ============================================================================
  // CefLifeSpanHandler methods
  // ============================================================================

  void OnAfterCreated(CefRefPtr<::CefBrowser> browser) override;
  bool DoClose(CefRefPtr<::CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<::CefBrowser> browser) override;

  // ============================================================================
  // CefDisplayHandler methods
  // ============================================================================

  void OnTitleChange(CefRefPtr<::CefBrowser> browser, const CefString& title) override;
  void OnAddressChange(CefRefPtr<::CefBrowser> browser,
                       CefRefPtr<::CefFrame> frame,
                       const CefString& url) override;

  // ============================================================================
  // CefLoadHandler methods
  // ============================================================================

  void OnLoadingStateChange(CefRefPtr<::CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override;

  // ============================================================================
  // CefRenderHandler methods (for OSR)
  // ============================================================================

  void GetViewRect(CefRefPtr<::CefBrowser> browser, CefRect& rect) override;
  bool GetScreenInfo(CefRefPtr<::CefBrowser> browser, CefScreenInfo& screen_info) override;
  void OnPaint(CefRefPtr<::CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override;

  // ============================================================================
  // Public API
  // ============================================================================

  /**
   * Get the browser instance.
   * @return Browser reference, or nullptr if not created yet
   */
  CefRefPtr<::CefBrowser> GetBrowser() const { return browser_; }

  /**
   * Set the logical view size.
   * This will notify CEF that the view was resized.
   */
  void SetSize(int width, int height);

  /**
   * Set the device scale factor (for HiDPI displays).
   * This will notify CEF that the scale changed.
   */
  void SetDeviceScaleFactor(float scale_factor);

  /**
   * Get current width in logical pixels.
   */
  int GetWidth() const { return width_; }

  /**
   * Get current height in logical pixels.
   */
  int GetHeight() const { return height_; }

  /**
   * Get current device scale factor.
   */
  float GetDeviceScaleFactor() const { return device_scale_factor_; }

  /**
   * Set callback for address changes.
   * Called when the URL in the address bar should be updated.
   */
  void SetAddressChangeCallback(std::function<void(const std::string&)> callback) {
    on_address_change_ = std::move(callback);
  }

  /**
   * Set callback for loading state changes.
   * Called when the loading state or navigation button states change.
   */
  void SetLoadingStateChangeCallback(std::function<void(bool, bool, bool)> callback) {
    on_loading_state_change_ = std::move(callback);
  }

  /**
   * Set callback for title changes.
   * Called when the page title changes.
   */
  void SetTitleChangeCallback(std::function<void(const std::string&)> callback) {
    on_title_change_ = std::move(callback);
  }

 private:
  void* native_window_;              // Platform-specific window handle (non-owning)
  CefRefPtr<::CefBrowser> browser_;  // CEF browser instance
  rendering::GLRenderer* gl_renderer_;  // GL renderer (non-owning)
  int width_;                        // Logical view width
  int height_;                       // Logical view height
  float device_scale_factor_;        // HiDPI scale factor (1.0, 2.0, etc.)

  // Callbacks for UI updates
  std::function<void(const std::string&)> on_address_change_;         // URL changed
  std::function<void(bool, bool, bool)> on_loading_state_change_;     // Loading state changed
  std::function<void(const std::string&)> on_title_change_;           // Title changed

  IMPLEMENT_REFCOUNTING(CefClient);
};

}  // namespace browser
}  // namespace athena

#endif  // ATHENA_BROWSER_CEF_CLIENT_H_

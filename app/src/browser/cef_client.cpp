#include "browser/cef_client.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include <iostream>

namespace athena {
namespace browser {

CefClient::CefClient(void* native_window, rendering::GLRenderer* gl_renderer)
    : native_window_(native_window),
      browser_(nullptr),
      gl_renderer_(gl_renderer),
      width_(0),
      height_(0),
      device_scale_factor_(1.0f) {
}

CefClient::~CefClient() {
  // GL resources cleaned up by GLRenderer (owned externally)
  // Browser cleanup happens via OnBeforeClose
}

// ============================================================================
// CefLifeSpanHandler methods
// ============================================================================

void CefClient::OnAfterCreated(CefRefPtr<::CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
  std::cout << "[CefClient::OnAfterCreated] OSR Browser created! Scale factor: "
            << device_scale_factor_ << std::endl;
}

bool CefClient::DoClose(CefRefPtr<::CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  return false;  // Allow the close
}

void CefClient::OnBeforeClose(CefRefPtr<::CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = nullptr;
  CefQuitMessageLoop();
}

// ============================================================================
// CefDisplayHandler methods
// ============================================================================

void CefClient::OnTitleChange(CefRefPtr<::CefBrowser> browser, const CefString& title) {
  (void)browser;
  CEF_REQUIRE_UI_THREAD();

  // Call the title change callback if set
  // With tabs, the window should update the tab label, not the window title
  if (on_title_change_) {
    on_title_change_(title.ToString());
  }
}

void CefClient::OnAddressChange(CefRefPtr<::CefBrowser> browser,
                                 CefRefPtr<::CefFrame> frame,
                                 const CefString& url) {
  CEF_REQUIRE_UI_THREAD();

  // Only update for the main frame
  if (frame->IsMain() && on_address_change_) {
    on_address_change_(url.ToString());
  }
}

// ============================================================================
// CefLoadHandler methods
// ============================================================================

void CefClient::OnLoadingStateChange(CefRefPtr<::CefBrowser> browser,
                                      bool isLoading,
                                      bool canGoBack,
                                      bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();

  if (on_loading_state_change_) {
    on_loading_state_change_(isLoading, canGoBack, canGoForward);
  }
}

void CefClient::OnPopupShow(CefRefPtr<::CefBrowser> browser, bool show) {
  CEF_REQUIRE_UI_THREAD();

  if (!gl_renderer_) {
    return;
  }

  gl_renderer_->OnPopupShow(browser, show);
}

void CefClient::OnPopupSize(CefRefPtr<::CefBrowser> browser, const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  if (!gl_renderer_) {
    return;
  }

  core::Rect popup_rect{rect.x, rect.y, rect.width, rect.height};
  gl_renderer_->OnPopupSize(browser, popup_rect);
}

// ============================================================================
// CefRenderHandler methods
// ============================================================================

void CefClient::GetViewRect(CefRefPtr<::CefBrowser> browser, CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  if (width_ > 0 && height_ > 0) {
    // Return LOGICAL size - CEF will apply device_scale_factor internally
    rect = CefRect(0, 0, width_, height_);
  } else {
    // Default size until widget is allocated
    rect = CefRect(0, 0, 1200, 800);
  }
}

bool CefClient::GetScreenInfo(CefRefPtr<::CefBrowser> browser, CefScreenInfo& screen_info) {
  CEF_REQUIRE_UI_THREAD();
  screen_info.device_scale_factor = device_scale_factor_;
  return true;
}

void CefClient::OnPaint(CefRefPtr<::CefBrowser> browser,
                        PaintElementType type,
                        const RectList& dirtyRects,
                        const void* buffer,
                        int width,
                        int height) {
  CEF_REQUIRE_UI_THREAD();

  if (!gl_renderer_) {
    return;
  }

  // Forward to GLRenderer which handles OpenGL texture updates
  gl_renderer_->OnPaint(browser, type, dirtyRects, buffer, width, height);

  if (on_render_invalidated_) {
    on_render_invalidated_(type);
  }
}

// ============================================================================
// Public API
// ============================================================================

void CefClient::SetSize(int width, int height) {
  width_ = width;
  height_ = height;

  if (gl_renderer_) {
    gl_renderer_->SetViewSize(width, height);
  }

  if (browser_) {
    browser_->GetHost()->WasResized();
  }
}

void CefClient::SetDeviceScaleFactor(float scale_factor) {
  if (device_scale_factor_ != scale_factor) {
    device_scale_factor_ = scale_factor;
    if (browser_) {
      browser_->GetHost()->WasResized();
    }
  }
}

}  // namespace browser
}  // namespace athena

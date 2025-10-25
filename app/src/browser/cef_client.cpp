#include "browser/cef_client.h"

#include "browser/message_router_handler.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "utils/logging.h"

#include <iostream>
#include <utility>

namespace athena {
namespace browser {

static utils::Logger logger("CefClient");
CefClient::CefClient(void* native_window, rendering::GLRenderer* gl_renderer)
    : native_window_(native_window),
      browser_(nullptr),
      gl_renderer_(gl_renderer),
      width_(0),
      height_(0),
      device_scale_factor_(1.0f),
      has_focus_(false) {}

CefClient::~CefClient() {
  // Clean up message router
  if (message_router_) {
    message_router_->RemoveHandler(message_router_handler_.get());
    message_router_ = nullptr;
  }
  message_router_handler_.reset();

  // GL resources cleaned up by GLRenderer (owned externally)
  // Browser cleanup happens via OnBeforeClose
}

void CefClient::InitializeMessageRouter() {
  // Create message router config
  CefMessageRouterConfig config;
  config.js_query_function = "athena.query";
  config.js_cancel_function = "athena.queryCancel";

  // Create the message router
  message_router_ = CefMessageRouterBrowserSide::Create(config);

  // Create and register our handler
  message_router_handler_ = std::make_unique<MessageRouterHandler>();
  message_router_->AddHandler(message_router_handler_.get(), false);

  logger.Info("Message router initialized: query={}, cancel={}",
              config.js_query_function,
              config.js_cancel_function);
}

// ============================================================================
// CefLifeSpanHandler methods
// ============================================================================

void CefClient::OnAfterCreated(CefRefPtr<::CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;

  // Notify message router of new browser
  if (message_router_) {
    message_router_->OnBeforeBrowse(browser, browser->GetMainFrame());
  }

  logger.Info("OSR Browser created! Scale factor: {}", device_scale_factor_);
}

bool CefClient::DoClose(CefRefPtr<::CefBrowser> browser) {
  (void)browser;  // Unused parameter
  CEF_REQUIRE_UI_THREAD();
  return false;  // Allow the close
}

void CefClient::OnBeforeClose(CefRefPtr<::CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Notify message router of browser close
  if (message_router_) {
    message_router_->OnBeforeClose(browser);
  }

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
  (void)browser;  // Unused parameter
  CEF_REQUIRE_UI_THREAD();

  // Only update for the main frame
  if (frame->IsMain()) {
    if (on_address_change_) {
      on_address_change_(url.ToString());
    }
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

  // Workaround for upstream CEF bug: cursor/caret becomes invisible after navigation
  // Root cause: CEF incorrectly assesses focus state after mouse-click navigation
  // Solution: Force SetFocus(true) after page load completes to refresh focus state
  // Related: CefSharp #4146, chromiumembedded/cef #3436, #3481
  if (!isLoading && has_focus_ && browser) {
    logger.Debug("Page load complete, refreshing focus to restore cursor visibility");
    browser->GetHost()->SetFocus(true);
  }

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
  (void)browser;  // Unused parameter
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
  (void)browser;  // Unused parameter
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

void CefClient::OnImeCompositionRangeChanged(CefRefPtr<::CefBrowser> browser,
                                             const CefRange& selected_range,
                                             const RectList& character_bounds) {
  (void)browser;  // Unused parameter
  CEF_REQUIRE_UI_THREAD();

  // NOTE: This method is ONLY called during IME composition (Chinese/Japanese/Korean input).
  // It is NOT called for regular Latin text input, so it won't help with cursor visibility
  // for English text. We keep it for IME debugging purposes.

  logger.Debug("OnImeCompositionRangeChanged: range({}, {}), bounds count: {}",
               selected_range.from,
               selected_range.to,
               character_bounds.size());

  if (!character_bounds.empty()) {
    const auto& last_char = character_bounds[character_bounds.size() - 1];
    logger.Debug("IME cursor position: ({}, {}) size: {}x{}",
                 last_char.x,
                 last_char.y,
                 last_char.width,
                 last_char.height);
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

void CefClient::SetFocus(bool focus) {
  has_focus_ = focus;
  logger.Debug("Focus state changed to: {}", focus);
}

void CefClient::ShowDevTools(const core::Point* inspect_element_at) {
  CEF_REQUIRE_UI_THREAD();

  if (!browser_) {
    logger.Warn("ShowDevTools: browser_ is null");
    return;
  }

  auto host = browser_->GetHost();
  if (!host) {
    logger.Warn("ShowDevTools: host is null");
    return;
  }

  // Configure DevTools window (empty for default native window)
  CefWindowInfo window_info;
  CefBrowserSettings settings;

  // If a specific point is provided, inspect that element
  CefPoint inspect_point;
  if (inspect_element_at) {
    inspect_point.x = inspect_element_at->x;
    inspect_point.y = inspect_element_at->y;
  }

  // Show DevTools in a new native window
  host->ShowDevTools(window_info, nullptr, settings, inspect_point);

  logger.Info("DevTools opened");
}

std::string CefClient::GenerateRequestId() {
  uint64_t id = next_js_request_id_.fetch_add(1, std::memory_order_relaxed);
  return std::to_string(id);
}

std::optional<std::string> CefClient::RequestJavaScriptEvaluation(const std::string& code) {
  CEF_REQUIRE_UI_THREAD();

  if (!browser_) {
    logger.Warn("RequestJavaScriptEvaluation: browser_ is null");
    return std::nullopt;
  }

  auto frame = browser_->GetMainFrame();
  if (!frame) {
    logger.Warn("RequestJavaScriptEvaluation: main frame is null");
    return std::nullopt;
  }

  const std::string request_id = GenerateRequestId();
  {
    std::lock_guard<std::mutex> lock(js_mutex_);
    pending_js_.emplace(request_id, JavaScriptRequest{});
  }

  CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("Athena.ExecuteJavaScript");
  CefRefPtr<CefListValue> args = message->GetArgumentList();
  args->SetString(0, request_id);
  args->SetString(1, code);

  logger.Debug("Dispatching JS evaluation request {}", request_id);
  frame->SendProcessMessage(PID_RENDERER, message);
  return request_id;
}

std::optional<std::string> CefClient::TryConsumeJavaScriptResult(const std::string& request_id) {
  std::lock_guard<std::mutex> lock(js_mutex_);
  auto it = pending_js_.find(request_id);
  if (it == pending_js_.end() || !it->second.completed) {
    return std::nullopt;
  }

  std::string result = std::move(it->second.result_json);
  pending_js_.erase(it);
  return result;
}

void CefClient::CancelJavaScriptEvaluation(const std::string& request_id) {
  std::lock_guard<std::mutex> lock(js_mutex_);
  pending_js_.erase(request_id);
}

bool CefClient::OnProcessMessageReceived(CefRefPtr<::CefBrowser> browser,
                                         CefRefPtr<::CefFrame> frame,
                                         CefProcessId source_process,
                                         CefRefPtr<CefProcessMessage> message) {
  if (!message) {
    return false;
  }

  // First, try message router
  if (message_router_ &&
      message_router_->OnProcessMessageReceived(browser, frame, source_process, message)) {
    return true;
  }

  // Then handle custom IPC messages
  const std::string name = message->GetName();
  if (name != "Athena.ExecuteJavaScriptResult") {
    return false;
  }

  CefRefPtr<CefListValue> args = message->GetArgumentList();
  if (!args || args->GetSize() < 2) {
    logger.Warn("ExecuteJavaScriptResult received with insufficient arguments");
    return true;
  }

  const std::string request_id = args->GetString(0);
  const std::string payload = args->GetString(1);

  {
    std::lock_guard<std::mutex> lock(js_mutex_);
    auto it = pending_js_.find(request_id);
    if (it == pending_js_.end()) {
      logger.Warn("ExecuteJavaScriptResult for unknown request {}", request_id);
      return true;
    }

    it->second.completed = true;
    it->second.result_json = payload;
  }

  logger.Debug("ExecuteJavaScriptResult received for request {}", request_id);
  return true;
}

}  // namespace browser
}  // namespace athena

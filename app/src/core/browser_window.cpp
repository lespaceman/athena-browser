#include "core/browser_window.h"
#include "utils/logging.h"
#include <iostream>

namespace athena {
namespace core {

// Static logger for this module
static utils::Logger logger("BrowserWindow");

BrowserWindow::BrowserWindow(const BrowserWindowConfig& config,
                             const BrowserWindowCallbacks& callbacks,
                             platform::WindowSystem* window_system,
                             browser::BrowserEngine* browser_engine)
    : config_(config),
      callbacks_(callbacks),
      window_system_(window_system),
      browser_engine_(browser_engine),
      window_(nullptr),
      browser_id_(browser::kInvalidBrowserId),
      initialized_(false) {
  logger.Debug("BrowserWindow::BrowserWindow - Creating browser window");
}

BrowserWindow::~BrowserWindow() {
  logger.Debug("BrowserWindow::~BrowserWindow - Destroying browser window");

  // Clean up browser first
  if (browser_id_ != browser::kInvalidBrowserId && browser_engine_) {
    browser_engine_->CloseBrowser(browser_id_, true);
    browser_id_ = browser::kInvalidBrowserId;
  }

  // Window is automatically destroyed (RAII)
  window_.reset();
}

// ============================================================================
// Lifecycle
// ============================================================================

utils::Result<void> BrowserWindow::Show() {
  if (!initialized_) {
    auto init_result = Initialize();
    if (!init_result) {
      return init_result;
    }
  }

  if (!window_) {
    return utils::Error("BrowserWindow::Show - Window not created");
  }

  window_->Show();

  // Load initial URL after window is shown
  if (!config_.url.empty()) {
    LoadURL(config_.url);
  }

  return utils::Ok();
}

void BrowserWindow::Hide() {
  if (window_) {
    window_->Hide();
  }
}

void BrowserWindow::Close(bool force) {
  logger.Debug("BrowserWindow::Close - Closing window");

  // Close browser first
  if (browser_id_ != browser::kInvalidBrowserId && browser_engine_) {
    browser_engine_->CloseBrowser(browser_id_, force);
    browser_id_ = browser::kInvalidBrowserId;
  }

  // Close window (this will trigger on_close callback)
  if (window_) {
    window_->Close(force);
  }
}

bool BrowserWindow::IsClosed() const {
  if (!window_) {
    return true;
  }
  return window_->IsClosed();
}

// ============================================================================
// Window Properties
// ============================================================================

std::string BrowserWindow::GetTitle() const {
  if (!window_) {
    return "";
  }
  return window_->GetTitle();
}

void BrowserWindow::SetTitle(const std::string& title) {
  if (window_) {
    window_->SetTitle(title);
  }
}

Size BrowserWindow::GetSize() const {
  if (!window_) {
    return Size{0, 0};
  }
  return window_->GetSize();
}

void BrowserWindow::SetSize(const Size& size) {
  if (window_) {
    window_->SetSize(size);

    // Notify browser of size change
    if (browser_id_ != browser::kInvalidBrowserId && browser_engine_) {
      browser_engine_->SetSize(browser_id_, size.width, size.height);
    }
  }
}

float BrowserWindow::GetScaleFactor() const {
  if (!window_) {
    return 1.0f;
  }
  return window_->GetScaleFactor();
}

bool BrowserWindow::IsVisible() const {
  if (!window_) {
    return false;
  }
  return window_->IsVisible();
}

bool BrowserWindow::HasFocus() const {
  if (!window_) {
    return false;
  }
  return window_->HasFocus();
}

void BrowserWindow::Focus() {
  if (window_) {
    window_->Focus();
  }
}

// ============================================================================
// Navigation
// ============================================================================

void BrowserWindow::LoadURL(const std::string& url) {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    logger.Error("BrowserWindow::LoadURL - Browser not initialized");
    return;
  }

  browser_engine_->LoadURL(browser_id_, url);
}

void BrowserWindow::GoBack() {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    return;
  }

  browser_engine_->GoBack(browser_id_);
}

void BrowserWindow::GoForward() {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    return;
  }

  browser_engine_->GoForward(browser_id_);
}

void BrowserWindow::Reload(bool ignore_cache) {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    return;
  }

  browser_engine_->Reload(browser_id_, ignore_cache);
}

void BrowserWindow::StopLoad() {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    return;
  }

  browser_engine_->StopLoad(browser_id_);
}

// ============================================================================
// Browser State
// ============================================================================

bool BrowserWindow::CanGoBack() const {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    return false;
  }

  return browser_engine_->CanGoBack(browser_id_);
}

bool BrowserWindow::CanGoForward() const {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    return false;
  }

  return browser_engine_->CanGoForward(browser_id_);
}

bool BrowserWindow::IsLoading() const {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    return false;
  }

  return browser_engine_->IsLoading(browser_id_);
}

std::string BrowserWindow::GetURL() const {
  if (browser_id_ == browser::kInvalidBrowserId || !browser_engine_) {
    return "";
  }

  return browser_engine_->GetURL(browser_id_);
}

// ============================================================================
// Browser & Window IDs
// ============================================================================

browser::BrowserId BrowserWindow::GetBrowserId() const {
  return browser_id_;
}

platform::Window* BrowserWindow::GetWindow() const {
  return window_.get();
}

// ============================================================================
// Private Methods
// ============================================================================

utils::Result<void> BrowserWindow::Initialize() {
  logger.Debug("BrowserWindow::Initialize - Initializing browser window");

  if (initialized_) {
    return utils::Ok();
  }

  // Validate dependencies
  if (!window_system_) {
    return utils::Error("BrowserWindow::Initialize - Window system is null");
  }

  if (!browser_engine_) {
    return utils::Error("BrowserWindow::Initialize - Browser engine is null");
  }

  if (!window_system_->IsInitialized()) {
    return utils::Error("BrowserWindow::Initialize - Window system not initialized");
  }

  if (!browser_engine_->IsInitialized()) {
    return utils::Error("BrowserWindow::Initialize - Browser engine not initialized");
  }

  // Create platform window
  platform::WindowConfig window_config;
  window_config.title = config_.title;
  window_config.size = config_.size;
  window_config.resizable = config_.resizable;
  window_config.enable_input = config_.enable_input;

  platform::WindowCallbacks window_callbacks;
  SetupWindowCallbacks();

  // Assign callbacks
  window_callbacks.on_resize = [this](int width, int height) {
    // Notify browser of size change
    if (browser_id_ != browser::kInvalidBrowserId && browser_engine_) {
      browser_engine_->SetSize(browser_id_, width, height);
    }

    // Forward to user callback
    if (callbacks_.on_resize) {
      callbacks_.on_resize(width, height);
    }
  };

  window_callbacks.on_close = [this]() {
    // Don't close browser here - it's already closed in Close()
    // This callback is just for notification

    // Forward to user callback
    if (callbacks_.on_close) {
      callbacks_.on_close();
    }
  };

  window_callbacks.on_destroy = [this]() {
    // Forward to user callback
    if (callbacks_.on_destroy) {
      callbacks_.on_destroy();
    }
  };

  window_callbacks.on_focus_changed = [this](bool focused) {
    // Notify browser of focus change
    if (browser_id_ != browser::kInvalidBrowserId && browser_engine_) {
      browser_engine_->SetFocus(browser_id_, focused);
    }

    // Forward to user callback
    if (callbacks_.on_focus_changed) {
      callbacks_.on_focus_changed(focused);
    }
  };

  auto window_result = window_system_->CreateWindow(window_config, window_callbacks);
  if (!window_result) {
    return utils::Error("BrowserWindow::Initialize - Failed to create window: " +
                        window_result.GetError().Message());
  }

  window_ = std::move(window_result.Value());

  // Create browser instance
  browser::BrowserConfig browser_config;
  browser_config.url = config_.url;
  browser_config.width = config_.size.width;
  browser_config.height = config_.size.height;
  browser_config.device_scale_factor = window_->GetScaleFactor();
  browser_config.native_window_handle = window_->GetNativeHandle();
  browser_config.gl_renderer = nullptr;  // Will be set by platform layer

  auto browser_result = browser_engine_->CreateBrowser(browser_config);
  if (!browser_result) {
    window_.reset();
    return utils::Error("BrowserWindow::Initialize - Failed to create browser: " +
                        browser_result.GetError().Message());
  }

  browser_id_ = browser_result.Value();

  // Associate browser with window
  window_->SetBrowser(browser_id_);

  initialized_ = true;
  logger.Debug("BrowserWindow::Initialize - Initialization complete");

  return utils::Ok();
}

void BrowserWindow::SetupWindowCallbacks() {
  // Placeholder for future callback setup
  // Currently all callbacks are set up in Initialize()
}

}  // namespace core
}  // namespace athena

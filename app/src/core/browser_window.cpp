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
      initialized_(false),
      browser_closed_(false) {
  logger.Debug("BrowserWindow::BrowserWindow - Creating browser window");
}

BrowserWindow::~BrowserWindow() {
  logger.Debug("BrowserWindow::~BrowserWindow - Destroying browser window");

  // If the browser wasn't explicitly closed, close it now
  // This ensures all tabs are properly cleaned up
  if (!browser_closed_) {
    auto browser_id = GetBrowserId();
    if (browser_id != browser::kInvalidBrowserId && browser_engine_) {
      browser_engine_->CloseBrowser(browser_id, true);
    }
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

  // Show the window - browser creation will happen asynchronously in
  // GtkWindow::OnRealize() after the GTK main loop starts
  window_->Show();

  return utils::Ok();
}

void BrowserWindow::Hide() {
  if (window_) {
    window_->Hide();
  }
}

void BrowserWindow::Close(bool force) {
  logger.Debug("BrowserWindow::Close - Closing window");

  // Close browser first (get active browser from window)
  auto browser_id = GetBrowserId();
  if (browser_id != browser::kInvalidBrowserId && browser_engine_) {
    browser_engine_->CloseBrowser(browser_id, force);
    browser_closed_ = true;  // Mark browser as closed
  }

  // Close window (this will trigger on_close callback and close all tabs)
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

    // Notify browser of size change (get active browser from window)
    auto browser_id = GetBrowserId();
    if (browser_id != browser::kInvalidBrowserId && browser_engine_) {
      browser_engine_->SetSize(browser_id, size.width, size.height);
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
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    logger.Error("BrowserWindow::LoadURL - Browser not initialized");
    return;
  }

  browser_engine_->LoadURL(browser_id, url);
}

void BrowserWindow::GoBack() {
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    return;
  }

  browser_engine_->GoBack(browser_id);
}

void BrowserWindow::GoForward() {
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    return;
  }

  browser_engine_->GoForward(browser_id);
}

void BrowserWindow::Reload(bool ignore_cache) {
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    return;
  }

  browser_engine_->Reload(browser_id, ignore_cache);
}

void BrowserWindow::StopLoad() {
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    return;
  }

  browser_engine_->StopLoad(browser_id);
}

// ============================================================================
// Browser State
// ============================================================================

bool BrowserWindow::CanGoBack() const {
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    return false;
  }

  return browser_engine_->CanGoBack(browser_id);
}

bool BrowserWindow::CanGoForward() const {
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    return false;
  }

  return browser_engine_->CanGoForward(browser_id);
}

bool BrowserWindow::IsLoading() const {
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    return false;
  }

  return browser_engine_->IsLoading(browser_id);
}

std::string BrowserWindow::GetURL() const {
  auto browser_id = GetBrowserId();
  if (browser_id == browser::kInvalidBrowserId || !browser_engine_) {
    return "";
  }

  return browser_engine_->GetURL(browser_id);
}

// ============================================================================
// Browser & Window IDs
// ============================================================================

browser::BrowserId BrowserWindow::GetBrowserId() const {
  // Delegate to the window to get the active tab's browser ID
  if (window_) {
    return window_->GetBrowser();
  }
  return browser::kInvalidBrowserId;
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
  window_config.url = config_.url;  // Pass URL for browser creation
  window_config.node_runtime = config_.node_runtime;  // Pass Node runtime for Claude chat

  platform::WindowCallbacks window_callbacks;
  SetupWindowCallbacks();

  // Assign callbacks
  window_callbacks.on_resize = [this](int width, int height) {
    // Notify browser of size change (get active browser from window)
    auto browser_id = GetBrowserId();
    if (browser_id != browser::kInvalidBrowserId && browser_engine_) {
      browser_engine_->SetSize(browser_id, width, height);
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
    // Notify browser of focus change (get active browser from window)
    auto browser_id = GetBrowserId();
    if (browser_id != browser::kInvalidBrowserId && browser_engine_) {
      browser_engine_->SetFocus(browser_id, focused);
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

  // NOTE: Browser creation is deferred until Show() is called.
  // This is because the GLRenderer is only available after the window is realized,
  // which happens when the window is shown.

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

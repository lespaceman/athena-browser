/**
 * QtMainWindow Tab Management Implementation
 *
 * Handles multi-tab support: creating, closing, switching tabs.
 * Each tab owns its own BrowserWidget, GLRenderer, and CEF browser instance.
 */

#include "browser/browser_engine.h"
#include "browser/cef_client.h"
#include "browser/cef_engine.h"
#include "browser/thread_safety.h"
#include "include/cef_browser.h"
#include "platform/qt_agent_panel.h"
#include "platform/qt_browserwidget.h"
#include "platform/qt_mainwindow.h"
#include "rendering/gl_renderer.h"
#include "utils/logging.h"

#include <algorithm>
#include <QApplication>
#include <QMetaObject>
#include <QPointer>
#include <QSignalBlocker>
#include <QTabBar>

namespace athena {
namespace platform {

using namespace browser;
using namespace rendering;
using namespace utils;

static Logger logger("QtMainWindow::Tabs");

// ============================================================================
// Tab Creation
// ============================================================================

int QtMainWindow::CreateTab(const QString& url) {
  if (!tabWidget_) {
    logger.Error("Tab widget not initialized");
    return -1;
  }

  if (!engine_) {
    logger.Error("BrowserEngine not available");
    return -1;
  }

  logger.Info("Creating tab with URL: " + url.toStdString());

  // Create Tab structure
  QtTab tab;
  tab.browser_id = 0;  // Will be set after browser creation
  tab.cef_client = nullptr;
  tab.browser_widget = nullptr;  // Will be set below
  tab.url = url;
  tab.title = "New Tab";
  tab.is_loading = true;
  tab.can_go_back = false;
  tab.can_go_forward = false;

  // Each tab owns its own GL renderer
  tab.renderer = std::make_unique<GLRenderer>();

  // Add tab to tabs_ vector FIRST to get the correct index
  size_t new_tab_index;
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    tabs_.push_back(std::move(tab));
    new_tab_index = tabs_.size() - 1;
  }

  // Create BrowserWidget for this tab with the correct tab index
  BrowserWidget* browserWidget = new BrowserWidget(this, new_tab_index, tabWidget_);
  browserWidget->setFocusPolicy(Qt::StrongFocus);

  // Update the tab's browser_widget pointer
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    tabs_[new_tab_index].browser_widget = browserWidget;
  }

  // Initialize GLRenderer for this tab (just sets the pointer, doesn't initialize yet)
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    browserWidget->InitializeBrowser(tabs_[new_tab_index].renderer.get());
  }

  // Connect signal to create browser when GL context is ready
  // IMPORTANT: Must capture new_tab_index by value, not by reference
  connect(browserWidget, &BrowserWidget::glContextReady, this, [this, new_tab_index]() {
    logger.Info("GL context ready for tab " + std::to_string(new_tab_index) + ", creating browser");
    createBrowserForTab(new_tab_index);
  });

  // Add to tab widget (must happen AFTER tabs_ vector is updated)
  int qt_index = tabWidget_->addTab(browserWidget, "New Tab");

  // Switch to the new tab
  tabWidget_->setCurrentIndex(qt_index);

  logger.Info("Tab widget created, index: " + std::to_string(new_tab_index) +
              " (browser will be created when GL is ready)");
  return static_cast<int>(new_tab_index);
}

void QtMainWindow::createBrowserForTab(size_t tab_index) {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tab_index >= tabs_.size()) {
    logger.Error("Invalid tab index: " + std::to_string(tab_index));
    return;
  }

  QtTab& tab = tabs_[tab_index];
  BrowserWidget* browserWidget = tab.browser_widget;

  if (!browserWidget) {
    logger.Error("BrowserWidget is null for tab " + std::to_string(tab_index));
    return;
  }

  logger.Info("Creating CEF browser for tab " + std::to_string(tab_index));

  // Create CEF browser instance
  float scale_factor = devicePixelRatioF();

  browser::BrowserConfig browser_config;
  browser_config.url = tab.url.toStdString();
  browser_config.width = browserWidget->width() > 0 ? browserWidget->width() : width();
  browser_config.height = browserWidget->height() > 0 ? browserWidget->height() : height();
  browser_config.device_scale_factor = scale_factor;
  browser_config.gl_renderer = tab.renderer.get();
  browser_config.native_window_handle = browserWidget;

  auto result = engine_->CreateBrowser(browser_config);
  if (!result.IsOk()) {
    logger.Error("Failed to create browser: " + result.GetError().Message());
    return;
  }

  tab.browser_id = result.Value();
  logger.Info("Browser created with ID: " + std::to_string(tab.browser_id));

  // Get the CEF client
  auto* cef_engine = dynamic_cast<browser::CefEngine*>(engine_);
  if (cef_engine) {
    auto client = cef_engine->GetCefClient(tab.browser_id);
    if (client) {
      tab.cef_client = client.get();

      // Wire up CEF callbacks for this tab with thread-safe marshaling
      browser::BrowserId bid = tab.browser_id;

      // Address change callback: marshals to Qt thread with weak pointer validation
      tab.cef_client->SetAddressChangeCallback([this, bid](const std::string& url_str) {
        // Use thread-safe callback wrapper to marshal from CEF → Qt main thread
        SafeInvokeQtCallback(
            this,
            [bid, url_str](QtMainWindow* window) {
              // This runs on Qt main thread with validated window pointer
              if (window->closed_) {
                return;
              }

              std::lock_guard<std::mutex> lock(window->tabs_mutex_);
              auto it = std::find_if(window->tabs_.begin(), window->tabs_.end(),
                                     [bid](const QtTab& t) { return t.browser_id == bid; });
              if (it != window->tabs_.end()) {
                it->url = QString::fromStdString(url_str);
                size_t tab_idx = std::distance(window->tabs_.begin(), it);
                if (tab_idx == window->active_tab_index_) {
                  window->UpdateAddressBar(QString::fromStdString(url_str));
                }
              }
            });
      });

      // Loading state callback: marshals to Qt thread with weak pointer validation
      tab.cef_client->SetLoadingStateChangeCallback(
          [this, bid](bool is_loading, bool can_go_back, bool can_go_forward) {
            // Use thread-safe callback wrapper to marshal from CEF → Qt main thread
            SafeInvokeQtCallback(
                this,
                [bid, is_loading, can_go_back, can_go_forward](QtMainWindow* window) {
                  // This runs on Qt main thread with validated window pointer
                  if (window->closed_) {
                    return;
                  }

                  std::lock_guard<std::mutex> lock(window->tabs_mutex_);
                  auto it = std::find_if(window->tabs_.begin(), window->tabs_.end(),
                                         [bid](const QtTab& t) { return t.browser_id == bid; });
                  if (it != window->tabs_.end()) {
                    it->is_loading = is_loading;
                    it->can_go_back = can_go_back;
                    it->can_go_forward = can_go_forward;
                    size_t tab_idx = std::distance(window->tabs_.begin(), it);
                    if (tab_idx == window->active_tab_index_) {
                      window->UpdateNavigationButtons(is_loading, can_go_back, can_go_forward);
                    }
                  }
                });
          });

      // Title change callback: marshals to Qt thread with weak pointer validation
      tab.cef_client->SetTitleChangeCallback([this, bid](const std::string& title_str) {
        // Use thread-safe callback wrapper to marshal from CEF → Qt main thread
        SafeInvokeQtCallback(
            this,
            [bid, title_str](QtMainWindow* window) {
              // This runs on Qt main thread with validated window pointer
              if (window->closed_) {
                return;
              }

              std::lock_guard<std::mutex> lock(window->tabs_mutex_);
              auto it = std::find_if(window->tabs_.begin(), window->tabs_.end(),
                                     [bid](const QtTab& t) { return t.browser_id == bid; });
              if (it != window->tabs_.end()) {
                it->title = QString::fromStdString(title_str);
                size_t tab_idx = std::distance(window->tabs_.begin(), it);
                window->tabWidget_->setTabText(tab_idx, QString::fromStdString(title_str));
              }
            });
      });

      // CRITICAL: Wire up render invalidation callback
      // This tells the widget to repaint when CEF has new content
      tab.cef_client->SetRenderInvalidatedCallback(
          [this, bid](CefRenderHandler::PaintElementType type) {
            (void)type;  // Unused parameter

            // Use thread-safe callback wrapper to marshal from CEF → Qt main thread
            SafeInvokeQtCallback(
                this,
                [bid](QtMainWindow* window) {
                  // This runs on Qt main thread with validated window pointer
                  if (window->closed_) {
                    return;
                  }

                  std::lock_guard<std::mutex> lock(window->tabs_mutex_);
                  auto it = std::find_if(window->tabs_.begin(), window->tabs_.end(),
                                         [bid](const QtTab& t) { return t.browser_id == bid; });
                  if (it != window->tabs_.end() && it->browser_widget) {
                    // Widget is guaranteed to be valid here since we're on Qt thread
                    // and holding the tabs_mutex_
                    it->browser_widget->update();
                  }
                });
          });

      logger.Info("Callbacks wired for browser_id: " + std::to_string(bid));
    }
  }
}

// ============================================================================
// Tab Closing
// ============================================================================

void QtMainWindow::CloseTab(size_t index) {
  browser::BrowserId browser_to_close = 0;
  size_t new_active_index = 0;
  bool should_close_window = false;
  std::unique_ptr<GLRenderer> renderer_to_destroy;
  CefClient* client_to_hide = nullptr;
  BrowserWidget* widget_to_delete = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (index >= tabs_.size()) {
      logger.Error("Invalid tab index: " + std::to_string(index));
      return;
    }

    logger.Info("Closing tab: " + std::to_string(index));

    browser_to_close = tabs_[index].browser_id;
    renderer_to_destroy = std::move(tabs_[index].renderer);
    client_to_hide = tabs_[index].cef_client;
    widget_to_delete = tabs_[index].browser_widget;

    // Remove from tabs vector while holding the lock to keep state consistent
    tabs_.erase(tabs_.begin() + index);

    // Update browser widget tab indices so they stay in sync with tabs_
    for (size_t i = 0; i < tabs_.size(); ++i) {
      if (tabs_[i].browser_widget) {
        tabs_[i].browser_widget->SetTabIndex(i);
      }
    }

    // Check if we closed the last tab
    should_close_window = tabs_.empty();

    // Adjust active tab index if needed
    if (should_close_window) {
      active_tab_index_ = 0;
      new_active_index = 0;
    } else {
      size_t previous_active = active_tab_index_;
      size_t max_index = tabs_.empty() ? 0 : tabs_.size() - 1;
      active_tab_index_ = std::min(previous_active, max_index);
      new_active_index = active_tab_index_;
    }
  }

  // Remove the tab page outside the lock to avoid re-entrant signal handling deadlocks
  {
    QSignalBlocker blocker(tabWidget_);
    tabWidget_->removeTab(static_cast<int>(index));
  }

  if (widget_to_delete) {
    widget_to_delete->deleteLater();
  }

  // Hide browser (outside lock)
  if (client_to_hide && client_to_hide->GetBrowser()) {
    client_to_hide->GetBrowser()->GetHost()->WasHidden(true);
  }

  // Cleanup renderer
  if (renderer_to_destroy) {
    renderer_to_destroy->Cleanup();
  }

  // Close the browser instance (outside lock)
  if (engine_ && browser_to_close != 0) {
    engine_->CloseBrowser(browser_to_close, false);
  }

  // If we closed the last tab, close the window
  if (should_close_window) {
    logger.Info("No tabs left, closing window");
    Close();
    return;
  }

  // Switch to the new active tab
  SwitchToTab(new_active_index);
}

void QtMainWindow::CloseTabByBrowserId(browser::BrowserId browser_id) {
  size_t index_to_close = 0;
  bool found = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [browser_id](const QtTab& t) {
      return t.browser_id == browser_id;
    });

    if (it != tabs_.end()) {
      found = true;
      index_to_close = std::distance(tabs_.begin(), it);
      logger.Info("Found tab at index " + std::to_string(index_to_close) + " for browser_id " +
                  std::to_string(browser_id));
    }
  }

  if (!found) {
    logger.Error("Tab with browser_id " + std::to_string(browser_id) + " not found");
    return;
  }

  CloseTab(index_to_close);
}

// ============================================================================
// Tab Switching
// ============================================================================

void QtMainWindow::SwitchToTab(size_t index) {
  CefClient* client_to_show = nullptr;
  CefClient* client_to_hide = nullptr;
  BrowserWidget* widget_to_update = nullptr;
  QString url;
  bool is_loading = false;
  bool can_go_back = false;
  bool can_go_forward = false;
  bool index_changed = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (index >= tabs_.size()) {
      logger.Error("Invalid tab index: " + std::to_string(index));
      return;
    }

    size_t previous_index = active_tab_index_;
    if (previous_index < tabs_.size()) {
      client_to_hide = tabs_[previous_index].cef_client;
    }

    logger.Info("Switching to tab: " + std::to_string(index));

    active_tab_index_ = index;
    QtTab& tab = tabs_[index];

    client_to_show = tab.cef_client;
    widget_to_update = tab.browser_widget;
    url = tab.url;
    is_loading = tab.is_loading;
    can_go_back = tab.can_go_back;
    can_go_forward = tab.can_go_forward;

    index_changed = (previous_index != index);
  }

  // Update UI
  UpdateAddressBar(url);
  UpdateNavigationButtons(is_loading, can_go_back, can_go_forward);

  // Hide previous browser (if different)
  if (index_changed && client_to_hide && client_to_hide != client_to_show) {
    if (auto browser = client_to_hide->GetBrowser()) {
      browser->GetHost()->WasHidden(true);
    }
  }

  // Show new browser
  if (client_to_show && client_to_show->GetBrowser()) {
    auto host = client_to_show->GetBrowser()->GetHost();
    host->WasHidden(false);
    host->SetFocus(has_focus_);

    // Force CEF to send a paint event immediately
    host->Invalidate(PET_VIEW);
  }

  // Trigger repaint (use the widget pointer we saved inside the lock)
  if (widget_to_update) {
    widget_to_update->update();
  }

  logger.Info("Switched to tab " + std::to_string(index) + ", URL: " + url.toStdString());
}

// ============================================================================
// Tab Event Handlers
// ============================================================================

void QtMainWindow::onNewTabClicked() {
  logger.Info("New tab button clicked");
  OnNewTabClicked();
}

void QtMainWindow::onTabCloseRequested(int index) {
  logger.Info("Tab close requested: " + std::to_string(index));
  OnCloseTabClicked(index);
}

void QtMainWindow::onCurrentTabChanged(int index) {
  logger.Info("Current tab changed to: " + std::to_string(index));
  if (index >= 0) {
    OnTabSwitch(index);
  }
}

void QtMainWindow::onTabMoved(int from, int to) {
  logger.Info("Tab moved from " + std::to_string(from) + " to " + std::to_string(to));

  if (from == to) {
    return;
  }

  std::lock_guard<std::mutex> lock(tabs_mutex_);

  const int tab_count = static_cast<int>(tabs_.size());
  if (from < 0 || to < 0 || from >= tab_count || to >= tab_count) {
    logger.Warn("onTabMoved: indices out of range");
    return;
  }

  QtTab moved_tab = std::move(tabs_[from]);
  tabs_.erase(tabs_.begin() + from);
  tabs_.insert(tabs_.begin() + to, std::move(moved_tab));

  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].browser_widget) {
      tabs_[i].browser_widget->SetTabIndex(i);
    }
  }

  active_tab_index_ = static_cast<size_t>(tabWidget_->currentIndex());
}

// ============================================================================
// Public Tab Interface
// ============================================================================

void QtMainWindow::OnTabSwitch(int index) {
  logger.Info("Tab switched to: " + std::to_string(index));
  if (index >= 0 && static_cast<size_t>(index) < GetTabCount()) {
    SwitchToTab(static_cast<size_t>(index));
  }
}

void QtMainWindow::OnNewTabClicked() {
  logger.Info("Creating new tab");
  CreateTab("https://www.google.com");
}

void QtMainWindow::OnCloseTabClicked(int index) {
  logger.Info("Closing tab: " + std::to_string(index));
  if (index >= 0) {
    CloseTab(static_cast<size_t>(index));
  }
}

size_t QtMainWindow::GetTabCount() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return tabs_.size();
}

size_t QtMainWindow::GetActiveTabIndex() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return active_tab_index_;
}

QtTab* QtMainWindow::GetActiveTab() {
  // No lock here - caller must lock
  if (tabs_.empty() || active_tab_index_ >= tabs_.size()) {
    return nullptr;
  }

  return &tabs_[active_tab_index_];
}

}  // namespace platform
}  // namespace athena

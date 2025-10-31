#ifndef ATHENA_PLATFORM_QT_MAINWINDOW_H_
#define ATHENA_PLATFORM_QT_MAINWINDOW_H_

#include "platform/window_system.h"

#include <memory>
#include <mutex>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <vector>

namespace athena {
namespace rendering {
class GLRenderer;
}

namespace browser {
class CefClient;
class BrowserEngine;
using BrowserId = uint64_t;
}  // namespace browser

namespace runtime {
class NodeRuntime;
}

namespace platform {

// Forward declarations
class BrowserWidget;
class AgentPanel;

/**
 * Represents a single browser tab (Qt version).
 *
 * Each tab owns its own:
 *   - BrowserWidget (GL rendering surface)
 *   - GLRenderer (texture management and rendering)
 *   - CefClient reference (non-owning, managed by BrowserEngine)
 */
struct QtTab {
  browser::BrowserId browser_id;                    // Browser instance ID
  browser::CefClient* cef_client;                   // Non-owning pointer to CefClient
  BrowserWidget* browser_widget;                    // Owned by QTabWidget, non-owning pointer here
  QString title;                                    // Page title
  QString url;                                      // Current URL
  bool is_loading;                                  // Loading state
  bool can_go_back;                                 // Can navigate back
  bool can_go_forward;                              // Can navigate forward
  std::unique_ptr<rendering::GLRenderer> renderer;  // Dedicated renderer surface
};

/**
 * Qt-based main window implementation with multi-tab support.
 *
 * Uses Qt's signal/slot system for clean event handling:
 *   - Direct C++ virtual functions for event handling
 *   - Parent-child ownership for automatic widget cleanup
 *   - QMetaObject::invokeMethod for thread-safe UI updates
 *
 * Architecture (Phase 2 - Multi-Tab Support):
 *   MainWindow -> QTabWidget -> Tab1: BrowserWidget1 -> Browser1 + GLRenderer1
 *                            -> Tab2: BrowserWidget2 -> Browser2 + GLRenderer2
 *                            -> ...
 *
 * Each tab has its own:
 *   - BrowserWidget (OpenGL rendering surface)
 *   - GLRenderer (manages textures and rendering)
 *   - CefClient (browser instance)
 *
 * Thread Safety & Threading Model:
 *   - All public methods MUST be called from Qt's main UI thread
 *   - tabs_mutex_ protects tab state accessed from both UI and CEF threads
 *   - CEF callbacks may arrive on different threads - use QMetaObject::invokeMethod
 *     with Qt::QueuedConnection to marshal calls back to the main thread
 *   - WaitForLoadToComplete() processes Qt events (QCoreApplication::processEvents())
 *     and CEF events (CefDoMessageLoopWork()) to prevent UI freezing during waits
 *   - ExecuteJavaScript() polls for results while processing events - does not block UI
 *
 * Performance Characteristics:
 *   - Event processing in WaitForLoadToComplete() keeps UI responsive during navigation
 *   - JavaScript execution returns structured objects directly (no double JSON parsing)
 *   - Lock scope minimization: extract data while holding lock, then release before
 *     calling external code (CEF, Qt widgets) to avoid deadlocks
 */
class QtMainWindow : public QMainWindow, public Window {
  Q_OBJECT

 public:
  /**
   * Create a Qt main window.
   *
   * @param config Window configuration
   * @param callbacks Event callbacks
   * @param engine Browser engine (non-owning pointer)
   * @param parent Parent widget (Qt ownership)
   */
  QtMainWindow(const WindowConfig& config,
               const WindowCallbacks& callbacks,
               browser::BrowserEngine* engine,
               QWidget* parent = nullptr);

  ~QtMainWindow() override;

  // Disable copy and move
  QtMainWindow(const QtMainWindow&) = delete;
  QtMainWindow& operator=(const QtMainWindow&) = delete;
  QtMainWindow(QtMainWindow&&) = delete;
  QtMainWindow& operator=(QtMainWindow&&) = delete;

  // ============================================================================
  // Window Properties (Window interface implementation)
  // ============================================================================

  std::string GetTitle() const override;
  void SetTitle(const std::string& title) override;

  core::Size GetSize() const override;
  void SetSize(const core::Size& size) override;

  float GetScaleFactor() const override;

  void* GetNativeHandle() const override;
  void* GetRenderWidget() const override;

  rendering::GLRenderer* GetGLRenderer() const override;

  // ============================================================================
  // Window State (Window interface implementation)
  // ============================================================================

  bool IsVisible() const override;
  void Show() override;
  void Hide() override;

  bool HasFocus() const override;
  void Focus() override;

  // ============================================================================
  // Browser Integration (Window interface implementation)
  // ============================================================================

  void SetBrowser(browser::BrowserId browser_id) override;
  browser::BrowserId GetBrowser() const override;

  // ============================================================================
  // Lifecycle (Window interface implementation)
  // ============================================================================

  void Close(bool force = false) override;
  bool IsClosed() const override;

  // ============================================================================
  // Public Methods (called from BrowserControlServer)
  // ============================================================================

  /**
   * Get the CefClient instance for the active tab.
   * Returns nullptr if no tabs exist or no browser is associated.
   * Thread-safe: uses tabs_mutex_ for access.
   */
  browser::CefClient* GetCefClient() const;

  /**
   * Get the CefClient instance for a specific tab.
   * Returns nullptr if tab doesn't exist or has no browser.
   * Thread-safe: uses tabs_mutex_ for access.
   */
  browser::CefClient* GetCefClientForTab(size_t tab_index) const;

  /**
   * Initialize the browser after window is shown and GL context is ready.
   * Called from showEvent.
   */
  void InitializeBrowser();

  /**
   * Called by BrowserWidget when its size changes.
   * @param tab_index Index of the tab being resized
   * @param width New width
   * @param height New height
   */
  void OnBrowserSizeChanged(size_t tab_index, int width, int height);

  /**
   * Load a URL in the browser.
   */
  void LoadURL(const QString& url);

  /**
   * Update the address bar with the current URL.
   * Thread-safe: can be called from any thread.
   */
  void UpdateAddressBar(const QString& url);

  /**
   * Update navigation button states.
   * Thread-safe: can be called from any thread.
   */
  void UpdateNavigationButtons(bool is_loading, bool can_go_back, bool can_go_forward);

  /**
   * Navigate back in browser history.
   */
  void GoBack();

  /**
   * Navigate forward in browser history.
   */
  void GoForward();

  /**
   * Reload the current page.
   */
  void Reload(bool ignore_cache = false);

  /**
   * Stop loading the current page.
   */
  void StopLoad();

  /**
   * Show DevTools for the current tab.
   * Opens Chrome DevTools in a new window for debugging the active tab.
   */
  void ShowDevTools();

  /**
   * Get the current URL.
   */
  QString GetCurrentUrl() const;

  /**
   * Get the HTML source of the current page.
   * This method blocks until the HTML is retrieved from CEF (with 5s timeout).
   * @return HTML source as string, or empty string on error
   */
  QString GetPageHTML() const;

  /**
   * Execute JavaScript code in the current page and return the result.
   * This method blocks until the result is available from CEF (with 5s timeout).
   * @param code JavaScript code to execute
   * @return JSON-encoded result, or error message on failure
   */
  QString ExecuteJavaScript(const QString& code) const;

  /**
   * Take a screenshot of the current page.
   * Captures the current GL framebuffer and encodes as base64 PNG.
   * Screenshots are automatically scaled to 50% resolution for optimal AI analysis.
   * @return Base64-encoded PNG image data
   */
  QString TakeScreenshot() const;

  // ============================================================================
  // Tab Management (Phase 2: Full Multi-Tab Support)
  // ============================================================================

  /**
   * Create a new tab with the specified URL.
   * @param url URL to load in the new tab
   * @return Index of created tab, or -1 on error
   */
  int CreateTab(const QString& url = "https://www.google.com");

  /**
   * Close a tab by index.
   * If this is the last tab, closes the window.
   * @param index Tab index to close
   */
  void CloseTab(size_t index);

  /**
   * Close a tab by browser ID (safer than index-based closing).
   * @param browser_id Browser ID of tab to close
   */
  void CloseTabByBrowserId(browser::BrowserId browser_id);

  /**
   * Switch to a different tab.
   * Hides the current tab's browser and shows the new tab's browser.
   * @param index Tab index to switch to
   */
  void SwitchToTab(size_t index);

  /**
   * Get the number of tabs.
   */
  size_t GetTabCount() const;

  /**
   * Get the active tab index.
   */
  size_t GetActiveTabIndex() const;

  /**
   * Get the active tab.
   * @return Pointer to active tab, or nullptr if no tabs exist
   */
  QtTab* GetActiveTab();

  /**
   * Wait until a tab has finished loading.
   * @param tab_index Tab index to monitor
   * @param timeout_ms Maximum time to wait
   * @return true if load completed, false on timeout or invalid tab
   */
  bool WaitForLoadToComplete(size_t tab_index, int timeout_ms = 15000) const;

  /**
   * Handle tab switch event from QTabWidget.
   * @param index New tab index
   */
  void OnTabSwitch(int index);

  /**
   * Handle new tab button click.
   */
  void OnNewTabClicked();

  /**
   * Handle tab close request.
   * @param index Tab index to close
   */
  void OnCloseTabClicked(int index);

 protected:
  // ============================================================================
  // Qt Event Handlers
  // ============================================================================

  void closeEvent(QCloseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;

 private slots:
  // ============================================================================
  // UI Event Slots
  // ============================================================================

  void onBackClicked();
  void onForwardClicked();
  void onReloadClicked();
  void onStopClicked();
  void onAddressBarReturnPressed();
  void onNewTabClicked();
  void onTabCloseRequested(int index);
  void onCurrentTabChanged(int index);
  void onAgentButtonClicked();
  void onAgentPanelVisibilityChanged(bool visible);
  void onTabMoved(int from, int to);
  void onSplitterMoved(int pos, int index);

 private:
  // ============================================================================
  // Setup Methods
  // ============================================================================

  void setupUI();
  void createToolbar();
  void createCentralWidget();
  void connectSignals();

  /**
   * Create the CEF browser instance for a tab after GL context is ready.
   * @param tab_index Index of the tab to create browser for
   */
  void createBrowserForTab(size_t tab_index);

  // ============================================================================
  // Member Variables
  // ============================================================================

  // Window configuration and state
  WindowConfig config_;
  WindowCallbacks callbacks_;
  browser::BrowserEngine* engine_;      // Non-owning
  runtime::NodeRuntime* node_runtime_;  // Non-owning
  bool closed_;
  bool visible_;
  bool has_focus_;
  bool browser_initialized_;

  // Qt widgets (owned by Qt parent-child system)
  QToolBar* toolbar_;
  QLineEdit* addressBar_;
  QPushButton* backButton_;
  QPushButton* forwardButton_;
  QPushButton* reloadButton_;
  QPushButton* stopButton_;
  QPushButton* newTabButton_;  // "+" button to create new tabs
  QPushButton* agentButton_;   // Toggle Agent sidebar
  QTabWidget* tabWidget_;      // Tab container (replaces single browserWidget_)
  AgentPanel* agentPanel_;     // Agent chat sidebar
  QSplitter* splitter_;        // Horizontal splitter between browser and sidebar
  int agent_panel_last_width_;  // Remember last visible width for restore

  // Tab management (Phase 2: full multi-tab support)
  std::vector<QtTab> tabs_;        // All open tabs
  size_t active_tab_index_;        // Index of currently active tab
  mutable std::mutex tabs_mutex_;  // Protects tabs_ and active_tab_index_

  QString current_url_;
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_MAINWINDOW_H_

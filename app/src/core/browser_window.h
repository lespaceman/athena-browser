#ifndef ATHENA_CORE_BROWSER_WINDOW_H_
#define ATHENA_CORE_BROWSER_WINDOW_H_

#include "browser/browser_engine.h"
#include "core/types.h"
#include "platform/window_system.h"
#include "utils/error.h"

#include <functional>
#include <memory>
#include <string>

namespace athena {

// Forward declaration
namespace runtime {
class NodeRuntime;
}

namespace core {

/**
 * Configuration for creating a browser window.
 */
struct BrowserWindowConfig {
  std::string title = "Athena Browser";
  Size size = {1200, 800};
  std::string url = "about:blank";
  bool resizable = true;
  bool enable_input = true;
  runtime::NodeRuntime* node_runtime = nullptr;  // Optional Node runtime for Agent chat
};

/**
 * Browser window event callbacks.
 */
struct BrowserWindowCallbacks {
  std::function<void(const std::string& url)> on_url_changed;
  std::function<void(const std::string& title)> on_title_changed;
  std::function<void(bool is_loading)> on_loading_state_changed;
  std::function<void(int width, int height)> on_resize;
  std::function<void()> on_close;
  std::function<void()> on_destroy;
  std::function<void(bool focused)> on_focus_changed;
};

/**
 * High-level browser window that combines platform window and browser engine.
 *
 * BrowserWindow provides a simple, high-level API for creating and managing
 * browser windows. It encapsulates the complexity of coordinating between
 * the platform layer (window management) and the browser engine (web content).
 *
 * Responsibilities:
 *   - Create and manage a platform window
 *   - Create and manage a browser instance
 *   - Coordinate window and browser lifecycle
 *   - Forward navigation commands to browser
 *   - Handle window-to-browser event forwarding
 *
 * Lifecycle:
 *   1. Create BrowserWindow with config and callbacks
 *   2. Show() - display the window and load initial URL
 *   3. Navigation operations (LoadURL, GoBack, etc.)
 *   4. Close() - close window and cleanup browser
 *   5. Destroy - automatic cleanup via RAII
 *
 * Example:
 * ```cpp
 * BrowserWindowConfig config;
 * config.title = "My Browser";
 * config.url = "https://example.com";
 *
 * BrowserWindowCallbacks callbacks;
 * callbacks.on_url_changed = [](const std::string& url) {
 *   std::cout << "Navigated to: " << url << std::endl;
 * };
 *
 * auto window = std::make_unique<BrowserWindow>(
 *     config, callbacks, &window_system, &browser_engine);
 * window->Show();
 * ```
 */
class BrowserWindow {
 public:
  /**
   * Create a browser window.
   *
   * @param config Window configuration
   * @param callbacks Event callbacks
   * @param window_system Platform window system (non-owning)
   * @param browser_engine Browser engine instance (non-owning)
   */
  BrowserWindow(const BrowserWindowConfig& config,
                const BrowserWindowCallbacks& callbacks,
                platform::WindowSystem* window_system,
                browser::BrowserEngine* browser_engine);

  /**
   * Destructor - cleans up browser and window.
   */
  ~BrowserWindow();

  // Non-copyable, movable
  BrowserWindow(const BrowserWindow&) = delete;
  BrowserWindow& operator=(const BrowserWindow&) = delete;
  BrowserWindow(BrowserWindow&&) = default;
  BrowserWindow& operator=(BrowserWindow&&) = default;

  // ============================================================================
  // Lifecycle
  // ============================================================================

  /**
   * Show the window and load the initial URL.
   * @return Ok on success, error on failure
   */
  utils::Result<void> Show();

  /**
   * Hide the window.
   */
  void Hide();

  /**
   * Close the window and browser.
   * @param force If true, close immediately without callbacks
   */
  void Close(bool force = false);

  /**
   * Check if the window is closed.
   */
  bool IsClosed() const;

  // ============================================================================
  // Window Properties
  // ============================================================================

  /**
   * Get the window title.
   */
  std::string GetTitle() const;

  /**
   * Set the window title.
   */
  void SetTitle(const std::string& title);

  /**
   * Get the window size in logical pixels.
   */
  Size GetSize() const;

  /**
   * Set the window size in logical pixels.
   */
  void SetSize(const Size& size);

  /**
   * Get the device scale factor.
   */
  float GetScaleFactor() const;

  /**
   * Check if the window is visible.
   */
  bool IsVisible() const;

  /**
   * Check if the window has focus.
   */
  bool HasFocus() const;

  /**
   * Request focus for the window.
   */
  void Focus();

  // ============================================================================
  // Navigation
  // ============================================================================

  /**
   * Load a URL in the browser.
   */
  void LoadURL(const std::string& url);

  /**
   * Navigate back in history.
   */
  void GoBack();

  /**
   * Navigate forward in history.
   */
  void GoForward();

  /**
   * Reload the current page.
   * @param ignore_cache If true, bypass cache
   */
  void Reload(bool ignore_cache = false);

  /**
   * Stop loading the current page.
   */
  void StopLoad();

  // ============================================================================
  // Browser State
  // ============================================================================

  /**
   * Check if the browser can go back.
   */
  bool CanGoBack() const;

  /**
   * Check if the browser can go forward.
   */
  bool CanGoForward() const;

  /**
   * Check if the browser is loading.
   */
  bool IsLoading() const;

  /**
   * Get the current URL.
   */
  std::string GetURL() const;

  // ============================================================================
  // Browser & Window IDs
  // ============================================================================

  /**
   * Get the browser ID.
   */
  browser::BrowserId GetBrowserId() const;

  /**
   * Get the underlying platform window.
   * @return Non-owning pointer to the window
   */
  platform::Window* GetWindow() const;
  std::shared_ptr<platform::Window> GetWindowShared() const;

 private:
  // Configuration
  BrowserWindowConfig config_;
  BrowserWindowCallbacks callbacks_;

  // Dependencies (non-owning)
  platform::WindowSystem* window_system_;
  browser::BrowserEngine* browser_engine_;

  // Owned resources
  std::shared_ptr<platform::Window> window_;
  // Note: browser_id is no longer tracked here - we delegate to the window's active tab

  // State
  bool initialized_;
  bool browser_closed_;  // Track if we've already closed the browser

  // Internal initialization
  utils::Result<void> Initialize();
  void SetupWindowCallbacks();
};

}  // namespace core
}  // namespace athena

#endif  // ATHENA_CORE_BROWSER_WINDOW_H_

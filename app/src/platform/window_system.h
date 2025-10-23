#ifndef ATHENA_PLATFORM_WINDOW_SYSTEM_H_
#define ATHENA_PLATFORM_WINDOW_SYSTEM_H_

#include "core/types.h"
#include "utils/error.h"

#include <functional>
#include <memory>
#include <string>

namespace athena {
namespace browser {
class BrowserEngine;
using BrowserId = uint64_t;
}  // namespace browser

namespace rendering {
class GLRenderer;
}

namespace runtime {
class NodeRuntime;
}

namespace platform {

// Forward declarations
class Window;

/**
 * Configuration for creating a window.
 */
struct WindowConfig {
  std::string title = "Athena Browser";
  core::Size size = {1200, 800};
  bool resizable = true;
  bool enable_input = true;
  std::string url = "about:blank";               // Initial URL to load
  runtime::NodeRuntime* node_runtime = nullptr;  // Optional Node runtime for Claude chat
};

/**
 * Window event callbacks.
 */
struct WindowCallbacks {
  std::function<void(int width, int height)> on_resize;
  std::function<void()> on_close;
  std::function<void()> on_destroy;
  std::function<void(bool focused)> on_focus_changed;
};

/**
 * Abstract window system interface.
 *
 * This interface abstracts the underlying windowing system (Qt, Win32, etc.)
 * to make the codebase more testable and maintainable.
 *
 * Responsibilities:
 *   - Create and manage native windows
 *   - Handle window lifecycle (show, hide, close, destroy)
 *   - Integrate with browser engine's message loop
 *   - Forward events to appropriate handlers
 *
 * Lifecycle:
 *   1. Create window system
 *   2. Initialize() with browser engine
 *   3. CreateWindow() - create window instances
 *   4. ShowWindow() - display window
 *   5. Run() - enter main event loop
 *   6. Windows automatically destroy on close
 *   7. Shutdown() - clean shutdown
 */
class WindowSystem {
 public:
  virtual ~WindowSystem() = default;

  // ============================================================================
  // Lifecycle Management
  // ============================================================================

  /**
   * Initialize the window system.
   * Must be called before any other operations.
   *
   * @param argc Command line argument count (passed by reference for Qt compatibility)
   * @param argv Command line arguments
   * @param engine Browser engine instance (non-owning pointer)
   * @return Ok on success, error on failure
   */
  virtual utils::Result<void> Initialize(int& argc,
                                         char* argv[],
                                         browser::BrowserEngine* engine) = 0;

  /**
   * Shutdown the window system.
   * All windows must be closed before calling this.
   */
  virtual void Shutdown() = 0;

  /**
   * Check if the window system is initialized.
   */
  virtual bool IsInitialized() const = 0;

  // ============================================================================
  // Window Management
  // ============================================================================

  /**
   * Create a new window.
   *
   * @param config Window configuration
   * @param callbacks Event callbacks
   * @return Window instance on success, error on failure
   */
  virtual utils::Result<std::shared_ptr<Window>> CreateWindow(const WindowConfig& config,
                                                              const WindowCallbacks& callbacks) = 0;

  // ============================================================================
  // Event Loop
  // ============================================================================

  /**
   * Run the main event loop.
   * This is a blocking call that returns when Quit() is called.
   */
  virtual void Run() = 0;

  /**
   * Quit the main event loop.
   * Safe to call from event handlers.
   */
  virtual void Quit() = 0;

  /**
   * Check if the event loop is running.
   */
  virtual bool IsRunning() const = 0;
};

/**
 * Abstract window interface.
 *
 * Represents a single native window.
 *
 * Responsibilities:
 *   - Manage window lifecycle (show, hide, close)
 *   - Provide native window handle for rendering
 *   - Forward input events to browser
 *   - Handle window-specific events (resize, focus, etc.)
 */
class Window {
 public:
  virtual ~Window() = default;

  // ============================================================================
  // Window Properties
  // ============================================================================

  /**
   * Get the window title.
   */
  virtual std::string GetTitle() const = 0;

  /**
   * Set the window title.
   */
  virtual void SetTitle(const std::string& title) = 0;

  /**
   * Get the window size in logical pixels.
   */
  virtual core::Size GetSize() const = 0;

  /**
   * Set the window size in logical pixels.
   */
  virtual void SetSize(const core::Size& size) = 0;

  /**
   * Get the device scale factor (for HiDPI displays).
   * Returns 1.0 for normal displays, 2.0 for Retina, etc.
   */
  virtual float GetScaleFactor() const = 0;

  /**
   * Get the native window handle.
   * Type depends on platform: QMainWindow*, HWND, NSWindow*, etc.
   */
  virtual void* GetNativeHandle() const = 0;

  /**
   * Get the native GL area/widget handle for rendering.
   * Type depends on platform: QOpenGLWidget*, etc.
   */
  virtual void* GetRenderWidget() const = 0;

  /**
   * Get the GL renderer for this window.
   * Returns nullptr if the renderer is not yet initialized (before window realization).
   */
  virtual rendering::GLRenderer* GetGLRenderer() const = 0;

  // ============================================================================
  // Window State
  // ============================================================================

  /**
   * Check if the window is visible.
   */
  virtual bool IsVisible() const = 0;

  /**
   * Show the window.
   */
  virtual void Show() = 0;

  /**
   * Hide the window.
   */
  virtual void Hide() = 0;

  /**
   * Check if the window has focus.
   */
  virtual bool HasFocus() const = 0;

  /**
   * Request focus for the window.
   */
  virtual void Focus() = 0;

  // ============================================================================
  // Browser Integration
  // ============================================================================

  /**
   * Associate a browser with this window.
   * The window will forward input events to the browser.
   *
   * @param browser_id Browser ID from the browser engine
   */
  virtual void SetBrowser(browser::BrowserId browser_id) = 0;

  /**
   * Get the associated browser ID.
   */
  virtual browser::BrowserId GetBrowser() const = 0;

  // ============================================================================
  // Lifecycle
  // ============================================================================

  /**
   * Close the window.
   * Triggers on_close callback, which may prevent closing.
   *
   * @param force If true, close immediately without callbacks
   */
  virtual void Close(bool force = false) = 0;

  /**
   * Check if the window is closed.
   */
  virtual bool IsClosed() const = 0;
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_WINDOW_SYSTEM_H_

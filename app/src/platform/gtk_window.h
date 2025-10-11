#ifndef ATHENA_PLATFORM_GTK_WINDOW_H_
#define ATHENA_PLATFORM_GTK_WINDOW_H_

#include "platform/window_system.h"
#include <gtk/gtk.h>
#include <memory>

namespace athena {
namespace rendering {
  class GLRenderer;
}

namespace browser {
  class CefClient;
  class BrowserEngine;
}

namespace platform {

/**
 * GTK-based window implementation.
 *
 * This class wraps GTK window management and integrates with:
 *   - CEF browser engine for rendering
 *   - GLRenderer for OpenGL rendering
 *   - Input event handling (mouse, keyboard, focus)
 *
 * Architecture:
 *   GtkWindow (this) -> GtkGLArea (rendering widget) -> GLRenderer -> CEF
 */
class GtkWindow : public Window {
 public:
  /**
   * Create a GTK window.
   *
   * @param config Window configuration
   * @param callbacks Event callbacks
   * @param engine Browser engine (non-owning pointer)
   */
  GtkWindow(const WindowConfig& config,
            const WindowCallbacks& callbacks,
            browser::BrowserEngine* engine);

  ~GtkWindow() override;

  // Disable copy and move
  GtkWindow(const GtkWindow&) = delete;
  GtkWindow& operator=(const GtkWindow&) = delete;
  GtkWindow(GtkWindow&&) = delete;
  GtkWindow& operator=(GtkWindow&&) = delete;

  // ============================================================================
  // Window Properties
  // ============================================================================

  std::string GetTitle() const override;
  void SetTitle(const std::string& title) override;

  core::Size GetSize() const override;
  void SetSize(const core::Size& size) override;

  float GetScaleFactor() const override;

  void* GetNativeHandle() const override;
  void* GetRenderWidget() const override;

  // ============================================================================
  // Window State
  // ============================================================================

  bool IsVisible() const override;
  void Show() override;
  void Hide() override;

  bool HasFocus() const override;
  void Focus() override;

  // ============================================================================
  // Browser Integration
  // ============================================================================

  void SetBrowser(browser::BrowserId browser_id) override;
  browser::BrowserId GetBrowser() const override;

  // ============================================================================
  // Lifecycle
  // ============================================================================

  void Close(bool force = false) override;
  bool IsClosed() const override;

  // ============================================================================
  // Internal Methods (GTK callbacks need access)
  // ============================================================================

  /**
   * Get the GLRenderer instance.
   * Returns nullptr if not yet initialized.
   */
  rendering::GLRenderer* GetGLRenderer() const { return gl_renderer_.get(); }

  /**
   * Get the CefClient instance.
   * Returns nullptr if no browser is associated.
   */
  browser::CefClient* GetCefClient() const { return cef_client_; }

  /**
   * Called when the GL area is realized (OpenGL context created).
   */
  void OnGLRealize();

  /**
   * Called when the GL area needs to render a frame.
   * @return TRUE on success, FALSE on error
   */
  gboolean OnGLRender();

  /**
   * Called when the window is realized (after widgets are created).
   */
  void OnRealize();

  /**
   * Called when the window size changes.
   */
  void OnSizeAllocate(int width, int height);

  /**
   * Called when the window receives a delete event (close button clicked).
   * @return TRUE to prevent close, FALSE to allow close
   */
  gboolean OnDelete();

  /**
   * Called when the window is being destroyed.
   */
  void OnDestroy();

  /**
   * Called when focus changes.
   */
  void OnFocusChanged(bool has_focus);

  /**
   * Update the address bar with the current URL.
   * Thread-safe: can be called from any thread.
   */
  void UpdateAddressBar(const std::string& url);

  /**
   * Update navigation button states.
   * Thread-safe: can be called from any thread.
   */
  void UpdateNavigationButtons(bool is_loading, bool can_go_back, bool can_go_forward);

  /**
   * Load a URL in the browser.
   */
  void LoadURL(const std::string& url);

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
  void Reload();

  /**
   * Stop loading the current page.
   */
  void StopLoad();

  // Friend functions for GTK idle callbacks
  friend gboolean update_address_bar_idle(gpointer user_data);
  friend gboolean update_navigation_buttons_idle(gpointer user_data);

 private:
  // Window configuration and state
  WindowConfig config_;
  WindowCallbacks callbacks_;
  browser::BrowserEngine* engine_;  // Non-owning
  browser::BrowserId browser_id_;
  bool closed_;
  bool visible_;
  bool has_focus_;

  // GTK widgets
  GtkWidget* window_;      // GtkWindow
  GtkWidget* vbox_;        // Main vertical container
  GtkWidget* toolbar_;     // Toolbar container (HBox)
  GtkWidget* back_button_;
  GtkWidget* forward_button_;
  GtkWidget* reload_button_;
  GtkWidget* stop_button_;
  GtkWidget* address_entry_;  // URL input field
  GtkWidget* gl_area_;     // GtkGLArea (rendering widget)

  // Rendering components
  std::unique_ptr<rendering::GLRenderer> gl_renderer_;
  browser::CefClient* cef_client_;  // Non-owning (managed by CEF)

  /**
   * Initialize the GTK window and widgets.
   */
  void InitializeWindow();

  /**
   * Create the toolbar with navigation controls and address bar.
   */
  void CreateToolbar();

  /**
   * Setup GTK event signals.
   */
  void SetupEventHandlers();

  /**
   * Create the browser instance with CEF.
   */
  utils::Result<void> CreateBrowser(const std::string& url);
};

/**
 * GTK-based window system implementation.
 *
 * Manages GTK initialization and the main event loop.
 * Integrates CEF message loop with GTK's event loop using g_idle_add.
 */
class GtkWindowSystem : public WindowSystem {
 public:
  GtkWindowSystem();
  ~GtkWindowSystem() override;

  // Disable copy and move
  GtkWindowSystem(const GtkWindowSystem&) = delete;
  GtkWindowSystem& operator=(const GtkWindowSystem&) = delete;
  GtkWindowSystem(GtkWindowSystem&&) = delete;
  GtkWindowSystem& operator=(GtkWindowSystem&&) = delete;

  // ============================================================================
  // Lifecycle Management
  // ============================================================================

  utils::Result<void> Initialize(int argc, char* argv[],
                                  browser::BrowserEngine* engine) override;
  void Shutdown() override;
  bool IsInitialized() const override;

  // ============================================================================
  // Window Management
  // ============================================================================

  utils::Result<std::unique_ptr<Window>> CreateWindow(
      const WindowConfig& config,
      const WindowCallbacks& callbacks) override;

  // ============================================================================
  // Event Loop
  // ============================================================================

  void Run() override;
  void Quit() override;
  bool IsRunning() const override;

 private:
  bool initialized_;
  bool running_;
  browser::BrowserEngine* engine_;  // Non-owning
  guint message_loop_source_id_;    // GTK idle callback source ID

  /**
   * GTK idle callback for CEF message loop.
   * Called regularly by GTK to process CEF events.
   */
  static gboolean OnCefMessageLoopWork(gpointer data);
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_GTK_WINDOW_H_

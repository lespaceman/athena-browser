#ifndef ATHENA_PLATFORM_GTK_WINDOW_H_
#define ATHENA_PLATFORM_GTK_WINDOW_H_

#include "platform/window_system.h"
#include <gtk/gtk.h>
#include <memory>
#include <mutex>
#include "include/cef_render_handler.h"

namespace athena {
namespace rendering {
  class GLRenderer;
}

namespace browser {
  class CefClient;
  class BrowserEngine;
}

namespace runtime {
  class NodeRuntime;
}

namespace platform {

/**
 * Represents a single browser tab.
 */
struct Tab {
  browser::BrowserId browser_id;       // Browser instance ID
  browser::CefClient* cef_client;      // Non-owning pointer to CefClient
  GtkWidget* tab_label;                // Tab label widget (for notebook)
  GtkWidget* close_button;             // Close button for this tab
  std::string title;                   // Page title
  std::string url;                     // Current URL
  bool is_loading;                     // Loading state
  bool can_go_back;                    // Can navigate back
  bool can_go_forward;                 // Can navigate forward
  std::unique_ptr<rendering::GLRenderer> renderer;  // Dedicated renderer surface
};

/**
 * GTK-based window implementation with multi-tab support.
 *
 * This class wraps GTK window management and integrates with:
 *   - CEF browser engine for rendering (multiple browser instances)
 *   - GLRenderer for OpenGL rendering (each tab owns its own renderer)
 *   - Input event handling (mouse, keyboard, focus)
 *   - Tab management (create, close, switch)
 *
 * Architecture:
 *   GtkWindow (this) -> GtkNotebook (tabs) -> Multiple browsers
 *                                           -> Each tab has its own GLRenderer
 *   Only the active tab's renderer blits to the shared GtkGLArea
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
  rendering::GLRenderer* GetGLRenderer() const override;

  /**
   * Get the CefClient instance for the active tab.
   * Returns nullptr if no tabs exist or no browser is associated.
   * Thread-safe: uses tabs_mutex_ for access.
   */
  browser::CefClient* GetCefClient() const;

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

  // ============================================================================
  // Tab Management
  // ============================================================================

  /**
   * Create a new tab with the given URL.
   * @param url URL to load in the new tab
   * @return Index of the created tab, or -1 on error
   */
  int CreateTab(const std::string& url);

  /**
   * Close a tab at the given index.
   * @param index Tab index to close
   */
  void CloseTab(size_t index);

  /**
   * Close a tab by browser ID.
   * @param browser_id Browser ID of the tab to close
   */
  void CloseTabByBrowserId(browser::BrowserId browser_id);

  /**
   * Switch to a tab at the given index.
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
  Tab* GetActiveTab();

  /**
   * Called when the notebook tab is switched.
   */
  void OnTabSwitch(int page_num);

  /**
   * Called when the new tab button is clicked.
   */
  void OnNewTabClicked();

  /**
   * Called when a tab's close button is clicked.
   */
  void OnCloseTabClicked(size_t tab_index);

  // ============================================================================
  // Claude Chat Sidebar
  // ============================================================================

  /**
   * Toggle the Claude chat sidebar visibility.
   */
  void ToggleSidebar();

  /**
   * Send a message to Claude via Node runtime.
   * @param message User message to send
   */
  void SendClaudeMessage(const std::string& message);

  /**
   * Append a message to the chat history UI.
   * @param role "user" or "assistant"
   * @param message Message text
   */
  void AppendChatMessage(const std::string& role, const std::string& message);

  /**
   * Replace the last assistant message in the chat history.
   * Used to update placeholder messages with actual responses.
   * Thread-safe: can be called from any thread.
   * @param role "user" or "assistant"
   * @param message New message text
   */
  void ReplaceLastChatMessage(const std::string& role, const std::string& message);

  /**
   * Clear all chat history.
   */
  void ClearChatHistory();

  /**
   * Trim chat history to maximum number of messages.
   * Removes oldest messages if count exceeds limit.
   */
  void TrimChatHistory();

  /**
   * Called when the chat input is activated (Enter key).
   */
  void OnChatInputActivate();

  /**
   * Called when the send button is clicked.
   */
  void OnChatSendClicked();

  /**
   * Called when the sidebar toggle button is clicked.
   */
  void OnSidebarToggleClicked();

  // Friend functions for GTK idle callbacks
  friend gboolean update_address_bar_idle(gpointer user_data);
  friend gboolean update_navigation_buttons_idle(gpointer user_data);
  friend gboolean replace_last_chat_message_idle(gpointer user_data);

 private:
  // Window configuration and state
  WindowConfig config_;
  WindowCallbacks callbacks_;
  browser::BrowserEngine* engine_;  // Non-owning
  runtime::NodeRuntime* node_runtime_;  // Non-owning
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
  GtkWidget* notebook_;    // GtkNotebook (tab container)
  GtkWidget* new_tab_button_;  // New tab button
  GtkWidget* hpaned_;      // Horizontal split container (browser | sidebar)
  GtkWidget* gl_area_;     // GtkGLArea (rendering widget) - shared across tabs

  // Claude Chat Sidebar widgets
  GtkWidget* sidebar_container_;      // Main sidebar VBox
  GtkWidget* sidebar_header_;         // Header box with title and close button
  GtkWidget* sidebar_toggle_button_;  // Toggle button in toolbar
  GtkWidget* sidebar_clear_button_;   // Clear chat history button
  GtkWidget* chat_scrolled_window_;   // Scrollable chat history
  GtkWidget* chat_text_view_;         // Text view for chat history
  GtkTextBuffer* chat_text_buffer_;   // Text buffer for chat
  GtkWidget* chat_input_box_;         // Input area container
  GtkWidget* chat_input_;             // User input entry
  GtkWidget* chat_send_button_;       // Send button

  // Tab management
  std::vector<Tab> tabs_;         // All open tabs
  size_t active_tab_index_;       // Index of currently active tab
  mutable std::mutex tabs_mutex_; // Protects tabs_ and active_tab_index_

  // Claude Chat Sidebar state
  bool sidebar_visible_;          // Track sidebar visibility
  std::string current_session_id_; // Claude conversation session ID

  /**
   * Initialize the GTK window and widgets.
   */
  void InitializeWindow();

  /**
   * Create the toolbar with navigation controls and address bar.
   */
  void CreateToolbar();

  /**
   * Create the Claude chat sidebar UI.
   */
  void CreateSidebar();

  /**
   * Setup GTK event signals.
   */
  void SetupEventHandlers();

  /**
   * Create the browser instance with CEF.
   */
  utils::Result<void> CreateBrowser(const std::string& url);

  /**
   * Queue a render for the active tab when its surface is invalidated.
   */
  void HandleTabRenderInvalidated(browser::BrowserId browser_id,
                                  CefRenderHandler::PaintElementType type);
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

  utils::Result<std::shared_ptr<Window>> CreateWindow(
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

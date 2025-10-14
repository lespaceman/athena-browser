#include "platform/gtk_window.h"
#include "platform/gtk_window_callbacks.h"
#include "browser/browser_engine.h"
#include "browser/cef_engine.h"
#include "browser/cef_client.h"
#include "rendering/gl_renderer.h"
#include "runtime/node_runtime.h"

#include "include/cef_browser.h"
#include "include/cef_app.h"

#include <GL/gl.h>
#include <iostream>
#include <thread>

namespace athena {
namespace platform {

// ============================================================================
// GtkWindow Implementation
// ============================================================================

GtkWindow::GtkWindow(const WindowConfig& config,
                     const WindowCallbacks& callbacks,
                     browser::BrowserEngine* engine)
    : config_(config),
      callbacks_(callbacks),
      engine_(engine),
      node_runtime_(config.node_runtime),
      closed_(false),
      visible_(false),
      has_focus_(false),
      window_(nullptr),
      vbox_(nullptr),
      toolbar_(nullptr),
      back_button_(nullptr),
      forward_button_(nullptr),
      reload_button_(nullptr),
      stop_button_(nullptr),
      address_entry_(nullptr),
      notebook_(nullptr),
      new_tab_button_(nullptr),
      hpaned_(nullptr),
      gl_area_(nullptr),
      sidebar_container_(nullptr),
      sidebar_header_(nullptr),
      sidebar_toggle_button_(nullptr),
      sidebar_clear_button_(nullptr),
      chat_scrolled_window_(nullptr),
      chat_text_view_(nullptr),
      chat_text_buffer_(nullptr),
      chat_input_box_(nullptr),
      chat_input_(nullptr),
      chat_send_button_(nullptr),
      active_tab_index_(0),
      sidebar_visible_(false) {
  InitializeWindow();
  SetupEventHandlers();
}

GtkWindow::~GtkWindow() {
  // Set closed flag immediately to prevent idle callbacks from running
  closed_ = true;

  // Clean up tabs before destroying the window
  // This ensures renderers are cleaned up while gl_area_ is still valid
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    for (auto& tab : tabs_) {
      // Reset the renderer unique_ptr, which calls ~GLRenderer() -> Cleanup()
      // while gl_area_ is still valid
      tab.renderer.reset();
    }
    tabs_.clear();
  }

  if (window_) {
    gtk_widget_destroy(window_);
  }
}

rendering::GLRenderer* GtkWindow::GetGLRenderer() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  if (active_tab_index_ < tabs_.size()) {
    return tabs_[active_tab_index_].renderer.get();
  }
  return nullptr;
}

void GtkWindow::InitializeWindow() {
  // Create GTK window
  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(reinterpret_cast<::GtkWindow*>(window_), config_.title.c_str());
  gtk_window_set_default_size(reinterpret_cast<::GtkWindow*>(window_), config_.size.width, config_.size.height);

  if (config_.resizable) {
    gtk_window_set_resizable(reinterpret_cast<::GtkWindow*>(window_), TRUE);
  }

  // Create main vertical container
  vbox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(window_), vbox_);

  // Create toolbar with address bar and navigation buttons
  CreateToolbar();
  gtk_box_pack_start(GTK_BOX(vbox_), toolbar_, FALSE, FALSE, 0);

  // Create notebook for tabs
  notebook_ = gtk_notebook_new();
  gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook_), TRUE);
  gtk_notebook_popup_enable(GTK_NOTEBOOK(notebook_));
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook_), TRUE);
  gtk_widget_set_size_request(notebook_, -1, 30);  // Minimum height for tab visibility
  gtk_box_pack_start(GTK_BOX(vbox_), notebook_, FALSE, TRUE, 0);

  // Create horizontal paned container for browser and sidebar
  hpaned_ = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox_), hpaned_, TRUE, TRUE, 0);

  // Create GL area for hardware-accelerated rendering (left pane)
  gl_area_ = gtk_gl_area_new();
  gtk_paned_pack1(GTK_PANED(hpaned_), gl_area_, TRUE, FALSE);  // Resizable, not shrinkable

  // Configure GL area
  gtk_gl_area_set_auto_render(GTK_GL_AREA(gl_area_), FALSE);  // Manual rendering
  gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl_area_), FALSE);

  // Create sidebar (right pane)
  CreateSidebar();
  gtk_paned_pack2(GTK_PANED(hpaned_), sidebar_container_, FALSE, TRUE);  // Not resizable, shrinkable
  gtk_paned_set_position(GTK_PANED(hpaned_), config_.size.width);  // Initially hide sidebar (position at far right)

  if (config_.enable_input) {
    // Enable focus for keyboard events
    gtk_widget_set_can_focus(gl_area_, TRUE);

    // Add event masks
    gtk_widget_add_events(gl_area_,
      GDK_BUTTON_PRESS_MASK |
      GDK_BUTTON_RELEASE_MASK |
      GDK_POINTER_MOTION_MASK |
      GDK_SCROLL_MASK |
      GDK_KEY_PRESS_MASK |
      GDK_KEY_RELEASE_MASK |
      GDK_FOCUS_CHANGE_MASK |
      GDK_LEAVE_NOTIFY_MASK);
  }
}

void GtkWindow::CreateToolbar() {
  // Create horizontal toolbar container
  toolbar_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_margin_start(toolbar_, 5);
  gtk_widget_set_margin_end(toolbar_, 5);
  gtk_widget_set_margin_top(toolbar_, 5);
  gtk_widget_set_margin_bottom(toolbar_, 5);

  // Create navigation buttons
  back_button_ = gtk_button_new_with_label("◄");
  forward_button_ = gtk_button_new_with_label("►");
  reload_button_ = gtk_button_new_with_label("↻");
  stop_button_ = gtk_button_new_with_label("■");

  // Create address entry
  address_entry_ = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(address_entry_), "Enter URL or search...");

  // Create new tab button
  new_tab_button_ = gtk_button_new_with_label("+");
  gtk_widget_set_tooltip_text(new_tab_button_, "New Tab");

  // Create sidebar toggle button
  sidebar_toggle_button_ = gtk_button_new_with_label("💬");
  gtk_widget_set_tooltip_text(sidebar_toggle_button_, "Toggle Claude Chat (Ctrl+Shift+C)");

  // Pack widgets into toolbar
  gtk_box_pack_start(GTK_BOX(toolbar_), back_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), forward_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), reload_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), stop_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), address_entry_, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), new_tab_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), sidebar_toggle_button_, FALSE, FALSE, 0);

  // Initially disable navigation buttons (will be enabled when browser loads)
  gtk_widget_set_sensitive(back_button_, FALSE);
  gtk_widget_set_sensitive(forward_button_, FALSE);
  gtk_widget_set_sensitive(reload_button_, FALSE);
  gtk_widget_set_sensitive(stop_button_, FALSE);
}

void GtkWindow::CreateSidebar() {
  // Main sidebar container (300px width)
  sidebar_container_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request(sidebar_container_, 400, -1);

  // Header with title and close button
  sidebar_header_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_margin_start(sidebar_header_, 10);
  gtk_widget_set_margin_end(sidebar_header_, 10);
  gtk_widget_set_margin_top(sidebar_header_, 10);
  gtk_widget_set_margin_bottom(sidebar_header_, 10);

  GtkWidget* title_label = gtk_label_new("Claude Chat");
  gtk_widget_set_halign(title_label, GTK_ALIGN_START);
  PangoAttrList* attrs = pango_attr_list_new();
  pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  pango_attr_list_insert(attrs, pango_attr_scale_new(1.2));
  gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
  pango_attr_list_unref(attrs);

  sidebar_clear_button_ = gtk_button_new_with_label("🗑");
  gtk_widget_set_tooltip_text(sidebar_clear_button_, "Clear Chat History");

  GtkWidget* close_button = gtk_button_new_with_label("✕");
  gtk_widget_set_halign(close_button, GTK_ALIGN_END);

  gtk_box_pack_start(GTK_BOX(sidebar_header_), title_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(sidebar_header_), sidebar_clear_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sidebar_header_), close_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sidebar_container_), sidebar_header_, FALSE, FALSE, 0);

  // Separator
  GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(sidebar_container_), separator, FALSE, FALSE, 0);

  // Chat history (scrollable text view)
  chat_scrolled_window_ = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(chat_scrolled_window_),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  chat_text_view_ = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_text_view_), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(chat_text_view_), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_text_view_), GTK_WRAP_WORD_CHAR);
  gtk_widget_set_margin_start(chat_text_view_, 10);
  gtk_widget_set_margin_end(chat_text_view_, 10);
  gtk_widget_set_margin_top(chat_text_view_, 10);
  gtk_widget_set_margin_bottom(chat_text_view_, 10);

  chat_text_buffer_ = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view_));

  // Create text tags for styling
  gtk_text_buffer_create_tag(chat_text_buffer_, "user",
                              "weight", PANGO_WEIGHT_BOLD,
                              "foreground", "#2563eb",
                              nullptr);
  gtk_text_buffer_create_tag(chat_text_buffer_, "assistant",
                              "weight", PANGO_WEIGHT_BOLD,
                              "foreground", "#16a34a",
                              nullptr);
  gtk_text_buffer_create_tag(chat_text_buffer_, "message",
                              "left-margin", 10,
                              nullptr);

  gtk_container_add(GTK_CONTAINER(chat_scrolled_window_), chat_text_view_);
  gtk_box_pack_start(GTK_BOX(sidebar_container_), chat_scrolled_window_, TRUE, TRUE, 0);

  // Input box at bottom
  chat_input_box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_margin_start(chat_input_box_, 10);
  gtk_widget_set_margin_end(chat_input_box_, 10);
  gtk_widget_set_margin_top(chat_input_box_, 5);
  gtk_widget_set_margin_bottom(chat_input_box_, 10);

  chat_input_ = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(chat_input_), "Ask Claude anything...");

  chat_send_button_ = gtk_button_new_with_label("➤");
  gtk_widget_set_size_request(chat_send_button_, 40, -1);

  gtk_box_pack_start(GTK_BOX(chat_input_box_), chat_input_, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(chat_input_box_), chat_send_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sidebar_container_), chat_input_box_, FALSE, FALSE, 0);

  // Connect close button to toggle
  g_signal_connect_swapped(close_button, "clicked",
                            G_CALLBACK(+[](GtkWindow* self) { self->ToggleSidebar(); }),
                            this);

  // Connect clear button to ClearChatHistory
  g_signal_connect_swapped(sidebar_clear_button_, "clicked",
                            G_CALLBACK(+[](GtkWindow* self) { self->ClearChatHistory(); }),
                            this);

  std::cout << "[GtkWindow] Sidebar created" << std::endl;
}

void GtkWindow::SetupEventHandlers() {
  callbacks::RegisterWindowCallbacks(window_, this);
  callbacks::RegisterGLCallbacks(gl_area_, this);
  callbacks::RegisterTabCallbacks(notebook_, this);

  if (config_.enable_input) {
    callbacks::RegisterInputCallbacks(gl_area_, this);
  }

  callbacks::RegisterToolbarCallbacks(
    back_button_, forward_button_, reload_button_,
    stop_button_, address_entry_, new_tab_button_, this);

  callbacks::RegisterSidebarCallbacks(
    chat_input_, chat_send_button_, sidebar_toggle_button_, this);
}

utils::Result<void> GtkWindow::CreateBrowser(const std::string& url) {
  // Legacy method - for backward compatibility, create a tab
  // Modern code should use CreateTab() directly
  int tab_index = CreateTab(url);
  if (tab_index >= 0) {
    return utils::Ok();
  }
  return utils::Error("Failed to create tab");
}

// ============================================================================
// Window Properties
// ============================================================================

browser::CefClient* GtkWindow::GetCefClient() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  if (active_tab_index_ < tabs_.size()) {
    return tabs_[active_tab_index_].cef_client;
  }
  return nullptr;
}

std::string GtkWindow::GetTitle() const {
  if (!window_) return config_.title;
  const char* title = gtk_window_get_title(reinterpret_cast<::GtkWindow*>(window_));
  return title ? std::string(title) : std::string();
}

void GtkWindow::SetTitle(const std::string& title) {
  config_.title = title;
  if (window_) {
    gtk_window_set_title(reinterpret_cast<::GtkWindow*>(window_), title.c_str());
  }
}

core::Size GtkWindow::GetSize() const {
  if (!window_) return config_.size;

  GtkAllocation allocation;
  gtk_widget_get_allocation(gl_area_, &allocation);
  return {allocation.width, allocation.height};
}

void GtkWindow::SetSize(const core::Size& size) {
  config_.size = size;
  if (window_) {
    gtk_window_resize(reinterpret_cast<::GtkWindow*>(window_), size.width, size.height);
  }
}

float GtkWindow::GetScaleFactor() const {
  if (!gl_area_) return 1.0f;
  return static_cast<float>(gtk_widget_get_scale_factor(gl_area_));
}

void* GtkWindow::GetNativeHandle() const {
  return window_;
}

void* GtkWindow::GetRenderWidget() const {
  return gl_area_;
}

// ============================================================================
// Window State
// ============================================================================

bool GtkWindow::IsVisible() const {
  return visible_;
}

void GtkWindow::Show() {
  if (window_) {
    gtk_widget_show_all(window_);
    visible_ = true;
  }
}

void GtkWindow::Hide() {
  if (window_) {
    gtk_widget_hide(window_);
    visible_ = false;
  }
}

bool GtkWindow::HasFocus() const {
  return has_focus_;
}

void GtkWindow::Focus() {
  if (window_) {
    gtk_window_present(reinterpret_cast<::GtkWindow*>(window_));
  }
  if (gl_area_) {
    gtk_widget_grab_focus(gl_area_);
  }
}

// ============================================================================
// Browser Integration
// ============================================================================

void GtkWindow::SetBrowser(browser::BrowserId browser_id) {
  // Legacy method - with tab support, browsers are managed per-tab
  // Find the tab with this browser_id and switch to it
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].browser_id == browser_id) {
      active_tab_index_ = i;
      std::cout << "[GtkWindow] Switched to tab with browser ID: " << browser_id << std::endl;
      return;
    }
  }
  std::cerr << "[GtkWindow] Browser ID " << browser_id << " not found in any tab" << std::endl;
}

browser::BrowserId GtkWindow::GetBrowser() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  if (active_tab_index_ < tabs_.size()) {
    return tabs_[active_tab_index_].browser_id;
  }
  return 0;
}

// ============================================================================
// Lifecycle
// ============================================================================

void GtkWindow::Close(bool force) {
  if (closed_) return;

  auto* client = GetCefClient();
  if (!force && client && client->GetBrowser()) {
    client->GetBrowser()->GetHost()->CloseBrowser(false);
  } else if (window_) {
    gtk_widget_destroy(window_);
  }
}

bool GtkWindow::IsClosed() const {
  return closed_;
}

// ============================================================================
// GTK Callbacks
// ============================================================================

void GtkWindow::OnGLRealize() {
  gtk_gl_area_make_current(GTK_GL_AREA(gl_area_));

  if (gtk_gl_area_get_error(GTK_GL_AREA(gl_area_)) != nullptr) {
    std::cerr << "[GtkWindow] OpenGL context error" << std::endl;
    return;
  }

  std::cout << "[GtkWindow] OpenGL context realized successfully" << std::endl;
}

gboolean GtkWindow::OnGLRender() {
  rendering::GLRenderer* renderer = GetGLRenderer();
  if (!renderer) {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return TRUE;
  }

  auto result = renderer->Render();
  if (!result) {
    std::cerr << "[GtkWindow] Render failed: " << result.GetError().Message() << std::endl;
    return FALSE;
  }

  return TRUE;
}

void GtkWindow::OnRealize() {
  std::cout << "[GtkWindow] Window realized, ready to create initial tab" << std::endl;

  // Create the initial tab now that the window is realized and GLRenderer is available
  // Use the URL from the config
  int tab_index = CreateTab(config_.url);
  if (tab_index < 0) {
    std::cerr << "[GtkWindow] Failed to create initial tab" << std::endl;
  } else {
    std::cout << "[GtkWindow] Initial tab created successfully" << std::endl;
  }
}

void GtkWindow::OnSizeAllocate(int width, int height) {
  config_.size = {width, height};

  // Get actual GL area size (which may be smaller than window due to sidebar)
  GtkAllocation gl_allocation;
  gtk_widget_get_allocation(gl_area_, &gl_allocation);
  int gl_width = gl_allocation.width;
  int gl_height = gl_allocation.height;

  // Resize all tabs with GL area size, not window size
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    for (auto& tab : tabs_) {
      if (tab.cef_client) {
        tab.cef_client->SetSize(gl_width, gl_height);
      }
      // Also update renderer view size
      if (tab.renderer) {
        tab.renderer->SetViewSize(gl_width, gl_height);
      }
    }
  }

  if (callbacks_.on_resize) {
    callbacks_.on_resize(width, height);
  }
}

gboolean GtkWindow::OnDelete() {
  if (callbacks_.on_close) {
    callbacks_.on_close();
  }

  auto* client = GetCefClient();
  if (client && client->GetBrowser()) {
    client->GetBrowser()->GetHost()->CloseBrowser(false);
    return TRUE;  // Prevent immediate close
  }

  return FALSE;  // Allow close
}

void GtkWindow::OnDestroy() {
  closed_ = true;
  visible_ = false;

  if (callbacks_.on_destroy) {
    callbacks_.on_destroy();
  }
}

void GtkWindow::OnFocusChanged(bool focused) {
  has_focus_ = focused;

  // Only set focus on the active tab
  auto* client = GetCefClient();
  if (client && client->GetBrowser()) {
    client->GetBrowser()->GetHost()->SetFocus(focused);
  }

  if (callbacks_.on_focus_changed) {
    callbacks_.on_focus_changed(focused);
  }
}

// ============================================================================
// Navigation Methods
// ============================================================================

void GtkWindow::LoadURL(const std::string& url) {
  browser::BrowserId browser_id = GetBrowser();
  if (engine_ && browser_id != 0) {
    engine_->LoadURL(browser_id, url);
  }
}

void GtkWindow::GoBack() {
  browser::BrowserId browser_id = GetBrowser();
  if (engine_ && browser_id != 0) {
    engine_->GoBack(browser_id);
  }
}

void GtkWindow::GoForward() {
  browser::BrowserId browser_id = GetBrowser();
  if (engine_ && browser_id != 0) {
    engine_->GoForward(browser_id);
  }
}

void GtkWindow::Reload() {
  browser::BrowserId browser_id = GetBrowser();
  if (engine_ && browser_id != 0) {
    engine_->Reload(browser_id);
  }
}

void GtkWindow::StopLoad() {
  browser::BrowserId browser_id = GetBrowser();
  if (engine_ && browser_id != 0) {
    engine_->StopLoad(browser_id);
  }
}

// ============================================================================
// Address Bar Update Methods
// ============================================================================

// Helper structure for thread-safe GTK updates
struct AddressBarUpdateData {
  GtkWindow* window;
  std::string url;
};

struct NavigationButtonsUpdateData {
  GtkWindow* window;
  bool is_loading;
  bool can_go_back;
  bool can_go_forward;
};

// GTK idle callback to update address bar on main thread
gboolean update_address_bar_idle(gpointer user_data) {
  auto* data = static_cast<AddressBarUpdateData*>(user_data);
  if (data && data->window && !data->window->IsClosed() &&
      data->window->address_entry_ && GTK_IS_WIDGET(data->window->address_entry_)) {
    gtk_entry_set_text(GTK_ENTRY(data->window->address_entry_), data->url.c_str());
  }
  delete data;
  return G_SOURCE_REMOVE;
}

// GTK idle callback to update navigation buttons on main thread
gboolean update_navigation_buttons_idle(gpointer user_data) {
  auto* data = static_cast<NavigationButtonsUpdateData*>(user_data);
  if (data && data->window && !data->window->IsClosed()) {
    // Update back/forward buttons based on history state
    if (data->window->back_button_ && GTK_IS_WIDGET(data->window->back_button_)) {
      gtk_widget_set_sensitive(data->window->back_button_, data->can_go_back);
    }
    if (data->window->forward_button_ && GTK_IS_WIDGET(data->window->forward_button_)) {
      gtk_widget_set_sensitive(data->window->forward_button_, data->can_go_forward);
    }

    // Update reload/stop buttons based on loading state
    if (data->window->reload_button_ && GTK_IS_WIDGET(data->window->reload_button_)) {
      gtk_widget_set_sensitive(data->window->reload_button_, !data->is_loading);
    }
    if (data->window->stop_button_ && GTK_IS_WIDGET(data->window->stop_button_)) {
      gtk_widget_set_sensitive(data->window->stop_button_, data->is_loading);
    }
  }
  delete data;
  return G_SOURCE_REMOVE;
}

void GtkWindow::UpdateAddressBar(const std::string& url) {
  // Thread-safe: marshal to GTK main thread using g_idle_add
  auto* data = new AddressBarUpdateData{this, url};
  g_idle_add(update_address_bar_idle, data);
}

void GtkWindow::UpdateNavigationButtons(bool is_loading, bool can_go_back, bool can_go_forward) {
  // Thread-safe: marshal to GTK main thread using g_idle_add
  auto* data = new NavigationButtonsUpdateData{this, is_loading, can_go_back, can_go_forward};
  g_idle_add(update_navigation_buttons_idle, data);
}

void GtkWindow::HandleTabRenderInvalidated(
    browser::BrowserId browser_id,
    CefRenderHandler::PaintElementType type) {
  (void)type;

  bool should_render = false;
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (active_tab_index_ < tabs_.size() &&
        tabs_[active_tab_index_].browser_id == browser_id) {
      should_render = true;
    }
  }

  if (should_render && gl_area_) {
    gtk_gl_area_queue_render(GTK_GL_AREA(gl_area_));
  }
}

// ============================================================================
// Claude Chat Sidebar Methods
// ============================================================================

// Sidebar methods are implemented in gtk_window_sidebar.cpp

// ============================================================================
// Tab Management Methods
// ============================================================================

// Tab management methods are implemented in gtk_window_tabs.cpp

}  // namespace platform
}  // namespace athena

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
  if (window_ && !closed_) {
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

  GtkWidget* close_button = gtk_button_new_with_label("✕");
  gtk_widget_set_halign(close_button, GTK_ALIGN_END);

  gtk_box_pack_start(GTK_BOX(sidebar_header_), title_label, TRUE, TRUE, 0);
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
  if (data && data->window && data->window->address_entry_) {
    gtk_entry_set_text(GTK_ENTRY(data->window->address_entry_), data->url.c_str());
  }
  delete data;
  return G_SOURCE_REMOVE;
}

// GTK idle callback to update navigation buttons on main thread
gboolean update_navigation_buttons_idle(gpointer user_data) {
  auto* data = static_cast<NavigationButtonsUpdateData*>(user_data);
  if (data && data->window) {
    // Update back/forward buttons based on history state
    if (data->window->back_button_) {
      gtk_widget_set_sensitive(data->window->back_button_, data->can_go_back);
    }
    if (data->window->forward_button_) {
      gtk_widget_set_sensitive(data->window->forward_button_, data->can_go_forward);
    }

    // Update reload/stop buttons based on loading state
    if (data->window->reload_button_) {
      gtk_widget_set_sensitive(data->window->reload_button_, !data->is_loading);
    }
    if (data->window->stop_button_) {
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

void GtkWindow::ToggleSidebar() {
  sidebar_visible_ = !sidebar_visible_;

  if (sidebar_visible_) {
    // Show sidebar: adjust paned position to make room for sidebar (400px)
    GtkAllocation allocation;
    gtk_widget_get_allocation(hpaned_, &allocation);
    int new_position = allocation.width - 400;  // 400px sidebar width
    gtk_paned_set_position(GTK_PANED(hpaned_), new_position);
    gtk_widget_show(sidebar_container_);
    gtk_widget_grab_focus(chat_input_);  // Focus input when opening
    std::cout << "[GtkWindow] Sidebar opened" << std::endl;
  } else {
    // Hide sidebar: move paned position to far right
    GtkAllocation allocation;
    gtk_widget_get_allocation(hpaned_, &allocation);
    gtk_paned_set_position(GTK_PANED(hpaned_), allocation.width);
    std::cout << "[GtkWindow] Sidebar closed" << std::endl;
  }

  // Force resize of GL area and browser after sidebar toggle
  // This ensures the browser renders at the correct size
  g_idle_add([](gpointer user_data) -> gboolean {
    GtkWindow* self = static_cast<GtkWindow*>(user_data);

    // Get new GL area size after paned position change
    GtkAllocation gl_allocation;
    gtk_widget_get_allocation(self->gl_area_, &gl_allocation);

    // Notify CEF of the new size
    {
      std::lock_guard<std::mutex> lock(self->tabs_mutex_);
      for (auto& tab : self->tabs_) {
        if (tab.cef_client) {
          tab.cef_client->SetSize(gl_allocation.width, gl_allocation.height);
        }
        if (tab.renderer) {
          tab.renderer->SetViewSize(gl_allocation.width, gl_allocation.height);
        }
      }
    }

    // Queue a render to update the display
    if (self->gl_area_) {
      gtk_gl_area_queue_render(GTK_GL_AREA(self->gl_area_));
    }

    return G_SOURCE_REMOVE;
  }, this);
}

void GtkWindow::SendClaudeMessage(const std::string& message) {
  if (message.empty()) {
    std::cerr << "[GtkWindow] Cannot send empty message" << std::endl;
    return;
  }

  // Append user message to chat immediately
  AppendChatMessage("user", message);

  std::cout << "[GtkWindow] Sending message to Claude: " << message << std::endl;

  // Check if node runtime is available
  if (!node_runtime_ || !node_runtime_->IsReady()) {
    std::cerr << "[GtkWindow] Node runtime not available" << std::endl;
    AppendChatMessage("assistant", "[Error] Claude Agent is not available. Please ensure Node.js runtime is running.");
    return;
  }

  // Show placeholder message immediately
  AppendChatMessage("assistant", "⏳ Thinking...");

  // Capture necessary data for the background thread
  // Copy the message and capture the node_runtime pointer
  std::string message_copy = message;
  runtime::NodeRuntime* node_runtime = node_runtime_;

  // Launch background thread to make the blocking API call
  std::thread([this, message_copy, node_runtime]() {
    // Build JSON request body (new Athena Agent format)
    // Escape quotes in message for JSON
    std::string escaped_message = message_copy;
    size_t pos = 0;
    while ((pos = escaped_message.find("\"", pos)) != std::string::npos) {
      escaped_message.replace(pos, 1, "\\\"");
      pos += 2;
    }
    std::string json_body = "{\"message\":\"" + escaped_message + "\"}";

    // Call the Athena Agent API (THIS BLOCKS FOR 5-15 SECONDS)
    auto response = node_runtime->Call("POST", "/v1/chat/send", json_body);

    if (!response.IsOk()) {
      std::cerr << "[GtkWindow] Failed to get response from Claude: "
                << response.GetError().Message() << std::endl;

      // Replace placeholder with error (thread-safe via g_idle_add)
      std::string error_msg = "[Error] Failed to communicate with Claude Agent: " + response.GetError().Message();
      this->ReplaceLastChatMessage("assistant", error_msg);
      return;
    }

    // Parse the JSON response from Athena Agent
    // New format: {"success": true/false, "response": "...", "error": "..."}
    std::string response_body = response.Value();

    std::cout << "[GtkWindow] Athena Agent response received (length=" << response_body.length() << ")" << std::endl;

    // Check for success field
    size_t success_pos = response_body.find("\"success\":");
    bool success = false;
    if (success_pos != std::string::npos) {
      size_t true_pos = response_body.find("true", success_pos);
      size_t false_pos = response_body.find("false", success_pos);
      if (true_pos != std::string::npos && (false_pos == std::string::npos || true_pos < false_pos)) {
        success = true;
      }
    }

    if (!success) {
      // Extract error message
      size_t error_pos = response_body.find("\"error\":\"");
      std::string error_msg;
      if (error_pos != std::string::npos) {
        size_t start = error_pos + 9;
        size_t end = response_body.find("\"", start);
        error_msg = "[Error] " + response_body.substr(start, end - start);
      } else {
        error_msg = "[Error] Request failed with unknown error";
      }

      this->ReplaceLastChatMessage("assistant", error_msg);
      return;
    }

    // Extract response field
    size_t response_pos = response_body.find("\"response\":\"");
    if (response_pos == std::string::npos) {
      this->ReplaceLastChatMessage("assistant", "[Error] Unexpected response format from Claude Agent");
      return;
    }

    // Extract the response string
    size_t start = response_pos + 12;  // Skip past "response":"
    size_t end = start;
    int escape_count = 0;

    // Find the end of the string, accounting for escaped quotes
    while (end < response_body.length()) {
      if (response_body[end] == '\\') {
        escape_count++;
        end++;
        continue;
      }
      if (response_body[end] == '\"' && escape_count % 2 == 0) {
        break;
      }
      escape_count = 0;
      end++;
    }

    std::string claude_response = response_body.substr(start, end - start);

    // Unescape basic JSON escape sequences
    pos = 0;
    while ((pos = claude_response.find("\\n", pos)) != std::string::npos) {
      claude_response.replace(pos, 2, "\n");
      pos += 1;
    }
    pos = 0;
    while ((pos = claude_response.find("\\\"", pos)) != std::string::npos) {
      claude_response.replace(pos, 2, "\"");
      pos += 1;
    }

    // Replace placeholder with actual response (thread-safe via g_idle_add)
    this->ReplaceLastChatMessage("assistant", claude_response);
  }).detach();  // Detach thread so it runs independently
}

void GtkWindow::AppendChatMessage(const std::string& role, const std::string& message) {
  if (!chat_text_buffer_) {
    std::cerr << "[GtkWindow] Chat text buffer not initialized" << std::endl;
    return;
  }

  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(chat_text_buffer_, &end_iter);

  // Add role prefix (User: or Claude:)
  std::string prefix = (role == "user") ? "You" : "Claude";
  std::string role_text = prefix + ":\n";

  // Ensure we use the correct tag name (must match tags created in CreateSidebar)
  const char* role_tag = (role == "user") ? "user" : "assistant";

  gtk_text_buffer_insert_with_tags_by_name(chat_text_buffer_, &end_iter,
                                            role_text.c_str(), -1,
                                            role_tag, nullptr);

  // Add message content
  gtk_text_buffer_get_end_iter(chat_text_buffer_, &end_iter);
  std::string message_with_newline = message + "\n\n";
  gtk_text_buffer_insert_with_tags_by_name(chat_text_buffer_, &end_iter,
                                            message_with_newline.c_str(), -1,
                                            "message", nullptr);

  // Auto-scroll to bottom
  gtk_text_buffer_get_end_iter(chat_text_buffer_, &end_iter);
  GtkTextMark* end_mark = gtk_text_buffer_create_mark(chat_text_buffer_, nullptr, &end_iter, FALSE);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(chat_text_view_), end_mark, 0.0, TRUE, 0.0, 1.0);
  gtk_text_buffer_delete_mark(chat_text_buffer_, end_mark);

  std::cout << "[GtkWindow] Appended chat message from " << role << std::endl;
}

// Helper structure for thread-safe chat message replacement
struct ChatMessageReplaceData {
  GtkWindow* window;
  std::string role;
  std::string message;
};

// GTK idle callback to replace last chat message on main thread
gboolean replace_last_chat_message_idle(gpointer user_data) {
  auto* data = static_cast<ChatMessageReplaceData*>(user_data);

  if (!data || !data->window || !data->window->chat_text_buffer_) {
    delete data;
    return G_SOURCE_REMOVE;
  }

  GtkTextBuffer* buffer = data->window->chat_text_buffer_;

  // Search backwards for the last occurrence of the role prefix
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  std::string prefix = (data->role == "user") ? "You:\n" : "Claude:\n";

  // Find last occurrence of the role prefix
  GtkTextIter match_start, match_end;
  GtkTextIter search_start = end;
  bool found = false;

  while (gtk_text_iter_backward_search(&search_start, prefix.c_str(),
                                       GTK_TEXT_SEARCH_TEXT_ONLY,
                                       &match_start, &match_end, nullptr)) {
    // Found a match - this is the last occurrence
    found = true;

    // Find the end of this message (next role prefix or end of buffer)
    GtkTextIter msg_end = match_end;
    GtkTextIter next_user_start, next_user_end;
    GtkTextIter next_claude_start, next_claude_end;

    bool has_next_user = gtk_text_iter_forward_search(&msg_end, "You:\n",
                                                        GTK_TEXT_SEARCH_TEXT_ONLY,
                                                        &next_user_start, &next_user_end, nullptr);
    bool has_next_claude = gtk_text_iter_forward_search(&msg_end, "Claude:\n",
                                                          GTK_TEXT_SEARCH_TEXT_ONLY,
                                                          &next_claude_start, &next_claude_end, nullptr);

    // Use the earliest next prefix, or end of buffer
    GtkTextIter content_end = end;
    if (has_next_user && has_next_claude) {
      content_end = gtk_text_iter_compare(&next_user_start, &next_claude_start) < 0
                    ? next_user_start : next_claude_start;
    } else if (has_next_user) {
      content_end = next_user_start;
    } else if (has_next_claude) {
      content_end = next_claude_start;
    }

    // Delete the old message content (everything after the role prefix)
    gtk_text_buffer_delete(buffer, &match_end, &content_end);

    // Insert new message content
    GtkTextIter insert_pos = match_end;
    std::string message_with_newline = data->message + "\n\n";
    gtk_text_buffer_insert_with_tags_by_name(buffer, &insert_pos,
                                              message_with_newline.c_str(), -1,
                                              "message", nullptr);

    // Auto-scroll to bottom
    gtk_text_buffer_get_end_iter(buffer, &end);
    GtkTextMark* end_mark = gtk_text_buffer_create_mark(buffer, nullptr, &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(data->window->chat_text_view_), end_mark, 0.0, TRUE, 0.0, 1.0);
    gtk_text_buffer_delete_mark(buffer, end_mark);

    break;
  }

  if (!found) {
    std::cerr << "[GtkWindow] Could not find last message from role: " << data->role << std::endl;
  }

  delete data;
  return G_SOURCE_REMOVE;
}

void GtkWindow::ReplaceLastChatMessage(const std::string& role, const std::string& message) {
  // Thread-safe: marshal to GTK main thread using g_idle_add
  auto* data = new ChatMessageReplaceData{this, role, message};
  g_idle_add(replace_last_chat_message_idle, data);
}

void GtkWindow::OnChatInputActivate() {
  const gchar* text = gtk_entry_get_text(GTK_ENTRY(chat_input_));
  std::string message(text);

  if (!message.empty()) {
    SendClaudeMessage(message);
    gtk_entry_set_text(GTK_ENTRY(chat_input_), "");  // Clear input
  }
}

void GtkWindow::OnChatSendClicked() {
  OnChatInputActivate();  // Reuse the same logic
}

void GtkWindow::OnSidebarToggleClicked() {
  ToggleSidebar();
}

// ============================================================================
// Tab Management Methods
// ============================================================================

// Tab management methods are implemented in gtk_window_tabs.cpp

}  // namespace platform
}  // namespace athena

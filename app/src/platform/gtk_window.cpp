#include "platform/gtk_window.h"
#include "browser/browser_engine.h"
#include "browser/cef_engine.h"
#include "browser/cef_client.h"
#include "rendering/gl_renderer.h"

#include "include/cef_browser.h"
#include "include/cef_app.h"

#include <GL/gl.h>
#include <iostream>

namespace athena {
namespace platform {

namespace {

// ============================================================================
// GTK Callback Helpers
// ============================================================================

/**
 * Get GtkWindow instance from user_data pointer.
 * Helper for GTK callbacks.
 */
inline GtkWindow* GetWindowFromUserData(gpointer user_data) {
  return static_cast<GtkWindow*>(user_data);
}

// ============================================================================
// GTK Signal Callbacks - Forward to GtkWindow methods
// ============================================================================

static void on_gl_realize(GtkGLArea* gl_area, gpointer user_data) {
  (void)gl_area;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->OnGLRealize();
  }
}

static gboolean on_gl_render(GtkGLArea* gl_area, GdkGLContext* context,
                              gpointer user_data) {
  (void)gl_area;
  (void)context;
  GtkWindow* window = GetWindowFromUserData(user_data);
  return window ? window->OnGLRender() : FALSE;
}

static void on_realize(GtkWidget* widget, gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->OnRealize();
  }
}

static void on_size_allocate(GtkWidget* widget, GdkRectangle* allocation,
                              gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window && allocation) {
    window->OnSizeAllocate(allocation->width, allocation->height);
  }
}

static gboolean on_delete(GtkWidget* widget, GdkEvent* event,
                           gpointer user_data) {
  (void)widget;
  (void)event;
  GtkWindow* window = GetWindowFromUserData(user_data);
  return window ? window->OnDelete() : FALSE;
}

static void on_destroy(GtkWidget* widget, gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->OnDestroy();
  }
}

static gboolean on_focus_in(GtkWidget* widget, GdkEventFocus* event,
                             gpointer user_data) {
  (void)widget;
  (void)event;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->OnFocusChanged(true);
  }
  return FALSE;
}

static gboolean on_focus_out(GtkWidget* widget, GdkEventFocus* event,
                              gpointer user_data) {
  (void)widget;
  (void)event;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->OnFocusChanged(false);
  }
  return FALSE;
}

// ============================================================================
// Input Event Helpers (from main_gtk_osr.cpp)
// ============================================================================

static guint32 GetCefModifiers(guint state) {
  guint32 modifiers = 0;
  if (state & GDK_SHIFT_MASK)   modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (state & GDK_CONTROL_MASK) modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (state & GDK_MOD1_MASK)    modifiers |= EVENTFLAG_ALT_DOWN;
  if (state & GDK_BUTTON1_MASK) modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  if (state & GDK_BUTTON2_MASK) modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
  if (state & GDK_BUTTON3_MASK) modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
  return modifiers;
}

static int GetWindowsKeyCode(guint keyval) {
  if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9) return keyval;
  if (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z) return keyval;
  if (keyval >= GDK_KEY_a && keyval <= GDK_KEY_z) return keyval - 32;

  if (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F24) {
    return 0x70 + (keyval - GDK_KEY_F1);
  }

  switch (keyval) {
    case GDK_KEY_Return: return 0x0D;
    case GDK_KEY_Escape: return 0x1B;
    case GDK_KEY_BackSpace: return 0x08;
    case GDK_KEY_Tab: return 0x09;
    case GDK_KEY_space: return 0x20;
    case GDK_KEY_Delete: return 0x2E;
    case GDK_KEY_Home: return 0x24;
    case GDK_KEY_End: return 0x23;
    case GDK_KEY_Page_Up: return 0x21;
    case GDK_KEY_Page_Down: return 0x22;
    case GDK_KEY_Left: return 0x25;
    case GDK_KEY_Up: return 0x26;
    case GDK_KEY_Right: return 0x27;
    case GDK_KEY_Down: return 0x28;
    case GDK_KEY_Insert: return 0x2D;
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R: return 0x10;
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R: return 0x11;
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R: return 0x12;
    default: return keyval;
  }
}

// ============================================================================
// Input Event Callbacks - Mouse
// ============================================================================

static gboolean on_button_press(GtkWidget* widget, GdkEventButton* event,
                                 gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window || !window->GetCefClient()) return FALSE;

  auto* client = window->GetCefClient();
  auto browser = client->GetBrowser();
  if (!browser) return FALSE;

  CefMouseEvent mouse_event;
  mouse_event.x = (int)event->x;
  mouse_event.y = (int)event->y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  CefBrowserHost::MouseButtonType button_type;
  switch (event->button) {
    case 1: button_type = MBT_LEFT; break;
    case 2: button_type = MBT_MIDDLE; break;
    case 3: button_type = MBT_RIGHT; break;
    default: return FALSE;
  }

  int click_count = 1;
  if (event->type == GDK_2BUTTON_PRESS) click_count = 2;
  else if (event->type == GDK_3BUTTON_PRESS) click_count = 3;

  browser->GetHost()->SendMouseClickEvent(mouse_event, button_type, false, click_count);
  return TRUE;
}

static gboolean on_button_release(GtkWidget* widget, GdkEventButton* event,
                                   gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window || !window->GetCefClient()) return FALSE;

  auto* client = window->GetCefClient();
  auto browser = client->GetBrowser();
  if (!browser) return FALSE;

  CefMouseEvent mouse_event;
  mouse_event.x = (int)event->x;
  mouse_event.y = (int)event->y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  CefBrowserHost::MouseButtonType button_type;
  switch (event->button) {
    case 1: button_type = MBT_LEFT; break;
    case 2: button_type = MBT_MIDDLE; break;
    case 3: button_type = MBT_RIGHT; break;
    default: return FALSE;
  }

  browser->GetHost()->SendMouseClickEvent(mouse_event, button_type, true, 1);
  return TRUE;
}

static gboolean on_motion_notify(GtkWidget* widget, GdkEventMotion* event,
                                  gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window || !window->GetCefClient()) return FALSE;

  auto* client = window->GetCefClient();
  auto browser = client->GetBrowser();
  if (!browser) return FALSE;

  CefMouseEvent mouse_event;
  mouse_event.x = (int)event->x;
  mouse_event.y = (int)event->y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
  return TRUE;
}

static gboolean on_scroll(GtkWidget* widget, GdkEventScroll* event,
                           gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window || !window->GetCefClient()) return FALSE;

  auto* client = window->GetCefClient();
  auto browser = client->GetBrowser();
  if (!browser) return FALSE;

  CefMouseEvent mouse_event;
  mouse_event.x = (int)event->x;
  mouse_event.y = (int)event->y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  int delta_x = 0;
  int delta_y = 0;

  switch (event->direction) {
    case GDK_SCROLL_UP: delta_y = 40; break;
    case GDK_SCROLL_DOWN: delta_y = -40; break;
    case GDK_SCROLL_LEFT: delta_x = 40; break;
    case GDK_SCROLL_RIGHT: delta_x = -40; break;
    case GDK_SCROLL_SMOOTH:
      delta_x = (int)(-event->delta_x * 40);
      delta_y = (int)(-event->delta_y * 40);
      break;
  }

  browser->GetHost()->SendMouseWheelEvent(mouse_event, delta_x, delta_y);
  return TRUE;
}

static gboolean on_leave_notify(GtkWidget* widget, GdkEventCrossing* event,
                                 gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window || !window->GetCefClient()) return FALSE;

  auto* client = window->GetCefClient();
  auto browser = client->GetBrowser();
  if (!browser) return FALSE;

  CefMouseEvent mouse_event;
  mouse_event.x = (int)event->x;
  mouse_event.y = (int)event->y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  browser->GetHost()->SendMouseMoveEvent(mouse_event, true);
  return FALSE;
}

// ============================================================================
// Input Event Callbacks - Keyboard
// ============================================================================

static gboolean on_window_key_press(GtkWidget* widget, GdkEventKey* event,
                                     gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window) return FALSE;

  // Check for keyboard shortcuts with Ctrl modifier
  if (event->state & GDK_CONTROL_MASK) {
    // Ctrl+T: New tab
    if (event->keyval == GDK_KEY_t || event->keyval == GDK_KEY_T) {
      window->OnNewTabClicked();
      return TRUE;  // Handled
    }

    // Ctrl+W: Close current tab
    if (event->keyval == GDK_KEY_w || event->keyval == GDK_KEY_W) {
      size_t active_index = window->GetActiveTabIndex();
      window->CloseTab(active_index);
      return TRUE;  // Handled
    }

    // Ctrl+Tab: Next tab
    if (event->keyval == GDK_KEY_Tab) {
      if (event->state & GDK_SHIFT_MASK) {
        // Ctrl+Shift+Tab: Previous tab
        size_t count = window->GetTabCount();
        if (count > 0) {
          size_t active = window->GetActiveTabIndex();
          size_t prev = (active == 0) ? count - 1 : active - 1;
          window->SwitchToTab(prev);
        }
      } else {
        // Ctrl+Tab: Next tab
        size_t count = window->GetTabCount();
        if (count > 0) {
          size_t active = window->GetActiveTabIndex();
          size_t next = (active + 1) % count;
          window->SwitchToTab(next);
        }
      }
      return TRUE;  // Handled
    }

    // Ctrl+1 through Ctrl+9: Switch to tab 1-9
    if (event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
      size_t tab_index = event->keyval - GDK_KEY_1;
      if (tab_index < window->GetTabCount()) {
        window->SwitchToTab(tab_index);
      }
      return TRUE;  // Handled
    }
  }

  return FALSE;  // Not handled, propagate to other handlers
}

static gboolean on_key_press(GtkWidget* widget, GdkEventKey* event,
                              gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window || !window->GetCefClient()) return FALSE;

  auto* client = window->GetCefClient();
  auto browser = client->GetBrowser();
  if (!browser) return FALSE;

  CefKeyEvent key_event;
  key_event.type = KEYEVENT_RAWKEYDOWN;
  key_event.modifiers = GetCefModifiers(event->state);
  key_event.windows_key_code = GetWindowsKeyCode(event->keyval);
  key_event.native_key_code = event->hardware_keycode;
  key_event.is_system_key = false;
  key_event.character = 0;
  key_event.unmodified_character = 0;
  key_event.focus_on_editable_field = false;

  browser->GetHost()->SendKeyEvent(key_event);

  // Send CHAR event for printable characters
  guint32 unicode_char = gdk_keyval_to_unicode(event->keyval);
  if (unicode_char != 0) {
    key_event.type = KEYEVENT_CHAR;
    key_event.windows_key_code = unicode_char;
    key_event.character = unicode_char;
    key_event.unmodified_character = unicode_char;
    browser->GetHost()->SendKeyEvent(key_event);
  }

  return TRUE;
}

static gboolean on_key_release(GtkWidget* widget, GdkEventKey* event,
                                gpointer user_data) {
  (void)widget;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window || !window->GetCefClient()) return FALSE;

  auto* client = window->GetCefClient();
  auto browser = client->GetBrowser();
  if (!browser) return FALSE;

  CefKeyEvent key_event;
  key_event.type = KEYEVENT_KEYUP;
  key_event.modifiers = GetCefModifiers(event->state);
  key_event.windows_key_code = GetWindowsKeyCode(event->keyval);
  key_event.native_key_code = event->hardware_keycode;
  key_event.is_system_key = false;
  key_event.character = 0;
  key_event.unmodified_character = 0;
  key_event.focus_on_editable_field = false;

  browser->GetHost()->SendKeyEvent(key_event);
  return TRUE;
}

// ============================================================================
// Navigation and Address Bar Callbacks
// ============================================================================

static void on_back_clicked(GtkButton* button, gpointer user_data) {
  (void)button;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->GoBack();
  }
}

static void on_forward_clicked(GtkButton* button, gpointer user_data) {
  (void)button;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->GoForward();
  }
}

static void on_reload_clicked(GtkButton* button, gpointer user_data) {
  (void)button;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->Reload();
  }
}

static void on_stop_clicked(GtkButton* button, gpointer user_data) {
  (void)button;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->StopLoad();
  }
}

static void on_address_activate(GtkEntry* entry, gpointer user_data) {
  (void)entry;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (!window) return;

  const char* url_text = gtk_entry_get_text(entry);
  if (!url_text || strlen(url_text) == 0) return;

  std::string url(url_text);

  // Add protocol if missing
  if (url.find("://") == std::string::npos) {
    // Check if it looks like a domain (has a dot and no spaces)
    if (url.find('.') != std::string::npos && url.find(' ') == std::string::npos) {
      url = "https://" + url;
    } else {
      // Treat as search query
      url = "https://www.google.com/search?q=" + url;
    }
  }

  std::cout << "[GtkWindow] Loading URL: " << url << std::endl;
  window->LoadURL(url);
}

// ============================================================================
// Tab Management Callbacks
// ============================================================================

static void on_tab_switch(GtkNotebook* notebook, GtkWidget* page, guint page_num, gpointer user_data) {
  (void)notebook;
  (void)page;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->OnTabSwitch(static_cast<int>(page_num));
  }
}

static void on_new_tab_clicked(GtkButton* button, gpointer user_data) {
  (void)button;
  GtkWindow* window = GetWindowFromUserData(user_data);
  if (window) {
    window->OnNewTabClicked();
  }
}

static void on_close_tab_button_clicked(GtkButton* button, gpointer user_data) {
  (void)user_data;
  // Retrieve the window and browser_id from the button's object data
  GtkWindow* window = static_cast<GtkWindow*>(g_object_get_data(G_OBJECT(button), "window"));
  browser::BrowserId browser_id = static_cast<browser::BrowserId>(
    reinterpret_cast<uintptr_t>(g_object_get_data(G_OBJECT(button), "browser_id")));

  if (window) {
    // Find the tab with this browser_id and close it
    for (size_t i = 0; i < window->GetTabCount(); ++i) {
      // We need to access tabs_ safely, but we can't do it from here
      // Instead, add a new method CloseTabByBrowserId
      // For now, we'll assume the window has a way to find it
      // Actually, let's just iterate and find the right tab
      window->CloseTabByBrowserId(browser_id);
      break;
    }
  }
}

static gboolean on_tab_button_press(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
  (void)widget;
  // Check for middle-click (button 2)
  if (event->button == 2) {
    // Retrieve the window and browser_id from the widget's object data
    GtkWindow* window = static_cast<GtkWindow*>(g_object_get_data(G_OBJECT(widget), "window"));
    browser::BrowserId browser_id = static_cast<browser::BrowserId>(
      reinterpret_cast<uintptr_t>(g_object_get_data(G_OBJECT(widget), "browser_id")));

    if (window) {
      window->CloseTabByBrowserId(browser_id);
      return TRUE;  // Handled
    }
  }

  return FALSE;  // Not handled, allow normal processing
}

}  // anonymous namespace

// ============================================================================
// GtkWindow Implementation
// ============================================================================

GtkWindow::GtkWindow(const WindowConfig& config,
                     const WindowCallbacks& callbacks,
                     browser::BrowserEngine* engine)
    : config_(config),
      callbacks_(callbacks),
      engine_(engine),
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
      gl_area_(nullptr),
      active_tab_index_(0) {
  InitializeWindow();
  SetupEventHandlers();
}

GtkWindow::~GtkWindow() {
  if (gl_renderer_) {
    gl_renderer_->Cleanup();
  }

  if (window_ && !closed_) {
    gtk_widget_destroy(window_);
  }
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
  gtk_box_pack_start(GTK_BOX(vbox_), notebook_, FALSE, FALSE, 0);

  // Create GL area for hardware-accelerated rendering
  gl_area_ = gtk_gl_area_new();
  gtk_box_pack_start(GTK_BOX(vbox_), gl_area_, TRUE, TRUE, 0);

  // Configure GL area
  gtk_gl_area_set_auto_render(GTK_GL_AREA(gl_area_), FALSE);  // Manual rendering
  gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl_area_), FALSE);

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

  // Pack widgets into toolbar
  gtk_box_pack_start(GTK_BOX(toolbar_), back_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), forward_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), reload_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), stop_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), address_entry_, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), new_tab_button_, FALSE, FALSE, 0);

  // Initially disable navigation buttons (will be enabled when browser loads)
  gtk_widget_set_sensitive(back_button_, FALSE);
  gtk_widget_set_sensitive(forward_button_, FALSE);
  gtk_widget_set_sensitive(reload_button_, FALSE);
  gtk_widget_set_sensitive(stop_button_, FALSE);

  // Connect signals
  g_signal_connect(back_button_, "clicked", G_CALLBACK(on_back_clicked), this);
  g_signal_connect(forward_button_, "clicked", G_CALLBACK(on_forward_clicked), this);
  g_signal_connect(reload_button_, "clicked", G_CALLBACK(on_reload_clicked), this);
  g_signal_connect(stop_button_, "clicked", G_CALLBACK(on_stop_clicked), this);
  g_signal_connect(address_entry_, "activate", G_CALLBACK(on_address_activate), this);
  g_signal_connect(new_tab_button_, "clicked", G_CALLBACK(on_new_tab_clicked), this);
}

void GtkWindow::SetupEventHandlers() {
  // Window events
  g_signal_connect(window_, "delete-event", G_CALLBACK(on_delete), this);
  g_signal_connect(window_, "destroy", G_CALLBACK(on_destroy), this);

  // Keyboard shortcuts (at window level, before other handlers)
  g_signal_connect(window_, "key-press-event", G_CALLBACK(on_window_key_press), this);

  // Tab events
  g_signal_connect(notebook_, "switch-page", G_CALLBACK(on_tab_switch), this);

  // GL area events
  g_signal_connect(gl_area_, "realize", G_CALLBACK(on_gl_realize), this);
  g_signal_connect(gl_area_, "render", G_CALLBACK(on_gl_render), this);
  g_signal_connect_after(gl_area_, "realize", G_CALLBACK(on_realize), this);
  g_signal_connect(gl_area_, "size-allocate", G_CALLBACK(on_size_allocate), this);

  if (config_.enable_input) {
    // Mouse events
    g_signal_connect(gl_area_, "button-press-event", G_CALLBACK(on_button_press), this);
    g_signal_connect(gl_area_, "button-release-event", G_CALLBACK(on_button_release), this);
    g_signal_connect(gl_area_, "motion-notify-event", G_CALLBACK(on_motion_notify), this);
    g_signal_connect(gl_area_, "scroll-event", G_CALLBACK(on_scroll), this);
    g_signal_connect(gl_area_, "leave-notify-event", G_CALLBACK(on_leave_notify), this);

    // Keyboard events
    g_signal_connect(gl_area_, "key-press-event", G_CALLBACK(on_key_press), this);
    g_signal_connect(gl_area_, "key-release-event", G_CALLBACK(on_key_release), this);

    // Focus events
    g_signal_connect(gl_area_, "focus-in-event", G_CALLBACK(on_focus_in), this);
    g_signal_connect(gl_area_, "focus-out-event", G_CALLBACK(on_focus_out), this);
  }
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

  gl_renderer_ = std::make_unique<rendering::GLRenderer>();
  auto result = gl_renderer_->Initialize(gl_area_);

  if (!result) {
    std::cerr << "[GtkWindow] Failed to initialize GLRenderer: "
              << result.GetError().Message() << std::endl;
    gl_renderer_.reset();
    return;
  }

  std::cout << "[GtkWindow] OpenGL renderer initialized successfully" << std::endl;
}

gboolean GtkWindow::OnGLRender() {
  if (!gl_renderer_) {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return TRUE;
  }

  auto result = gl_renderer_->Render();
  if (!result) {
    std::cerr << "[GtkWindow] Render failed: " << result.GetError().Message() << std::endl;
    return FALSE;
  }

  return TRUE;
}

void GtkWindow::OnRealize() {
  if (!gl_renderer_) {
    std::cerr << "[GtkWindow] GLRenderer not initialized! OnGLRealize should have been called first." << std::endl;
    return;
  }

  std::cout << "[GtkWindow] Window realized, GLRenderer ready" << std::endl;

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

  // Resize all tabs, not just the active one
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    for (auto& tab : tabs_) {
      if (tab.cef_client) {
        tab.cef_client->SetSize(width, height);
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

// ============================================================================
// Tab Management Methods
// ============================================================================

int GtkWindow::CreateTab(const std::string& url) {
  if (!gl_renderer_) {
    std::cerr << "[GtkWindow::CreateTab] GLRenderer not initialized" << std::endl;
    return -1;
  }

  if (!engine_) {
    std::cerr << "[GtkWindow::CreateTab] BrowserEngine not available" << std::endl;
    return -1;
  }

  std::cout << "[GtkWindow::CreateTab] Creating tab with URL: " << url << std::endl;

  // Create Tab structure
  Tab tab;
  tab.url = url;
  tab.title = "New Tab";
  tab.is_loading = true;
  tab.can_go_back = false;
  tab.can_go_forward = false;

  // Create browser instance
  float scale_factor = static_cast<float>(gtk_widget_get_scale_factor(gl_area_));

  browser::BrowserConfig browser_config;
  browser_config.url = url;
  browser_config.width = config_.size.width;
  browser_config.height = config_.size.height;
  browser_config.device_scale_factor = scale_factor;
  browser_config.gl_renderer = gl_renderer_.get();
  browser_config.native_window_handle = gl_area_;

  auto result = engine_->CreateBrowser(browser_config);
  if (!result) {
    std::cerr << "[GtkWindow::CreateTab] Failed to create browser: "
              << result.GetError().Message() << std::endl;
    return -1;
  }

  tab.browser_id = result.Value();

  // Get the CEF client
  auto* cef_engine = dynamic_cast<browser::CefEngine*>(engine_);
  if (cef_engine) {
    auto client = cef_engine->GetCefClient(tab.browser_id);
    if (client) {
      tab.cef_client = client.get();

      // Wire up callbacks for this tab
      // Use browser_id instead of index to avoid stale references after tab closure
      browser::BrowserId bid = tab.browser_id;
      tab.cef_client->SetAddressChangeCallback([this, bid](const std::string& url) {
        std::lock_guard<std::mutex> lock(tabs_mutex_);
        auto it = std::find_if(tabs_.begin(), tabs_.end(),
          [bid](const Tab& t) { return t.browser_id == bid; });
        if (it != tabs_.end()) {
          it->url = url;
          size_t tab_index = std::distance(tabs_.begin(), it);
          if (tab_index == active_tab_index_) {
            this->UpdateAddressBar(url);
          }
        }
      });

      tab.cef_client->SetLoadingStateChangeCallback([this, bid](bool is_loading, bool can_go_back, bool can_go_forward) {
        std::lock_guard<std::mutex> lock(tabs_mutex_);
        auto it = std::find_if(tabs_.begin(), tabs_.end(),
          [bid](const Tab& t) { return t.browser_id == bid; });
        if (it != tabs_.end()) {
          it->is_loading = is_loading;
          it->can_go_back = can_go_back;
          it->can_go_forward = can_go_forward;
          size_t tab_index = std::distance(tabs_.begin(), it);
          if (tab_index == active_tab_index_) {
            this->UpdateNavigationButtons(is_loading, can_go_back, can_go_forward);
          }
        }
      });

      tab.cef_client->SetTitleChangeCallback([this, bid](const std::string& title) {
        std::lock_guard<std::mutex> lock(tabs_mutex_);
        auto it = std::find_if(tabs_.begin(), tabs_.end(),
          [bid](const Tab& t) { return t.browser_id == bid; });
        if (it != tabs_.end()) {
          it->title = title;
          // Update the tab label on the GTK main thread
          g_idle_add([](gpointer user_data) -> gboolean {
            auto* data = static_cast<std::pair<GtkWidget*, std::string>*>(user_data);
            gtk_label_set_text(GTK_LABEL(data->first), data->second.c_str());
            delete data;
            return G_SOURCE_REMOVE;
          }, new std::pair<GtkWidget*, std::string>(it->tab_label, title));
        }
      });

      std::cout << "[GtkWindow::CreateTab] Callbacks wired for browser_id " << bid << std::endl;
    }
  }

  // Create tab label with close button
  GtkWidget* tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget* label = gtk_label_new(tab.title.c_str());
  GtkWidget* close_btn = gtk_button_new_with_label("✕");
  gtk_widget_set_size_request(close_btn, 20, 20);

  gtk_box_pack_start(GTK_BOX(tab_box), label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tab_box), close_btn, FALSE, FALSE, 0);
  gtk_widget_show_all(tab_box);

  // Enable middle-click to close on the tab box
  GtkWidget* event_box = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(event_box), tab_box);
  gtk_widget_add_events(event_box, GDK_BUTTON_PRESS_MASK);
  g_object_set_data(G_OBJECT(event_box), "window", this);
  g_object_set_data(G_OBJECT(event_box), "browser_id", reinterpret_cast<gpointer>(static_cast<uintptr_t>(tab.browser_id)));
  g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_tab_button_press), nullptr);
  gtk_widget_show(event_box);

  tab.tab_label = label;
  tab.close_button = close_btn;

  // Add empty page to notebook (we don't need content, just the tab)
  GtkWidget* empty_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), empty_page, event_box);

  // Store tab
  tabs_.push_back(tab);
  size_t new_tab_index = tabs_.size() - 1;

  // Connect close button signal
  // Note: We store the browser_id as user data to identify which tab to close
  // This is critical - using tab_index would become stale after tab closures
  g_object_set_data(G_OBJECT(close_btn), "window", this);
  g_object_set_data(G_OBJECT(close_btn), "browser_id", reinterpret_cast<gpointer>(static_cast<uintptr_t>(tab.browser_id)));
  g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_tab_button_clicked), nullptr);

  // Switch to the new tab
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), new_tab_index);

  std::cout << "[GtkWindow::CreateTab] Tab created successfully, index: " << new_tab_index << std::endl;
  return static_cast<int>(new_tab_index);
}

void GtkWindow::CloseTab(size_t index) {
  browser::BrowserId browser_to_close = 0;
  size_t new_active_index = 0;
  bool should_close_window = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (index >= tabs_.size()) {
      std::cerr << "[GtkWindow::CloseTab] Invalid tab index: " << index << std::endl;
      return;
    }

    std::cout << "[GtkWindow::CloseTab] Closing tab " << index << std::endl;

    browser_to_close = tabs_[index].browser_id;

    // Remove the notebook page
    gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_), index);

    // Remove from tabs vector
    tabs_.erase(tabs_.begin() + index);

    // Check if we closed the last tab
    should_close_window = tabs_.empty();

    // Adjust active tab index if needed
    if (!should_close_window && active_tab_index_ >= tabs_.size()) {
      active_tab_index_ = tabs_.size() - 1;
    }

    new_active_index = active_tab_index_;
  }

  // Close the browser instance (outside lock)
  if (engine_ && browser_to_close != 0) {
    engine_->CloseBrowser(browser_to_close, false);
  }

  // If we closed the last tab, close the window
  if (should_close_window) {
    std::cout << "[GtkWindow::CloseTab] No tabs left, closing window" << std::endl;
    Close();
    return;
  }

  // Switch to the new active tab (outside lock to avoid deadlock)
  SwitchToTab(new_active_index);
}

void GtkWindow::CloseTabByBrowserId(browser::BrowserId browser_id) {
  size_t index_to_close = 0;
  bool found = false;
  size_t new_active_index = 0;
  bool should_close_window = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
      [browser_id](const Tab& t) { return t.browser_id == browser_id; });

    if (it != tabs_.end()) {
      found = true;
      index_to_close = std::distance(tabs_.begin(), it);
      std::cout << "[GtkWindow::CloseTabByBrowserId] Found tab at index " << index_to_close
                << " for browser_id " << browser_id << std::endl;

      // Remove the notebook page
      gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_), index_to_close);

      // Remove from tabs vector
      tabs_.erase(it);

      // Check if we closed the last tab
      should_close_window = tabs_.empty();

      // Adjust active tab index if needed
      if (!should_close_window && active_tab_index_ >= tabs_.size()) {
        active_tab_index_ = tabs_.size() - 1;
      }

      new_active_index = active_tab_index_;
    }
  }

  if (!found) {
    std::cerr << "[GtkWindow::CloseTabByBrowserId] Tab with browser_id " << browser_id
              << " not found" << std::endl;
    return;
  }

  // Close the browser instance (outside lock)
  if (engine_ && browser_id != 0) {
    engine_->CloseBrowser(browser_id, false);
  }

  // If we closed the last tab, close the window
  if (should_close_window) {
    std::cout << "[GtkWindow::CloseTabByBrowserId] No tabs left, closing window" << std::endl;
    Close();
    return;
  }

  // Switch to the new active tab (outside lock to avoid deadlock)
  SwitchToTab(new_active_index);
}

void GtkWindow::SwitchToTab(size_t index) {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  if (index >= tabs_.size()) {
    std::cerr << "[GtkWindow::SwitchToTab] Invalid tab index: " << index << std::endl;
    return;
  }

  std::cout << "[GtkWindow::SwitchToTab] Switching to tab " << index << std::endl;

  active_tab_index_ = index;
  Tab& tab = tabs_[index];

  // Update address bar and navigation buttons
  UpdateAddressBar(tab.url);
  UpdateNavigationButtons(tab.is_loading, tab.can_go_back, tab.can_go_forward);

  // Set focus to the browser
  if (tab.cef_client && tab.cef_client->GetBrowser()) {
    tab.cef_client->GetBrowser()->GetHost()->SetFocus(has_focus_);
  }

  // Request a render to show the new tab's content
  if (gl_area_) {
    gtk_gl_area_queue_render(GTK_GL_AREA(gl_area_));
  }

  std::cout << "[GtkWindow::SwitchToTab] Switched to tab " << index
            << ", URL: " << tab.url << std::endl;
}

size_t GtkWindow::GetTabCount() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return tabs_.size();
}

size_t GtkWindow::GetActiveTabIndex() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return active_tab_index_;
}

Tab* GtkWindow::GetActiveTab() {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  if (active_tab_index_ < tabs_.size()) {
    return &tabs_[active_tab_index_];
  }
  return nullptr;
}

void GtkWindow::OnTabSwitch(int page_num) {
  std::cout << "[GtkWindow::OnTabSwitch] Tab switched to page: " << page_num << std::endl;
  if (page_num >= 0) {
    size_t tab_count = GetTabCount();  // Thread-safe
    if (static_cast<size_t>(page_num) < tab_count) {
      SwitchToTab(static_cast<size_t>(page_num));
    }
  }
}

void GtkWindow::OnNewTabClicked() {
  std::cout << "[GtkWindow::OnNewTabClicked] Creating new tab" << std::endl;
  CreateTab("https://www.google.com");
}

void GtkWindow::OnCloseTabClicked(size_t tab_index) {
  std::cout << "[GtkWindow::OnCloseTabClicked] Closing tab: " << tab_index << std::endl;
  CloseTab(tab_index);
}

// ============================================================================
// GtkWindowSystem Implementation
// ============================================================================

GtkWindowSystem::GtkWindowSystem()
    : initialized_(false),
      running_(false),
      engine_(nullptr),
      message_loop_source_id_(0) {}

GtkWindowSystem::~GtkWindowSystem() {
  Shutdown();
}

utils::Result<void> GtkWindowSystem::Initialize(int argc, char* argv[],
                                                 browser::BrowserEngine* engine) {
  if (initialized_) {
    return utils::Error("WindowSystem already initialized");
  }

  if (!engine) {
    return utils::Error("BrowserEngine cannot be null");
  }

  // Disable GTK setlocale (CEF requirement)
  gtk_disable_setlocale();

  // Initialize GTK
  gtk_init(&argc, &argv);

  engine_ = engine;
  initialized_ = true;

  // Setup CEF message loop integration
  message_loop_source_id_ = g_idle_add(OnCefMessageLoopWork, this);

  return utils::Ok();
}

void GtkWindowSystem::Shutdown() {
  if (!initialized_) return;

  // Remove CEF message loop callback
  if (message_loop_source_id_ != 0) {
    g_source_remove(message_loop_source_id_);
    message_loop_source_id_ = 0;
  }

  initialized_ = false;
  running_ = false;
  engine_ = nullptr;
}

bool GtkWindowSystem::IsInitialized() const {
  return initialized_;
}

utils::Result<std::unique_ptr<Window>> GtkWindowSystem::CreateWindow(
    const WindowConfig& config,
    const WindowCallbacks& callbacks) {
  if (!initialized_) {
    return utils::Error("WindowSystem not initialized");
  }

  auto window = std::make_unique<GtkWindow>(config, callbacks, engine_);
  return std::unique_ptr<Window>(std::move(window));
}

void GtkWindowSystem::Run() {
  if (!initialized_) {
    std::cerr << "[GtkWindowSystem] Cannot run: WindowSystem not initialized" << std::endl;
    return;
  }

  running_ = true;
  gtk_main();
  running_ = false;
}

void GtkWindowSystem::Quit() {
  if (running_) {
    gtk_main_quit();
    running_ = false;
  }
}

bool GtkWindowSystem::IsRunning() const {
  return running_;
}

gboolean GtkWindowSystem::OnCefMessageLoopWork(gpointer data) {
  (void)data;
  CefDoMessageLoopWork();
  return G_SOURCE_CONTINUE;
}

}  // namespace platform
}  // namespace athena

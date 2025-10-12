/**
 * GTK Window Event Callbacks Implementation
 *
 * Handles all GTK event callbacks for GtkWindow.
 * All callbacks forward to GtkWindow methods for testability.
 */
#include "platform/gtk_window_callbacks.h"
#include "platform/gtk_window.h"
#include "browser/cef_client.h"
#include "include/cef_browser.h"

#include <iostream>
#include <string>

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
// Window Lifecycle Callbacks
// ============================================================================

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

// ============================================================================
// OpenGL Callbacks
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

}  // anonymous namespace

// ============================================================================
// Callback Registration Functions
// ============================================================================

namespace callbacks {

void RegisterWindowCallbacks(GtkWidget* window, GtkWindow* self) {
  g_signal_connect(window, "delete-event", G_CALLBACK(on_delete), self);
  g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), self);
  g_signal_connect(window, "key-press-event", G_CALLBACK(on_window_key_press), self);
}

void RegisterGLCallbacks(GtkWidget* gl_area, GtkWindow* self) {
  g_signal_connect(gl_area, "realize", G_CALLBACK(on_gl_realize), self);
  g_signal_connect(gl_area, "render", G_CALLBACK(on_gl_render), self);
  g_signal_connect_after(gl_area, "realize", G_CALLBACK(on_realize), self);
  g_signal_connect(gl_area, "size-allocate", G_CALLBACK(on_size_allocate), self);
}

void RegisterInputCallbacks(GtkWidget* gl_area, GtkWindow* self) {
  // Mouse events
  g_signal_connect(gl_area, "button-press-event", G_CALLBACK(on_button_press), self);
  g_signal_connect(gl_area, "button-release-event", G_CALLBACK(on_button_release), self);
  g_signal_connect(gl_area, "motion-notify-event", G_CALLBACK(on_motion_notify), self);
  g_signal_connect(gl_area, "scroll-event", G_CALLBACK(on_scroll), self);
  g_signal_connect(gl_area, "leave-notify-event", G_CALLBACK(on_leave_notify), self);

  // Keyboard events
  g_signal_connect(gl_area, "key-press-event", G_CALLBACK(on_key_press), self);
  g_signal_connect(gl_area, "key-release-event", G_CALLBACK(on_key_release), self);

  // Focus events
  g_signal_connect(gl_area, "focus-in-event", G_CALLBACK(on_focus_in), self);
  g_signal_connect(gl_area, "focus-out-event", G_CALLBACK(on_focus_out), self);
}

void RegisterToolbarCallbacks(GtkWidget* back_btn,
                               GtkWidget* forward_btn,
                               GtkWidget* reload_btn,
                               GtkWidget* stop_btn,
                               GtkWidget* address_entry,
                               GtkWidget* new_tab_btn,
                               GtkWindow* self) {
  g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), self);
  g_signal_connect(forward_btn, "clicked", G_CALLBACK(on_forward_clicked), self);
  g_signal_connect(reload_btn, "clicked", G_CALLBACK(on_reload_clicked), self);
  g_signal_connect(stop_btn, "clicked", G_CALLBACK(on_stop_clicked), self);
  g_signal_connect(address_entry, "activate", G_CALLBACK(on_address_activate), self);
  g_signal_connect(new_tab_btn, "clicked", G_CALLBACK(on_new_tab_clicked), self);
}

void RegisterTabCallbacks(GtkWidget* notebook, GtkWindow* self) {
  g_signal_connect(notebook, "switch-page", G_CALLBACK(on_tab_switch), self);
}

}  // namespace callbacks

}  // namespace platform
}  // namespace athena

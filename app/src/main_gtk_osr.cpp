#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/wrapper/cef_helpers.h"
#include "app_handler.h"

#include <gtk/gtk.h>
#include <cairo.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <cstring>
#include <mutex>
#include <thread>

// OSR (Off-Screen Rendering) client handler
class OSRClient : public CefClient,
                  public CefLifeSpanHandler,
                  public CefDisplayHandler,
                  public CefRenderHandler {
 public:
  OSRClient(GtkWidget* widget)
    : widget_(widget),
      buffer_(nullptr),
      width_(0),
      height_(0),
      buffer_width_(0),
      buffer_height_(0),
      device_scale_factor_(1.0f) {}

  ~OSRClient() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (buffer_) {
      delete[] buffer_;
    }
  }

  // CefClient methods
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  // CefLifeSpanHandler methods
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = browser;
    std::cout << "[OnAfterCreated] OSR Browser created! Scale factor: " << device_scale_factor_ << std::endl;
  }

  bool DoClose(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    return false;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = nullptr;
    CefQuitMessageLoop();
  }

  // CefDisplayHandler methods
  void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override {
    // Update window title
    if (widget_) {
      GtkWidget* window = gtk_widget_get_toplevel(widget_);
      if (window && GTK_IS_WINDOW(window)) {
        gtk_window_set_title(GTK_WINDOW(window), title.ToString().c_str());
      }
    }
  }

  // CefRenderHandler methods (for OSR)
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    CEF_REQUIRE_UI_THREAD();
    if (width_ > 0 && height_ > 0) {
      // Return LOGICAL size - CEF will apply device_scale_factor internally
      rect = CefRect(0, 0, width_, height_);
    } else {
      // Default size until widget is allocated
      rect = CefRect(0, 0, 1200, 800);
    }
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info) override {
    CEF_REQUIRE_UI_THREAD();
    screen_info.device_scale_factor = device_scale_factor_;
    return true;
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override {
    CEF_REQUIRE_UI_THREAD();

    if (type != PET_VIEW) {
      return;
    }

    if (width <= 0 || height <= 0 || !buffer) {
      return;
    }

    // Lock buffer for thread-safe access
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // Update buffer if size changed
    if (width != buffer_width_ || height != buffer_height_) {
      if (buffer_) {
        delete[] buffer_;
        buffer_ = nullptr;
      }
      // CRITICAL: Allocate with proper stride BEFORE updating dimensions
      // This ensures buffer size always matches buffer_width_/buffer_height_
      int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
      buffer_ = new unsigned char[stride * height];

      // NOW update buffer dimensions - this order is critical to avoid TOCTOU race
      buffer_width_ = width;
      buffer_height_ = height;

      std::cout << "[OnPaint] Buffer resized to " << width << "x" << height
                << " (stride: " << stride << ", scale: " << device_scale_factor_ << ")" << std::endl;
    }

    if (!buffer_) {
      return;
    }

    // Copy BGRA data from CEF to our buffer
    // CEF gives us tightly packed data, but Cairo may need stride alignment
    int cef_stride = width * 4;
    int cairo_stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);

    if (cef_stride == cairo_stride) {
      // Same stride, direct copy
      memcpy(buffer_, buffer, cairo_stride * height);
    } else {
      // Different stride, copy line by line
      const unsigned char* src = static_cast<const unsigned char*>(buffer);
      unsigned char* dst = buffer_;
      for (int y = 0; y < height; y++) {
        memcpy(dst, src, cef_stride);
        src += cef_stride;
        dst += cairo_stride;
      }
    }

    // Queue redraw only for dirty regions (convert physical to logical pixels)
    if (widget_) {
      for (const auto& rect : dirtyRects) {
        // Convert physical pixel rect to logical pixel rect for GTK
        gtk_widget_queue_draw_area(widget_,
                                    rect.x / device_scale_factor_,
                                    rect.y / device_scale_factor_,
                                    rect.width / device_scale_factor_,
                                    rect.height / device_scale_factor_);
      }
    }
  }

  // Draw the buffer to Cairo surface
  void DrawToSurface(cairo_t* cr, int widget_width, int widget_height) {
    // Lock buffer for thread-safe access
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (!buffer_ || buffer_width_ == 0 || buffer_height_ == 0) {
      // No content yet, draw white background
      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
      cairo_paint(cr);
      return;
    }

    // CRITICAL: Capture buffer dimensions NOW to prevent TOCTOU race condition
    int local_width = buffer_width_;
    int local_height = buffer_height_;
    float local_scale = device_scale_factor_;

    // Save Cairo state
    cairo_save(cr);

    // Create a NEW Cairo image surface and COPY our buffer data into it
    // This ensures Cairo has its own copy and isn't affected by buffer changes
    // CEF provides BGRA, which Cairo treats as ARGB32 on little-endian systems
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, local_width);
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, local_width, local_height);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
      cairo_surface_destroy(surface);
      cairo_restore(cr);
      return;
    }

    // Copy our buffer data into Cairo's surface
    cairo_surface_flush(surface);  // Prepare for direct access
    unsigned char* cairo_data = cairo_image_surface_get_data(surface);
    if (cairo_data && buffer_) {
      memcpy(cairo_data, buffer_, stride * local_height);
      cairo_surface_mark_dirty(surface);  // Tell Cairo we modified the data
    }

    // CEF rendered at physical pixels (logical_size * device_scale_factor)
    // Tell Cairo about the device scale so it renders at the correct size
    cairo_surface_set_device_scale(surface, local_scale, local_scale);

    // Now just paint directly - Cairo will handle the scaling
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(surface);
    cairo_restore(cr);
  }

  CefRefPtr<CefBrowser> GetBrowser() { return browser_; }

  void SetSize(int width, int height) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    width_ = width;
    height_ = height;
    if (browser_) {
      browser_->GetHost()->WasResized();
    }
  }

  void SetDeviceScaleFactor(float scale_factor) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (device_scale_factor_ != scale_factor) {
      device_scale_factor_ = scale_factor;
      if (browser_) {
        browser_->GetHost()->WasResized();
      }
    }
  }

 private:
  GtkWidget* widget_;
  CefRefPtr<CefBrowser> browser_;
  unsigned char* buffer_;
  int width_;   // Logical widget size
  int height_;  // Logical widget size
  int buffer_width_;   // Physical buffer dimensions (logical * scale_factor)
  int buffer_height_;  // Physical buffer dimensions (logical * scale_factor)
  float device_scale_factor_;  // HiDPI scale factor (1.0, 2.0, etc.)
  std::mutex buffer_mutex_;  // Protects buffer_, buffer_width_, buffer_height_, device_scale_factor_

  IMPLEMENT_REFCOUNTING(OSRClient);
};

// Global data
static CefRefPtr<OSRClient> g_client;

// ============================================================================
// INPUT EVENT HANDLING - Forward GTK events to CEF
// ============================================================================

// Helper: Convert GTK modifiers to CEF modifiers
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

// Helper: Transform mouse coordinates accounting for Cairo scaling
static void TransformMouseCoordinates(GtkWidget* widget, double event_x, double event_y, int* out_x, int* out_y) {
  if (!g_client) {
    *out_x = (int)event_x;
    *out_y = (int)event_y;
    return;
  }

  // Get widget size
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);

  if (allocation.width == 0 || allocation.height == 0) {
    *out_x = (int)event_x;
    *out_y = (int)event_y;
    return;
  }

  // Get CEF view rect to determine actual buffer dimensions
  CefRefPtr<CefBrowser> browser = g_client->GetBrowser();
  if (!browser) {
    *out_x = (int)event_x;
    *out_y = (int)event_y;
    return;
  }

  CefRect view_rect;
  g_client->GetViewRect(browser, view_rect);

  if (view_rect.width == 0 || view_rect.height == 0) {
    // Buffer not initialized yet, use widget coordinates
    *out_x = (int)event_x;
    *out_y = (int)event_y;
    return;
  }

  // Since GetViewRect returns logical size (same as widget size),
  // and CEF handles device scaling internally, we can pass coordinates directly
  *out_x = (int)event_x;
  *out_y = (int)event_y;
}

// Mouse button press event
static gboolean on_button_press(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
  (void)user_data;  // Unused
  if (!g_client || !g_client->GetBrowser()) {
    return FALSE;
  }

  int x, y;
  TransformMouseCoordinates(widget, event->x, event->y, &x, &y);

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  CefBrowserHost::MouseButtonType button_type;
  switch (event->button) {
    case 1: button_type = MBT_LEFT; break;
    case 2: button_type = MBT_MIDDLE; break;
    case 3: button_type = MBT_RIGHT; break;
    default: return FALSE;
  }

  int click_count = 1;
  if (event->type == GDK_2BUTTON_PRESS) {
    click_count = 2;
  } else if (event->type == GDK_3BUTTON_PRESS) {
    click_count = 3;
  }

  g_client->GetBrowser()->GetHost()->SendMouseClickEvent(mouse_event, button_type, false, click_count);

  return TRUE;
}

// Mouse button release event
static gboolean on_button_release(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
  (void)user_data;  // Unused
  if (!g_client || !g_client->GetBrowser()) {
    return FALSE;
  }

  int x, y;
  TransformMouseCoordinates(widget, event->x, event->y, &x, &y);

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  CefBrowserHost::MouseButtonType button_type;
  switch (event->button) {
    case 1: button_type = MBT_LEFT; break;
    case 2: button_type = MBT_MIDDLE; break;
    case 3: button_type = MBT_RIGHT; break;
    default: return FALSE;
  }

  g_client->GetBrowser()->GetHost()->SendMouseClickEvent(mouse_event, button_type, true, 1);

  return TRUE;
}

// Mouse motion event
static gboolean on_motion_notify(GtkWidget* widget, GdkEventMotion* event, gpointer user_data) {
  if (!g_client || !g_client->GetBrowser()) {
    return FALSE;
  }

  int x, y;
  TransformMouseCoordinates(widget, event->x, event->y, &x, &y);

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  g_client->GetBrowser()->GetHost()->SendMouseMoveEvent(mouse_event, false);

  return TRUE;
}

// Mouse scroll/wheel event
static gboolean on_scroll(GtkWidget* widget, GdkEventScroll* event, gpointer user_data) {
  if (!g_client || !g_client->GetBrowser()) {
    return FALSE;
  }

  int x, y;
  TransformMouseCoordinates(widget, event->x, event->y, &x, &y);

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  // Convert scroll direction to deltas
  // CEF expects pixel deltas; use 40 pixels per scroll step (common value)
  int delta_x = 0;
  int delta_y = 0;

  switch (event->direction) {
    case GDK_SCROLL_UP:
      delta_y = 40;
      break;
    case GDK_SCROLL_DOWN:
      delta_y = -40;
      break;
    case GDK_SCROLL_LEFT:
      delta_x = 40;
      break;
    case GDK_SCROLL_RIGHT:
      delta_x = -40;
      break;
    case GDK_SCROLL_SMOOTH:
      // GTK 3.4+ supports smooth scrolling with delta values
      delta_x = (int)(-event->delta_x * 40);
      delta_y = (int)(-event->delta_y * 40);
      break;
  }

  g_client->GetBrowser()->GetHost()->SendMouseWheelEvent(mouse_event, delta_x, delta_y);

  return TRUE;
}

// Helper: Translate GDK key code to Windows virtual key code
static int GetWindowsKeyCode(guint keyval) {
  // Map common GDK keys to Windows VK codes
  // Full mapping available in CEF's browser_window_osr_gtk.cc

  if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9) {
    return keyval;  // 0-9 are same
  }
  if (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z) {
    return keyval;  // A-Z are same
  }
  if (keyval >= GDK_KEY_a && keyval <= GDK_KEY_z) {
    return keyval - 32;  // Convert to uppercase
  }

  // Function keys
  if (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F24) {
    return 0x70 + (keyval - GDK_KEY_F1);  // VK_F1 = 0x70
  }

  // Special keys
  switch (keyval) {
    case GDK_KEY_Return: return 0x0D;  // VK_RETURN
    case GDK_KEY_Escape: return 0x1B;  // VK_ESCAPE
    case GDK_KEY_BackSpace: return 0x08;  // VK_BACK
    case GDK_KEY_Tab: return 0x09;  // VK_TAB
    case GDK_KEY_space: return 0x20;  // VK_SPACE
    case GDK_KEY_Delete: return 0x2E;  // VK_DELETE
    case GDK_KEY_Home: return 0x24;  // VK_HOME
    case GDK_KEY_End: return 0x23;  // VK_END
    case GDK_KEY_Page_Up: return 0x21;  // VK_PRIOR
    case GDK_KEY_Page_Down: return 0x22;  // VK_NEXT
    case GDK_KEY_Left: return 0x25;  // VK_LEFT
    case GDK_KEY_Up: return 0x26;  // VK_UP
    case GDK_KEY_Right: return 0x27;  // VK_RIGHT
    case GDK_KEY_Down: return 0x28;  // VK_DOWN
    case GDK_KEY_Insert: return 0x2D;  // VK_INSERT
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R: return 0x10;  // VK_SHIFT
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R: return 0x11;  // VK_CONTROL
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R: return 0x12;  // VK_MENU
    default: return keyval;
  }
}

// Key press event
static gboolean on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)user_data;  // Unused
  if (!g_client || !g_client->GetBrowser()) {
    return FALSE;
  }

  CefKeyEvent key_event;
  key_event.type = KEYEVENT_RAWKEYDOWN;
  key_event.modifiers = GetCefModifiers(event->state);
  key_event.windows_key_code = GetWindowsKeyCode(event->keyval);
  key_event.native_key_code = event->hardware_keycode;
  key_event.is_system_key = false;
  key_event.character = 0;
  key_event.unmodified_character = 0;
  key_event.focus_on_editable_field = false;

  g_client->GetBrowser()->GetHost()->SendKeyEvent(key_event);

  // Also send CHAR event for printable characters
  // Use gdk_keyval_to_unicode for proper international character support
  guint32 unicode_char = gdk_keyval_to_unicode(event->keyval);
  if (unicode_char != 0) {
    key_event.type = KEYEVENT_CHAR;
    key_event.windows_key_code = unicode_char;
    key_event.character = unicode_char;
    key_event.unmodified_character = unicode_char;
    g_client->GetBrowser()->GetHost()->SendKeyEvent(key_event);
  }

  return TRUE;
}

// Key release event
static gboolean on_key_release(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)user_data;  // Unused
  if (!g_client || !g_client->GetBrowser()) {
    return FALSE;
  }

  CefKeyEvent key_event;
  key_event.type = KEYEVENT_KEYUP;
  key_event.modifiers = GetCefModifiers(event->state);
  key_event.windows_key_code = GetWindowsKeyCode(event->keyval);
  key_event.native_key_code = event->hardware_keycode;
  key_event.is_system_key = false;
  key_event.character = 0;
  key_event.unmodified_character = 0;
  key_event.focus_on_editable_field = false;

  g_client->GetBrowser()->GetHost()->SendKeyEvent(key_event);

  return TRUE;
}

// Focus in event
static gboolean on_focus_in(GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)event;   // Unused
  (void)user_data;  // Unused
  if (g_client && g_client->GetBrowser()) {
    g_client->GetBrowser()->GetHost()->SetFocus(true);
  }
  return FALSE;
}

// Focus out event
static gboolean on_focus_out(GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)event;   // Unused
  (void)user_data;  // Unused
  if (g_client && g_client->GetBrowser()) {
    g_client->GetBrowser()->GetHost()->SetFocus(false);
  }
  return FALSE;
}

// Mouse leave event
static gboolean on_leave_notify(GtkWidget* widget, GdkEventCrossing* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)user_data;  // Unused
  if (!g_client || !g_client->GetBrowser()) {
    return FALSE;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = (int)event->x;
  mouse_event.y = (int)event->y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  // Notify CEF that mouse left the view
  g_client->GetBrowser()->GetHost()->SendMouseMoveEvent(mouse_event, true);

  return FALSE;
}

// GTK idle callback for CEF message loop
static gboolean cef_do_message_loop_work(gpointer data) {
  (void)data;  // Unused
  CefDoMessageLoopWork();
  return G_SOURCE_CONTINUE;
}

// Draw callback - paint the OSR buffer
static gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
  (void)user_data;  // Unused
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);

  if (g_client) {
    g_client->DrawToSurface(cr, allocation.width, allocation.height);
  } else {
    // No browser yet, draw white
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
  }

  return FALSE;
}

// Size allocate callback - notify CEF of resize
static void on_size_allocate(GtkWidget* widget, GdkRectangle* allocation, gpointer user_data) {
  (void)widget;  // Unused
  (void)user_data;  // Unused
  if (g_client) {
    g_client->SetSize(allocation->width, allocation->height);
  }
}

// Realize callback - create the OSR browser
static void on_realize(GtkWidget* widget, gpointer user_data) {
  (void)user_data;  // Unused

  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);

  // Detect HiDPI scale factor from GTK
  int scale_factor = gtk_widget_get_scale_factor(widget);

  std::cout << "[on_realize] Widget size: " << allocation.width << "x" << allocation.height
            << ", GTK scale factor: " << scale_factor << std::endl;

  // Get URL from environment or use default
  const char* dev_url = std::getenv("DEV_URL");
  std::string url = dev_url ? dev_url : "https://www.google.com";

  // Create OSR client with device scale factor
  g_client = new OSRClient(widget);
  g_client->SetDeviceScaleFactor(static_cast<float>(scale_factor));

  // Create OSR browser
  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);  // 0 = no parent window handle

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = 60;  // 60 FPS

  CefBrowserHost::CreateBrowser(window_info, g_client, url, browser_settings, nullptr, nullptr);
}

// Window delete callback
static gboolean on_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)event;   // Unused
  (void)user_data;  // Unused
  if (g_client && g_client->GetBrowser()) {
    g_client->GetBrowser()->GetHost()->CloseBrowser(false);
  }
  return TRUE;
}

// Window destroy callback
static void on_destroy(GtkWidget* widget, gpointer user_data) {
  (void)widget;     // Unused
  (void)user_data;  // Unused
  gtk_main_quit();
}

// Get executable path
static std::string GetExecutablePath() {
  char path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len != -1) {
    path[len] = '\0';
    return std::string(path);
  }
  return "";
}

int main(int argc, char* argv[]) {
  // Disable GTK setlocale first
  gtk_disable_setlocale();

  // CEF initialization
  CefMainArgs main_args(argc, argv);
  CefRefPtr<AppHandler> app = new AppHandler();

  // Handle subprocesses
  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    return exit_code;
  }

  // Initialize GTK
  gtk_init(&argc, &argv);

  // Configure CEF settings
  CefSettings settings;
  settings.no_sandbox = true;
  settings.multi_threaded_message_loop = false;
  settings.external_message_pump = false;
  settings.windowless_rendering_enabled = true;  // Enable OSR
  CefString(&settings.cache_path).FromString("/tmp/athena_browser_cache");

  std::string exe_path = GetExecutablePath();
  if (!exe_path.empty()) {
    CefString(&settings.browser_subprocess_path).FromString(exe_path);
  }

  // Initialize CEF
  if (!CefInitialize(main_args, settings, app, nullptr)) {
    std::cerr << "ERROR: CefInitialize failed!" << std::endl;
    return -1;
  }

  // Create GTK window
  GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Athena Browser");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

  // Connect window signals
  g_signal_connect(window, "delete-event", G_CALLBACK(on_delete), nullptr);
  g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), nullptr);

  // Create drawing area for OSR rendering
  GtkWidget* drawing_area = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER(window), drawing_area);

  // Enable focus for keyboard events
  gtk_widget_set_can_focus(drawing_area, TRUE);

  // Add event masks to receive input events
  gtk_widget_add_events(drawing_area,
    GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK |
    GDK_POINTER_MOTION_MASK |
    GDK_SCROLL_MASK |
    GDK_KEY_PRESS_MASK |
    GDK_KEY_RELEASE_MASK |
    GDK_FOCUS_CHANGE_MASK |
    GDK_LEAVE_NOTIFY_MASK);

  // Connect drawing area signals
  g_signal_connect(drawing_area, "realize", G_CALLBACK(on_realize), nullptr);
  g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw), nullptr);
  g_signal_connect(drawing_area, "size-allocate", G_CALLBACK(on_size_allocate), nullptr);

  // Connect input event handlers
  g_signal_connect(drawing_area, "button-press-event", G_CALLBACK(on_button_press), nullptr);
  g_signal_connect(drawing_area, "button-release-event", G_CALLBACK(on_button_release), nullptr);
  g_signal_connect(drawing_area, "motion-notify-event", G_CALLBACK(on_motion_notify), nullptr);
  g_signal_connect(drawing_area, "scroll-event", G_CALLBACK(on_scroll), nullptr);
  g_signal_connect(drawing_area, "key-press-event", G_CALLBACK(on_key_press), nullptr);
  g_signal_connect(drawing_area, "key-release-event", G_CALLBACK(on_key_release), nullptr);
  g_signal_connect(drawing_area, "focus-in-event", G_CALLBACK(on_focus_in), nullptr);
  g_signal_connect(drawing_area, "focus-out-event", G_CALLBACK(on_focus_out), nullptr);
  g_signal_connect(drawing_area, "leave-notify-event", G_CALLBACK(on_leave_notify), nullptr);

  // Show all widgets
  gtk_widget_show_all(window);

  // Set up CEF message loop work
  g_idle_add(cef_do_message_loop_work, nullptr);

  // Run GTK main loop
  gtk_main();

  // Cleanup
  g_client = nullptr;
  CefShutdown();

  return 0;
}

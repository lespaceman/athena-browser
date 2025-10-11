#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/wrapper/cef_helpers.h"
#include "browser/app_handler.h"
#include "browser/cef_engine.h"
#include "rendering/gl_renderer.h"

#include <gtk/gtk.h>
#include <GL/gl.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <cstring>
#include <mutex>
#include <thread>

// Context for GTK callbacks - replaces global state
struct BrowserContext {
  GtkWidget* widget;
  std::unique_ptr<athena::rendering::GLRenderer> gl_renderer;
  CefRefPtr<athena::browser::CefClient> client;
  CefRefPtr<CefBrowser> browser;
  athena::browser::BrowserId browser_id;
};

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
static void TransformMouseCoordinates(BrowserContext* ctx, double event_x, double event_y, int* out_x, int* out_y) {
  if (!ctx || !ctx->client) {
    *out_x = (int)event_x;
    *out_y = (int)event_y;
    return;
  }

  // Get widget size
  GtkAllocation allocation;
  gtk_widget_get_allocation(ctx->widget, &allocation);

  if (allocation.width == 0 || allocation.height == 0) {
    *out_x = (int)event_x;
    *out_y = (int)event_y;
    return;
  }

  // Get CEF view rect to determine actual buffer dimensions
  CefRefPtr<CefBrowser> browser = ctx->client->GetBrowser();
  if (!browser) {
    *out_x = (int)event_x;
    *out_y = (int)event_y;
    return;
  }

  CefRect view_rect;
  ctx->client->GetViewRect(browser, view_rect);

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
  (void)widget;  // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx || !ctx->client || !ctx->client->GetBrowser()) {
    return FALSE;
  }

  int x, y;
  TransformMouseCoordinates(ctx, event->x, event->y, &x, &y);

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

  ctx->client->GetBrowser()->GetHost()->SendMouseClickEvent(mouse_event, button_type, false, click_count);

  return TRUE;
}

// Mouse button release event
static gboolean on_button_release(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
  (void)widget;  // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx || !ctx->client || !ctx->client->GetBrowser()) {
    return FALSE;
  }

  int x, y;
  TransformMouseCoordinates(ctx, event->x, event->y, &x, &y);

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

  ctx->client->GetBrowser()->GetHost()->SendMouseClickEvent(mouse_event, button_type, true, 1);

  return TRUE;
}

// Mouse motion event
static gboolean on_motion_notify(GtkWidget* widget, GdkEventMotion* event, gpointer user_data) {
  (void)widget;  // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx || !ctx->client || !ctx->client->GetBrowser()) {
    return FALSE;
  }

  int x, y;
  TransformMouseCoordinates(ctx, event->x, event->y, &x, &y);

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  ctx->client->GetBrowser()->GetHost()->SendMouseMoveEvent(mouse_event, false);

  return TRUE;
}

// Mouse scroll/wheel event
static gboolean on_scroll(GtkWidget* widget, GdkEventScroll* event, gpointer user_data) {
  (void)widget;  // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx || !ctx->client || !ctx->client->GetBrowser()) {
    return FALSE;
  }

  int x, y;
  TransformMouseCoordinates(ctx, event->x, event->y, &x, &y);

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

  ctx->client->GetBrowser()->GetHost()->SendMouseWheelEvent(mouse_event, delta_x, delta_y);

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
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx || !ctx->client || !ctx->client->GetBrowser()) {
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

  ctx->client->GetBrowser()->GetHost()->SendKeyEvent(key_event);

  // Also send CHAR event for printable characters
  // Use gdk_keyval_to_unicode for proper international character support
  guint32 unicode_char = gdk_keyval_to_unicode(event->keyval);
  if (unicode_char != 0) {
    key_event.type = KEYEVENT_CHAR;
    key_event.windows_key_code = unicode_char;
    key_event.character = unicode_char;
    key_event.unmodified_character = unicode_char;
    ctx->client->GetBrowser()->GetHost()->SendKeyEvent(key_event);
  }

  return TRUE;
}

// Key release event
static gboolean on_key_release(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
  (void)widget;  // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx || !ctx->client || !ctx->client->GetBrowser()) {
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

  ctx->client->GetBrowser()->GetHost()->SendKeyEvent(key_event);

  return TRUE;
}

// Focus in event
static gboolean on_focus_in(GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)event;   // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (ctx && ctx->client && ctx->client->GetBrowser()) {
    ctx->client->GetBrowser()->GetHost()->SetFocus(true);
  }
  return FALSE;
}

// Focus out event
static gboolean on_focus_out(GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)event;   // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (ctx && ctx->client && ctx->client->GetBrowser()) {
    ctx->client->GetBrowser()->GetHost()->SetFocus(false);
  }
  return FALSE;
}

// Mouse leave event
static gboolean on_leave_notify(GtkWidget* widget, GdkEventCrossing* event, gpointer user_data) {
  (void)widget;  // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx || !ctx->client || !ctx->client->GetBrowser()) {
    return FALSE;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = (int)event->x;
  mouse_event.y = (int)event->y;
  mouse_event.modifiers = GetCefModifiers(event->state);

  // Notify CEF that mouse left the view
  ctx->client->GetBrowser()->GetHost()->SendMouseMoveEvent(mouse_event, true);

  return FALSE;
}

// GTK idle callback for CEF message loop
static gboolean cef_do_message_loop_work(gpointer data) {
  (void)data;  // Unused
  CefDoMessageLoopWork();
  return G_SOURCE_CONTINUE;
}

// GL realize callback - initialize OpenGL
static void on_gl_realize(GtkGLArea* gl_area, gpointer user_data) {
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx) {
    std::cerr << "[on_gl_realize] ERROR: No browser context!" << std::endl;
    return;
  }

  gtk_gl_area_make_current(gl_area);

  if (gtk_gl_area_get_error(gl_area) != nullptr) {
    std::cerr << "[on_gl_realize] OpenGL context error!" << std::endl;
    return;
  }

  // Create GLRenderer
  ctx->gl_renderer = std::make_unique<athena::rendering::GLRenderer>();
  auto result = ctx->gl_renderer->Initialize(GTK_WIDGET(gl_area));

  if (!result) {
    std::cerr << "[on_gl_realize] Failed to initialize GLRenderer: "
              << result.GetError().Message() << std::endl;
    ctx->gl_renderer.reset();
    return;
  }

  std::cout << "[on_gl_realize] OpenGL renderer initialized successfully" << std::endl;
}

// GL render callback - render the frame
static gboolean on_gl_render(GtkGLArea* gl_area, GdkGLContext* context, gpointer user_data) {
  (void)gl_area;  // Unused
  (void)context;  // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);

  if (!ctx || !ctx->gl_renderer) {
    // Not initialized yet, clear to white
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return TRUE;
  }

  auto result = ctx->gl_renderer->Render();
  if (!result) {
    std::cerr << "[on_gl_render] Render failed: "
              << result.GetError().Message() << std::endl;
    return FALSE;
  }

  return TRUE;
}

// Size allocate callback - notify CEF of resize
static void on_size_allocate(GtkWidget* widget, GdkRectangle* allocation, gpointer user_data) {
  (void)widget;  // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (ctx && ctx->client) {
    ctx->client->SetSize(allocation->width, allocation->height);
  }
}

// Realize callback - create the OSR browser (called AFTER on_gl_realize)
static void on_realize(GtkWidget* widget, gpointer user_data) {
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (!ctx) {
    std::cerr << "[on_realize] ERROR: No browser context!" << std::endl;
    return;
  }

  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);

  // Detect HiDPI scale factor from GTK
  int scale_factor = gtk_widget_get_scale_factor(widget);

  std::cout << "[on_realize] Widget size: " << allocation.width << "x" << allocation.height
            << ", GTK scale factor: " << scale_factor << std::endl;

  // Ensure GLRenderer is initialized
  if (!ctx->gl_renderer) {
    std::cerr << "[on_realize] ERROR: GLRenderer not initialized! on_gl_realize should have been called first." << std::endl;
    return;
  }

  // Get URL from environment or use default
  const char* dev_url = std::getenv("DEV_URL");
  std::string url = dev_url ? dev_url : "https://www.google.com";

  // Create CefClient with device scale factor and GLRenderer
  ctx->client = new athena::browser::CefClient(widget, ctx->gl_renderer.get());
  ctx->client->SetDeviceScaleFactor(static_cast<float>(scale_factor));
  ctx->client->SetSize(allocation.width, allocation.height);

  // Create OSR browser
  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);  // 0 = no parent window handle

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = 60;  // 60 FPS

  CefBrowserHost::CreateBrowser(window_info, ctx->client, url, browser_settings, nullptr, nullptr);
}

// Window delete callback
static gboolean on_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data) {
  (void)widget;  // Unused
  (void)event;   // Unused
  BrowserContext* ctx = static_cast<BrowserContext*>(user_data);
  if (ctx && ctx->client && ctx->client->GetBrowser()) {
    ctx->client->GetBrowser()->GetHost()->CloseBrowser(false);
  }
  return TRUE;
}

// Window destroy callback
static void on_destroy(GtkWidget* widget, gpointer user_data) {
  (void)widget;  // Unused
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

  // Create browser context (replaces global state)
  auto browser_ctx = std::make_unique<BrowserContext>();

  // Create GTK window
  GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Athena Browser");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

  // Connect window signals - pass context as user_data
  g_signal_connect(window, "delete-event", G_CALLBACK(on_delete), browser_ctx.get());
  g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), browser_ctx.get());

  // Create GL area for hardware-accelerated OSR rendering
  GtkWidget* gl_area = gtk_gl_area_new();
  gtk_container_add(GTK_CONTAINER(window), gl_area);

  // Store widget in context
  browser_ctx->widget = gl_area;

  // Configure GL area
  gtk_gl_area_set_auto_render(GTK_GL_AREA(gl_area), FALSE);  // Manual rendering
  gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl_area), FALSE);  // No depth buffer needed

  // Enable focus for keyboard events
  gtk_widget_set_can_focus(gl_area, TRUE);

  // Add event masks to receive input events
  gtk_widget_add_events(gl_area,
    GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK |
    GDK_POINTER_MOTION_MASK |
    GDK_SCROLL_MASK |
    GDK_KEY_PRESS_MASK |
    GDK_KEY_RELEASE_MASK |
    GDK_FOCUS_CHANGE_MASK |
    GDK_LEAVE_NOTIFY_MASK);

  // Connect GL area signals - pass context as user_data
  g_signal_connect(gl_area, "realize", G_CALLBACK(on_gl_realize), browser_ctx.get());
  g_signal_connect(gl_area, "render", G_CALLBACK(on_gl_render), browser_ctx.get());
  g_signal_connect_after(gl_area, "realize", G_CALLBACK(on_realize), browser_ctx.get());
  g_signal_connect(gl_area, "size-allocate", G_CALLBACK(on_size_allocate), browser_ctx.get());

  // Connect input event handlers - pass context as user_data
  g_signal_connect(gl_area, "button-press-event", G_CALLBACK(on_button_press), browser_ctx.get());
  g_signal_connect(gl_area, "button-release-event", G_CALLBACK(on_button_release), browser_ctx.get());
  g_signal_connect(gl_area, "motion-notify-event", G_CALLBACK(on_motion_notify), browser_ctx.get());
  g_signal_connect(gl_area, "scroll-event", G_CALLBACK(on_scroll), browser_ctx.get());
  g_signal_connect(gl_area, "key-press-event", G_CALLBACK(on_key_press), browser_ctx.get());
  g_signal_connect(gl_area, "key-release-event", G_CALLBACK(on_key_release), browser_ctx.get());
  g_signal_connect(gl_area, "focus-in-event", G_CALLBACK(on_focus_in), browser_ctx.get());
  g_signal_connect(gl_area, "focus-out-event", G_CALLBACK(on_focus_out), browser_ctx.get());
  g_signal_connect(gl_area, "leave-notify-event", G_CALLBACK(on_leave_notify), browser_ctx.get());

  // Show all widgets
  gtk_widget_show_all(window);

  // Set up CEF message loop work
  g_idle_add(cef_do_message_loop_work, nullptr);

  // Run GTK main loop
  gtk_main();

  // Cleanup - context will be automatically destroyed
  if (browser_ctx->gl_renderer) {
    browser_ctx->gl_renderer->Cleanup();
  }
  browser_ctx->client = nullptr;
  browser_ctx->browser = nullptr;

  CefShutdown();

  return 0;
}

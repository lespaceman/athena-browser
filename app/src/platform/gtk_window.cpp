#include "platform/gtk_window.h"
#include "browser/browser_engine.h"
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
      browser_id_(0),
      closed_(false),
      visible_(false),
      has_focus_(false),
      window_(nullptr),
      gl_area_(nullptr),
      cef_client_(nullptr) {
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

  // Create GL area for hardware-accelerated rendering
  gl_area_ = gtk_gl_area_new();
  gtk_container_add(GTK_CONTAINER(window_), gl_area_);

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

void GtkWindow::SetupEventHandlers() {
  // Window events
  g_signal_connect(window_, "delete-event", G_CALLBACK(on_delete), this);
  g_signal_connect(window_, "destroy", G_CALLBACK(on_destroy), this);

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
  if (!gl_renderer_) {
    return utils::Error("GLRenderer not initialized");
  }

  // Get scale factor
  float scale_factor = static_cast<float>(gtk_widget_get_scale_factor(gl_area_));

  // Create CefClient
  cef_client_ = new browser::CefClient(gl_area_, gl_renderer_.get());
  cef_client_->SetDeviceScaleFactor(scale_factor);
  cef_client_->SetSize(config_.size.width, config_.size.height);

  // Create OSR browser
  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = 60;

  CefBrowserHost::CreateBrowser(window_info, cef_client_, url, browser_settings, nullptr, nullptr);

  return utils::Ok();
}

// ============================================================================
// Window Properties
// ============================================================================

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
  browser_id_ = browser_id;
}

browser::BrowserId GtkWindow::GetBrowser() const {
  return browser_id_;
}

// ============================================================================
// Lifecycle
// ============================================================================

void GtkWindow::Close(bool force) {
  if (closed_) return;

  if (!force && cef_client_ && cef_client_->GetBrowser()) {
    cef_client_->GetBrowser()->GetHost()->CloseBrowser(false);
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

  // Get URL from environment or use default
  const char* dev_url = std::getenv("DEV_URL");
  std::string url = dev_url ? dev_url : "https://www.google.com";

  auto result = CreateBrowser(url);
  if (!result) {
    std::cerr << "[GtkWindow] Failed to create browser: " << result.GetError().Message() << std::endl;
  }
}

void GtkWindow::OnSizeAllocate(int width, int height) {
  config_.size = {width, height};

  if (cef_client_) {
    cef_client_->SetSize(width, height);
  }

  if (callbacks_.on_resize) {
    callbacks_.on_resize(width, height);
  }
}

gboolean GtkWindow::OnDelete() {
  if (callbacks_.on_close) {
    callbacks_.on_close();
  }

  if (cef_client_ && cef_client_->GetBrowser()) {
    cef_client_->GetBrowser()->GetHost()->CloseBrowser(false);
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

  if (cef_client_ && cef_client_->GetBrowser()) {
    cef_client_->GetBrowser()->GetHost()->SetFocus(focused);
  }

  if (callbacks_.on_focus_changed) {
    callbacks_.on_focus_changed(focused);
  }
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

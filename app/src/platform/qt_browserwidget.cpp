/**
 * BrowserWidget Implementation
 *
 * Qt OpenGL widget for CEF browser rendering using Qt's signal/slot system.
 */

#include "platform/qt_browserwidget.h"

#include "browser/cef_client.h"
#include "include/cef_browser.h"
#include "platform/qt_mainwindow.h"
#include "rendering/gl_renderer.h"
#include "utils/logging.h"

#include <GL/gl.h>

#include <cmath>
#include <cstdlib>
#include <QColor>
#include <QDebug>
#include <QPalette>
#include <QTimer>

namespace athena {
namespace platform {

using namespace browser;
using namespace rendering;
using namespace utils;

static Logger logger("BrowserWidget");

namespace {

void ClearToWidgetBackground(const QOpenGLWidget* widget) {
  const QColor bg = widget->palette().color(QPalette::Window);
  glClearColor(bg.redF(), bg.greenF(), bg.blueF(), 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

}  // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

BrowserWidget::BrowserWidget(QtMainWindow* window, size_t tab_index, QWidget* parent)
    : QOpenGLWidget(parent),
      window_(window),
      tab_index_(tab_index),
      renderer_(nullptr),
      gl_initialized_(false),
      pending_width_(0),
      pending_height_(0),
      last_painted_width_(0),
      last_painted_height_(0),
      awaiting_paint_for_size_(false) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);

  // Enable OpenGL updates
  setUpdateBehavior(QOpenGLWidget::PartialUpdate);

  logger.Debug("BrowserWidget created for tab " + std::to_string(tab_index));
}

BrowserWidget::~BrowserWidget() {
  makeCurrent();
  // Renderer cleanup happens in QtMainWindow destructor
  doneCurrent();
  logger.Debug("BrowserWidget destroyed");
}

void BrowserWidget::InitializeBrowser(GLRenderer* renderer) {
  renderer_ = renderer;
  logger.Debug("Renderer set, will initialize when GL context is ready");
}

CefClient* BrowserWidget::GetCefClientForThisTab() const {
  if (!window_) {
    return nullptr;
  }

  // Get the CEF client for this specific tab (thread-safe)
  return window_->GetCefClientForTab(tab_index_);
}

void BrowserWidget::OnCefPaint(CefRenderHandler::PaintElementType type, int width, int height) {
  const float device_scale = devicePixelRatioF();

  if (type != CefRenderHandler::PaintElementType::PET_VIEW) {
    // Popups and other auxiliary paints should render immediately.
    update();
    return;
  }

  if (awaiting_paint_for_size_) {
    const int expected_width = static_cast<int>(std::lround(pending_width_ * device_scale));
    const int expected_height = static_cast<int>(std::lround(pending_height_ * device_scale));
    const int width_delta = std::abs(width - expected_width);
    const int height_delta = std::abs(height - expected_height);
    constexpr int kPixelTolerance = 2;  // Allow small rounding differences

    if (width_delta <= kPixelTolerance && height_delta <= kPixelTolerance) {
      awaiting_paint_for_size_ = false;
      last_painted_width_ = pending_width_;
      last_painted_height_ = pending_height_;
      logger.Debug("CEF paint matches pending size: {}x{} (scale {} expected {}x{})",
                   width,
                   height,
                   device_scale,
                   expected_width,
                   expected_height);
      update();
    } else {
      logger.Debug(
          "CEF paint size mismatch: got {}x{}, waiting for {}x{} (scale {} expected {}x{})",
          width,
          height,
          pending_width_,
          pending_height_,
          device_scale,
          expected_width,
          expected_height);
    }
  } else {
    last_painted_width_ = static_cast<int>(std::lround(width / device_scale));
    last_painted_height_ = static_cast<int>(std::lround(height / device_scale));
    update();
  }
}

// ============================================================================
// OpenGL Overrides (Qt OpenGL widget callbacks)
// ============================================================================

void BrowserWidget::initializeGL() {
  // Called when OpenGL context is ready
  logger.Info("OpenGL context initialized");
  logger.Info(std::string("OpenGL version: ") +
              reinterpret_cast<const char*>(glGetString(GL_VERSION)));

  gl_initialized_ = true;

  // Clear to the widget background color for consistency with the Qt theme
  ClearToWidgetBackground(this);

  // Initialize GLRenderer now that GL context is ready
  if (renderer_) {
    auto result = renderer_->Initialize(this);
    if (!result.IsOk()) {
      logger.Error("Failed to initialize GLRenderer: " + result.GetError().Message());
    } else {
      logger.Info("GLRenderer initialized successfully");
      // Emit signal to notify that GL is ready for browser creation
      emit glContextReady();
    }
  }
}

void BrowserWidget::paintGL() {
  // Called to render the current frame

  if (!renderer_) {
    // No renderer yet, draw widget background
    ClearToWidgetBackground(this);
    return;
  }

  if (awaiting_paint_for_size_) {
    // Waiting for CEF to deliver a buffer that matches current widget size;
    // keep the area cleared so we don't stretch the previous frame.
    ClearToWidgetBackground(this);
    return;
  }

  // Render browser frame
  auto result = renderer_->Render();
  if (!result.IsOk()) {
    logger.Warn("Render failed: " + result.GetError().Message());
  }
}

void BrowserWidget::resizeGL(int w, int h) {
  // Event-driven resize sync (no timers):
  // 1. Update GL viewport immediately
  // 2. Notify CEF via WasResized()
  // 3. Record pending size and wait for matching OnPaint
  // 4. Only present frame when CEF buffer matches current size

  // Update GL viewport to match new widget size
  glViewport(0, 0, w, h);

  const bool size_changed = (w != pending_width_) || (h != pending_height_);

  pending_width_ = w;
  pending_height_ = h;
  awaiting_paint_for_size_ = size_changed && (w > 0 && h > 0);

  if (window_ && w > 0 && h > 0) {
    const float device_scale = devicePixelRatioF();
    const int expected_width = static_cast<int>(std::lround(w * device_scale));
    const int expected_height = static_cast<int>(std::lround(h * device_scale));

    if (size_changed) {
      logger.Debug("Browser widget resized: {}x{} (scale {}, buffer {}x{})",
                   w,
                   h,
                   device_scale,
                   expected_width,
                   expected_height);
      window_->OnBrowserSizeChanged(tab_index_, w, h);
    }
  }
}

// ============================================================================
// Input Event Handlers (Qt event callbacks)
// ============================================================================

void BrowserWidget::mouseMoveEvent(QMouseEvent* event) {
  // Handle mouse movement

  auto* client = GetCefClientForThisTab();
  if (!client || !client->GetBrowser()) {
    return;
  }

  CefMouseEvent mouseEvent;
  mouseEvent.x = event->pos().x();
  mouseEvent.y = event->pos().y();
  mouseEvent.modifiers = getCefModifiers(event->modifiers(), event->buttons());

  client->GetBrowser()->GetHost()->SendMouseMoveEvent(mouseEvent, false);
}

void BrowserWidget::mousePressEvent(QMouseEvent* event) {
  // Handle mouse button press

  // Grab focus when clicked
  setFocus();

  auto* client = GetCefClientForThisTab();
  if (!client || !client->GetBrowser()) {
    return;
  }

  CefMouseEvent mouseEvent;
  mouseEvent.x = event->pos().x();
  mouseEvent.y = event->pos().y();
  mouseEvent.modifiers = getCefModifiers(event->modifiers(), event->buttons());

  CefBrowserHost::MouseButtonType buttonType;
  switch (event->button()) {
    case Qt::LeftButton:
      buttonType = MBT_LEFT;
      break;
    case Qt::MiddleButton:
      buttonType = MBT_MIDDLE;
      break;
    case Qt::RightButton:
      buttonType = MBT_RIGHT;
      break;
    default:
      return;
  }

  int clickCount = 1;
  if (event->type() == QEvent::MouseButtonDblClick) {
    clickCount = 2;
  }

  client->GetBrowser()->GetHost()->SendMouseClickEvent(mouseEvent, buttonType, false, clickCount);
}

void BrowserWidget::mouseReleaseEvent(QMouseEvent* event) {
  // Handle mouse button release

  auto* client = GetCefClientForThisTab();
  if (!client || !client->GetBrowser()) {
    return;
  }

  CefMouseEvent mouseEvent;
  mouseEvent.x = event->pos().x();
  mouseEvent.y = event->pos().y();
  mouseEvent.modifiers = getCefModifiers(event->modifiers(), event->buttons());

  CefBrowserHost::MouseButtonType buttonType;
  switch (event->button()) {
    case Qt::LeftButton:
      buttonType = MBT_LEFT;
      break;
    case Qt::MiddleButton:
      buttonType = MBT_MIDDLE;
      break;
    case Qt::RightButton:
      buttonType = MBT_RIGHT;
      break;
    default:
      return;
  }

  client->GetBrowser()->GetHost()->SendMouseClickEvent(mouseEvent, buttonType, true, 1);
}

void BrowserWidget::wheelEvent(QWheelEvent* event) {
  // Handle mouse wheel scrolling

  auto* client = GetCefClientForThisTab();
  if (!client || !client->GetBrowser()) {
    return;
  }

  CefMouseEvent mouseEvent;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  mouseEvent.x = event->position().x();
  mouseEvent.y = event->position().y();
#else
  mouseEvent.x = event->pos().x();
  mouseEvent.y = event->pos().y();
#endif
  mouseEvent.modifiers = getCefModifiers(event->modifiers(), Qt::NoButton);

  // Qt uses delta in 8ths of a degree, CEF uses pixels
  // Scale factor: 40 pixels per scroll unit (standard)
  int deltaX = event->angleDelta().x() / 8 * 5;
  int deltaY = event->angleDelta().y() / 8 * 5;

  client->GetBrowser()->GetHost()->SendMouseWheelEvent(mouseEvent, deltaX, deltaY);
}

void BrowserWidget::keyPressEvent(QKeyEvent* event) {
  // Handle key press events

  auto* client = GetCefClientForThisTab();
  if (!client || !client->GetBrowser()) {
    return;
  }

  CefKeyEvent keyEvent;
  keyEvent.type = KEYEVENT_RAWKEYDOWN;
  keyEvent.modifiers = getCefModifiers(event->modifiers(), Qt::NoButton);
  keyEvent.windows_key_code = getWindowsKeyCode(event->key());
  keyEvent.native_key_code = event->nativeScanCode();
  keyEvent.is_system_key = false;
  keyEvent.character = 0;
  keyEvent.unmodified_character = 0;
  keyEvent.focus_on_editable_field = false;

  client->GetBrowser()->GetHost()->SendKeyEvent(keyEvent);

  // Send CHAR event for printable characters
  QString text = event->text();
  if (!text.isEmpty()) {
    for (const QChar& ch : text) {
      CefKeyEvent charEvent;
      charEvent.type = KEYEVENT_CHAR;
      charEvent.modifiers = keyEvent.modifiers;
      charEvent.windows_key_code = ch.unicode();
      charEvent.character = ch.unicode();
      charEvent.unmodified_character = ch.unicode();
      charEvent.is_system_key = false;
      charEvent.focus_on_editable_field = false;

      client->GetBrowser()->GetHost()->SendKeyEvent(charEvent);
    }
  }
}

void BrowserWidget::keyReleaseEvent(QKeyEvent* event) {
  // Handle key release events

  auto* client = GetCefClientForThisTab();
  if (!client || !client->GetBrowser()) {
    return;
  }

  CefKeyEvent keyEvent;
  keyEvent.type = KEYEVENT_KEYUP;
  keyEvent.modifiers = getCefModifiers(event->modifiers(), Qt::NoButton);
  keyEvent.windows_key_code = getWindowsKeyCode(event->key());
  keyEvent.native_key_code = event->nativeScanCode();
  keyEvent.is_system_key = false;
  keyEvent.character = 0;
  keyEvent.unmodified_character = 0;
  keyEvent.focus_on_editable_field = false;

  client->GetBrowser()->GetHost()->SendKeyEvent(keyEvent);
}

void BrowserWidget::focusInEvent(QFocusEvent* event) {
  // Handle focus gain
  QOpenGLWidget::focusInEvent(event);

  auto* client = GetCefClientForThisTab();
  if (client && client->GetBrowser()) {
    // Update CEF browser focus
    client->GetBrowser()->GetHost()->SetFocus(true);
    // CRITICAL: Also update has_focus_ tracking for cursor visibility workaround
    client->SetFocus(true);
  }
}

void BrowserWidget::focusOutEvent(QFocusEvent* event) {
  // Handle focus loss
  QOpenGLWidget::focusOutEvent(event);

  auto* client = GetCefClientForThisTab();
  if (client && client->GetBrowser()) {
    // Update CEF browser focus
    client->GetBrowser()->GetHost()->SetFocus(false);
    // CRITICAL: Also update has_focus_ tracking for cursor visibility workaround
    client->SetFocus(false);
  }
}

// ============================================================================
// Helper Methods
// ============================================================================

uint32_t BrowserWidget::getCefModifiers(Qt::KeyboardModifiers qtMods,
                                        Qt::MouseButtons qtButtons) const {
  uint32_t cefMods = 0;

  // Keyboard modifiers
  if (qtMods & Qt::ShiftModifier)
    cefMods |= EVENTFLAG_SHIFT_DOWN;
  if (qtMods & Qt::ControlModifier)
    cefMods |= EVENTFLAG_CONTROL_DOWN;
  if (qtMods & Qt::AltModifier)
    cefMods |= EVENTFLAG_ALT_DOWN;
  if (qtMods & Qt::KeypadModifier)
    cefMods |= EVENTFLAG_IS_KEY_PAD;

  // Mouse button modifiers
  if (qtButtons & Qt::LeftButton)
    cefMods |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  if (qtButtons & Qt::MiddleButton)
    cefMods |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
  if (qtButtons & Qt::RightButton)
    cefMods |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

  return cefMods;
}

int BrowserWidget::getWindowsKeyCode(int qtKey) const {
  // Map Qt keys to Windows virtual key codes
  // Based on comprehensive mapping from QCefView KeyboardUtils

  // Standard alphanumeric keys
  if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
    return qtKey;  // '0'-'9' (0x30-0x39)
  }
  if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
    return qtKey;  // 'A'-'Z' (0x41-0x5A)
  }

  // Function keys (F1-F24)
  if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
    return 0x70 + (qtKey - Qt::Key_F1);  // VK_F1 (0x70) through VK_F24 (0x87)
  }

  // Comprehensive key mapping switch
  switch (qtKey) {
    // Control keys
    case Qt::Key_Backspace:
      return 0x08;  // VK_BACK
    case Qt::Key_Tab:
      return 0x09;  // VK_TAB
    case Qt::Key_Backtab:
      return 0x09;  // VK_TAB (Shift+Tab)
    case Qt::Key_Clear:
      return 0x0C;  // VK_CLEAR
    case Qt::Key_Return:
    case Qt::Key_Enter:
      return 0x0D;  // VK_RETURN

    // Modifier keys
    case Qt::Key_Shift:
      return 0x10;  // VK_SHIFT
    case Qt::Key_Control:
      return 0x11;  // VK_CONTROL
    case Qt::Key_Alt:
      return 0x12;  // VK_MENU
    case Qt::Key_Pause:
      return 0x13;  // VK_PAUSE
    case Qt::Key_CapsLock:
      return 0x14;  // VK_CAPITAL

    // Navigation keys
    case Qt::Key_Escape:
      return 0x1B;  // VK_ESCAPE
    case Qt::Key_Space:
      return 0x20;  // VK_SPACE
    case Qt::Key_PageUp:
      return 0x21;  // VK_PRIOR
    case Qt::Key_PageDown:
      return 0x22;  // VK_NEXT
    case Qt::Key_End:
      return 0x23;  // VK_END
    case Qt::Key_Home:
      return 0x24;  // VK_HOME
    case Qt::Key_Left:
      return 0x25;  // VK_LEFT
    case Qt::Key_Up:
      return 0x26;  // VK_UP
    case Qt::Key_Right:
      return 0x27;  // VK_RIGHT
    case Qt::Key_Down:
      return 0x28;  // VK_DOWN

    // Special keys
    case Qt::Key_Select:
      return 0x29;  // VK_SELECT
    case Qt::Key_Print:
      return 0x2A;  // VK_PRINT
    case Qt::Key_Execute:
      return 0x2B;  // VK_EXECUTE
    case Qt::Key_Printer:
      return 0x2C;  // VK_SNAPSHOT (PrintScreen)
    case Qt::Key_Insert:
      return 0x2D;  // VK_INSERT
    case Qt::Key_Delete:
      return 0x2E;  // VK_DELETE
    case Qt::Key_Help:
      return 0x2F;  // VK_HELP

    // Numpad keys (0x60-0x69)
    // Note: These need to be distinguished from regular number keys
    // by checking KeypadModifier in the actual event handling

    // Numpad operators
    case Qt::Key_multiply:
      return 0x6A;  // VK_MULTIPLY
    case Qt::Key_Asterisk:
      return 0x6A;  // VK_MULTIPLY (same key)

    // Lock keys
    case Qt::Key_NumLock:
      return 0x90;  // VK_NUMLOCK
    case Qt::Key_ScrollLock:
      return 0x91;  // VK_SCROLL

    // Media keys
    case Qt::Key_VolumeMute:
      return 0xAD;  // VK_VOLUME_MUTE
    case Qt::Key_VolumeDown:
      return 0xAE;  // VK_VOLUME_DOWN
    case Qt::Key_VolumeUp:
      return 0xAF;  // VK_VOLUME_UP
    case Qt::Key_MediaStop:
      return 0xB2;  // VK_MEDIA_STOP
    case Qt::Key_MediaPlay:
      return 0xB3;  // VK_MEDIA_PLAY_PAUSE

    // Punctuation and symbols (OEM keys)
    case Qt::Key_Semicolon:
    case Qt::Key_Colon:
      return 0xBA;  // VK_OEM_1 (';:' for US)
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      return 0xBB;  // VK_OEM_PLUS ('=+' for US)
    case Qt::Key_Comma:
    case Qt::Key_Less:
      return 0xBC;  // VK_OEM_COMMA (',<' for US)
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
      return 0xBD;  // VK_OEM_MINUS ('-_' for US)
    case Qt::Key_Period:
    case Qt::Key_Greater:
      return 0xBE;  // VK_OEM_PERIOD ('.>' for US)
    case Qt::Key_Slash:
    case Qt::Key_Question:
      return 0xBF;  // VK_OEM_2 ('/?' for US)
    case Qt::Key_QuoteLeft:
    case Qt::Key_AsciiTilde:
      return 0xC0;  // VK_OEM_3 ('`~' for US)
    case Qt::Key_BracketLeft:
    case Qt::Key_BraceLeft:
      return 0xDB;  // VK_OEM_4 ('[{' for US)
    case Qt::Key_Backslash:
    case Qt::Key_Bar:
      return 0xDC;  // VK_OEM_5 ('\|' for US)
    case Qt::Key_BracketRight:
    case Qt::Key_BraceRight:
      return 0xDD;  // VK_OEM_6 (']}' for US)
    case Qt::Key_Apostrophe:
    case Qt::Key_QuoteDbl:
      return 0xDE;  // VK_OEM_7 (''"' for US)

    // Symbol keys (shifted number keys)
    case Qt::Key_ParenRight:
      return 0x30;  // '0'
    case Qt::Key_Exclam:
      return 0x31;  // '1'
    case Qt::Key_At:
      return 0x32;  // '2'
    case Qt::Key_NumberSign:
      return 0x33;  // '3'
    case Qt::Key_Dollar:
      return 0x34;  // '4'
    case Qt::Key_Percent:
      return 0x35;  // '5'
    case Qt::Key_AsciiCircum:
      return 0x36;  // '6'
    case Qt::Key_Ampersand:
      return 0x37;  // '7'
    case Qt::Key_ParenLeft:
      return 0x39;  // '9'

    default:
      return qtKey;
  }
}

}  // namespace platform
}  // namespace athena

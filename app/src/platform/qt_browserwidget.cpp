/**
 * BrowserWidget Implementation
 *
 * Qt OpenGL widget for CEF browser rendering.
 * Replaces GTK's gl_area_ with Qt's signal/slot system.
 */

#include "platform/qt_browserwidget.h"
#include "platform/qt_mainwindow.h"
#include "browser/cef_client.h"
#include "rendering/gl_renderer.h"
#include "utils/logging.h"
#include "include/cef_browser.h"

#include <QDebug>
#include <GL/gl.h>

namespace athena {
namespace platform {

using namespace browser;
using namespace rendering;
using namespace utils;

static Logger logger("BrowserWidget");

// ============================================================================
// Constructor / Destructor
// ============================================================================

BrowserWidget::BrowserWidget(QtMainWindow* window, size_t tab_index, QWidget* parent)
    : QOpenGLWidget(parent)
    , window_(window)
    , tab_index_(tab_index)
    , renderer_(nullptr)
    , gl_initialized_(false)
{
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

// ============================================================================
// OpenGL Overrides (replace GTK GL callbacks)
// ============================================================================

void BrowserWidget::initializeGL() {
  // Replaces GTK on_gl_realize callback
  logger.Info("OpenGL context initialized");
  logger.Info(std::string("OpenGL version: ") +
              reinterpret_cast<const char*>(glGetString(GL_VERSION)));

  gl_initialized_ = true;

  // Clear to white
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

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
  // Replaces GTK on_gl_render callback

  if (!renderer_) {
    // No renderer yet, draw white background
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }

  // Render browser frame
  auto result = renderer_->Render();
  if (!result.IsOk()) {
    logger.Warn("Render failed: " + result.GetError().Message());
  }
}

void BrowserWidget::resizeGL(int w, int h) {
  // Replaces GTK on_size_allocate callback (for GL widget only)

  glViewport(0, 0, w, h);

  // Notify QtMainWindow → which updates CEF and renderer for THIS tab
  if (window_) {
    window_->OnBrowserSizeChanged(tab_index_, w, h);
  }
}

// ============================================================================
// Input Event Handlers (replace GTK input callbacks)
// ============================================================================

void BrowserWidget::mouseMoveEvent(QMouseEvent* event) {
  // Replaces GTK on_motion_notify

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
  // Replaces GTK on_button_press

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
    case Qt::LeftButton:   buttonType = MBT_LEFT; break;
    case Qt::MiddleButton: buttonType = MBT_MIDDLE; break;
    case Qt::RightButton:  buttonType = MBT_RIGHT; break;
    default: return;
  }

  int clickCount = 1;
  if (event->type() == QEvent::MouseButtonDblClick) {
    clickCount = 2;
  }

  client->GetBrowser()->GetHost()->SendMouseClickEvent(
      mouseEvent, buttonType, false, clickCount
  );
}

void BrowserWidget::mouseReleaseEvent(QMouseEvent* event) {
  // Replaces GTK on_button_release

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
    case Qt::LeftButton:   buttonType = MBT_LEFT; break;
    case Qt::MiddleButton: buttonType = MBT_MIDDLE; break;
    case Qt::RightButton:  buttonType = MBT_RIGHT; break;
    default: return;
  }

  client->GetBrowser()->GetHost()->SendMouseClickEvent(
      mouseEvent, buttonType, true, 1
  );
}

void BrowserWidget::wheelEvent(QWheelEvent* event) {
  // Replaces GTK on_scroll

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
  // Scale factor to match GTK behavior (GTK uses 40 pixels per scroll unit)
  int deltaX = event->angleDelta().x() / 8 * 5;
  int deltaY = event->angleDelta().y() / 8 * 5;

  client->GetBrowser()->GetHost()->SendMouseWheelEvent(
      mouseEvent, deltaX, deltaY
  );
}

void BrowserWidget::keyPressEvent(QKeyEvent* event) {
  // Replaces GTK on_key_press

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
  // Replaces GTK on_key_release

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
  // Replaces GTK on_focus_in
  QOpenGLWidget::focusInEvent(event);

  auto* client = GetCefClientForThisTab();
  if (client && client->GetBrowser()) {
    client->GetBrowser()->GetHost()->SetFocus(true);
  }
}

void BrowserWidget::focusOutEvent(QFocusEvent* event) {
  // Replaces GTK on_focus_out
  QOpenGLWidget::focusOutEvent(event);

  auto* client = GetCefClientForThisTab();
  if (client && client->GetBrowser()) {
    client->GetBrowser()->GetHost()->SetFocus(false);
  }
}

// ============================================================================
// Helper Methods (same logic as GTK callbacks)
// ============================================================================

uint32_t BrowserWidget::getCefModifiers(Qt::KeyboardModifiers qtMods,
                                       Qt::MouseButtons qtButtons) const {
  uint32_t cefMods = 0;

  // Keyboard modifiers
  if (qtMods & Qt::ShiftModifier)   cefMods |= EVENTFLAG_SHIFT_DOWN;
  if (qtMods & Qt::ControlModifier) cefMods |= EVENTFLAG_CONTROL_DOWN;
  if (qtMods & Qt::AltModifier)     cefMods |= EVENTFLAG_ALT_DOWN;

  // Mouse button modifiers
  if (qtButtons & Qt::LeftButton)   cefMods |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  if (qtButtons & Qt::MiddleButton) cefMods |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
  if (qtButtons & Qt::RightButton)  cefMods |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

  return cefMods;
}

int BrowserWidget::getWindowsKeyCode(int qtKey) const {
  // Map Qt keys to Windows virtual key codes
  // Same logic as GTK's GetWindowsKeyCode

  if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
    return qtKey;  // '0'-'9' same in both
  }
  if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
    return qtKey;  // 'A'-'Z' same in both
  }

  if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
    return 0x70 + (qtKey - Qt::Key_F1);
  }

  switch (qtKey) {
    case Qt::Key_Return:     return 0x0D;
    case Qt::Key_Escape:     return 0x1B;
    case Qt::Key_Backspace:  return 0x08;
    case Qt::Key_Tab:        return 0x09;
    case Qt::Key_Space:      return 0x20;
    case Qt::Key_Delete:     return 0x2E;
    case Qt::Key_Home:       return 0x24;
    case Qt::Key_End:        return 0x23;
    case Qt::Key_PageUp:     return 0x21;
    case Qt::Key_PageDown:   return 0x22;
    case Qt::Key_Left:       return 0x25;
    case Qt::Key_Up:         return 0x26;
    case Qt::Key_Right:      return 0x27;
    case Qt::Key_Down:       return 0x28;
    case Qt::Key_Insert:     return 0x2D;
    case Qt::Key_Shift:      return 0x10;
    case Qt::Key_Control:    return 0x11;
    case Qt::Key_Alt:        return 0x12;
    default:                 return qtKey;
  }
}

}  // namespace platform
}  // namespace athena

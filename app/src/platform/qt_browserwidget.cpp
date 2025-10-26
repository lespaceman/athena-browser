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

#include <QDebug>
#include <QTimer>

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
    : QOpenGLWidget(parent),
      window_(window),
      tab_index_(tab_index),
      renderer_(nullptr),
      gl_initialized_(false) {
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
// OpenGL Overrides (Qt OpenGL widget callbacks)
// ============================================================================

void BrowserWidget::initializeGL() {
  // Called when OpenGL context is ready
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
  // Called to render the current frame

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
  // Called when widget is resized

  glViewport(0, 0, w, h);

  // Defer resize notification until after Qt layout completes
  // This prevents race conditions during window maximize/resize where the widget's
  // dimensions might be stale (before the QSplitter redistributes space).
  // QTimer::singleShot(0, ...) defers execution to the next event loop iteration.
  if (window_) {
    // Capture by value to ensure thread safety and handle widget deletion
    QTimer::singleShot(0, this, [this, w, h]() {
      // Check if window_ is still valid (widget might have been deleted)
      if (window_) {
        // Get current dimensions (layout might have updated since resizeGL was called)
        int current_width = width();
        int current_height = height();

        // Use current dimensions if they differ significantly from the resize event values
        // This ensures we always use the post-layout dimensions
        if (current_width > 0 && current_height > 0) {
          window_->OnBrowserSizeChanged(tab_index_, current_width, current_height);
        } else {
          // Fallback to original dimensions if current dimensions are invalid
          window_->OnBrowserSizeChanged(tab_index_, w, h);
        }
      }
    });
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
    case Qt::Key_Return:
      return 0x0D;
    case Qt::Key_Escape:
      return 0x1B;
    case Qt::Key_Backspace:
      return 0x08;
    case Qt::Key_Tab:
      return 0x09;
    case Qt::Key_Space:
      return 0x20;
    case Qt::Key_Delete:
      return 0x2E;
    case Qt::Key_Home:
      return 0x24;
    case Qt::Key_End:
      return 0x23;
    case Qt::Key_PageUp:
      return 0x21;
    case Qt::Key_PageDown:
      return 0x22;
    case Qt::Key_Left:
      return 0x25;
    case Qt::Key_Up:
      return 0x26;
    case Qt::Key_Right:
      return 0x27;
    case Qt::Key_Down:
      return 0x28;
    case Qt::Key_Insert:
      return 0x2D;
    case Qt::Key_Shift:
      return 0x10;
    case Qt::Key_Control:
      return 0x11;
    case Qt::Key_Alt:
      return 0x12;
    default:
      return qtKey;
  }
}

}  // namespace platform
}  // namespace athena

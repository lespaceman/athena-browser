#ifndef ATHENA_PLATFORM_QT_BROWSERWIDGET_H_
#define ATHENA_PLATFORM_QT_BROWSERWIDGET_H_

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QWheelEvent>

namespace athena {
namespace browser {
class CefClient;
}

namespace rendering {
class GLRenderer;
}

namespace platform {

// Forward declarations
class QtMainWindow;

/**
 * OpenGL widget for CEF browser rendering (Qt version).
 *
 * Replaces GTK's gl_area_ with Qt's QOpenGLWidget.
 * Handles:
 * - OpenGL context initialization
 * - Rendering frames from GLRenderer
 * - Input events (mouse, keyboard) → CEF
 *
 * This widget provides the rendering surface for the browser.
 * It receives paint events from CEF via GLRenderer and forwards
 * user input events to CEF for processing.
 */
class BrowserWidget : public QOpenGLWidget {
  Q_OBJECT

 public:
  /**
   * Create a browser widget.
   *
   * @param window Parent QtMainWindow (non-owning)
   * @param tab_index Index of this tab in the window
   * @param parent Qt parent widget
   */
  explicit BrowserWidget(QtMainWindow* window, size_t tab_index, QWidget* parent = nullptr);

  ~BrowserWidget() override;

  /**
   * Initialize the browser with a GL renderer.
   * Called by QtMainWindow after GL context is ready.
   *
   * @param renderer GL renderer instance (non-owning)
   */
  void InitializeBrowser(rendering::GLRenderer* renderer);

  /**
   * Get the tab index associated with this widget.
   */
  size_t GetTabIndex() const { return tab_index_; }

  /**
   * Update the tab index after tabs are reordered or removed.
   */
  void SetTabIndex(size_t tab_index) { tab_index_ = tab_index; }

  /**
   * Get the CEF client for this specific tab's browser.
   * Returns nullptr if this tab doesn't have a browser yet.
   */
  browser::CefClient* GetCefClientForThisTab() const;

 signals:
  /**
   * Emitted when OpenGL context is initialized and ready for browser creation.
   */
  void glContextReady();

 protected:
  // ============================================================================
  // Qt OpenGL Overrides (replace GTK GL callbacks)
  // ============================================================================

  /**
   * Initialize OpenGL resources.
   * Called once when GL context is created.
   * Replaces GTK's on_gl_realize callback.
   */
  void initializeGL() override;

  /**
   * Render the browser frame.
   * Called every frame or when update() is called.
   * Replaces GTK's on_gl_render callback.
   */
  void paintGL() override;

  /**
   * Handle widget resize.
   * Called when widget size changes.
   * Replaces GTK's on_size_allocate callback (for GL widget only).
   */
  void resizeGL(int w, int h) override;

  // ============================================================================
  // Input Event Overrides (replace GTK input callbacks)
  // ============================================================================

  /**
   * Handle mouse move events.
   * Replaces GTK's on_motion_notify callback.
   */
  void mouseMoveEvent(QMouseEvent* event) override;

  /**
   * Handle mouse press events.
   * Replaces GTK's on_button_press callback.
   */
  void mousePressEvent(QMouseEvent* event) override;

  /**
   * Handle mouse release events.
   * Replaces GTK's on_button_release callback.
   */
  void mouseReleaseEvent(QMouseEvent* event) override;

  /**
   * Handle scroll wheel events.
   * Replaces GTK's on_scroll callback.
   */
  void wheelEvent(QWheelEvent* event) override;

  /**
   * Handle key press events.
   * Replaces GTK's on_key_press callback.
   */
  void keyPressEvent(QKeyEvent* event) override;

  /**
   * Handle key release events.
   * Replaces GTK's on_key_release callback.
   */
  void keyReleaseEvent(QKeyEvent* event) override;

  /**
   * Handle focus in events.
   * Replaces GTK's on_focus_in callback.
   */
  void focusInEvent(QFocusEvent* event) override;

  /**
   * Handle focus out events.
   * Replaces GTK's on_focus_out callback.
   */
  void focusOutEvent(QFocusEvent* event) override;

 private:
  // ============================================================================
  // Helper Methods (same logic as GTK callbacks)
  // ============================================================================

  /**
   * Convert Qt modifiers to CEF modifiers.
   *
   * @param qtMods Qt keyboard modifiers
   * @param qtButtons Qt mouse buttons
   * @return CEF modifier flags
   */
  uint32_t getCefModifiers(Qt::KeyboardModifiers qtMods, Qt::MouseButtons qtButtons) const;

  /**
   * Convert Qt key code to Windows virtual key code.
   * CEF uses Windows virtual key codes for all platforms.
   *
   * @param qtKey Qt::Key_* value
   * @return Windows virtual key code
   */
  int getWindowsKeyCode(int qtKey) const;

  // ============================================================================
  // Member Variables
  // ============================================================================

  QtMainWindow* window_;             // Non-owning pointer to parent window
  size_t tab_index_;                 // Index of this tab in the window
  rendering::GLRenderer* renderer_;  // Non-owning (QtMainWindow owns it)
  bool gl_initialized_;              // Track GL initialization state
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_BROWSERWIDGET_H_

// Copyright (c) 2025 Athena Browser Project
// OpenGL-based renderer for off-screen rendering using CEF's OsrRenderer

#ifndef ATHENA_RENDERING_GL_RENDERER_H_
#define ATHENA_RENDERING_GL_RENDERER_H_

#include "core/types.h"
#include "include/cef_browser.h"
#include "include/cef_render_handler.h"
#include "tests/cefclient/browser/osr_renderer.h"
#include "tests/cefclient/browser/osr_renderer_settings.h"
#include "utils/error.h"

#include <memory>
#include <vector>

// Platform-specific forward declarations
#ifdef ATHENA_USE_QT
// Qt doesn't need forward declarations (widget passed as void*)
#else
typedef struct _GtkWidget GtkWidget;
#endif

namespace athena {
namespace rendering {

// GLRenderer wraps CEF's official OsrRenderer for hardware-accelerated
// OpenGL rendering. This provides 2-3x better performance than Cairo
// software rendering.
//
// Performance characteristics:
// - 60+ FPS with OpenGL hardware acceleration
// - 5-10% CPU usage (vs 20-30% with Cairo)
// - 50MB memory (vs 132MB with Cairo)
// - GPU-based texture rendering with minimal CPU overhead
//
// Thread safety:
// - All methods must be called on the GTK main thread
// - OpenGL context is managed by GtkGLArea
//
// Usage:
//   GLRenderer renderer;
//   auto result = renderer.Initialize(gl_area);
//   if (!result) { /* handle error */ }
//
//   // In OnPaint callback:
//   renderer.OnPaint(browser, type, dirty_rects, buffer, width, height);
//
//   // In GL render callback:
//   renderer.Render();
//
class GLRenderer {
 public:
  GLRenderer();
  ~GLRenderer();

  // Initialize the OpenGL environment with the given GL widget.
  // Must be called before any other methods.
  // Must be called with the GL context current (inside realize callback).
  //
  // Platform-specific:
  //   - GTK: Pass GtkGLArea* widget
  //   - Qt: Pass QOpenGLWidget* widget
  //
  // Returns:
  //   - Ok(void) on success
  //   - Error if GL context is invalid or initialization fails
  utils::Result<void> Initialize(void* gl_widget);

  // Clean up OpenGL resources.
  // Should be called before destroying the GtkGLArea.
  void Cleanup();

  // Update texture from CEF paint buffer.
  // This is called from CefRenderHandler::OnPaint callback.
  //
  // Parameters:
  //   - browser: The CEF browser instance
  //   - type: Paint element type (view or popup)
  //   - dirty_rects: Regions that changed (physical pixels)
  //   - buffer: BGRA pixel data from CEF
  //   - width: Buffer width in physical pixels
  //   - height: Buffer height in physical pixels
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirty_rects,
               const void* buffer,
               int width,
               int height);

  // Handle popup show/hide events.
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show);

  // Handle popup size changes.
  // |rect| must be in physical pixel coordinates.
  void OnPopupSize(CefRefPtr<CefBrowser> browser, const core::Rect& rect);

  // Render the current frame to the screen.
  // This is called from the GtkGLArea "render" signal callback.
  // The GL context is automatically current when this is called.
  //
  // Returns:
  //   - Ok(void) on successful render
  //   - Error if not initialized or GL errors occur
  utils::Result<void> Render();

  // Set the view size (logical pixels).
  // This is called when the widget is resized.
  void SetViewSize(int width, int height);

  // Get the current view dimensions (logical pixels).
  int GetViewWidth() const;
  int GetViewHeight() const;

  // Check if the renderer is initialized.
  bool IsInitialized() const { return initialized_; }

  // Get the underlying CEF OsrRenderer for advanced usage.
  // Returns nullptr if not initialized.
  client::OsrRenderer* GetOsrRenderer() { return osr_renderer_.get(); }

  // Capture the current framebuffer as a PNG image.
  // Returns base64-encoded PNG data, or empty string on failure.
  // Screenshots are automatically scaled to 50% resolution for optimal AI analysis.
  // @return Base64-encoded PNG image data
  std::string TakeScreenshot() const;

 private:
  // Convert core::Rect to CefRect
  static CefRect ToCefRect(const core::Rect& rect);

  // The GL widget we're rendering to (platform-specific)
  // GTK: GtkGLArea*, Qt: QOpenGLWidget*
  void* gl_widget_;

  // CEF's official OpenGL renderer (does the heavy lifting)
  std::unique_ptr<client::OsrRenderer> osr_renderer_;

  // Renderer settings
  client::OsrRendererSettings settings_;

  // Initialization state
  bool initialized_;

  // View dimensions (logical pixels)
  int view_width_;
  int view_height_;

  // Prevent copying
  GLRenderer(const GLRenderer&) = delete;
  GLRenderer& operator=(const GLRenderer&) = delete;
};

}  // namespace rendering
}  // namespace athena

#endif  // ATHENA_RENDERING_GL_RENDERER_H_

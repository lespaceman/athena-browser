// Copyright (c) 2025 Athena Browser Project
// OpenGL-based renderer implementation

#include "rendering/gl_renderer.h"

#include "include/base/cef_logging.h"
#include "utils/logging.h"

#include <GL/gl.h>

#include <iostream>

// Platform-specific includes
#ifdef ATHENA_USE_QT
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QOpenGLWidget>
#else
#include <gtk/gtk.h>
#endif

namespace athena {
namespace rendering {

namespace {

#ifndef ATHENA_USE_QT
// GTK-specific: Helper to manage GL context
class ScopedGLContext {
 public:
  explicit ScopedGLContext(void* gl_widget) : gl_widget_(gl_widget), valid_(false) {
    if (!gl_widget_) {
      return;
    }

    auto* gtk_widget = static_cast<GtkWidget*>(gl_widget_);
    gtk_gl_area_make_current(GTK_GL_AREA(gtk_widget));
    valid_ = gtk_gl_area_get_error(GTK_GL_AREA(gtk_widget)) == nullptr;
  }

  bool IsValid() const { return valid_; }

 private:
  void* gl_widget_;
  bool valid_;
};
#else
// Qt-specific: Make GL context current for CEF threads
class ScopedGLContext {
 public:
  explicit ScopedGLContext(void* gl_widget) : gl_widget_(gl_widget), valid_(false) {
    if (!gl_widget_) {
      return;
    }

    auto* widget = static_cast<QOpenGLWidget*>(gl_widget_);
    widget->makeCurrent();
    valid_ = true;  // Qt's makeCurrent() always succeeds or throws
  }

  ~ScopedGLContext() {
    if (valid_ && gl_widget_) {
      auto* widget = static_cast<QOpenGLWidget*>(gl_widget_);
      widget->doneCurrent();
    }
  }

  bool IsValid() const { return valid_; }

 private:
  void* gl_widget_;
  bool valid_;
};
#endif

}  // namespace

GLRenderer::GLRenderer()
    : gl_widget_(nullptr),
      osr_renderer_(nullptr),
      initialized_(false),
      view_width_(0),
      view_height_(0) {
  // Configure renderer settings
  settings_.show_update_rect = false;                                // Disable debug rectangles
  settings_.background_color = CefColorSetARGB(255, 255, 255, 255);  // White background
  settings_.real_screen_bounds = true;             // Enable correct screen bounds reporting
  settings_.shared_texture_enabled = false;        // Not supported on Linux
  settings_.external_begin_frame_enabled = false;  // Use CEF's internal timing
}

GLRenderer::~GLRenderer() {
  Cleanup();
}

utils::Result<void> GLRenderer::Initialize(void* gl_widget) {
  if (initialized_) {
    return utils::Error("Renderer already initialized");
  }

  if (!gl_widget) {
    return utils::Error("gl_widget cannot be null");
  }

#ifndef ATHENA_USE_QT
  // GTK-specific validation
  auto* gtk_widget = static_cast<GtkWidget*>(gl_widget);
  if (!GTK_IS_GL_AREA(gtk_widget)) {
    return utils::Error("widget must be a GtkGLArea");
  }
#else
  // Qt-specific: Assume QOpenGLWidget* - context is managed by Qt
  // No validation needed, Qt handles everything
#endif

  ScopedGLContext context(gl_widget);
  if (!context.IsValid()) {
    return utils::Error("Failed to make GL context current for renderer initialization");
  }

  gl_widget_ = gl_widget;

  // Create CEF's OsrRenderer
  osr_renderer_ = std::make_unique<client::OsrRenderer>(settings_);

  // Initialize OpenGL resources
  osr_renderer_->Initialize();

  initialized_ = true;

  std::cout << "[GLRenderer] Initialized successfully with OpenGL acceleration" << std::endl;

  return utils::Ok();
}

void GLRenderer::Cleanup() {
  if (!initialized_) {
    return;
  }

  if (osr_renderer_) {
    // Platform-specific widget validation
    bool widget_valid = false;

#ifndef ATHENA_USE_QT
    // GTK: Check if widget is still valid
    if (gl_widget_) {
      auto* gtk_widget = static_cast<GtkWidget*>(gl_widget_);
      widget_valid = (G_IS_OBJECT(gtk_widget) && GTK_IS_GL_AREA(gtk_widget));
    }
#else
    // Qt: Widget pointer is valid (Qt manages lifecycle)
    widget_valid = (gl_widget_ != nullptr);
#endif

    if (widget_valid) {
      ScopedGLContext context(gl_widget_);
      if (!context.IsValid()) {
        std::cerr << "[GLRenderer] Warning: GL context invalid during cleanup" << std::endl;
      }
      osr_renderer_->Cleanup();
    } else {
      // Widget already destroyed, just clean up CEF renderer without GL context
      osr_renderer_->Cleanup();
    }
    osr_renderer_.reset();
  }

  initialized_ = false;
  gl_widget_ = nullptr;

  std::cout << "[GLRenderer] Cleaned up" << std::endl;
}

void GLRenderer::OnPaint(CefRefPtr<CefBrowser> browser,
                         CefRenderHandler::PaintElementType type,
                         const CefRenderHandler::RectList& dirty_rects,
                         const void* buffer,
                         int width,
                         int height) {
  if (!initialized_ || !osr_renderer_) {
    std::cerr << "[GLRenderer] Warning: OnPaint called but renderer not initialized" << std::endl;
    return;
  }

  ScopedGLContext context(gl_widget_);
  if (!context.IsValid()) {
    std::cerr << "[GLRenderer] Warning: Unable to make GL context current during OnPaint"
              << std::endl;
    return;
  }

  // Forward to CEF's OsrRenderer which will update the GL texture
  osr_renderer_->OnPaint(browser, type, dirty_rects, buffer, width, height);
}

void GLRenderer::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) {
  if (!initialized_ || !osr_renderer_) {
    return;
  }

  osr_renderer_->OnPopupShow(browser, show);
}

void GLRenderer::OnPopupSize(CefRefPtr<CefBrowser> browser, const core::Rect& rect) {
  if (!initialized_ || !osr_renderer_) {
    return;
  }

  CefRect cef_rect = ToCefRect(rect);
  osr_renderer_->OnPopupSize(browser, cef_rect);
}

utils::Result<void> GLRenderer::Render() {
  if (!initialized_ || !osr_renderer_) {
    return utils::Error("Renderer not initialized");
  }

  if (!gl_widget_) {
    return utils::Error("No GL widget set");
  }

  // The GL context is automatically current when this is called
  // GTK: from GtkGLArea "render" signal
  // Qt: from QOpenGLWidget::paintGL()

  // Let CEF's renderer do the actual OpenGL rendering
  osr_renderer_->Render();

  // Check for GL errors
  GLenum gl_error = glGetError();
  if (gl_error != GL_NO_ERROR) {
    std::string error_msg = "OpenGL error during render: " + std::to_string(gl_error);
    return utils::Error(error_msg);
  }

  return utils::Ok();
}

void GLRenderer::SetViewSize(int width, int height) {
  view_width_ = width;
  view_height_ = height;

  if (initialized_ && osr_renderer_) {
    // CEF's OsrRenderer doesn't have a SetViewSize method
    // It gets size from OnPaint calls
    // We just track it for GetViewWidth/Height
  }
}

int GLRenderer::GetViewWidth() const {
  if (initialized_ && osr_renderer_) {
    return osr_renderer_->GetViewWidth();
  }
  return view_width_;
}

int GLRenderer::GetViewHeight() const {
  if (initialized_ && osr_renderer_) {
    return osr_renderer_->GetViewHeight();
  }
  return view_height_;
}

CefRect GLRenderer::ToCefRect(const core::Rect& rect) {
  return CefRect(rect.x, rect.y, rect.width, rect.height);
}

std::string GLRenderer::TakeScreenshot() const {
  if (!initialized_ || !osr_renderer_ || !gl_widget_) {
    std::cerr << "[GLRenderer] Warning: Cannot take screenshot - renderer not initialized"
              << std::endl;
    return "";
  }

  // Make GL context current
  ScopedGLContext context(gl_widget_);
  if (!context.IsValid()) {
    std::cerr << "[GLRenderer] Warning: Unable to make GL context current for screenshot"
              << std::endl;
    return "";
  }

  int width = GetViewWidth();
  int height = GetViewHeight();

  if (width <= 0 || height <= 0) {
    std::cerr << "[GLRenderer] Warning: Invalid view size for screenshot" << std::endl;
    return "";
  }

  // Read pixels from the framebuffer
  std::vector<unsigned char> pixels(width * height * 4);  // RGBA
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  // Check for GL errors
  GLenum gl_error = glGetError();
  if (gl_error != GL_NO_ERROR) {
    std::cerr << "[GLRenderer] OpenGL error during screenshot: " << gl_error << std::endl;
    return "";
  }

  // Flip image vertically (OpenGL bottom-left origin -> PNG top-left origin)
  std::vector<unsigned char> flipped(width * height * 4);
  for (int y = 0; y < height; y++) {
    memcpy(&flipped[y * width * 4], &pixels[(height - 1 - y) * width * 4], width * 4);
  }

#ifdef ATHENA_USE_QT
  // Use Qt to encode as PNG and convert to base64
  QImage image(flipped.data(), width, height, width * 4, QImage::Format_RGBA8888);

  QByteArray byte_array;
  QBuffer buffer(&byte_array);
  buffer.open(QIODevice::WriteOnly);

  if (!image.save(&buffer, "PNG")) {
    std::cerr << "[GLRenderer] Failed to encode PNG" << std::endl;
    return "";
  }

  // Convert to base64
  QByteArray base64 = byte_array.toBase64();
  return std::string(base64.constData(), base64.size());
#else
  // GTK version: Would need to implement PNG encoding using libpng
  std::cerr << "[GLRenderer] PNG encoding not yet implemented for GTK" << std::endl;
  return "";
#endif
}

}  // namespace rendering
}  // namespace athena

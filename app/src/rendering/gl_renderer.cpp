// Copyright (c) 2025 Athena Browser Project
// OpenGL-based renderer implementation

#include "rendering/gl_renderer.h"

#include "include/base/cef_logging.h"
#include "utils/logging.h"

#include <GL/gl.h>

#include <iostream>

// Platform-specific includes
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QOpenGLWidget>

namespace athena {
namespace rendering {

namespace {

// Helper to manage GL context for CEF threads
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

}  // namespace

// Static logger for this module
static utils::Logger logger("GLRenderer");

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

  // Assume QOpenGLWidget* - context is managed by Qt
  // No validation needed, Qt handles everything

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

  logger.Info("Initialized successfully with OpenGL acceleration");

  return utils::Ok();
}

void GLRenderer::Cleanup() {
  if (!initialized_) {
    return;
  }

  if (osr_renderer_) {
    // Qt: Widget pointer is valid (Qt manages lifecycle)
    bool widget_valid = (gl_widget_ != nullptr);

    if (widget_valid) {
      ScopedGLContext context(gl_widget_);
      if (!context.IsValid()) {
        logger.Warn("GL context invalid during cleanup");
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

  logger.Info("Cleaned up");
}

void GLRenderer::OnPaint(CefRefPtr<CefBrowser> browser,
                         CefRenderHandler::PaintElementType type,
                         const CefRenderHandler::RectList& dirty_rects,
                         const void* buffer,
                         int width,
                         int height) {
  if (!initialized_ || !osr_renderer_) {
    logger.Warn("OnPaint called but renderer not initialized");
    return;
  }

  ScopedGLContext context(gl_widget_);
  if (!context.IsValid()) {
    logger.Warn("Unable to make GL context current during OnPaint");
    return;
  }

  // Performance Optimization: Dirty Rect Updates
  // CEF's OsrRenderer automatically uses dirty_rects to optimize texture updates:
  // - Full update (glTexImage2D): When size changes or full-screen dirty rect
  // - Partial update (glTexSubImage2D): Only updates changed regions (~2x FPS improvement)
  // This provides significant performance gains by avoiding unnecessary texture uploads.
  if (logger.IsDebugEnabled() && !dirty_rects.empty()) {
    bool is_full_update =
        (dirty_rects.size() == 1 && dirty_rects[0] == CefRect(0, 0, width, height));
    if (is_full_update) {
      logger.Debug("OnPaint: Full texture update ({}x{})", width, height);
    } else {
      logger.Debug("OnPaint: Partial update ({} dirty rects)", dirty_rects.size());
    }
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
    logger.Warn("Cannot take screenshot - renderer not initialized");
    return "";
  }

  // Fixed scale for optimal AI analysis (50% of original resolution)
  const float scale = 0.5f;

  // Make GL context current
  ScopedGLContext context(gl_widget_);
  if (!context.IsValid()) {
    logger.Warn("Unable to make GL context current for screenshot");
    return "";
  }

  int width = GetViewWidth();
  int height = GetViewHeight();

  if (width <= 0 || height <= 0) {
    logger.Warn("Invalid view size for screenshot");
    return "";
  }

  // Read pixels from the framebuffer
  std::vector<unsigned char> pixels(width * height * 4);  // RGBA
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  // Check for GL errors
  GLenum gl_error = glGetError();
  if (gl_error != GL_NO_ERROR) {
    logger.Error("OpenGL error during screenshot: {}", gl_error);
    return "";
  }

  // Flip image vertically (OpenGL bottom-left origin -> PNG top-left origin)
  std::vector<unsigned char> flipped(width * height * 4);
  for (int y = 0; y < height; y++) {
    memcpy(&flipped[y * width * 4], &pixels[(height - 1 - y) * width * 4], width * 4);
  }

  // Use Qt to encode as PNG and convert to base64
  QImage image(flipped.data(), width, height, width * 4, QImage::Format_RGBA8888);

  // Scale down the image if requested (scale < 1.0)
  if (scale < 1.0f) {
    int scaled_width = static_cast<int>(width * scale);
    int scaled_height = static_cast<int>(height * scale);

    // Ensure minimum 1x1 pixel image
    scaled_width = std::max(1, scaled_width);
    scaled_height = std::max(1, scaled_height);

    image =
        image.scaled(scaled_width, scaled_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    logger.Debug("Screenshot scaled from {}x{} to {}x{} (scale={})",
                 width,
                 height,
                 scaled_width,
                 scaled_height,
                 scale);
  }

  QByteArray byte_array;
  QBuffer buffer(&byte_array);
  buffer.open(QIODevice::WriteOnly);

  if (!image.save(&buffer, "PNG")) {
    logger.Error("Failed to encode PNG");
    return "";
  }

  // Convert to base64
  QByteArray base64 = byte_array.toBase64();
  return std::string(base64.constData(), base64.size());
}

}  // namespace rendering
}  // namespace athena

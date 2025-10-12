// Copyright (c) 2025 Athena Browser Project
// OpenGL-based renderer implementation

#include "rendering/gl_renderer.h"

#include <GL/gl.h>
#include <iostream>

#include "include/base/cef_logging.h"
#include "utils/logging.h"

namespace athena {
namespace rendering {

namespace {

class ScopedGLContext {
 public:
  explicit ScopedGLContext(GtkWidget* gl_area)
      : gl_area_(gl_area), valid_(false) {
    if (!gl_area_) {
      return;
    }

    gtk_gl_area_make_current(GTK_GL_AREA(gl_area_));
    valid_ = gtk_gl_area_get_error(GTK_GL_AREA(gl_area_)) == nullptr;
  }

  bool IsValid() const { return valid_; }

 private:
  GtkWidget* gl_area_;
  bool valid_;
};

}  // namespace

GLRenderer::GLRenderer()
    : gl_area_(nullptr),
      osr_renderer_(nullptr),
      initialized_(false),
      view_width_(0),
      view_height_(0) {
  // Configure renderer settings
  settings_.show_update_rect = false;  // Disable debug rectangles
  settings_.background_color = CefColorSetARGB(255, 255, 255, 255);  // White background
  settings_.real_screen_bounds = true;  // Enable correct screen bounds reporting
  settings_.shared_texture_enabled = false;  // Not supported on Linux
  settings_.external_begin_frame_enabled = false;  // Use CEF's internal timing
}

GLRenderer::~GLRenderer() {
  Cleanup();
}

utils::Result<void> GLRenderer::Initialize(GtkWidget* gl_area) {
  if (initialized_) {
    return utils::Error("Renderer already initialized");
  }

  if (!gl_area) {
    return utils::Error("gl_area cannot be null");
  }

  if (!GTK_IS_GL_AREA(gl_area)) {
    return utils::Error("widget must be a GtkGLArea");
  }

  ScopedGLContext context(gl_area);
  if (!context.IsValid()) {
    return utils::Error("Failed to make GL context current for renderer initialization");
  }

  gl_area_ = gl_area;

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
    ScopedGLContext context(gl_area_);
    if (!context.IsValid()) {
      std::cerr << "[GLRenderer] Warning: GL context invalid during cleanup" << std::endl;
    }
    osr_renderer_->Cleanup();
    osr_renderer_.reset();
  }

  initialized_ = false;
  gl_area_ = nullptr;

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

  ScopedGLContext context(gl_area_);
  if (!context.IsValid()) {
    std::cerr << "[GLRenderer] Warning: Unable to make GL context current during OnPaint" << std::endl;
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

void GLRenderer::OnPopupSize(CefRefPtr<CefBrowser> browser,
                             const core::Rect& rect) {
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

  if (!gl_area_) {
    return utils::Error("No GL area set");
  }

  // The GL context is automatically current when this is called
  // from the GtkGLArea "render" signal

  // Let CEF's renderer do the actual OpenGL rendering
  osr_renderer_->Render();

  // Check for GL errors
  GLenum gl_error = glGetError();
  if (gl_error != GL_NO_ERROR) {
    std::string error_msg = "OpenGL error during render: " +
                           std::to_string(gl_error);
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

}  // namespace rendering
}  // namespace athena

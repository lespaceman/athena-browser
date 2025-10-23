#ifndef ATHENA_TESTS_MOCKS_MOCK_GL_RENDERER_H_
#define ATHENA_TESTS_MOCKS_MOCK_GL_RENDERER_H_

#include "rendering/gl_renderer.h"

#include <gmock/gmock.h>

namespace athena {
namespace rendering {
namespace testing {

/**
 * Mock implementation of GLRenderer for testing.
 *
 * This mock is used to test components that depend on GLRenderer
 * without requiring actual OpenGL initialization or Qt widgets.
 *
 * Since GLRenderer is not an interface, this mock provides a simple
 * fake implementation that tracks method calls.
 */
class MockGLRenderer {
 public:
  MockGLRenderer() = default;
  ~MockGLRenderer() = default;

  MOCK_METHOD(void,
              OnPaint,
              (CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirty_rects,
               const void* buffer,
               int width,
               int height),
              ());

  MOCK_METHOD(void, OnPopupShow, (CefRefPtr<CefBrowser> browser, bool show), ());
  MOCK_METHOD(void, OnPopupSize, (CefRefPtr<CefBrowser> browser, const core::Rect& rect), ());
  MOCK_METHOD(void, SetViewSize, (int width, int height), ());
  MOCK_METHOD(int, GetViewWidth, (), (const));
  MOCK_METHOD(int, GetViewHeight, (), (const));
  MOCK_METHOD(bool, IsInitialized, (), (const));
};

}  // namespace testing
}  // namespace rendering
}  // namespace athena

#endif  // ATHENA_TESTS_MOCKS_MOCK_GL_RENDERER_H_

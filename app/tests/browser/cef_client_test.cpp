#include "browser/cef_client.h"

#include "rendering/gl_renderer.h"

#include <gtest/gtest.h>

/**
 * CefClient Unit Tests
 *
 * These tests verify CefClient's state management without requiring
 * actual CEF browser initialization.
 *
 * We test:
 * - Constructor initialization
 * - Size management (GetWidth/GetHeight/SetSize)
 * - Device scale factor management
 * - GetViewRect behavior
 * - GetScreenInfo behavior
 */
class CefClientTest : public ::testing::Test {
 protected:
  void SetUp() override { window_handle_ = nullptr; }

  void* window_handle_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(CefClientTest, ConstructorInitializesDefaults) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  EXPECT_EQ(athena_client.GetWidth(), 0);
  EXPECT_EQ(athena_client.GetHeight(), 0);
  EXPECT_FLOAT_EQ(athena_client.GetDeviceScaleFactor(), 1.0f);
  EXPECT_EQ(athena_client.GetBrowser(), nullptr);
}

TEST_F(CefClientTest, ConstructorAcceptsNullWindow) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(nullptr, &gl_renderer);

  EXPECT_EQ(athena_client.GetBrowser(), nullptr);
}

TEST_F(CefClientTest, ConstructorAcceptsNullRenderer) {
  athena::browser::CefClient athena_client(window_handle_, nullptr);

  EXPECT_EQ(athena_client.GetBrowser(), nullptr);
}

// ============================================================================
// Size Management Tests
// ============================================================================

TEST_F(CefClientTest, SetSizeUpdatesWidthAndHeight) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetSize(1920, 1080);

  EXPECT_EQ(athena_client.GetWidth(), 1920);
  EXPECT_EQ(athena_client.GetHeight(), 1080);
}

TEST_F(CefClientTest, SetSizeZeroDimensions) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetSize(0, 0);

  EXPECT_EQ(athena_client.GetWidth(), 0);
  EXPECT_EQ(athena_client.GetHeight(), 0);
}

TEST_F(CefClientTest, SetSizeSmallDimensions) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetSize(1, 1);

  EXPECT_EQ(athena_client.GetWidth(), 1);
  EXPECT_EQ(athena_client.GetHeight(), 1);
}

TEST_F(CefClientTest, SetSizeLargeDimensions) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetSize(3840, 2160);  // 4K

  EXPECT_EQ(athena_client.GetWidth(), 3840);
  EXPECT_EQ(athena_client.GetHeight(), 2160);
}

TEST_F(CefClientTest, SetSizeMultipleTimes) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetSize(800, 600);
  EXPECT_EQ(athena_client.GetWidth(), 800);

  athena_client.SetSize(1200, 800);
  EXPECT_EQ(athena_client.GetWidth(), 1200);

  athena_client.SetSize(1920, 1080);
  EXPECT_EQ(athena_client.GetWidth(), 1920);
}

// ============================================================================
// Device Scale Factor Tests
// ============================================================================

TEST_F(CefClientTest, SetDeviceScaleFactorNormal) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetDeviceScaleFactor(1.0f);

  EXPECT_FLOAT_EQ(athena_client.GetDeviceScaleFactor(), 1.0f);
}

TEST_F(CefClientTest, SetDeviceScaleFactorRetina) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetDeviceScaleFactor(2.0f);

  EXPECT_FLOAT_EQ(athena_client.GetDeviceScaleFactor(), 2.0f);
}

TEST_F(CefClientTest, SetDeviceScaleFactorFractional) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetDeviceScaleFactor(1.5f);

  EXPECT_FLOAT_EQ(athena_client.GetDeviceScaleFactor(), 1.5f);
}

TEST_F(CefClientTest, SetDeviceScaleFactorHiDPI) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetDeviceScaleFactor(3.0f);

  EXPECT_FLOAT_EQ(athena_client.GetDeviceScaleFactor(), 3.0f);
}

// ============================================================================
// GetViewRect Tests
// ============================================================================

TEST_F(CefClientTest, GetViewRectWithValidSize) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetSize(1200, 800);

  CefRect rect;
  athena_client.GetViewRect(nullptr, rect);

  EXPECT_EQ(rect.x, 0);
  EXPECT_EQ(rect.y, 0);
  EXPECT_EQ(rect.width, 1200);
  EXPECT_EQ(rect.height, 800);
}

TEST_F(CefClientTest, GetViewRectWithZeroSize) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  CefRect rect;
  athena_client.GetViewRect(nullptr, rect);

  EXPECT_EQ(rect.x, 0);
  EXPECT_EQ(rect.y, 0);
  // Should return default when size is 0
  EXPECT_EQ(rect.width, 1200);
  EXPECT_EQ(rect.height, 800);
}

// ============================================================================
// GetScreenInfo Tests
// ============================================================================

TEST_F(CefClientTest, GetScreenInfoReturnsScaleFactor) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetDeviceScaleFactor(2.0f);

  CefScreenInfo screen_info;
  bool result = athena_client.GetScreenInfo(nullptr, screen_info);

  EXPECT_TRUE(result);
  EXPECT_FLOAT_EQ(screen_info.device_scale_factor, 2.0f);
}

// ============================================================================
// Handler Interface Tests
// ============================================================================

// Note: Handler interface tests are skipped because they require CEF initialization
// which is not available in the unit test environment. These are tested in integration tests.

// TEST_F(CefClientTest, GetLifeSpanHandlerReturnsThis) - Requires CEF init
// TEST_F(CefClientTest, GetDisplayHandlerReturnsThis) - Requires CEF init
// TEST_F(CefClientTest, GetRenderHandlerReturnsThis) - Requires CEF init

// ============================================================================
// Combined Scenarios
// ============================================================================

TEST_F(CefClientTest, SetSizeAndScaleFactorTogether) {
  athena::rendering::GLRenderer gl_renderer;
  athena::browser::CefClient athena_client(window_handle_, &gl_renderer);

  athena_client.SetSize(1920, 1080);
  athena_client.SetDeviceScaleFactor(2.0f);

  EXPECT_EQ(athena_client.GetWidth(), 1920);
  EXPECT_EQ(athena_client.GetHeight(), 1080);
  EXPECT_FLOAT_EQ(athena_client.GetDeviceScaleFactor(), 2.0f);
}

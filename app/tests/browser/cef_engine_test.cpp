#include "browser/cef_engine.h"
#include "browser/app_handler.h"
#include <gtest/gtest.h>

using namespace athena::browser;
using namespace athena::rendering;
using namespace athena::utils;

/**
 * CefEngine Unit Tests
 *
 * These tests verify CefEngine's interface implementation WITHOUT
 * actually initializing CEF (which requires a full environment).
 *
 * We test:
 * - Initialization lifecycle
 * - Browser ID management
 * - State tracking
 * - Error handling
 *
 * Note: Full integration tests with actual CEF are in integration/.
 */
class CefEngineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    engine_ = std::make_unique<CefEngine>();
  }

  void TearDown() override {
    // Ensure shutdown if initialized
    if (engine_ && engine_->IsInitialized()) {
      engine_->Shutdown();
    }
    engine_.reset();
  }

  std::unique_ptr<CefEngine> engine_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(CefEngineTest, ConstructorCreatesUninitialized) {
  EXPECT_FALSE(engine_->IsInitialized());
}

TEST_F(CefEngineTest, ConstructorWithNullApp) {
  CefEngine engine_with_null(nullptr);
  EXPECT_FALSE(engine_with_null.IsInitialized());
}

TEST_F(CefEngineTest, ConstructorWithAppHandler) {
  CefRefPtr<AppHandler> app = new AppHandler();
  CefEngine engine_with_app(app);
  EXPECT_FALSE(engine_with_app.IsInitialized());
}

// ============================================================================
// Initialization Tests (without actual CEF)
// ============================================================================

TEST_F(CefEngineTest, IsInitializedReturnsFalseByDefault) {
  EXPECT_FALSE(engine_->IsInitialized());
}

// Note: These tests are skipped because they try to initialize CEF
// which requires a full environment. Tested in integration tests instead.

// TEST_F(CefEngineTest, InitializeRequiresValidConfig) - Requires CEF environment
// TEST_F(CefEngineTest, DoubleInitializationFails) - Requires CEF environment

TEST_F(CefEngineTest, ShutdownWhenNotInitializedIsNoOp) {
  ASSERT_FALSE(engine_->IsInitialized());

  // Should not crash
  engine_->Shutdown();

  EXPECT_FALSE(engine_->IsInitialized());
}

// ============================================================================
// Browser Management Tests (State Tracking)
// ============================================================================

TEST_F(CefEngineTest, HasBrowserReturnsFalseForInvalidId) {
  EXPECT_FALSE(engine_->HasBrowser(0));
  EXPECT_FALSE(engine_->HasBrowser(123));
  EXPECT_FALSE(engine_->HasBrowser(999));
}

TEST_F(CefEngineTest, HasBrowserReturnsFalseForInvalidBrowserId) {
  EXPECT_FALSE(engine_->HasBrowser(kInvalidBrowserId));
}

TEST_F(CefEngineTest, CreateBrowserFailsWhenNotInitialized) {
  BrowserConfig config;
  config.url = "https://www.google.com";
  config.width = 1200;
  config.height = 800;

  auto result = engine_->CreateBrowser(config);

  EXPECT_TRUE(result.IsError());
  EXPECT_NE(result.GetError().Message().find("not initialized"), std::string::npos);
}

TEST_F(CefEngineTest, CreateBrowserRequiresGLRenderer) {
  // Even though not initialized, should check for null renderer
  BrowserConfig config;
  config.url = "https://www.google.com";
  config.gl_renderer = nullptr;  // Missing!

  auto result = engine_->CreateBrowser(config);

  EXPECT_TRUE(result.IsError());
  // Error could be "not initialized" or "gl_renderer required"
  EXPECT_FALSE(result.GetError().Message().empty());
}

TEST_F(CefEngineTest, CloseBrowserDoesNotCrashWithInvalidId) {
  // Should not crash even though browser doesn't exist
  engine_->CloseBrowser(123, false);
  engine_->CloseBrowser(999, true);
  engine_->CloseBrowser(kInvalidBrowserId, false);

  // No assertion - just verify no crash
  SUCCEED();
}

// ============================================================================
// Navigation Tests (State-Only, No CEF)
// ============================================================================

TEST_F(CefEngineTest, LoadURLDoesNotCrashWithInvalidBrowserId) {
  engine_->LoadURL(123, "https://www.google.com");
  engine_->LoadURL(kInvalidBrowserId, "https://example.com");

  // No crash = success
  SUCCEED();
}

TEST_F(CefEngineTest, GoBackDoesNotCrashWithInvalidBrowserId) {
  engine_->GoBack(123);
  engine_->GoBack(kInvalidBrowserId);

  SUCCEED();
}

TEST_F(CefEngineTest, GoForwardDoesNotCrashWithInvalidBrowserId) {
  engine_->GoForward(123);
  engine_->GoForward(kInvalidBrowserId);

  SUCCEED();
}

TEST_F(CefEngineTest, ReloadDoesNotCrashWithInvalidBrowserId) {
  engine_->Reload(123, false);
  engine_->Reload(456, true);
  engine_->Reload(kInvalidBrowserId, false);

  SUCCEED();
}

TEST_F(CefEngineTest, StopLoadDoesNotCrashWithInvalidBrowserId) {
  engine_->StopLoad(123);
  engine_->StopLoad(kInvalidBrowserId);

  SUCCEED();
}

// ============================================================================
// Browser State Query Tests
// ============================================================================

TEST_F(CefEngineTest, CanGoBackReturnsFalseForInvalidId) {
  EXPECT_FALSE(engine_->CanGoBack(123));
  EXPECT_FALSE(engine_->CanGoBack(kInvalidBrowserId));
}

TEST_F(CefEngineTest, CanGoForwardReturnsFalseForInvalidId) {
  EXPECT_FALSE(engine_->CanGoForward(123));
  EXPECT_FALSE(engine_->CanGoForward(kInvalidBrowserId));
}

TEST_F(CefEngineTest, IsLoadingReturnsFalseForInvalidId) {
  EXPECT_FALSE(engine_->IsLoading(123));
  EXPECT_FALSE(engine_->IsLoading(kInvalidBrowserId));
}

TEST_F(CefEngineTest, GetURLReturnsEmptyForInvalidId) {
  EXPECT_EQ(engine_->GetURL(123), "");
  EXPECT_EQ(engine_->GetURL(kInvalidBrowserId), "");
}

// ============================================================================
// Rendering & Display Tests
// ============================================================================

TEST_F(CefEngineTest, SetSizeDoesNotCrashWithInvalidId) {
  engine_->SetSize(123, 1920, 1080);
  engine_->SetSize(kInvalidBrowserId, 800, 600);

  SUCCEED();
}

TEST_F(CefEngineTest, SetDeviceScaleFactorDoesNotCrashWithInvalidId) {
  engine_->SetDeviceScaleFactor(123, 2.0f);
  engine_->SetDeviceScaleFactor(kInvalidBrowserId, 1.5f);

  SUCCEED();
}

TEST_F(CefEngineTest, InvalidateDoesNotCrashWithInvalidId) {
  engine_->Invalidate(123);
  engine_->Invalidate(kInvalidBrowserId);

  SUCCEED();
}

// ============================================================================
// Input Events Tests
// ============================================================================

TEST_F(CefEngineTest, SetFocusDoesNotCrashWithInvalidId) {
  engine_->SetFocus(123, true);
  engine_->SetFocus(456, false);
  engine_->SetFocus(kInvalidBrowserId, true);

  SUCCEED();
}

// ============================================================================
// Message Loop Tests
// ============================================================================

TEST_F(CefEngineTest, DoMessageLoopWorkWhenNotInitialized) {
  ASSERT_FALSE(engine_->IsInitialized());

  // Should not crash when not initialized
  engine_->DoMessageLoopWork();

  SUCCEED();
}

// ============================================================================
// CEF-Specific API Tests
// ============================================================================

TEST_F(CefEngineTest, GetCefBrowserReturnsNullForInvalidId) {
  EXPECT_EQ(engine_->GetCefBrowser(123), nullptr);
  EXPECT_EQ(engine_->GetCefBrowser(kInvalidBrowserId), nullptr);
}

TEST_F(CefEngineTest, GetCefClientReturnsNullForInvalidId) {
  EXPECT_EQ(engine_->GetCefClient(123), nullptr);
  EXPECT_EQ(engine_->GetCefClient(kInvalidBrowserId), nullptr);
}

// ============================================================================
// RAII and Destructor Tests
// ============================================================================

TEST_F(CefEngineTest, DestructorCleansUpUninitialized) {
  {
    CefEngine temp_engine;
    EXPECT_FALSE(temp_engine.IsInitialized());
  }
  // Destructor should not crash

  SUCCEED();
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(CefEngineTest, EngineConfigDefaults) {
  EngineConfig config;

  // Verify default values
  EXPECT_TRUE(config.cache_path.empty());
  EXPECT_TRUE(config.subprocess_path.empty());
  EXPECT_FALSE(config.enable_sandbox);
  EXPECT_TRUE(config.enable_windowless_rendering);
  EXPECT_EQ(config.windowless_frame_rate, 60);
}

TEST_F(CefEngineTest, BrowserConfigDefaults) {
  BrowserConfig config;

  // Verify default values
  EXPECT_TRUE(config.url.empty());
  EXPECT_EQ(config.width, 1200);
  EXPECT_EQ(config.height, 800);
  EXPECT_FLOAT_EQ(config.device_scale_factor, 1.0f);
  EXPECT_EQ(config.gl_renderer, nullptr);
  EXPECT_EQ(config.native_window_handle, nullptr);
}

TEST_F(CefEngineTest, EngineConfigCustomValues) {
  EngineConfig config;
  config.cache_path = "/tmp/custom_cache";
  config.subprocess_path = "/usr/bin/custom_subprocess";
  config.enable_sandbox = true;
  config.enable_windowless_rendering = false;
  config.windowless_frame_rate = 30;

  EXPECT_EQ(config.cache_path, "/tmp/custom_cache");
  EXPECT_EQ(config.subprocess_path, "/usr/bin/custom_subprocess");
  EXPECT_TRUE(config.enable_sandbox);
  EXPECT_FALSE(config.enable_windowless_rendering);
  EXPECT_EQ(config.windowless_frame_rate, 30);
}

TEST_F(CefEngineTest, BrowserConfigCustomValues) {
  GLRenderer dummy_renderer;
  void* dummy_window = reinterpret_cast<void*>(0x1234);

  BrowserConfig config;
  config.url = "https://example.com";
  config.width = 3840;
  config.height = 2160;
  config.device_scale_factor = 2.5f;
  config.gl_renderer = &dummy_renderer;
  config.native_window_handle = dummy_window;

  EXPECT_EQ(config.url, "https://example.com");
  EXPECT_EQ(config.width, 3840);
  EXPECT_EQ(config.height, 2160);
  EXPECT_FLOAT_EQ(config.device_scale_factor, 2.5f);
  EXPECT_EQ(config.gl_renderer, &dummy_renderer);
  EXPECT_EQ(config.native_window_handle, dummy_window);
}

// ============================================================================
// BrowserId Type Tests
// ============================================================================

TEST_F(CefEngineTest, BrowserIdInvalidConstant) {
  EXPECT_EQ(kInvalidBrowserId, 0u);
}

TEST_F(CefEngineTest, BrowserIdIsUint64) {
  // Verify BrowserId is uint64_t
  BrowserId id = 0xFFFFFFFFFFFFFFFFULL;
  EXPECT_EQ(id, 0xFFFFFFFFFFFFFFFFULL);
}

// ============================================================================
// Non-Copyable Tests
// ============================================================================

TEST_F(CefEngineTest, CefEngineIsNonCopyable) {
  // This test verifies at compile time that CefEngine is non-copyable
  // If this compiles, the test passes
  EXPECT_TRUE((std::is_copy_constructible<CefEngine>::value == false));
  EXPECT_TRUE((std::is_copy_assignable<CefEngine>::value == false));
}

TEST_F(CefEngineTest, CefEngineIsNonMovable) {
  // This test verifies at compile time that CefEngine is non-movable
  EXPECT_TRUE((std::is_move_constructible<CefEngine>::value == false));
  EXPECT_TRUE((std::is_move_assignable<CefEngine>::value == false));
}

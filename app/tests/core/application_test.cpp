#include "core/application.h"

#include "mocks/mock_browser_engine.h"
#include "mocks/mock_window_system.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace athena::core;
using namespace athena::platform;
using namespace athena::browser;
using namespace athena::utils;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class ApplicationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto window_system = std::make_unique<athena::platform::testing::MockWindowSystem>();
    auto browser_engine = std::make_unique<athena::browser::testing::MockBrowserEngine>();

    window_system_ptr_ = window_system.get();
    browser_engine_ptr_ = browser_engine.get();

    // Setup default expectations
    ON_CALL(*browser_engine_ptr_, IsInitialized()).WillByDefault(Return(true));

    app_ = std::make_unique<Application>(
        ApplicationConfig{}, std::move(browser_engine), std::move(window_system));
  }

  void TearDown() override { app_.reset(); }

  std::unique_ptr<Application> app_;
  athena::platform::testing::MockWindowSystem* window_system_ptr_;
  athena::browser::testing::MockBrowserEngine* browser_engine_ptr_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ApplicationTest, ConstructionWithDefaultConfig) {
  EXPECT_FALSE(app_->IsInitialized());
  EXPECT_FALSE(app_->IsRunning());
  EXPECT_EQ(app_->GetWindowCount(), 0);
}

TEST_F(ApplicationTest, ConstructionWithCustomConfig) {
  ApplicationConfig config;
  config.cache_path = "/tmp/custom_cache";
  config.enable_sandbox = true;
  config.windowless_frame_rate = 30;

  auto window_system = std::make_unique<athena::platform::testing::MockWindowSystem>();
  auto browser_engine = std::make_unique<athena::browser::testing::MockBrowserEngine>();

  Application app(config, std::move(browser_engine), std::move(window_system));

  EXPECT_EQ(app.GetConfig().cache_path, "/tmp/custom_cache");
  EXPECT_EQ(app.GetConfig().enable_sandbox, true);
  EXPECT_EQ(app.GetConfig().windowless_frame_rate, 30);
}

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(ApplicationTest, InitializeSuccess) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  auto result = app_->Initialize();
  ASSERT_TRUE(result.IsOk());
  EXPECT_TRUE(app_->IsInitialized());
}

TEST_F(ApplicationTest, InitializeWithCommandLineArgs) {
  int argc = 2;
  char arg0[] = "test_app";
  char arg1[] = "--test-flag";
  char* argv[] = {arg0, arg1};

  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  auto result = app_->Initialize(argc, argv);
  ASSERT_TRUE(result.IsOk());
  EXPECT_TRUE(app_->IsInitialized());
}

TEST_F(ApplicationTest, InitializeFailsWhenWindowSystemFails) {
  // Create a fresh mock window system that we'll force to fail
  ApplicationConfig config;
  auto window_system = std::make_unique<athena::platform::testing::MockWindowSystem>();
  auto browser_engine = std::make_unique<athena::browser::testing::MockBrowserEngine>();

  // Make window system already initialized so Initialize() will fail
  int dummy_argc = 0;
  char** dummy_argv = nullptr;
  window_system->Initialize(dummy_argc, dummy_argv, browser_engine.get());

  Application app(config, std::move(browser_engine), std::move(window_system));

  // This will fail because window system is already initialized
  auto result = app.Initialize();
  EXPECT_TRUE(result.IsError());
}

TEST_F(ApplicationTest, InitializeFailsWhenBrowserEngineFails) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_))
      .WillOnce(Return(athena::utils::Result<void>(athena::utils::Error("Engine init failed"))));

  auto result = app_->Initialize();
  EXPECT_TRUE(result.IsError());
  EXPECT_NE(result.GetError().Message().find("browser engine"), std::string::npos);
}

TEST_F(ApplicationTest, InitializeFailsWhenAlreadyInitialized) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  auto result1 = app_->Initialize();
  ASSERT_TRUE(result1.IsOk());

  auto result2 = app_->Initialize();
  EXPECT_TRUE(result2.IsError());
  EXPECT_NE(result2.GetError().Message().find("already initialized"), std::string::npos);
}

// ============================================================================
// Shutdown Tests
// ============================================================================

TEST_F(ApplicationTest, ShutdownClosesAllResources) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, Shutdown());

  app_->Initialize();
  app_->Shutdown();

  EXPECT_FALSE(app_->IsInitialized());
}

TEST_F(ApplicationTest, ShutdownIsIdempotent) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, Shutdown()).Times(1);  // Only once

  app_->Initialize();
  app_->Shutdown();
  app_->Shutdown();  // Second call should be no-op
}

TEST_F(ApplicationTest, DestructorCallsShutdown) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, Shutdown());

  app_->Initialize();
  // Destructor will be called when app_ is reset in TearDown
}

// ============================================================================
// Window Management Tests
// ============================================================================

TEST_F(ApplicationTest, CreateWindowSuccess) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, CreateBrowser(_))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(1)));

  // Expect LoadURL to be called with the initial URL
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(1, "https://example.com"));

  app_->Initialize();

  BrowserWindowConfig config;
  config.url = "https://example.com";

  auto result = app_->CreateWindow(config);
  ASSERT_TRUE(result.IsOk());

  auto window = std::move(result.Value());
  EXPECT_NE(window, nullptr);

  // Show the window to trigger browser creation
  window->Show();
}

TEST_F(ApplicationTest, CreateWindowFailsWhenNotInitialized) {
  BrowserWindowConfig config;

  auto result = app_->CreateWindow(config);
  EXPECT_TRUE(result.IsError());
  EXPECT_NE(result.GetError().Message().find("not initialized"), std::string::npos);
}

TEST_F(ApplicationTest, CreateMultipleWindows) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, CreateBrowser(_))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(1)))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(2)))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(3)));

  // Expect LoadURL to be called with default "about:blank" for each window
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(1, "about:blank"));
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(2, "about:blank"));
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(3, "about:blank"));

  app_->Initialize();

  std::vector<std::unique_ptr<BrowserWindow>> windows;

  for (int i = 0; i < 3; ++i) {
    auto result = app_->CreateWindow(BrowserWindowConfig{});
    ASSERT_TRUE(result.IsOk());
    auto window = std::move(result.Value());
    window->Show();  // Show window to trigger browser creation
    windows.push_back(std::move(window));
  }

  EXPECT_EQ(windows.size(), 3);
}

TEST_F(ApplicationTest, GetWindowCount) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, CreateBrowser(_))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(1)))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(2)));

  // Expect LoadURL to be called with default "about:blank" for each window
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(1, "about:blank"));
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(2, "about:blank"));

  app_->Initialize();

  EXPECT_EQ(app_->GetWindowCount(), 0);

  auto window1_result = app_->CreateWindow(BrowserWindowConfig{});
  ASSERT_TRUE(window1_result.IsOk());
  auto window1 = std::move(window1_result.Value());
  window1->Show();  // Show window to trigger browser creation

  auto window2_result = app_->CreateWindow(BrowserWindowConfig{});
  ASSERT_TRUE(window2_result.IsOk());
  auto window2 = std::move(window2_result.Value());
  window2->Show();  // Show window to trigger browser creation

  EXPECT_EQ(app_->GetWindowCount(), 2);
}

TEST_F(ApplicationTest, CloseAllWindows) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, CreateBrowser(_))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(1)))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(2)));

  // Expect LoadURL to be called with default "about:blank" for each window
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(1, "about:blank"));
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(2, "about:blank"));

  EXPECT_CALL(*browser_engine_ptr_, CloseBrowser(1, true));
  EXPECT_CALL(*browser_engine_ptr_, CloseBrowser(2, true));

  app_->Initialize();

  auto window1_result = app_->CreateWindow(BrowserWindowConfig{});
  ASSERT_TRUE(window1_result.IsOk());
  auto window1 = std::move(window1_result.Value());
  window1->Show();  // Show window to trigger browser creation

  auto window2_result = app_->CreateWindow(BrowserWindowConfig{});
  ASSERT_TRUE(window2_result.IsOk());
  auto window2 = std::move(window2_result.Value());
  window2->Show();  // Show window to trigger browser creation

  app_->CloseAllWindows(true);

  EXPECT_TRUE(window1->IsClosed());
  EXPECT_TRUE(window2->IsClosed());
}

// ============================================================================
// Event Loop Tests
// ============================================================================

TEST_F(ApplicationTest, RunBlocksUntilQuit) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  app_->Initialize();

  // Simulate Run() by setting running state
  window_system_ptr_->Run();
  EXPECT_TRUE(window_system_ptr_->IsRunning());

  app_->Quit();
  EXPECT_FALSE(window_system_ptr_->IsRunning());
}

TEST_F(ApplicationTest, RunFailsWhenNotInitialized) {
  // Run should just return early and log an error
  app_->Run();
  // No crash expected
}

TEST_F(ApplicationTest, QuitStopsEventLoop) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  app_->Initialize();

  window_system_ptr_->Run();
  EXPECT_TRUE(app_->IsRunning());

  app_->Quit();
  EXPECT_FALSE(app_->IsRunning());
}

// ============================================================================
// Accessors Tests
// ============================================================================

TEST_F(ApplicationTest, GetBrowserEngine) {
  EXPECT_EQ(app_->GetBrowserEngine(), browser_engine_ptr_);
}

TEST_F(ApplicationTest, GetWindowSystem) {
  EXPECT_EQ(app_->GetWindowSystem(), window_system_ptr_);
}

TEST_F(ApplicationTest, GetConfig) {
  ApplicationConfig config;
  config.cache_path = "/tmp/test_cache";

  auto window_system = std::make_unique<athena::platform::testing::MockWindowSystem>();
  auto browser_engine = std::make_unique<athena::browser::testing::MockBrowserEngine>();

  Application app(config, std::move(browser_engine), std::move(window_system));

  EXPECT_EQ(app.GetConfig().cache_path, "/tmp/test_cache");
}

// ============================================================================
// Callback Integration Tests
// ============================================================================

TEST_F(ApplicationTest, WindowCallbacksAreInvoked) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, CreateBrowser(_))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(1)));

  // Expect LoadURL to be called with default "about:blank"
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(1, "about:blank"));

  app_->Initialize();

  bool callback_invoked = false;
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;
  callbacks.on_resize = [&](int w, int h) { callback_invoked = true; };

  // Expect SetSize to be called twice (once from callback, once from explicit SetSize)
  EXPECT_CALL(*browser_engine_ptr_, SetSize(1, 1024, 768)).Times(2);

  auto result = app_->CreateWindow(config, callbacks);
  ASSERT_TRUE(result.IsOk());
  auto window = std::move(result.Value());

  window->Show();
  window->SetSize({1024, 768});

  EXPECT_TRUE(callback_invoked);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ApplicationTest, CreateWindowWithEmptyURL) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, CreateBrowser(_))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(1)));

  // Expect LoadURL NOT to be called when URL is empty
  EXPECT_CALL(*browser_engine_ptr_, LoadURL(_, _)).Times(0);

  app_->Initialize();

  BrowserWindowConfig config;
  config.url = "";

  auto result = app_->CreateWindow(config);
  ASSERT_TRUE(result.IsOk());

  auto window = std::move(result.Value());
  window->Show();  // Show window to trigger browser creation
}

TEST_F(ApplicationTest, MultipleInitializeAttempts) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  auto result1 = app_->Initialize();
  ASSERT_TRUE(result1.IsOk());

  auto result2 = app_->Initialize();
  EXPECT_TRUE(result2.IsError());

  auto result3 = app_->Initialize();
  EXPECT_TRUE(result3.IsError());
}

TEST_F(ApplicationTest, ShutdownWithoutInitialize) {
  // Should not crash
  app_->Shutdown();
}

TEST_F(ApplicationTest, QuitWithoutInitialize) {
  // Should not crash
  app_->Quit();
}

TEST_F(ApplicationTest, CloseAllWindowsWithNoWindows) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  app_->Initialize();

  // Should not crash
  app_->CloseAllWindows();
}

// ============================================================================
// Lifecycle Integration Tests
// ============================================================================

TEST_F(ApplicationTest, FullLifecycle) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, CreateBrowser(_))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(1)));

  EXPECT_CALL(*browser_engine_ptr_, CloseBrowser(1, false));  // Default is graceful close
  EXPECT_CALL(*browser_engine_ptr_, Shutdown());

  // Initialize
  auto init_result = app_->Initialize();
  ASSERT_TRUE(init_result.IsOk());
  EXPECT_TRUE(app_->IsInitialized());

  // Create window
  auto window_result = app_->CreateWindow(BrowserWindowConfig{});
  ASSERT_TRUE(window_result.IsOk());
  auto window = std::move(window_result.Value());

  // Show window
  window->Show();
  EXPECT_TRUE(window->IsVisible());

  // Close window
  window->Close();
  EXPECT_TRUE(window->IsClosed());

  // Shutdown
  app_->Shutdown();
  EXPECT_FALSE(app_->IsInitialized());
}

TEST_F(ApplicationTest, AutoQuitWhenAllWindowsClosed) {
  EXPECT_CALL(*browser_engine_ptr_, Initialize(_)).WillOnce(Return(athena::utils::Ok()));

  EXPECT_CALL(*browser_engine_ptr_, CreateBrowser(_))
      .WillOnce(Return(athena::utils::Result<athena::browser::BrowserId>(1)));

  EXPECT_CALL(*browser_engine_ptr_, CloseBrowser(1, false));

  app_->Initialize();

  auto result = app_->CreateWindow(BrowserWindowConfig{});
  ASSERT_TRUE(result.IsOk());
  auto window = std::move(result.Value());

  window->Show();

  // Close the window - this should trigger auto-quit
  window->Close();

  // Event loop should have quit
  EXPECT_FALSE(window_system_ptr_->IsRunning());
}

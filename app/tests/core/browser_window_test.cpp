#include "core/browser_window.h"
#include "mocks/mock_window_system.h"
#include "mocks/mock_browser_engine.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace athena::core;
using namespace athena::platform;
using namespace athena::browser;
using namespace athena::utils;
using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;

class BrowserWindowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    window_system_ = std::make_unique<athena::platform::testing::MockWindowSystem>();
    browser_engine_ = std::make_unique<athena::browser::testing::MockBrowserEngine>();

    // Setup default expectations for initialization
    int dummy_argc = 0;
    char** dummy_argv = nullptr;
    window_system_->Initialize(dummy_argc, dummy_argv, browser_engine_.get());

    ON_CALL(*browser_engine_, IsInitialized())
        .WillByDefault(Return(true));
  }

  void TearDown() override {
    window_system_->Shutdown();
  }

  std::unique_ptr<athena::platform::testing::MockWindowSystem> window_system_;
  std::unique_ptr<athena::browser::testing::MockBrowserEngine> browser_engine_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(BrowserWindowTest, ConstructionWithDefaultConfig) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());

  EXPECT_FALSE(window.IsVisible());
  EXPECT_TRUE(window.IsClosed());
}

TEST_F(BrowserWindowTest, ConstructionWithCustomConfig) {
  BrowserWindowConfig config;
  config.title = "Test Window";
  config.size = {800, 600};
  config.url = "https://example.com";
  config.resizable = false;

  BrowserWindowCallbacks callbacks;

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());

  EXPECT_FALSE(window.IsVisible());
}

// ============================================================================
// Show Tests
// ============================================================================

TEST_F(BrowserWindowTest, ShowInitializesWindow) {
  BrowserWindowConfig config;
  config.url = "https://example.com";
  BrowserWindowCallbacks callbacks;

  // Expect browser creation
  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());

  auto result = window.Show();
  ASSERT_TRUE(result.IsOk());
  EXPECT_TRUE(window.IsVisible());
  EXPECT_FALSE(window.IsClosed());
}

TEST_F(BrowserWindowTest, ShowLoadsInitialURL) {
  BrowserWindowConfig config;
  config.url = "https://example.com";
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  // Expect LoadURL to be called with initial URL
  EXPECT_CALL(*browser_engine_, LoadURL(1, "https://example.com"));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();
}

TEST_F(BrowserWindowTest, ShowWithEmptyURLDoesNotLoad) {
  BrowserWindowConfig config;
  config.url = "";
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  // LoadURL should NOT be called
  EXPECT_CALL(*browser_engine_, LoadURL(_, _)).Times(0);

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();
}

TEST_F(BrowserWindowTest, ShowFailsWhenWindowSystemNotInitialized) {
  window_system_->Shutdown();

  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());

  auto result = window.Show();
  EXPECT_TRUE(result.IsError());
  EXPECT_NE(result.GetError().Message().find("not initialized"), std::string::npos);
}

TEST_F(BrowserWindowTest, ShowFailsWhenBrowserEngineNotInitialized) {
  ON_CALL(*browser_engine_, IsInitialized())
      .WillByDefault(Return(false));

  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());

  auto result = window.Show();
  EXPECT_TRUE(result.IsError());
  EXPECT_NE(result.GetError().Message().find("not initialized"), std::string::npos);
}

TEST_F(BrowserWindowTest, ShowSucceedsEvenWhenBrowserCreationFails) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  // Browser creation fails (but Show() still succeeds)
  // With the tab architecture, browser creation happens asynchronously
  // and failures are logged but don't prevent the window from showing
  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(Error("Browser creation failed"))));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());

  auto result = window.Show();
  // Show() should succeed - the window can be shown even if browser creation fails
  EXPECT_TRUE(result.IsOk());
  EXPECT_TRUE(window.IsVisible());
  // But no browser will be created
  EXPECT_EQ(window.GetBrowserId(), athena::browser::kInvalidBrowserId);
}

// ============================================================================
// Hide Tests
// ============================================================================

TEST_F(BrowserWindowTest, HideHidesWindow) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  ASSERT_TRUE(window.IsVisible());

  window.Hide();
  EXPECT_FALSE(window.IsVisible());
}

// ============================================================================
// Close Tests
// ============================================================================

TEST_F(BrowserWindowTest, CloseClosesWindowAndBrowser) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, CloseBrowser(1, false));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.Close(false);
  EXPECT_TRUE(window.IsClosed());
}

TEST_F(BrowserWindowTest, CloseWithForceFlag) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, CloseBrowser(1, true));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.Close(true);
  EXPECT_TRUE(window.IsClosed());
}

// ============================================================================
// Window Property Tests
// ============================================================================

TEST_F(BrowserWindowTest, GetSetTitle) {
  BrowserWindowConfig config;
  config.title = "Initial Title";
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_EQ(window.GetTitle(), "Initial Title");

  window.SetTitle("New Title");
  EXPECT_EQ(window.GetTitle(), "New Title");
}

TEST_F(BrowserWindowTest, GetSetSize) {
  BrowserWindowConfig config;
  config.size = {800, 600};
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  // Expect LoadURL to be called with default "about:blank"
  EXPECT_CALL(*browser_engine_, LoadURL(1, "about:blank"));

  // Expect browser to be notified of size change (called twice - once from callback, once from explicit SetSize)
  EXPECT_CALL(*browser_engine_, SetSize(1, 1024, 768))
      .Times(2);

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_EQ(window.GetSize().width, 800);
  EXPECT_EQ(window.GetSize().height, 600);

  window.SetSize({1024, 768});
  EXPECT_EQ(window.GetSize().width, 1024);
  EXPECT_EQ(window.GetSize().height, 768);
}

TEST_F(BrowserWindowTest, GetScaleFactor) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_EQ(window.GetScaleFactor(), 1.0f);
}

TEST_F(BrowserWindowTest, FocusRequestsFocus) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_FALSE(window.HasFocus());

  window.Focus();
  EXPECT_TRUE(window.HasFocus());
}

// ============================================================================
// Navigation Tests
// ============================================================================

TEST_F(BrowserWindowTest, LoadURL) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  // Expect LoadURL to be called with default "about:blank" first, then with "https://example.com"
  EXPECT_CALL(*browser_engine_, LoadURL(1, "about:blank"))
      .Times(1);
  EXPECT_CALL(*browser_engine_, LoadURL(1, "https://example.com"))
      .Times(1);

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.LoadURL("https://example.com");
}

TEST_F(BrowserWindowTest, GoBack) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, GoBack(1));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.GoBack();
}

TEST_F(BrowserWindowTest, GoForward) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, GoForward(1));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.GoForward();
}

TEST_F(BrowserWindowTest, Reload) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, Reload(1, false));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.Reload(false);
}

TEST_F(BrowserWindowTest, ReloadIgnoreCache) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, Reload(1, true));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.Reload(true);
}

TEST_F(BrowserWindowTest, StopLoad) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, StopLoad(1));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.StopLoad();
}

// ============================================================================
// Browser State Tests
// ============================================================================

TEST_F(BrowserWindowTest, CanGoBack) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, CanGoBack(1))
      .WillOnce(Return(true));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_TRUE(window.CanGoBack());
}

TEST_F(BrowserWindowTest, CanGoForward) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, CanGoForward(1))
      .WillOnce(Return(false));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_FALSE(window.CanGoForward());
}

TEST_F(BrowserWindowTest, IsLoading) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, IsLoading(1))
      .WillOnce(Return(true));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_TRUE(window.IsLoading());
}

TEST_F(BrowserWindowTest, GetURL) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, GetURL(1))
      .WillOnce(Return("https://example.com"));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_EQ(window.GetURL(), "https://example.com");
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(BrowserWindowTest, ResizeCallback) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  int callback_width = 0;
  int callback_height = 0;
  callbacks.on_resize = [&](int w, int h) {
    callback_width = w;
    callback_height = h;
  };

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  // Expect LoadURL to be called with default "about:blank"
  EXPECT_CALL(*browser_engine_, LoadURL(1, "about:blank"));

  // Expect SetSize to be called twice (once from callback, once from explicit SetSize)
  EXPECT_CALL(*browser_engine_, SetSize(1, 1024, 768))
      .Times(2);

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.SetSize({1024, 768});

  EXPECT_EQ(callback_width, 1024);
  EXPECT_EQ(callback_height, 768);
}

TEST_F(BrowserWindowTest, CloseCallback) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  bool close_called = false;
  callbacks.on_close = [&]() {
    close_called = true;
  };

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, CloseBrowser(1, false));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.Close(false);

  EXPECT_TRUE(close_called);
}

TEST_F(BrowserWindowTest, FocusChangeCallback) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  bool focus_state = false;
  callbacks.on_focus_changed = [&](bool focused) {
    focus_state = focused;
  };

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, SetFocus(1, true));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  window.Focus();

  EXPECT_TRUE(focus_state);
}

// ============================================================================
// Browser & Window ID Tests
// ============================================================================

TEST_F(BrowserWindowTest, GetBrowserId) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(42)));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_EQ(window.GetBrowserId(), 42);
}

TEST_F(BrowserWindowTest, GetWindow) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
  window.Show();

  EXPECT_NE(window.GetWindow(), nullptr);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(BrowserWindowTest, OperationsBeforeShowAreNoOps) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());

  // These should not crash
  window.LoadURL("https://example.com");
  window.GoBack();
  window.GoForward();
  window.Reload();
  window.StopLoad();

  EXPECT_FALSE(window.CanGoBack());
  EXPECT_FALSE(window.CanGoForward());
  EXPECT_FALSE(window.IsLoading());
  EXPECT_EQ(window.GetURL(), "");
}

TEST_F(BrowserWindowTest, DestructorClosesBrowser) {
  BrowserWindowConfig config;
  BrowserWindowCallbacks callbacks;

  EXPECT_CALL(*browser_engine_, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  EXPECT_CALL(*browser_engine_, CloseBrowser(1, true));

  {
    BrowserWindow window(config, callbacks, window_system_.get(), browser_engine_.get());
    window.Show();
  }
  // Destructor should have closed browser
}

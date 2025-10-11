#ifndef ATHENA_TESTS_MOCKS_MOCK_BROWSER_ENGINE_H_
#define ATHENA_TESTS_MOCKS_MOCK_BROWSER_ENGINE_H_

#include "browser/browser_engine.h"
#include <gmock/gmock.h>

namespace athena {
namespace browser {
namespace testing {

/**
 * Mock implementation of BrowserEngine for testing.
 *
 * This mock allows tests to verify interactions with the browser engine
 * without requiring actual CEF initialization or browser creation.
 *
 * Example usage:
 *   MockBrowserEngine engine;
 *   EXPECT_CALL(engine, Initialize(_)).WillOnce(Return(utils::Ok()));
 *   EXPECT_CALL(engine, CreateBrowser(_)).WillOnce(Return(utils::Result<BrowserId>(1)));
 */
class MockBrowserEngine : public BrowserEngine {
 public:
  MockBrowserEngine() = default;
  ~MockBrowserEngine() override = default;

  // Lifecycle Management
  MOCK_METHOD(utils::Result<void>, Initialize, (const EngineConfig& config), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(bool, IsInitialized, (), (const, override));

  // Browser Management
  MOCK_METHOD(utils::Result<BrowserId>, CreateBrowser, (const BrowserConfig& config), (override));
  MOCK_METHOD(void, CloseBrowser, (BrowserId id, bool force_close), (override));
  MOCK_METHOD(bool, HasBrowser, (BrowserId id), (const, override));

  // Navigation
  MOCK_METHOD(void, LoadURL, (BrowserId id, const std::string& url), (override));
  MOCK_METHOD(void, GoBack, (BrowserId id), (override));
  MOCK_METHOD(void, GoForward, (BrowserId id), (override));
  MOCK_METHOD(void, Reload, (BrowserId id, bool ignore_cache), (override));
  MOCK_METHOD(void, StopLoad, (BrowserId id), (override));

  // Browser State
  MOCK_METHOD(bool, CanGoBack, (BrowserId id), (const, override));
  MOCK_METHOD(bool, CanGoForward, (BrowserId id), (const, override));
  MOCK_METHOD(bool, IsLoading, (BrowserId id), (const, override));
  MOCK_METHOD(std::string, GetURL, (BrowserId id), (const, override));

  // Rendering & Display
  MOCK_METHOD(void, SetSize, (BrowserId id, int width, int height), (override));
  MOCK_METHOD(void, SetDeviceScaleFactor, (BrowserId id, float scale_factor), (override));
  MOCK_METHOD(void, Invalidate, (BrowserId id), (override));

  // Input Events
  MOCK_METHOD(void, SetFocus, (BrowserId id, bool focus), (override));

  // Message Loop Integration
  MOCK_METHOD(void, DoMessageLoopWork, (), (override));
};

}  // namespace testing
}  // namespace browser
}  // namespace athena

#endif  // ATHENA_TESTS_MOCKS_MOCK_BROWSER_ENGINE_H_

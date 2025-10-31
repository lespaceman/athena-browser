/**
 * Unit tests for renderer crash recovery in CefClient.
 *
 * Tests the OnRenderProcessTerminated callback and crash handling logic.
 */

#include "browser/cef_client.h"
#include "include/cef_client.h"
#include "include/cef_request_handler.h"
#include "rendering/gl_renderer.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

/**
 * Mock GLRenderer for testing (doesn't require OpenGL context)
 */
class MockGLRenderer : public athena::rendering::GLRenderer {
 public:
  MockGLRenderer() = default;
  ~MockGLRenderer() = default;
};

/**
 * Test fixture for CefClient crash recovery tests.
 */
class CefClientCrashTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

/**
 * Test that OnRenderProcessTerminated is callable without crashing.
 */
TEST_F(CefClientCrashTest, OnRenderProcessTerminatedCallable) {
  MockGLRenderer gl_renderer;
  athena::browser::CefClient client(nullptr, &gl_renderer);

  // Should not crash even without callback registered
  EXPECT_NO_THROW(client.OnRenderProcessTerminated(nullptr, TS_ABNORMAL_TERMINATION, 0, ""));
  EXPECT_NO_THROW(client.OnRenderProcessTerminated(nullptr, TS_PROCESS_WAS_KILLED, 0, ""));
  EXPECT_NO_THROW(client.OnRenderProcessTerminated(nullptr, TS_PROCESS_CRASHED, 0, ""));
  EXPECT_NO_THROW(client.OnRenderProcessTerminated(nullptr, TS_PROCESS_OOM, 0, ""));
}

/**
 * Test that crash callback is invoked for abnormal termination.
 */
TEST_F(CefClientCrashTest, CallbackInvokedForAbnormalTermination) {
  MockGLRenderer gl_renderer;
  athena::browser::CefClient client(nullptr, &gl_renderer);

  std::string received_reason;
  bool received_should_reload = true;  // Initialize to opposite of expected
  bool callback_invoked = false;

  // Register callback
  client.SetRendererCrashedCallback([&](const std::string& reason, bool should_reload) {
    received_reason = reason;
    received_should_reload = should_reload;
    callback_invoked = true;
  });

  // Trigger crash
  client.OnRenderProcessTerminated(nullptr, TS_ABNORMAL_TERMINATION, 0, "");

  // Verify callback was invoked
  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(received_reason, "abnormal termination");
  EXPECT_FALSE(received_should_reload);  // Abnormal = unsafe to reload
}

/**
 * Test that crash callback is invoked for process killed.
 */
TEST_F(CefClientCrashTest, CallbackInvokedForProcessKilled) {
  MockGLRenderer gl_renderer;
  athena::browser::CefClient client(nullptr, &gl_renderer);

  std::string received_reason;
  bool received_should_reload = false;  // Initialize to opposite of expected
  bool callback_invoked = false;

  client.SetRendererCrashedCallback([&](const std::string& reason, bool should_reload) {
    received_reason = reason;
    received_should_reload = should_reload;
    callback_invoked = true;
  });

  client.OnRenderProcessTerminated(nullptr, TS_PROCESS_WAS_KILLED, 0, "");

  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(received_reason, "process was killed");
  EXPECT_TRUE(received_should_reload);  // Killed = safe to reload
}

/**
 * Test that crash callback is invoked for process crashed.
 */
TEST_F(CefClientCrashTest, CallbackInvokedForProcessCrashed) {
  MockGLRenderer gl_renderer;
  athena::browser::CefClient client(nullptr, &gl_renderer);

  std::string received_reason;
  bool received_should_reload = false;
  bool callback_invoked = false;

  client.SetRendererCrashedCallback([&](const std::string& reason, bool should_reload) {
    received_reason = reason;
    received_should_reload = should_reload;
    callback_invoked = true;
  });

  client.OnRenderProcessTerminated(nullptr, TS_PROCESS_CRASHED, 0, "");

  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(received_reason, "process crashed");
  EXPECT_TRUE(received_should_reload);  // Standard crash = safe to reload
}

/**
 * Test that crash callback is invoked for out-of-memory.
 */
TEST_F(CefClientCrashTest, CallbackInvokedForOutOfMemory) {
  MockGLRenderer gl_renderer;
  athena::browser::CefClient client(nullptr, &gl_renderer);

  std::string received_reason;
  bool received_should_reload = true;
  bool callback_invoked = false;

  client.SetRendererCrashedCallback([&](const std::string& reason, bool should_reload) {
    received_reason = reason;
    received_should_reload = should_reload;
    callback_invoked = true;
  });

  client.OnRenderProcessTerminated(nullptr, TS_PROCESS_OOM, 0, "");

  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(received_reason, "out of memory");
  EXPECT_FALSE(received_should_reload);  // OOM = unsafe to reload
}

/**
 * Test that callback is not invoked if not registered.
 */
TEST_F(CefClientCrashTest, NoCallbackIfNotRegistered) {
  MockGLRenderer gl_renderer;
  athena::browser::CefClient client(nullptr, &gl_renderer);

  // Don't register callback
  // Should not crash
  EXPECT_NO_THROW(client.OnRenderProcessTerminated(nullptr, TS_PROCESS_CRASHED, 0, ""));
}

/**
 * Test all termination status codes are handled.
 */
TEST_F(CefClientCrashTest, AllStatusCodesHandled) {
  struct TestCase {
    CefRequestHandler::TerminationStatus status_code;
    std::string expected_reason;
    bool expected_should_reload;
  };

  std::vector<TestCase> test_cases = {
      {TS_ABNORMAL_TERMINATION, "abnormal termination", false},
      {TS_PROCESS_WAS_KILLED, "process was killed", true},
      {TS_PROCESS_CRASHED, "process crashed", true},
      {TS_PROCESS_OOM, "out of memory", false},
  };

  for (const auto& test_case : test_cases) {
    MockGLRenderer gl_renderer;
    athena::browser::CefClient client(nullptr, &gl_renderer);

    std::string received_reason;
    bool received_should_reload = false;
    bool callback_invoked = false;

    client.SetRendererCrashedCallback([&](const std::string& reason, bool should_reload) {
      received_reason = reason;
      received_should_reload = should_reload;
      callback_invoked = true;
    });

    client.OnRenderProcessTerminated(nullptr, test_case.status_code, 0, "");

    EXPECT_TRUE(callback_invoked) << "Status: " << test_case.status_code;
    EXPECT_EQ(received_reason, test_case.expected_reason) << "Status: " << test_case.status_code;
    EXPECT_EQ(received_should_reload, test_case.expected_should_reload)
        << "Status: " << test_case.status_code;
  }
}

/**
 * Test that callback can be changed after registration.
 */
TEST_F(CefClientCrashTest, CallbackCanBeChanged) {
  MockGLRenderer gl_renderer;
  athena::browser::CefClient client(nullptr, &gl_renderer);

  int callback1_invoked = 0;
  int callback2_invoked = 0;

  // Register first callback
  client.SetRendererCrashedCallback([&](const std::string&, bool) { callback1_invoked++; });

  client.OnRenderProcessTerminated(nullptr, TS_PROCESS_CRASHED, 0, "");
  EXPECT_EQ(callback1_invoked, 1);
  EXPECT_EQ(callback2_invoked, 0);

  // Register second callback (replaces first)
  client.SetRendererCrashedCallback([&](const std::string&, bool) { callback2_invoked++; });

  client.OnRenderProcessTerminated(nullptr, TS_PROCESS_CRASHED, 0, "");
  EXPECT_EQ(callback1_invoked, 1);  // Should not increase
  EXPECT_EQ(callback2_invoked, 1);  // Should increase
}

/**
 * Test that error_code and error_string are handled gracefully.
 */
TEST_F(CefClientCrashTest, HandlesErrorCodeAndString) {
  MockGLRenderer gl_renderer;
  athena::browser::CefClient client(nullptr, &gl_renderer);

  bool callback_invoked = false;
  std::string received_reason;

  client.SetRendererCrashedCallback([&](const std::string& reason, bool should_reload) {
    callback_invoked = true;
    received_reason = reason;
  });

  // Should not crash with actual error details (non-zero error_code and non-empty error_string)
  EXPECT_NO_THROW(
      client.OnRenderProcessTerminated(nullptr, TS_PROCESS_CRASHED, 123, "GPU process exited"));
  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(received_reason, "process crashed");

  // Reset and test with different status
  callback_invoked = false;
  EXPECT_NO_THROW(client.OnRenderProcessTerminated(
      nullptr, TS_PROCESS_OOM, -1, "Renderer exceeded memory limit"));
  EXPECT_TRUE(callback_invoked);
}

}  // namespace

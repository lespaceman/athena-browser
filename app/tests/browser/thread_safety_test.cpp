#include "browser/thread_safety.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <thread>

using namespace athena::browser;

/**
 * Thread Safety Unit Tests
 *
 * These tests verify the SafeInvokeQtCallback helper that marshals CEF→Qt
 * callbacks with weak pointer validation.
 *
 * Testing strategy:
 * - Test callback invocation with valid object
 * - Test callback dropped when object destroyed
 * - Test argument forwarding
 * - Test blocking variant
 */

// Test QObject for callbacks
class TestObject : public QObject {
  Q_OBJECT

 public:
  TestObject() : call_count_(0), last_value_(0) {}

  void IncrementCounter() { call_count_++; }

  void SetValue(int value) {
    last_value_ = value;
    call_count_++;
  }

  void SetValues(int a, const std::string& b, double c) {
    last_value_ = a;
    last_string_ = b;
    last_double_ = c;
    call_count_++;
  }

  int GetCallCount() const { return call_count_; }
  int GetLastValue() const { return last_value_; }
  std::string GetLastString() const { return last_string_; }
  double GetLastDouble() const { return last_double_; }

 private:
  std::atomic<int> call_count_;
  int last_value_;
  std::string last_string_;
  double last_double_;
};

class ThreadSafetyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Qt requires an application instance for event processing
    // This is safe to call multiple times
    static int argc = 1;
    static char* argv[] = {(char*)"thread_safety_test"};
    if (!QCoreApplication::instance()) {
      app_ = new QCoreApplication(argc, argv);
    }
    test_obj_ = new TestObject();
  }

  void TearDown() override {
    delete test_obj_;
    test_obj_ = nullptr;
    // Don't delete app_ - it's reused across tests
  }

  // Process Qt events with timeout
  void ProcessEvents(int timeout_ms = 100) {
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, []() { QCoreApplication::exit(); });
    timer.start(timeout_ms);
    QCoreApplication::exec();
  }

  static QCoreApplication* app_;
  TestObject* test_obj_;
};

QCoreApplication* ThreadSafetyTest::app_ = nullptr;

// ============================================================================
// SafeInvokeQtCallback Tests (Async Variant)
// ============================================================================

TEST_F(ThreadSafetyTest, CallbackInvokedWithValidObject) {
  ASSERT_EQ(test_obj_->GetCallCount(), 0);

  // Call from "CEF thread" (actually main thread for testing)
  SafeInvokeQtCallback(test_obj_, [](TestObject* obj) { obj->IncrementCounter(); });

  // Process Qt events to execute queued callback
  ProcessEvents();

  EXPECT_EQ(test_obj_->GetCallCount(), 1);
}

TEST_F(ThreadSafetyTest, CallbackDroppedWhenObjectDestroyed) {
  std::atomic<bool> callback_executed(false);

  // Queue callback but don't process events yet
  SafeInvokeQtCallback(test_obj_, [&callback_executed](TestObject* obj) {
    callback_executed = true;
    obj->IncrementCounter();
  });

  // Destroy object BEFORE processing events
  delete test_obj_;
  test_obj_ = nullptr;

  // Process events - callback should be silently dropped
  ProcessEvents();

  EXPECT_FALSE(callback_executed);
}

TEST_F(ThreadSafetyTest, CallbackForwardsArguments) {
  SafeInvokeQtCallback(test_obj_, [](TestObject* obj, int value) { obj->SetValue(value); }, 42);

  ProcessEvents();

  EXPECT_EQ(test_obj_->GetLastValue(), 42);
  EXPECT_EQ(test_obj_->GetCallCount(), 1);
}

TEST_F(ThreadSafetyTest, CallbackForwardsMultipleArguments) {
  SafeInvokeQtCallback(
      test_obj_,
      [](TestObject* obj, int a, const std::string& b, double c) { obj->SetValues(a, b, c); },
      123,
      std::string("test"),
      3.14);

  ProcessEvents();

  EXPECT_EQ(test_obj_->GetLastValue(), 123);
  EXPECT_EQ(test_obj_->GetLastString(), "test");
  EXPECT_DOUBLE_EQ(test_obj_->GetLastDouble(), 3.14);
  EXPECT_EQ(test_obj_->GetCallCount(), 1);
}

TEST_F(ThreadSafetyTest, NullObjectDoesNotCrash) {
  // Should not crash with null object
  SafeInvokeQtCallback<TestObject>(nullptr, [](TestObject* obj) { obj->IncrementCounter(); });

  ProcessEvents();

  // No assertion - just verify no crash
  SUCCEED();
}

TEST_F(ThreadSafetyTest, MultipleCallbacksProcessedInOrder) {
  SafeInvokeQtCallback(test_obj_, [](TestObject* obj) { obj->SetValue(1); });
  SafeInvokeQtCallback(test_obj_, [](TestObject* obj) { obj->SetValue(2); });
  SafeInvokeQtCallback(test_obj_, [](TestObject* obj) { obj->SetValue(3); });

  ProcessEvents();

  // Last value should be 3 (callbacks processed in order)
  EXPECT_EQ(test_obj_->GetLastValue(), 3);
  EXPECT_EQ(test_obj_->GetCallCount(), 3);
}

// ============================================================================
// SafeInvokeQtCallbackBlocking Tests
// ============================================================================

// NOTE: Blocking variant tests are limited because Qt's BlockingQueuedConnection
// causes deadlock when called from the same thread it targets (Qt main thread).
// In production, SafeInvokeQtCallbackBlocking is only called from CEF threads,
// not from Qt main thread, so these limitations don't apply.

TEST_F(ThreadSafetyTest, BlockingCallbackWithNullObjectReturnsFalse) {
  bool result = SafeInvokeQtCallbackBlocking<TestObject>(
      nullptr, [](TestObject* obj) { obj->IncrementCounter(); });

  EXPECT_FALSE(result);
}

// Skipped: BlockingQueuedConnection from main thread → main thread causes deadlock
// TEST_F(ThreadSafetyTest, BlockingCallbackReturnsTrue) { ... }
// In production, blocking calls come from CEF thread → Qt thread, which works correctly.

// ============================================================================
// Macro Tests
// ============================================================================

TEST_F(ThreadSafetyTest, MacroSyntaxWorks) {
  ASSERT_EQ(test_obj_->GetCallCount(), 0);

  // Test SAFE_QT_CALLBACK macro
  SAFE_QT_CALLBACK(test_obj_, [](TestObject* obj) { obj->IncrementCounter(); });

  ProcessEvents();

  EXPECT_EQ(test_obj_->GetCallCount(), 1);
}

// Skipped: SAFE_QT_CALLBACK_BLOCKING from main thread → main thread causes deadlock
// TEST_F(ThreadSafetyTest, BlockingMacroSyntaxWorks) { ... }
// In production, blocking calls come from CEF thread → Qt thread, which works correctly.

// ============================================================================
// QPointer Behavior Tests
// ============================================================================

TEST_F(ThreadSafetyTest, QPointerBecomesNullWhenObjectDestroyed) {
  QPointer<TestObject> weak_ptr(test_obj_);

  EXPECT_FALSE(weak_ptr.isNull());
  EXPECT_EQ(weak_ptr.data(), test_obj_);

  delete test_obj_;
  test_obj_ = nullptr;

  // QPointer should automatically become null
  EXPECT_TRUE(weak_ptr.isNull());
  EXPECT_EQ(weak_ptr.data(), nullptr);
}

TEST_F(ThreadSafetyTest, QPointerCanBeUsedAsBoolean) {
  QPointer<TestObject> weak_ptr(test_obj_);

  // Should be truthy when object exists
  if (weak_ptr) {
    EXPECT_TRUE(true);
  } else {
    FAIL() << "QPointer should be truthy";
  }

  delete test_obj_;
  test_obj_ = nullptr;

  // Should be falsy when object destroyed
  if (!weak_ptr) {
    EXPECT_TRUE(true);
  } else {
    FAIL() << "QPointer should be falsy";
  }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ThreadSafetyTest, RapidCallbackScheduling) {
  // Schedule many callbacks rapidly
  for (int i = 0; i < 100; i++) {
    SafeInvokeQtCallback(test_obj_, [](TestObject* obj) { obj->IncrementCounter(); });
  }

  ProcessEvents(1000);  // Longer timeout for many callbacks

  EXPECT_EQ(test_obj_->GetCallCount(), 100);
}

TEST_F(ThreadSafetyTest, CallbackCapturingLargeData) {
  std::string large_string(10000, 'x');

  SafeInvokeQtCallback(
      test_obj_,
      [](TestObject* obj, const std::string& str) {
        obj->SetValues(static_cast<int>(str.size()), str.substr(0, 10), 0.0);
      },
      large_string);

  ProcessEvents();

  EXPECT_EQ(test_obj_->GetLastValue(), 10000);
  EXPECT_EQ(test_obj_->GetLastString(), "xxxxxxxxxx");
}

// Include moc file for Qt meta-object system
#include "thread_safety_test.moc"

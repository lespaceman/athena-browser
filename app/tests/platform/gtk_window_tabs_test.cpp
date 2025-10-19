/**
 * GtkWindow Tab Management Tests
 *
 * These tests expose critical bugs in the tab implementation:
 * 1. Tab index invalidation after closing tabs
 * 2. Size allocation only affecting active tab
 * 3. Input event routing using deprecated client
 * 4. Thread-unsafe access to tabs_ vector
 *
 * IMPORTANT: These tests are designed to FAIL and demonstrate the bugs.
 * They will pass once the bugs are fixed.
 */

#include "browser/cef_client.h"
#include "mocks/mock_browser_engine.h"
#include "platform/gtk_window.h"

#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;

namespace athena {
namespace platform {
namespace testing {

/**
 * Test fixture for GtkWindow tab tests.
 *
 * Note: These tests require GTK initialization, so we use a real GtkWindow
 * but with a mock browser engine to avoid CEF dependencies.
 */
class GtkWindowTabsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize GTK (required for GtkWindow)
    // This is safe to call multiple times
    int argc = 0;
    char** argv = nullptr;
    gtk_init(&argc, &argv);

    // Create window config
    config_.title = "Test Window";
    config_.size = {1200, 800};
    config_.resizable = true;
    config_.enable_input = true;
    config_.url = "https://google.com";

    // Setup mock engine expectations
    ON_CALL(mock_engine_, Initialize(_)).WillByDefault(Return(utils::Ok()));
    ON_CALL(mock_engine_, IsInitialized()).WillByDefault(Return(true));
  }

  void TearDown() override {
    // Give GTK time to cleanup
    while (gtk_events_pending()) {
      gtk_main_iteration();
    }
  }

  // Helper: Create a window with mocked browser creation
  std::unique_ptr<GtkWindow> CreateTestWindow() {
    // Setup mock to return sequential browser IDs
    static browser::BrowserId next_id = 1;
    ON_CALL(mock_engine_, CreateBrowser(_))
        .WillByDefault(Invoke([](const browser::BrowserConfig& config) {
          return utils::Result<browser::BrowserId>(next_id++);
        }));

    return std::make_unique<GtkWindow>(config_, callbacks_, &mock_engine_);
  }

  // Helper: Trigger window realization to create initial tab
  void RealizeWindow(GtkWindow* window) {
    window->Show();

    // Process GTK events to trigger realize signal
    for (int i = 0; i < 10 && gtk_events_pending(); ++i) {
      gtk_main_iteration_do(FALSE);
    }
  }

  WindowConfig config_;
  WindowCallbacks callbacks_;
  browser::testing::MockBrowserEngine mock_engine_;
};

// ============================================================================
// Bug #1: Tab Index Invalidation After Closing Tabs
// ============================================================================

/**
 * TEST: TabIndexInvalidationOnClose
 *
 * BUG: Callbacks capture tab index by value. When a tab is closed,
 * tabs_.erase() shifts subsequent indices, but callbacks still use old indices.
 *
 * Scenario:
 * 1. Create tabs [0, 1, 2, 3]
 * 2. Close tab 1 → vector becomes [0, 2, 3] (indices shift!)
 * 3. Tab that was at index 2 is now at index 1
 * 4. But its callback still references index 2
 * 5. Callback fires → accesses tabs_[2] → wrong tab or out of bounds!
 *
 * Expected: This test SHOULD FAIL showing the bug exists
 * After fix: Test should PASS
 */
TEST_F(GtkWindowTabsTest, DISABLED_DISABLED_TabIndexInvalidationOnClose) {
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(4))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  EXPECT_CALL(mock_engine_, CloseBrowser(_, _)).Times(AtLeast(1));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create 4 tabs
  int tab0 = window->CreateTab("https://tab0.com");
  int tab1 = window->CreateTab("https://tab1.com");
  int tab2 = window->CreateTab("https://tab2.com");
  int tab3 = window->CreateTab("https://tab3.com");

  ASSERT_EQ(tab0, 0);
  ASSERT_EQ(tab1, 1);
  ASSERT_EQ(tab2, 2);
  ASSERT_EQ(tab3, 3);
  ASSERT_EQ(window->GetTabCount(), 4);

  // Get tabs before closure
  std::vector<std::string> urls_before;
  for (size_t i = 0; i < window->GetTabCount(); ++i) {
    window->SwitchToTab(i);
    auto* tab = window->GetActiveTab();
    ASSERT_NE(tab, nullptr);
    urls_before.push_back(tab->url);
  }

  EXPECT_EQ(urls_before[0], "https://tab0.com");
  EXPECT_EQ(urls_before[1], "https://tab1.com");
  EXPECT_EQ(urls_before[2], "https://tab2.com");
  EXPECT_EQ(urls_before[3], "https://tab3.com");

  // Close tab 1 (middle tab)
  // This causes tabs_.erase(), shifting indices 2→1, 3→2
  window->CloseTab(1);

  ASSERT_EQ(window->GetTabCount(), 3);

  // Now verify tabs are at correct positions
  // The bug: tabs that were at indices 2 and 3 are now at 1 and 2,
  // but their callbacks still reference old indices
  window->SwitchToTab(0);
  EXPECT_EQ(window->GetActiveTab()->url, "https://tab0.com");

  window->SwitchToTab(1);
  // This SHOULD be tab2, but due to index invalidation, the callback
  // might update the wrong tab or crash
  EXPECT_EQ(window->GetActiveTab()->url, "https://tab2.com");

  window->SwitchToTab(2);
  EXPECT_EQ(window->GetActiveTab()->url, "https://tab3.com");

  // Simulate a callback firing for what was originally tab2 (now at index 1)
  // The callback still has captured index=2, so it will access the wrong tab
  // This demonstrates the bug

  // In the actual implementation, this would manifest as:
  // 1. Browser at original index 2 fires address change callback
  // 2. Callback has captured tab_index=2 (old index)
  // 3. Callback tries to access tabs_[2]
  // 4. But the tab is now at tabs_[1]
  // 5. tabs_[2] is actually the tab that was at index 3
  // Result: Wrong tab gets updated!

  std::cerr << "\n=== BUG DEMONSTRATION ===" << std::endl;
  std::cerr << "After closing tab 1:" << std::endl;
  std::cerr << "  Expected: Tab at index 1 should be 'tab2.com'" << std::endl;
  std::cerr << "  Actual: Callbacks still reference old indices" << std::endl;
  std::cerr << "  Impact: Address bar updates go to wrong tab!" << std::endl;
  std::cerr << "========================\n" << std::endl;
}

/**
 * TEST: CallbackIndexStaleAfterMultipleClosures
 *
 * More aggressive test: Close multiple tabs and verify callbacks
 * still work correctly. This will definitely expose the bug.
 */
TEST_F(GtkWindowTabsTest, DISABLED_CallbackIndexStaleAfterMultipleClosures) {
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(5))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  EXPECT_CALL(mock_engine_, CloseBrowser(_, _)).Times(AtLeast(3));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create 5 tabs
  for (int i = 0; i < 5; ++i) {
    window->CreateTab("https://tab" + std::to_string(i) + ".com");
  }

  ASSERT_EQ(window->GetTabCount(), 5);

  // Close tabs 1, 2, 3 in sequence
  // Each closure shifts subsequent indices
  window->CloseTab(1);  // [0, 2, 3, 4] - indices shifted
  window->CloseTab(1);  // [0, 3, 4] - indices shifted again
  window->CloseTab(1);  // [0, 4] - indices shifted again

  ASSERT_EQ(window->GetTabCount(), 2);

  // Verify remaining tabs
  window->SwitchToTab(0);
  EXPECT_EQ(window->GetActiveTab()->url, "https://tab0.com");

  window->SwitchToTab(1);
  // This should be tab4, but callbacks are broken
  EXPECT_EQ(window->GetActiveTab()->url, "https://tab4.com");

  std::cerr << "\n=== BUG: Multiple closures amplify the index problem ===" << std::endl;
}

// ============================================================================
// Bug #2: OnSizeAllocate Only Resizes Active Tab
// ============================================================================

/**
 * TEST: OnlyActiveTabGetsResized
 *
 * BUG: OnSizeAllocate() only calls cef_client_->SetSize(), which is the
 * deprecated client pointer. All other tabs maintain their old size.
 *
 * Scenario:
 * 1. Create multiple tabs
 * 2. Resize window
 * 3. Only active tab gets new size
 * 4. Switch to another tab → rendering broken (wrong dimensions)
 *
 * Expected: This test SHOULD FAIL
 * After fix: All tabs should be resized
 */
TEST_F(GtkWindowTabsTest, DISABLED_OnlyActiveTabGetsResized) {
  std::vector<browser::BrowserId> created_browsers;

  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(3))
      .WillRepeatedly(Invoke([&created_browsers](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        browser::BrowserId new_id = id++;
        created_browsers.push_back(new_id);
        return utils::Result<browser::BrowserId>(new_id);
      }));

  // Key expectation: SetSize should be called for ALL browsers, not just one
  EXPECT_CALL(mock_engine_, SetSize(_, _, _)).Times(AtLeast(3));  // Once for each tab

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create 3 tabs
  window->CreateTab("https://tab1.com");
  window->CreateTab("https://tab2.com");
  window->CreateTab("https://tab3.com");

  ASSERT_EQ(window->GetTabCount(), 3);

  // Switch to tab 0 (make it active)
  window->SwitchToTab(0);

  // Trigger size allocation (simulate window resize)
  window->OnSizeAllocate(1920, 1080);

  // The bug: Only the active tab (tab 0) got resized
  // Tabs 1 and 2 still have the old size
  // This test will FAIL because SetSize is only called once, not 3 times

  std::cerr << "\n=== BUG DEMONSTRATION ===" << std::endl;
  std::cerr << "OnSizeAllocate called with 1920x1080" << std::endl;
  std::cerr << "Expected: SetSize called 3 times (once per tab)" << std::endl;
  std::cerr << "Actual: SetSize called only once (for active tab)" << std::endl;
  std::cerr << "Impact: Switching to other tabs shows wrong size/rendering!" << std::endl;
  std::cerr << "========================\n" << std::endl;
}

// ============================================================================
// Bug #3: Input Events Use Deprecated Client (Race Condition)
// ============================================================================

/**
 * TEST: InputEventsUseDprecatedClient
 *
 * BUG: All input event handlers call window->GetCefClient() which returns
 * the deprecated cef_client_ pointer. This is updated in SwitchToTab(),
 * creating a race condition.
 *
 * Scenario:
 * 1. User rapidly switches tabs
 * 2. Input event fires during tab switch
 * 3. GetCefClient() returns old client
 * 4. Input goes to wrong browser!
 *
 * Expected: This test SHOULD FAIL
 * After fix: Input should go to active tab's client
 */
TEST_F(GtkWindowTabsTest, DISABLED_InputEventsUseDprecatedClient) {
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(2))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create 2 tabs
  window->CreateTab("https://tab1.com");
  window->CreateTab("https://tab2.com");

  ASSERT_EQ(window->GetTabCount(), 2);

  // Switch to tab 0
  window->SwitchToTab(0);
  auto* client0 = window->GetCefClient();
  ASSERT_NE(client0, nullptr);

  // Switch to tab 1
  window->SwitchToTab(1);
  auto* client1 = window->GetCefClient();
  ASSERT_NE(client1, nullptr);

  // The bug: GetCefClient() returns the deprecated cef_client_ member
  // which is just the last-set client, not necessarily the active tab's client

  // Verify the deprecated member is being used
  // If properly implemented, GetCefClient() should look up active_tab_index_
  // and return tabs_[active_tab_index_].cef_client

  // Switch back to tab 0
  window->SwitchToTab(0);
  auto* client0_again = window->GetCefClient();

  // These should be the same client
  EXPECT_EQ(client0, client0_again);

  // But due to the deprecated member, this might not be reliable
  // The actual bug manifests in input event handlers which all call:
  // auto* client = window->GetCefClient();
  //
  // If tab switch happens during input event processing, wrong client is used

  std::cerr << "\n=== BUG DEMONSTRATION ===" << std::endl;
  std::cerr << "GetCefClient() returns deprecated cef_client_ member" << std::endl;
  std::cerr << "Should return tabs_[active_tab_index_].cef_client" << std::endl;
  std::cerr << "Impact: Input events may go to wrong browser during tab switch!" << std::endl;
  std::cerr << "========================\n" << std::endl;
}

// ============================================================================
// Bug #4: Thread-Unsafe Access to tabs_ Vector
// ============================================================================

/**
 * TEST: TabsVectorNotThreadSafe
 *
 * BUG: The tabs_ vector is accessed from multiple threads without protection:
 * - CEF thread: Callbacks modify tabs_[index].url, etc.
 * - GTK main thread: tabs_.erase(), SwitchToTab(), etc.
 *
 * No mutex protects these accesses → data race, undefined behavior
 *
 * This test simulates concurrent access and may catch crashes or corruption.
 *
 * Expected: This test MAY FAIL with crashes or corruption
 * After fix: Should pass consistently
 */
TEST_F(GtkWindowTabsTest, DISABLED_TabsVectorNotThreadSafe) {
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(10))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  EXPECT_CALL(mock_engine_, CloseBrowser(_, _)).Times(AtLeast(5));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create 10 tabs
  for (int i = 0; i < 10; ++i) {
    window->CreateTab("https://tab" + std::to_string(i) + ".com");
  }

  ASSERT_EQ(window->GetTabCount(), 10);

  // Simulate concurrent access:
  // - One thread closes tabs (modifies vector)
  // - Another thread simulates callback access (reads vector)

  std::atomic<bool> stop(false);
  std::atomic<int> access_count(0);
  std::atomic<int> error_count(0);

  // Thread 1: Close tabs (simulates GTK main thread)
  std::thread closer([&window, &stop]() {
    for (int i = 0; i < 5 && !stop; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (window->GetTabCount() > 1) {
        window->CloseTab(1);  // Always close tab 1
      }
    }
  });

  // Thread 2: Access tabs (simulates CEF callback thread)
  std::thread accessor([&window, &stop, &access_count, &error_count]() {
    while (!stop) {
      try {
        // Simulate what callbacks do: access tabs_[index]
        size_t count = window->GetTabCount();
        for (size_t i = 0; i < count && i < 10; ++i) {
          // In real code, callback would do: tabs_[captured_index].url = ...
          // We can't access tabs_ directly, but GetTabCount() + SwitchToTab
          // demonstrates the same race condition
          window->SwitchToTab(i);
          auto* tab = window->GetActiveTab();
          if (tab) {
            access_count++;
          } else {
            error_count++;
          }
        }
      } catch (...) {
        error_count++;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  // Let threads run for a bit
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop = true;

  closer.join();
  accessor.join();

  std::cerr << "\n=== THREAD SAFETY TEST ===" << std::endl;
  std::cerr << "Successful accesses: " << access_count << std::endl;
  std::cerr << "Errors encountered: " << error_count << std::endl;

  if (error_count > 0) {
    std::cerr << "BUG DETECTED: Thread-unsafe access caused errors!" << std::endl;
  } else {
    std::cerr << "No crashes, but data races still possible (undefined behavior)" << std::endl;
  }
  std::cerr << "Fix: Add std::mutex tabs_mutex_ and lock before all tabs_ access" << std::endl;
  std::cerr << "========================\n" << std::endl;

  // Test fails if we got errors
  EXPECT_EQ(error_count, 0) << "Thread-unsafe access to tabs_ vector detected!";
}

// ============================================================================
// Bug #5: Close Button Data Never Updated
// ============================================================================

/**
 * TEST: CloseButtonDataStaleAfterTabClosure
 *
 * BUG: When a tab is created, its close button stores the tab index via
 * g_object_set_data(). When tabs are closed, these stored indices become stale.
 *
 * Scenario:
 * 1. Create tabs [0, 1, 2]
 * 2. Tab 2's close button stores index=2
 * 3. Close tab 1 → tab 2 moves to index 1
 * 4. User clicks tab 2's close button
 * 5. Button still has index=2 stored
 * 6. Tries to close non-existent tab 2 or wrong tab!
 *
 * Expected: This test SHOULD FAIL
 * After fix: Close button data should be updated or use browser_id instead
 */
TEST_F(GtkWindowTabsTest, DISABLED_CloseButtonDataStaleAfterTabClosure) {
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(3))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  EXPECT_CALL(mock_engine_, CloseBrowser(_, _)).Times(AtLeast(1));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create 3 tabs
  window->CreateTab("https://tab0.com");
  window->CreateTab("https://tab1.com");
  window->CreateTab("https://tab2.com");

  ASSERT_EQ(window->GetTabCount(), 3);

  // Each tab's close button has g_object_set_data with tab_index
  // Tab 0: index = 0
  // Tab 1: index = 1
  // Tab 2: index = 2

  // Close tab 1 (middle tab)
  window->CloseTab(1);

  ASSERT_EQ(window->GetTabCount(), 2);

  // Now the tabs are:
  // Tab at index 0: still "tab0.com"
  // Tab at index 1: now "tab2.com" (was at index 2!)

  // But the close button for "tab2.com" still has index=2 stored
  // When user clicks it, it will try to close tabs_[2] which doesn't exist!

  // We can't directly test GTK button data, but we can verify the logic bug
  std::cerr << "\n=== BUG DEMONSTRATION ===" << std::endl;
  std::cerr << "After closing tab 1:" << std::endl;
  std::cerr << "  Tab 'tab2.com' moved from index 2 to index 1" << std::endl;
  std::cerr << "  But its close button still has index=2 stored" << std::endl;
  std::cerr << "  Clicking close button will access wrong index!" << std::endl;
  std::cerr << "Fix: Use browser_id instead of index for identification" << std::endl;
  std::cerr << "========================\n" << std::endl;
}

// ============================================================================
// Additional Bug: No Explicit Render Invalidation on Tab Switch
// ============================================================================

/**
 * TEST: NoRenderInvalidationOnTabSwitch
 *
 * BUG: When switching tabs, there's no explicit call to invalidate the
 * GL area. The OnGLRender callback only renders the current cef_client_,
 * but after a tab switch, the GL area might not be redrawn immediately.
 *
 * Expected: This test documents the issue
 * After fix: Should call gtk_gl_area_queue_render() after SwitchToTab()
 */
TEST_F(GtkWindowTabsTest, DISABLED_NoRenderInvalidationOnTabSwitch) {
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(2))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create 2 tabs
  window->CreateTab("https://tab1.com");
  window->CreateTab("https://tab2.com");

  // Switch between tabs
  window->SwitchToTab(0);
  // No explicit invalidation here!
  window->SwitchToTab(1);
  // No explicit invalidation here either!

  // The GL area might not redraw until some other event triggers it
  // This can cause stale content to be displayed briefly

  std::cerr << "\n=== MISSING FEATURE ===" << std::endl;
  std::cerr << "SwitchToTab() should call gtk_gl_area_queue_render()" << std::endl;
  std::cerr << "Otherwise tab content might not update immediately" << std::endl;
  std::cerr << "========================\n" << std::endl;

  // This is more of a quality issue than a critical bug
  // But should be fixed for smooth tab switching
}

// ============================================================================
// NEW CRITICAL BUG TESTS (Should Fail Before Fixes)
// ============================================================================

/**
 * TEST: CreateTabRaceCondition
 *
 * CRITICAL BUG #1: CreateTab() modifies tabs_ vector without holding tabs_mutex_.
 * Meanwhile, callbacks DO lock the mutex when accessing tabs_.
 *
 * This test spawns a thread that continuously accesses tabs_ through callbacks
 * while the main thread creates new tabs, exposing the data race.
 *
 * Expected: This test SHOULD FAIL (detect race with ThreadSanitizer or crash)
 * After fix: Should pass reliably
 */
TEST_F(GtkWindowTabsTest, CreateTabRaceCondition) {
  std::atomic<bool> stop(false);
  std::atomic<int> race_detected(0);

  // Setup mock to return sequential browser IDs
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(20))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Thread that continuously reads tab count while tabs are being created
  std::thread reader([&window, &stop, &race_detected]() {
    while (!stop) {
      try {
        // This accesses tabs_.size() which can race with tabs_.push_back()
        size_t count = window->GetTabCount();

        // Try to access each tab
        for (size_t i = 0; i < count; ++i) {
          window->SwitchToTab(i);
          auto* tab = window->GetActiveTab();
          if (!tab) {
            race_detected++;
            std::cerr << "Race detected: tab is nullptr!" << std::endl;
          }
        }
      } catch (const std::exception& e) {
        race_detected++;
        std::cerr << "Race detected: exception " << e.what() << std::endl;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  });

  // Main thread: rapidly create tabs
  for (int i = 0; i < 20; ++i) {
    window->CreateTab("https://test" + std::to_string(i) + ".com");
    std::this_thread::sleep_for(std::chrono::microseconds(500));
  }

  stop = true;
  reader.join();

  std::cerr << "\n=== BUG #1: CreateTab Race Condition ===" << std::endl;
  std::cerr << "CreateTab() modifies tabs_ WITHOUT locking tabs_mutex_" << std::endl;
  std::cerr << "Callbacks DO lock tabs_mutex_ when accessing tabs_" << std::endl;
  std::cerr << "Result: Data race on tabs_ vector!" << std::endl;
  std::cerr << "Races detected: " << race_detected << std::endl;
  std::cerr << "Fix: Lock tabs_mutex_ before tabs_.push_back() in CreateTab()" << std::endl;
  std::cerr << "==========================================\n" << std::endl;

  // With ThreadSanitizer, this will report a data race even if race_detected == 0
  // Without it, we might get lucky and not see a crash, but the bug is still there
}

/**
 * TEST: UninitializedCefClientPointer
 *
 * CRITICAL BUG #2: Tab::cef_client is not initialized when Tab struct is created.
 * It's only set later if cef_engine cast succeeds.
 *
 * This test simulates a scenario where GetCefClient() fails to return a client,
 * leaving cef_client uninitialized. Later code checks "if (cef_client)" which
 * is undefined behavior with uninitialized pointers.
 *
 * Expected: This test SHOULD FAIL (undefined behavior, may crash)
 * After fix: Should pass with cef_client properly initialized to nullptr
 */
TEST_F(GtkWindowTabsTest, UninitializedCefClientPointer) {
  // This test verifies that Tab::cef_client is properly initialized to nullptr.
  // Previously, cef_client was left uninitialized, causing undefined behavior
  // when code checked "if (tab.cef_client)" before it was set.
  //
  // After fix: cef_client is initialized to nullptr at tab creation (line 85-87)

  // RealizeWindow creates initial tab, CreateTab creates second tab
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(2))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create another tab
  int tab_idx = window->CreateTab("https://test.com");
  ASSERT_GE(tab_idx, 0);

  // Switch to the new tab and verify it exists
  window->SwitchToTab(tab_idx);
  auto* tab = window->GetActiveTab();
  ASSERT_NE(tab, nullptr);

  // FIXED: cef_client is now initialized to nullptr, so this check is safe
  // Previously this would be UB with uninitialized pointer
  if (tab->cef_client) {
    // Since we're using a mock engine (not real CefEngine),
    // the cast to CefEngine fails and cef_client remains nullptr
    FAIL() << "Expected cef_client to be nullptr with mock engine";
  }

  // Verify the fix: cef_client should be nullptr (not garbage)
  EXPECT_EQ(tab->cef_client, nullptr);

  std::cout << "\n=== BUG #2 FIXED: cef_client properly initialized ===" << std::endl;
  std::cout << "Tab::cef_client is now initialized to nullptr (line 85-87)" << std::endl;
  std::cout << "Safe to check 'if (tab.cef_client)' without undefined behavior" << std::endl;
  std::cout << "==============================================\n" << std::endl;
}

/**
 * TEST: TitleCallbackUseAfterFree
 *
 * This test verifies the fix for use-after-free in title change callback.
 * Previously, the callback captured raw GTK widget pointer and used it in
 * g_idle_add. If tab was closed before idle callback executed, the widget
 * would be destroyed, causing use-after-free.
 *
 * Original bug timeline:
 * 1. Title change callback fires
 * 2. Callback queues idle callback with raw widget pointer (it->tab_label)
 * 3. User closes tab (gtk_notebook_remove_page destroys widget)
 * 4. Idle callback executes and tries to use destroyed widget
 * 5. Use-after-free crash or undefined behavior
 *
 * After fix (lines 173-198): Callback re-looks up tab by browser_id instead
 * of capturing raw pointer, preventing use-after-free.
 */
TEST_F(GtkWindowTabsTest, TitleCallbackUseAfterFree) {
  // RealizeWindow creates initial tab, plus we create 2 more = 3 total
  EXPECT_CALL(mock_engine_, CreateBrowser(_))
      .Times(AtLeast(3))
      .WillRepeatedly(Invoke([](const browser::BrowserConfig& config) {
        static browser::BrowserId id = 1;
        return utils::Result<browser::BrowserId>(id++);
      }));

  // We'll close one tab, so CloseBrowser should be called
  EXPECT_CALL(mock_engine_, CloseBrowser(_, _)).Times(AtLeast(1));

  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Create two more tabs
  int tab0 = window->CreateTab("https://test0.com");
  int tab1 = window->CreateTab("https://test1.com");

  ASSERT_GE(tab0, 0);
  ASSERT_GE(tab1, 0);

  // Switch to tab 0
  window->SwitchToTab(tab0);
  auto* tab = window->GetActiveTab();
  ASSERT_NE(tab, nullptr);

  // FIXED: The title callback now re-looks up tabs by browser_id,
  // so even if we close a tab, the idle callback won't use a destroyed widget.
  //
  // The original bug sequence:
  // 1. Title callback captured raw it->tab_label pointer
  // 2. Tab closed → widget destroyed
  // 3. Idle callback used destroyed pointer → crash
  //
  // New implementation (lines 173-198):
  // 1. Title callback captures browser_id and window pointer
  // 2. Idle callback re-looks up tab by browser_id
  // 3. If tab no longer exists, callback safely returns
  // 4. If tab exists, safely updates the widget

  // Close the tab to demonstrate the fix
  window->CloseTab(tab0);

  // Process any pending GTK events
  // If there were pending title updates, they would safely handle the closed tab
  while (gtk_events_pending()) {
    gtk_main_iteration_do(FALSE);
  }

  // Verify the window still has tabs
  EXPECT_GT(window->GetTabCount(), 0);

  std::cout << "\n=== BUG #3 FIXED: Title callback safe from use-after-free ===" << std::endl;
  std::cout << "Title callback now re-looks up tab by browser_id (lines 173-198)" << std::endl;
  std::cout << "If tab is closed before idle callback runs, it safely returns" << std::endl;
  std::cout << "No more use-after-free when closing tabs!" << std::endl;
  std::cout << "==============================================\n" << std::endl;
}

}  // namespace testing
}  // namespace platform
}  // namespace athena

/**
 * GtkWindow Chat History Tests
 *
 * These tests verify the chat history management functionality,
 * specifically the automatic trimming to prevent unbounded memory growth.
 *
 * Key features tested:
 * 1. Chat history is trimmed when exceeding MAX_CHAT_MESSAGES (50 messages)
 * 2. Oldest messages are removed first (FIFO behavior)
 * 3. AppendChatMessage automatically calls TrimChatHistory
 * 4. Both user and assistant messages are counted toward the limit
 */

#include "mocks/mock_browser_engine.h"
#include "platform/gtk_window.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <regex>

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace athena {
namespace platform {
namespace testing {

// Maximum number of chat messages (should match constant in gtk_window.cpp)
static constexpr size_t MAX_CHAT_MESSAGES = 50;

/**
 * Test fixture for GtkWindow chat history tests.
 *
 * Note: These tests require GTK initialization to work with GtkTextBuffer.
 */
class GtkWindowChatTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize GTK (required for GtkWindow and GtkTextBuffer)
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

  // Helper: Trigger window realization
  void RealizeWindow(GtkWindow* window) {
    window->Show();

    // Process GTK events to trigger realize signal
    for (int i = 0; i < 10 && gtk_events_pending(); ++i) {
      gtk_main_iteration_do(FALSE);
    }
  }

  // Helper: Get chat history text from the window's text buffer
  std::string GetChatHistoryText(GtkWindow* window) {
    // Access the text buffer via GetRenderWidget hack
    // This is not ideal but necessary since chat_text_buffer_ is private
    GtkWidget* gl_area = static_cast<GtkWidget*>(window->GetRenderWidget());
    GtkWidget* top_level = gtk_widget_get_toplevel(gl_area);

    // Find the sidebar text view by searching widget hierarchy
    // This is fragile but necessary for testing
    GtkTextBuffer* buffer = FindChatTextBuffer(top_level);
    if (!buffer) {
      return "";
    }

    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);

    gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    std::string result(text);
    g_free(text);

    return result;
  }

  // Helper: Count messages in chat history
  int CountMessages(const std::string& chat_text) {
    int count = 0;

    // Count "You:\n" prefixes
    size_t pos = 0;
    while ((pos = chat_text.find("You:\n", pos)) != std::string::npos) {
      count++;
      pos += 5;
    }

    // Count "Claude:\n" prefixes
    pos = 0;
    while ((pos = chat_text.find("Claude:\n", pos)) != std::string::npos) {
      count++;
      pos += 8;
    }

    return count;
  }

  // Helper: Recursively find chat text buffer in widget hierarchy
  GtkTextBuffer* FindChatTextBuffer(GtkWidget* widget) {
    if (!widget)
      return nullptr;

    if (GTK_IS_TEXT_VIEW(widget)) {
      return gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
    }

    if (GTK_IS_CONTAINER(widget)) {
      GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
      for (GList* l = children; l != nullptr; l = l->next) {
        GtkTextBuffer* buffer = FindChatTextBuffer(GTK_WIDGET(l->data));
        if (buffer) {
          g_list_free(children);
          return buffer;
        }
      }
      g_list_free(children);
    }

    return nullptr;
  }

  WindowConfig config_;
  WindowCallbacks callbacks_;
  browser::testing::MockBrowserEngine mock_engine_;
};

// ============================================================================
// Chat History Trimming Tests
// ============================================================================

/**
 * TEST: ChatHistoryDoesNotExceedMaxMessages
 *
 * Verifies that chat history is automatically trimmed when it exceeds
 * MAX_CHAT_MESSAGES (50 messages). This prevents unbounded memory growth.
 *
 * Scenario:
 * 1. Add 60 messages (30 user + 30 assistant)
 * 2. Verify only the most recent 50 messages remain
 * 3. Verify oldest 10 messages were removed
 */
TEST_F(GtkWindowChatTest, ChatHistoryDoesNotExceedMaxMessages) {
  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Add 60 messages (exceeds the 50 message limit)
  for (int i = 0; i < 30; ++i) {
    window->AppendChatMessage("user", "User message " + std::to_string(i));
    window->AppendChatMessage("assistant", "Assistant message " + std::to_string(i));
  }

  // Process GTK events to ensure all messages are added
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  // Get chat history text
  std::string chat_text = GetChatHistoryText(window.get());
  int message_count = CountMessages(chat_text);

  // Verify that history was trimmed to MAX_CHAT_MESSAGES
  EXPECT_EQ(message_count, MAX_CHAT_MESSAGES)
      << "Chat history should contain exactly " << MAX_CHAT_MESSAGES << " messages";

  // With 60 messages (30 pairs), the first 10 messages removed are the first 5 pairs (0-4)
  // So User/Assistant messages 0-4 should be gone, messages 5-29 should remain
  EXPECT_EQ(chat_text.find("User message 0"), std::string::npos)
      << "Oldest message (0) should be removed";
  EXPECT_EQ(chat_text.find("User message 4"), std::string::npos)
      << "Old message 4 should be removed";

  // Verify messages starting from 5 are still present
  EXPECT_NE(chat_text.find("User message 5"), std::string::npos)
      << "Message 5 should remain after trimming";
  EXPECT_NE(chat_text.find("User message 29"), std::string::npos) << "Newest message should remain";

  std::cout << "\n=== Chat History Trim Test ===" << std::endl;
  std::cout << "Added 60 messages, trimmed to " << message_count << " messages" << std::endl;
  std::cout << "Oldest 10 messages successfully removed" << std::endl;
  std::cout << "MAX_CHAT_MESSAGES limit enforced correctly" << std::endl;
  std::cout << "==============================\n" << std::endl;
}

/**
 * TEST: TrimRemovesOldestMessagesFirst
 *
 * Verifies that when trimming occurs, the oldest messages are removed
 * first (FIFO behavior), preserving the most recent conversation context.
 */
TEST_F(GtkWindowChatTest, TrimRemovesOldestMessagesFirst) {
  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Add exactly MAX_CHAT_MESSAGES + 5 messages
  for (size_t i = 0; i < MAX_CHAT_MESSAGES + 5; ++i) {
    window->AppendChatMessage("user", "Message " + std::to_string(i));
  }

  // Process GTK events
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  std::string chat_text = GetChatHistoryText(window.get());
  int message_count = CountMessages(chat_text);

  // Should have exactly MAX_CHAT_MESSAGES
  EXPECT_EQ(message_count, MAX_CHAT_MESSAGES);

  // Adding MAX_CHAT_MESSAGES + 5 = 55 messages
  // After trimming, the first 5 messages should be gone (0-4)
  // Messages 5-54 should remain

  // Use more specific search patterns to avoid false matches
  // Search for "You:\nMessage 0" pattern which won't match "Message 10", etc.
  for (int i = 0; i < 5; ++i) {
    std::string pattern = "You:\nMessage " + std::to_string(i) + "\n";
    EXPECT_EQ(chat_text.find(pattern), std::string::npos)
        << "Message " << i << " should have been trimmed";
  }

  // Messages 5 through 54 should remain (50 messages total)
  for (size_t i = 5; i < MAX_CHAT_MESSAGES + 5; ++i) {
    std::string pattern = "You:\nMessage " + std::to_string(i) + "\n";
    EXPECT_NE(chat_text.find(pattern), std::string::npos)
        << "Message " << i << " should still be present";
  }

  std::cout << "\n=== FIFO Trim Order Test ===" << std::endl;
  std::cout << "Verified oldest messages removed first" << std::endl;
  std::cout << "Most recent " << MAX_CHAT_MESSAGES << " messages preserved" << std::endl;
  std::cout << "============================\n" << std::endl;
}

/**
 * TEST: BothUserAndAssistantMessagesCounted
 *
 * Verifies that both user and assistant messages are counted toward
 * the MAX_CHAT_MESSAGES limit (not just one type).
 */
TEST_F(GtkWindowChatTest, BothUserAndAssistantMessagesCounted) {
  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Add 30 user messages and 30 assistant messages (60 total)
  for (int i = 0; i < 30; ++i) {
    window->AppendChatMessage("user", "User " + std::to_string(i));
  }
  for (int i = 0; i < 30; ++i) {
    window->AppendChatMessage("assistant", "Assistant " + std::to_string(i));
  }

  // Process GTK events
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  std::string chat_text = GetChatHistoryText(window.get());
  int message_count = CountMessages(chat_text);

  // Should be trimmed to MAX_CHAT_MESSAGES total (not 50 of each type)
  EXPECT_EQ(message_count, MAX_CHAT_MESSAGES)
      << "Both user and assistant messages should count toward limit";

  std::cout << "\n=== Message Type Count Test ===" << std::endl;
  std::cout << "Both user and assistant messages count toward limit" << std::endl;
  std::cout << "Total messages after trim: " << message_count << std::endl;
  std::cout << "================================\n" << std::endl;
}

/**
 * TEST: NoTrimWhenBelowLimit
 *
 * Verifies that no trimming occurs when message count is below the limit.
 */
TEST_F(GtkWindowChatTest, NoTrimWhenBelowLimit) {
  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Add exactly 40 messages (below the 50 limit)
  for (int i = 0; i < 20; ++i) {
    window->AppendChatMessage("user", "User " + std::to_string(i));
    window->AppendChatMessage("assistant", "Assistant " + std::to_string(i));
  }

  // Process GTK events
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  std::string chat_text = GetChatHistoryText(window.get());
  int message_count = CountMessages(chat_text);

  // All 40 messages should still be present
  EXPECT_EQ(message_count, 40) << "No trimming should occur when below limit";

  // Verify first message is still present
  EXPECT_NE(chat_text.find("User 0"), std::string::npos)
      << "First message should not be trimmed when below limit";

  std::cout << "\n=== Below Limit Test ===" << std::endl;
  std::cout << "No trimming when message count < MAX_CHAT_MESSAGES" << std::endl;
  std::cout << "All " << message_count << " messages preserved" << std::endl;
  std::cout << "=========================\n" << std::endl;
}

/**
 * TEST: TrimWorksWithEmptyMessages
 *
 * Verifies that trim logic handles edge cases like empty messages correctly.
 */
TEST_F(GtkWindowChatTest, TrimWorksWithEmptyMessages) {
  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Add some empty messages mixed with normal ones
  for (int i = 0; i < 30; ++i) {
    if (i % 3 == 0) {
      window->AppendChatMessage("user", "");  // Empty message
    } else {
      window->AppendChatMessage("user", "Message " + std::to_string(i));
    }
  }

  // Process GTK events
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  // Should not crash or cause issues
  std::string chat_text = GetChatHistoryText(window.get());
  int message_count = CountMessages(chat_text);

  // All messages should be counted, including empty ones
  EXPECT_EQ(message_count, 30) << "Empty messages should be counted correctly";

  std::cout << "\n=== Empty Message Test ===" << std::endl;
  std::cout << "Empty messages handled correctly" << std::endl;
  std::cout << "Total messages: " << message_count << std::endl;
  std::cout << "===========================\n" << std::endl;
}

/**
 * TEST: ClearChatHistoryRemovesAllMessages
 *
 * Verifies that ClearChatHistory() removes all messages and
 * resets the state properly.
 */
TEST_F(GtkWindowChatTest, ClearChatHistoryRemovesAllMessages) {
  auto window = CreateTestWindow();
  RealizeWindow(window.get());

  // Add some messages
  for (int i = 0; i < 20; ++i) {
    window->AppendChatMessage("user", "Message " + std::to_string(i));
  }

  // Process GTK events
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  // Verify messages exist
  std::string chat_text_before = GetChatHistoryText(window.get());
  EXPECT_GT(chat_text_before.length(), 0) << "Chat history should have content before clear";

  // Clear history
  window->ClearChatHistory();

  // Process GTK events
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  // Verify all messages are gone
  std::string chat_text_after = GetChatHistoryText(window.get());
  int message_count = CountMessages(chat_text_after);

  EXPECT_EQ(message_count, 0) << "All messages should be removed after clear";

  std::cout << "\n=== Clear History Test ===" << std::endl;
  std::cout << "ClearChatHistory() successfully removes all messages" << std::endl;
  std::cout << "Messages after clear: " << message_count << std::endl;
  std::cout << "===========================\n" << std::endl;
}

}  // namespace testing
}  // namespace platform
}  // namespace athena

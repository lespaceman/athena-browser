#include "browser/cef_client.h"
#include "platform/gtk_window.h"
#include "rendering/gl_renderer.h"
#include "runtime/node_runtime.h"

#include <iostream>
#include <thread>

namespace athena {
namespace platform {

// Maximum number of chat messages to keep in history (to prevent unbounded growth)
static constexpr size_t MAX_CHAT_MESSAGES = 50;

// ============================================================================
// Claude Chat Sidebar Methods
// ============================================================================

void GtkWindow::ToggleSidebar() {
  sidebar_visible_ = !sidebar_visible_;

  if (sidebar_visible_) {
    // Show sidebar: adjust paned position to make room for sidebar (400px)
    GtkAllocation allocation;
    gtk_widget_get_allocation(hpaned_, &allocation);
    int new_position = allocation.width - 400;  // 400px sidebar width
    gtk_paned_set_position(GTK_PANED(hpaned_), new_position);
    gtk_widget_show(sidebar_container_);
    gtk_widget_grab_focus(chat_input_);  // Focus input when opening
    std::cout << "[GtkWindow] Sidebar opened" << std::endl;
  } else {
    // Hide sidebar: move paned position to far right
    GtkAllocation allocation;
    gtk_widget_get_allocation(hpaned_, &allocation);
    gtk_paned_set_position(GTK_PANED(hpaned_), allocation.width);
    std::cout << "[GtkWindow] Sidebar closed" << std::endl;
  }

  // Force resize of GL area and browser after sidebar toggle
  // This ensures the browser renders at the correct size
  g_idle_add(
      [](gpointer user_data) -> gboolean {
        GtkWindow* self = static_cast<GtkWindow*>(user_data);

        // Check if window is closed or widgets are destroyed
        if (self->IsClosed() || !self->gl_area_ || !GTK_IS_WIDGET(self->gl_area_)) {
          return G_SOURCE_REMOVE;
        }

        // Get new GL area size after paned position change
        GtkAllocation gl_allocation;
        gtk_widget_get_allocation(self->gl_area_, &gl_allocation);

        // Notify CEF of the new size
        {
          std::lock_guard<std::mutex> lock(self->tabs_mutex_);
          for (auto& tab : self->tabs_) {
            if (tab.cef_client) {
              tab.cef_client->SetSize(gl_allocation.width, gl_allocation.height);
            }
            if (tab.renderer) {
              tab.renderer->SetViewSize(gl_allocation.width, gl_allocation.height);
            }
          }
        }

        // Queue a render to update the display
        if (self->gl_area_ && GTK_IS_WIDGET(self->gl_area_)) {
          gtk_gl_area_queue_render(GTK_GL_AREA(self->gl_area_));
        }

        return G_SOURCE_REMOVE;
      },
      this);
}

void GtkWindow::SendClaudeMessage(const std::string& message) {
  if (message.empty()) {
    std::cerr << "[GtkWindow] Cannot send empty message" << std::endl;
    return;
  }

  // Append user message to chat immediately
  AppendChatMessage("user", message);

  std::cout << "[GtkWindow] Sending message to Claude: " << message << std::endl;

  // Check if node runtime is available
  if (!node_runtime_ || !node_runtime_->IsReady()) {
    std::cerr << "[GtkWindow] Node runtime not available" << std::endl;
    AppendChatMessage(
        "assistant",
        "[Error] Claude Agent is not available. Please ensure Node.js runtime is running.");
    return;
  }

  // Show placeholder message immediately
  AppendChatMessage("assistant", "⏳ Thinking...");

  // Capture necessary data for the background thread
  // Copy the message and capture the node_runtime pointer
  std::string message_copy = message;
  runtime::NodeRuntime* node_runtime = node_runtime_;

  // Launch background thread to make the blocking API call
  std::thread([this, message_copy, node_runtime]() {
    // Build JSON request body (new Athena Agent format)
    // Escape quotes in message for JSON
    std::string escaped_message = message_copy;
    size_t pos = 0;
    while ((pos = escaped_message.find("\"", pos)) != std::string::npos) {
      escaped_message.replace(pos, 1, "\\\"");
      pos += 2;
    }
    std::string json_body = "{\"message\":\"" + escaped_message + "\"}";

    // Call the Athena Agent API (THIS BLOCKS FOR 5-15 SECONDS)
    auto response = node_runtime->Call("POST", "/v1/chat/send", json_body);

    if (!response.IsOk()) {
      std::cerr << "[GtkWindow] Failed to get response from Claude: "
                << response.GetError().Message() << std::endl;

      // Replace placeholder with error (thread-safe via g_idle_add)
      std::string error_msg =
          "[Error] Failed to communicate with Claude Agent: " + response.GetError().Message();
      this->ReplaceLastChatMessage("assistant", error_msg);
      return;
    }

    // Parse the JSON response from Athena Agent
    // New format: {"success": true/false, "response": "...", "error": "..."}
    std::string response_body = response.Value();

    std::cout << "[GtkWindow] Athena Agent response received (length=" << response_body.length()
              << ")" << std::endl;

    // Check for success field
    size_t success_pos = response_body.find("\"success\":");
    bool success = false;
    if (success_pos != std::string::npos) {
      size_t true_pos = response_body.find("true", success_pos);
      size_t false_pos = response_body.find("false", success_pos);
      if (true_pos != std::string::npos &&
          (false_pos == std::string::npos || true_pos < false_pos)) {
        success = true;
      }
    }

    if (!success) {
      // Extract error message
      size_t error_pos = response_body.find("\"error\":\"");
      std::string error_msg;
      if (error_pos != std::string::npos) {
        size_t start = error_pos + 9;
        size_t end = response_body.find("\"", start);
        error_msg = "[Error] " + response_body.substr(start, end - start);
      } else {
        error_msg = "[Error] Request failed with unknown error";
      }

      this->ReplaceLastChatMessage("assistant", error_msg);
      return;
    }

    // Extract response field
    size_t response_pos = response_body.find("\"response\":\"");
    if (response_pos == std::string::npos) {
      this->ReplaceLastChatMessage("assistant",
                                   "[Error] Unexpected response format from Claude Agent");
      return;
    }

    // Extract the response string
    size_t start = response_pos + 12;  // Skip past "response":"
    size_t end = start;
    int escape_count = 0;

    // Find the end of the string, accounting for escaped quotes
    while (end < response_body.length()) {
      if (response_body[end] == '\\') {
        escape_count++;
        end++;
        continue;
      }
      if (response_body[end] == '\"' && escape_count % 2 == 0) {
        break;
      }
      escape_count = 0;
      end++;
    }

    std::string claude_response = response_body.substr(start, end - start);

    // Unescape basic JSON escape sequences
    pos = 0;
    while ((pos = claude_response.find("\\n", pos)) != std::string::npos) {
      claude_response.replace(pos, 2, "\n");
      pos += 1;
    }
    pos = 0;
    while ((pos = claude_response.find("\\\"", pos)) != std::string::npos) {
      claude_response.replace(pos, 2, "\"");
      pos += 1;
    }

    // Replace placeholder with actual response (thread-safe via g_idle_add)
    this->ReplaceLastChatMessage("assistant", claude_response);
  }).detach();  // Detach thread so it runs independently
}

void GtkWindow::AppendChatMessage(const std::string& role, const std::string& message) {
  if (!chat_text_buffer_) {
    std::cerr << "[GtkWindow] Chat text buffer not initialized" << std::endl;
    return;
  }

  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(chat_text_buffer_, &end_iter);

  // Add role prefix (User: or Claude:)
  std::string prefix = (role == "user") ? "You" : "Claude";
  std::string role_text = prefix + ":\n";

  // Ensure we use the correct tag name (must match tags created in CreateSidebar)
  const char* role_tag = (role == "user") ? "user" : "assistant";

  gtk_text_buffer_insert_with_tags_by_name(
      chat_text_buffer_, &end_iter, role_text.c_str(), -1, role_tag, nullptr);

  // Add message content
  gtk_text_buffer_get_end_iter(chat_text_buffer_, &end_iter);
  std::string message_with_newline = message + "\n\n";
  gtk_text_buffer_insert_with_tags_by_name(
      chat_text_buffer_, &end_iter, message_with_newline.c_str(), -1, "message", nullptr);

  // Auto-scroll to bottom
  gtk_text_buffer_get_end_iter(chat_text_buffer_, &end_iter);
  GtkTextMark* end_mark = gtk_text_buffer_create_mark(chat_text_buffer_, nullptr, &end_iter, FALSE);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(chat_text_view_), end_mark, 0.0, TRUE, 0.0, 1.0);
  gtk_text_buffer_delete_mark(chat_text_buffer_, end_mark);

  // Trim history to prevent unbounded growth
  TrimChatHistory();

  std::cout << "[GtkWindow] Appended chat message from " << role << std::endl;
}

// Helper structure for thread-safe chat message replacement
struct ChatMessageReplaceData {
  GtkWindow* window;
  std::string role;
  std::string message;
};

// GTK idle callback to replace last chat message on main thread
gboolean replace_last_chat_message_idle(gpointer user_data) {
  auto* data = static_cast<ChatMessageReplaceData*>(user_data);

  if (!data || !data->window || data->window->IsClosed() || !data->window->chat_text_buffer_ ||
      !GTK_IS_TEXT_BUFFER(data->window->chat_text_buffer_) || !data->window->chat_text_view_ ||
      !GTK_IS_WIDGET(data->window->chat_text_view_)) {
    delete data;
    return G_SOURCE_REMOVE;
  }

  GtkTextBuffer* buffer = data->window->chat_text_buffer_;

  // Search backwards for the last occurrence of the role prefix
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  std::string prefix = (data->role == "user") ? "You:\n" : "Claude:\n";

  // Find last occurrence of the role prefix
  GtkTextIter match_start, match_end;
  GtkTextIter search_start = end;
  bool found = false;

  while (gtk_text_iter_backward_search(&search_start,
                                       prefix.c_str(),
                                       GTK_TEXT_SEARCH_TEXT_ONLY,
                                       &match_start,
                                       &match_end,
                                       nullptr)) {
    // Found a match - this is the last occurrence
    found = true;

    // Find the end of this message (next role prefix or end of buffer)
    GtkTextIter msg_end = match_end;
    GtkTextIter next_user_start, next_user_end;
    GtkTextIter next_claude_start, next_claude_end;

    bool has_next_user = gtk_text_iter_forward_search(
        &msg_end, "You:\n", GTK_TEXT_SEARCH_TEXT_ONLY, &next_user_start, &next_user_end, nullptr);
    bool has_next_claude = gtk_text_iter_forward_search(&msg_end,
                                                        "Claude:\n",
                                                        GTK_TEXT_SEARCH_TEXT_ONLY,
                                                        &next_claude_start,
                                                        &next_claude_end,
                                                        nullptr);

    // Use the earliest next prefix, or end of buffer
    GtkTextIter content_end = end;
    if (has_next_user && has_next_claude) {
      content_end = gtk_text_iter_compare(&next_user_start, &next_claude_start) < 0
                        ? next_user_start
                        : next_claude_start;
    } else if (has_next_user) {
      content_end = next_user_start;
    } else if (has_next_claude) {
      content_end = next_claude_start;
    }

    // Delete the old message content (everything after the role prefix)
    gtk_text_buffer_delete(buffer, &match_end, &content_end);

    // Insert new message content
    GtkTextIter insert_pos = match_end;
    std::string message_with_newline = data->message + "\n\n";
    gtk_text_buffer_insert_with_tags_by_name(
        buffer, &insert_pos, message_with_newline.c_str(), -1, "message", nullptr);

    // Auto-scroll to bottom
    gtk_text_buffer_get_end_iter(buffer, &end);
    GtkTextMark* end_mark = gtk_text_buffer_create_mark(buffer, nullptr, &end, FALSE);
    gtk_text_view_scroll_to_mark(
        GTK_TEXT_VIEW(data->window->chat_text_view_), end_mark, 0.0, TRUE, 0.0, 1.0);
    gtk_text_buffer_delete_mark(buffer, end_mark);

    break;
  }

  if (!found) {
    std::cerr << "[GtkWindow] Could not find last message from role: " << data->role << std::endl;
  }

  delete data;
  return G_SOURCE_REMOVE;
}

void GtkWindow::ReplaceLastChatMessage(const std::string& role, const std::string& message) {
  // Thread-safe: marshal to GTK main thread using g_idle_add
  auto* data = new ChatMessageReplaceData{this, role, message};
  g_idle_add(replace_last_chat_message_idle, data);
}

void GtkWindow::ClearChatHistory() {
  if (!chat_text_buffer_) {
    std::cerr << "[GtkWindow] Chat text buffer not initialized" << std::endl;
    return;
  }

  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(chat_text_buffer_, &start);
  gtk_text_buffer_get_end_iter(chat_text_buffer_, &end);
  gtk_text_buffer_delete(chat_text_buffer_, &start, &end);

  std::cout << "[GtkWindow] Chat history cleared" << std::endl;
}

void GtkWindow::TrimChatHistory() {
  if (!chat_text_buffer_) {
    return;
  }

  // Count the number of messages by counting "You:\n" and "Claude:\n" prefixes
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(chat_text_buffer_, &start);
  gtk_text_buffer_get_end_iter(chat_text_buffer_, &end);

  gchar* text = gtk_text_buffer_get_text(chat_text_buffer_, &start, &end, FALSE);
  std::string buffer_text(text);
  g_free(text);

  // Count occurrences of "You:\n" and "Claude:\n"
  size_t message_count = 0;
  size_t pos = 0;
  while ((pos = buffer_text.find("You:\n", pos)) != std::string::npos) {
    message_count++;
    pos += 5;  // Length of "You:\n"
  }
  pos = 0;
  while ((pos = buffer_text.find("Claude:\n", pos)) != std::string::npos) {
    message_count++;
    pos += 8;  // Length of "Claude:\n"
  }

  // If we exceed the limit, remove the oldest messages
  if (message_count > MAX_CHAT_MESSAGES) {
    size_t messages_to_remove = message_count - MAX_CHAT_MESSAGES;
    std::cout << "[GtkWindow] Trimming " << messages_to_remove
              << " old messages (total: " << message_count << ")" << std::endl;

    // Find and delete the first N messages
    gtk_text_buffer_get_start_iter(chat_text_buffer_, &start);
    GtkTextIter delete_end = start;

    for (size_t i = 0; i < messages_to_remove; ++i) {
      GtkTextIter user_match_start, user_match_end;
      GtkTextIter claude_match_start, claude_match_end;

      bool found_user = gtk_text_iter_forward_search(&delete_end,
                                                     "You:\n",
                                                     GTK_TEXT_SEARCH_TEXT_ONLY,
                                                     &user_match_start,
                                                     &user_match_end,
                                                     nullptr);
      bool found_claude = gtk_text_iter_forward_search(&delete_end,
                                                       "Claude:\n",
                                                       GTK_TEXT_SEARCH_TEXT_ONLY,
                                                       &claude_match_start,
                                                       &claude_match_end,
                                                       nullptr);

      if (!found_user && !found_claude) {
        break;  // No more messages
      }

      // Use the earliest match
      GtkTextIter next_message_start;
      if (found_user && found_claude) {
        next_message_start = gtk_text_iter_compare(&user_match_start, &claude_match_start) < 0
                                 ? user_match_start
                                 : claude_match_start;
      } else if (found_user) {
        next_message_start = user_match_start;
      } else {
        next_message_start = claude_match_start;
      }

      // Find the next message prefix (or end of buffer)
      delete_end = next_message_start;
      GtkTextIter next_prefix_start, next_prefix_end;

      // Skip past current prefix
      if (found_user && gtk_text_iter_equal(&next_message_start, &user_match_start)) {
        delete_end = user_match_end;
      } else if (found_claude && gtk_text_iter_equal(&next_message_start, &claude_match_start)) {
        delete_end = claude_match_end;
      }

      // Find next prefix
      GtkTextIter next_user_start, next_user_end;
      GtkTextIter next_claude_start, next_claude_end;

      bool has_next_user = gtk_text_iter_forward_search(&delete_end,
                                                        "You:\n",
                                                        GTK_TEXT_SEARCH_TEXT_ONLY,
                                                        &next_user_start,
                                                        &next_user_end,
                                                        nullptr);
      bool has_next_claude = gtk_text_iter_forward_search(&delete_end,
                                                          "Claude:\n",
                                                          GTK_TEXT_SEARCH_TEXT_ONLY,
                                                          &next_claude_start,
                                                          &next_claude_end,
                                                          nullptr);

      if (has_next_user && has_next_claude) {
        delete_end = gtk_text_iter_compare(&next_user_start, &next_claude_start) < 0
                         ? next_user_start
                         : next_claude_start;
      } else if (has_next_user) {
        delete_end = next_user_start;
      } else if (has_next_claude) {
        delete_end = next_claude_start;
      } else {
        // This is the last message, delete to end of buffer
        gtk_text_buffer_get_end_iter(chat_text_buffer_, &delete_end);
      }
    }

    // Delete all messages from start to delete_end
    gtk_text_buffer_get_start_iter(chat_text_buffer_, &start);
    gtk_text_buffer_delete(chat_text_buffer_, &start, &delete_end);

    std::cout << "[GtkWindow] Trimmed chat history" << std::endl;
  }
}

void GtkWindow::OnChatInputActivate() {
  const gchar* text = gtk_entry_get_text(GTK_ENTRY(chat_input_));
  std::string message(text);

  if (!message.empty()) {
    SendClaudeMessage(message);
    gtk_entry_set_text(GTK_ENTRY(chat_input_), "");  // Clear input
  }
}

void GtkWindow::OnChatSendClicked() {
  OnChatInputActivate();  // Reuse the same logic
}

void GtkWindow::OnSidebarToggleClicked() {
  ToggleSidebar();
}

}  // namespace platform
}  // namespace athena

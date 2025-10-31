#ifndef ATHENA_PLATFORM_QT_AGENT_PANEL_H_
#define ATHENA_PLATFORM_QT_AGENT_PANEL_H_

#include "qt_agent_panel_theme.h"
#include "qt_chat_bubble.h"
#include "qt_chat_input_widget.h"
#include "qt_thinking_indicator.h"

#include <deque>
#include <QFrame>
#include <QLocalSocket>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class QEvent;
class QGraphicsDropShadowEffect;
class QPropertyAnimation;

namespace athena {
namespace runtime {
class NodeRuntime;
}

namespace platform {

// Forward declarations
class QtMainWindow;

/**
 * Modern Agent chat panel with beautiful UI.
 *
 * Features:
 * - Chat bubbles with proper styling (user vs assistant)
 * - Markdown rendering support
 * - Code syntax highlighting
 * - Smooth scroll animations
 * - Typing indicators ("Agent is thinking...")
 * - Message history management
 * - Resizable input area
 * - Copy button for messages
 *
 * Design inspired by modern chat interfaces:
 * - Clean, minimalist design
 * - Proper spacing and typography
 * - Subtle shadows and borders
 * - Smooth animations
 */
class AgentPanel : public QWidget {
  Q_OBJECT

 public:
  explicit AgentPanel(QtMainWindow* window, QWidget* parent = nullptr);
  ~AgentPanel() override;

  /**
   * Set the Node.js runtime for API calls.
   * @param runtime Node runtime instance (non-owning)
   */
  void SetNodeRuntime(runtime::NodeRuntime* runtime);

  /**
   * Send a message to the Agent.
   * Shows thinking indicator, makes async call to Athena Agent API.
   * @param message User message text
   */
  void SendMessage(const QString& message);

  /**
   * Clear all chat history.
   */
  void ClearHistory();

  /**
   * Toggle the panel visibility with animation.
   */
  void ToggleVisibility();

  /**
   * Check if panel is visible.
   */
  bool IsVisible() const { return panel_visible_; }

 signals:
  /**
   * Emitted when user sends a message.
   */
  void messageSent(const QString& message);

  /**
   * Emitted when panel visibility changes.
   */
  void visibilityChanged(bool visible);

 private slots:
  void onSendClicked();
  void onStopClicked();
  void onInputTextChanged();

  // Streaming socket handlers
  void onSocketConnected();
  void onSocketReadyRead();
  void onSocketError(QLocalSocket::LocalSocketError error);
  void onSocketDisconnected();

 protected:
  void changeEvent(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  // ============================================================================
  // Setup Methods
  // ============================================================================

  void setupUI();
  void setupStyles();
  void applyPalette();
  void refreshSendStopIcons();
  void applyPaletteToInput();
  void applyPaletteToScrollArea();
  void applyPaletteToButtons();
  void applyPaletteToMessages();
  void applyPaletteToThinkingIndicator();
  void updateActionButtons();
  AgentPanelPalette buildPalette(bool darkMode) const;
  bool detectDarkMode() const;
  void connectSignals();

  // ============================================================================
  // Message Handling
  // ============================================================================

  /**
   * Add a message bubble to the chat.
   * @param role "user" or "assistant"
   * @param message Message text (supports markdown)
   * @param animate Whether to animate the message appearing
   */
  void addMessage(const QString& role, const QString& message, bool animate = true);

  /**
   * Replace the last assistant message (used for streaming or error updates).
   * @param message New message text
   */
  void replaceLastAssistantMessage(const QString& message);

  /**
   * Show or hide the thinking indicator.
   */
  void showThinkingIndicator(bool show);

  /**
   * Scroll to the bottom of the chat area.
   * @param animated Whether to animate the scroll
   */
  void scrollToBottom(bool animated = true);
  void scheduleScrollToBottom(bool animated);
  void flushPendingScroll();
  void performScrollToBottom(bool animated);
  void onScrollValueChanged(int value);
  void onScrollActionTriggered(int action);
  void onScrollSliderPressed();
  void onScrollSliderReleased();
  void updateAutoScrollStateFromPosition();
  bool isNearBottom() const;

  /**
   * Trim old messages if history exceeds max size.
   */
  void trimHistory();

  /**
   * Parse and process SSE (Server-Sent Events) chunks from streaming response.
   * @param data Raw SSE data to parse
   */
  void parseSSEChunks(const QString& data);

  // ============================================================================
  // Member Variables
  // ============================================================================

  QtMainWindow* window_;                // Non-owning
  runtime::NodeRuntime* node_runtime_;  // Non-owning

  // State
  bool panel_visible_;
  static constexpr size_t MAX_MESSAGES = 100;  // Maximum message history

  // UI Components
  QVBoxLayout* mainLayout_;
  QScrollArea* scrollArea_;
  QWidget* messagesContainer_;
  QVBoxLayout* messagesLayout_;

  // Footer / Input area
  QFrame* inputFrame_;
  QFrame* inputCard_;
  ChatInputWidget* inputWidget_;
  QPushButton* sendButton_;
  QPushButton* stopButton_;
  QGraphicsDropShadowEffect* inputShadow_;

  // Thinking indicator
  ThinkingIndicator* thinkingIndicator_;

  // Message tracking
  std::deque<ChatBubble*> messageBubbles_;
  bool waiting_for_response_;
  bool userCanceledResponse_;

  // Streaming HTTP connection
  QLocalSocket* streaming_socket_;
  QString response_buffer_;   // Buffers incomplete HTTP data
  QString accumulated_text_;  // Accumulated SSE content for current response
  bool headers_received_;     // Track if HTTP headers have been parsed

  // Session management
  QString current_session_id_;  // Current agent session ID for continuity

  // Theming
  AgentPanelPalette palette_;

  // Scroll management state
  bool autoScrollEnabled_;
  bool suppressScrollEvents_;
  bool pendingScrollToBottom_;
  bool pendingScrollAnimated_;
  QPointer<QPropertyAnimation> scrollAnimation_;

  static constexpr int kAutoScrollLockThresholdPx = 72;
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_AGENT_PANEL_H_

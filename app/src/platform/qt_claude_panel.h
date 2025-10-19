#ifndef ATHENA_PLATFORM_QT_CLAUDE_PANEL_H_
#define ATHENA_PLATFORM_QT_CLAUDE_PANEL_H_

#include <deque>
#include <memory>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace athena {
namespace runtime {
class NodeRuntime;
}

namespace platform {

// Forward declarations
class QtMainWindow;
class ChatBubble;
class ChatInputWidget;
class ThinkingIndicator;

/**
 * Modern Claude chat panel with beautiful UI.
 *
 * Features:
 * - Chat bubbles with proper styling (user vs assistant)
 * - Markdown rendering support
 * - Code syntax highlighting
 * - Smooth scroll animations
 * - Typing indicators ("Claude is thinking...")
 * - Message history management
 * - Resizable input area
 * - Copy button for messages
 * - Regenerate button for last response
 *
 * Design inspired by Claude.ai's interface:
 * - Clean, minimalist design
 * - Proper spacing and typography
 * - Subtle shadows and borders
 * - Smooth animations
 */
class ClaudePanel : public QWidget {
  Q_OBJECT

 public:
  explicit ClaudePanel(QtMainWindow* window, QWidget* parent = nullptr);
  ~ClaudePanel() override;

  /**
   * Set the Node.js runtime for API calls.
   * @param runtime Node runtime instance (non-owning)
   */
  void SetNodeRuntime(runtime::NodeRuntime* runtime);

  /**
   * Send a message to Claude.
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
  void onClearClicked();
  void onInputTextChanged();
  void onRegenerateClicked();

 private:
  // ============================================================================
  // Setup Methods
  // ============================================================================

  void setupUI();
  void setupStyles();
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

  /**
   * Trim old messages if history exceeds max size.
   */
  void trimHistory();

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

  // Header
  QFrame* headerFrame_;
  QLabel* headerLabel_;
  QPushButton* clearButton_;

  // Footer / Input area
  QFrame* inputFrame_;
  ChatInputWidget* inputWidget_;
  QPushButton* sendButton_;
  QPushButton* regenerateButton_;

  // Thinking indicator
  ThinkingIndicator* thinkingIndicator_;

  // Message tracking
  std::deque<ChatBubble*> messageBubbles_;
  bool waiting_for_response_;
};

/**
 * Custom chat input widget with multiline support.
 *
 * Features:
 * - Auto-expanding height (1-5 lines)
 * - Enter to send, Shift+Enter for newline
 * - Placeholder text
 * - Focus styling
 */
class ChatInputWidget : public QTextEdit {
  Q_OBJECT

 public:
  explicit ChatInputWidget(QWidget* parent = nullptr);

  /**
   * Get the input text.
   */
  QString GetText() const;

  /**
   * Clear the input text.
   */
  void Clear();

 signals:
  /**
   * Emitted when user presses Enter (without Shift).
   */
  void sendRequested();

 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;

 private slots:
  void adjustHeight();

 private:
  void setupUI();
  int calculateIdealHeight();

  static constexpr int MIN_HEIGHT = 40;
  static constexpr int MAX_HEIGHT = 120;
};

/**
 * Chat message bubble with role-specific styling.
 *
 * Features:
 * - Different colors for user vs assistant
 * - Markdown rendering
 * - Code syntax highlighting
 * - Copy button
 * - Smooth fade-in animation
 */
class ChatBubble : public QFrame {
  Q_OBJECT

 public:
  enum class Role { User, Assistant };

  explicit ChatBubble(Role role, const QString& message, QWidget* parent = nullptr);

  /**
   * Update the message content.
   * Used for replacing thinking indicator with actual response.
   */
  void SetMessage(const QString& message);

  /**
   * Get the message content.
   */
  QString GetMessage() const;

  /**
   * Get the role.
   */
  Role GetRole() const { return role_; }

  /**
   * Animate the bubble appearing.
   */
  void AnimateIn();

 private slots:
  void onCopyClicked();

 private:
  void setupUI();
  void setupStyles();
  void renderMarkdown(const QString& markdown);

  Role role_;
  QString message_;

  // UI Components
  QVBoxLayout* layout_;
  QLabel* roleLabel_;         // "You" or "Claude"
  QTextEdit* contentWidget_;  // Message content (read-only)
  QPushButton* copyButton_;   // Copy message button

  // Animation
  QGraphicsOpacityEffect* opacityEffect_;
  QPropertyAnimation* fadeInAnimation_;
};

/**
 * Animated thinking indicator ("Claude is thinking...").
 *
 * Shows animated dots while waiting for Claude's response.
 */
class ThinkingIndicator : public QWidget {
  Q_OBJECT

 public:
  explicit ThinkingIndicator(QWidget* parent = nullptr);

  /**
   * Start the animation.
   */
  void Start();

  /**
   * Stop the animation.
   */
  void Stop();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private slots:
  void updateAnimation();

 private:
  QTimer* animationTimer_;
  int animationFrame_;
  QString text_;
  static constexpr int ANIMATION_FRAMES = 4;  // ".", "..", "...", "...."
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_CLAUDE_PANEL_H_

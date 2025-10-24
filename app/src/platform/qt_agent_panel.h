#ifndef ATHENA_PLATFORM_QT_AGENT_PANEL_H_
#define ATHENA_PLATFORM_QT_AGENT_PANEL_H_

#include <deque>
#include <memory>
#include <QColor>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLineEdit>
#include <QLocalSocket>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class QEvent;

namespace athena {
namespace runtime {
class NodeRuntime;
}

namespace platform {

struct ScrollbarPalette {
  QColor track;
  QColor thumb;
  QColor thumbHover;
};

struct BubblePalette {
  QColor background;
  QColor text;
  QColor label;
  QColor codeBackground;
  QColor codeText;
};

struct InputPalette {
  QColor background;
  QColor border;
  QColor borderFocused;
  QColor text;
  QColor placeholder;
  QColor caret;
};

struct IconButtonPalette {
  QColor background;
  QColor backgroundHover;
  QColor backgroundPressed;
  QColor backgroundDisabled;
  QColor icon;
  QColor iconDisabled;
};

struct ChipPalette {
  QColor background;
  QColor text;
  QColor border;
};

struct AgentPanelPalette {
  bool dark = false;
  QColor panelBackground;
  QColor panelBorder;
  QColor messageAreaBackground;
  QColor composerBackground;
  QColor composerBorder;
  QColor composerShadow;
  QColor keyboardHintText;
  QColor thinkingBackground;
  QColor thinkingText;
  QColor secondaryText;
  QColor accent;

  ScrollbarPalette scrollbar;
  BubblePalette userBubble;
  BubblePalette assistantBubble;
  InputPalette input;
  IconButtonPalette sendButton;
  IconButtonPalette stopButton;
  ChipPalette chip;
};

// Forward declarations
class QtMainWindow;
class ChatBubble;
class ChatInputWidget;
class ThinkingIndicator;

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

  /**
   * Apply themed styling.
   */
  void ApplyTheme(const AgentPanelPalette& palette);

 signals:
  /**
   * Emitted when user presses Enter (without Shift).
   */
  void sendRequested();

  /**
   * Emitted when focus state changes.
   */
  void focusChanged(bool focused);

 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;

 private slots:
  void adjustHeight();

 private:
  void setupUI();
  int calculateIdealHeight();
  void applyPalette(const AgentPanelPalette& palette);

  static constexpr int MIN_HEIGHT = 40;
  static constexpr int MAX_HEIGHT = 120;
  AgentPanelPalette currentPalette_;
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

  explicit ChatBubble(Role role,
                      const QString& message,
                      const AgentPanelPalette& palette,
                      QWidget* parent = nullptr);

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

  /**
   * Apply theme colors to the bubble.
   */
 void ApplyTheme(const AgentPanelPalette& palette);

 private:
  void setupUI();
  void renderMarkdown(const QString& markdown);
  void applyPalette(const AgentPanelPalette& palette);

  Role role_;
  QString message_;
  BubblePalette bubblePalette_;

  // UI Components
  QVBoxLayout* layout_;
  QLabel* roleLabel_;         // "You" or "Agent"
  QTextEdit* contentWidget_;  // Message content (read-only)

  // Animation
  QGraphicsOpacityEffect* opacityEffect_;
  QPropertyAnimation* fadeInAnimation_;
};

/**
 * Animated thinking indicator ("Agent is thinking...").
 *
 * Shows animated dots while waiting for Agent's response.
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

  /**
   * Apply theme styling.
   */
  void ApplyTheme(const AgentPanelPalette& palette);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private slots:
  void updateAnimation();

 private:
  QTimer* animationTimer_;
  int animationFrame_;
  QString text_;
  static constexpr int ANIMATION_FRAMES = 4;  // ".", "..", "...", "...."
  QColor textColor_;
  QColor backgroundColor_;
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_AGENT_PANEL_H_

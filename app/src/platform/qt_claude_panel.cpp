#include "qt_claude_panel.h"

#include "qt_mainwindow.h"
#include "runtime/node_runtime.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QThread>
#include <thread>

namespace athena {
namespace platform {

// ============================================================================
// ClaudePanel Implementation
// ============================================================================

ClaudePanel::ClaudePanel(QtMainWindow* window, QWidget* parent)
    : QWidget(parent),
      window_(window),
      node_runtime_(nullptr),
      panel_visible_(true),
      waiting_for_response_(false) {
  setupUI();
  setupStyles();
  connectSignals();
}

ClaudePanel::~ClaudePanel() = default;

void ClaudePanel::setupUI() {
  mainLayout_ = new QVBoxLayout(this);
  mainLayout_->setContentsMargins(0, 0, 0, 0);
  mainLayout_->setSpacing(0);

  // ============================================================================
  // Header
  // ============================================================================

  headerFrame_ = new QFrame(this);
  auto* headerLayout = new QHBoxLayout(headerFrame_);
  headerLayout->setContentsMargins(16, 12, 16, 12);

  // Claude logo/title
  headerLabel_ = new QLabel("Claude", headerFrame_);
  QFont headerFont = headerLabel_->font();
  headerFont.setPointSize(14);
  headerFont.setBold(true);
  headerLabel_->setFont(headerFont);

  // Clear button
  clearButton_ = new QPushButton("Clear", headerFrame_);
  clearButton_->setCursor(Qt::PointingHandCursor);

  headerLayout->addWidget(headerLabel_);
  headerLayout->addStretch();
  headerLayout->addWidget(clearButton_);

  mainLayout_->addWidget(headerFrame_);

  // ============================================================================
  // Messages Area (Scrollable)
  // ============================================================================

  scrollArea_ = new QScrollArea(this);
  scrollArea_->setWidgetResizable(true);
  scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollArea_->setFrameShape(QFrame::NoFrame);  // Remove frame border

  messagesContainer_ = new QWidget();
  messagesContainer_->setStyleSheet("background-color: #FFFFFF;");  // Ensure white background
  messagesLayout_ = new QVBoxLayout(messagesContainer_);
  messagesLayout_->setContentsMargins(20, 20, 20, 20);  // Increased padding
  messagesLayout_->setSpacing(20);                      // Increased spacing between messages
  messagesLayout_->addStretch();                        // Push messages to top

  scrollArea_->setWidget(messagesContainer_);
  mainLayout_->addWidget(scrollArea_, 1);  // Stretch factor 1

  // ============================================================================
  // Thinking Indicator
  // ============================================================================

  thinkingIndicator_ = new ThinkingIndicator(messagesContainer_);
  thinkingIndicator_->hide();
  messagesLayout_->insertWidget(messagesLayout_->count() - 1, thinkingIndicator_);

  // ============================================================================
  // Input Area (Footer)
  // ============================================================================

  inputFrame_ = new QFrame(this);
  auto* inputLayout = new QVBoxLayout(inputFrame_);
  inputLayout->setContentsMargins(16, 12, 16, 12);
  inputLayout->setSpacing(8);

  // Input text area + send button row
  auto* inputRowWidget = new QWidget(inputFrame_);
  auto* inputRowLayout = new QHBoxLayout(inputRowWidget);
  inputRowLayout->setContentsMargins(0, 0, 0, 0);
  inputRowLayout->setSpacing(8);

  inputWidget_ = new ChatInputWidget(inputRowWidget);
  inputWidget_->setPlaceholderText("Message Claude...");

  sendButton_ = new QPushButton("Send", inputRowWidget);
  sendButton_->setCursor(Qt::PointingHandCursor);
  sendButton_->setEnabled(false);  // Disabled until text is entered

  inputRowLayout->addWidget(inputWidget_, 1);
  inputRowLayout->addWidget(sendButton_);

  inputLayout->addWidget(inputRowWidget);

  // Regenerate button (hidden by default)
  regenerateButton_ = new QPushButton("↻ Regenerate", inputFrame_);
  regenerateButton_->setCursor(Qt::PointingHandCursor);
  regenerateButton_->hide();
  inputLayout->addWidget(regenerateButton_);

  mainLayout_->addWidget(inputFrame_);

  // Set size policy
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  setMinimumWidth(300);
}

void ClaudePanel::setupStyles() {
  // Force light theme colors globally for the panel
  // This overrides any dark theme that might be applied by the system
  setStyleSheet(R"(
    ClaudePanel {
      background-color: #FFFFFF;
      border-left: 1px solid #E5E7EB;
    }
    ClaudePanel * {
      color: #111827;  /* Force dark text on all children */
    }
  )");

  // Header styling - Enhanced with better visual separation
  headerFrame_->setStyleSheet(R"(
    QFrame {
      background-color: #F9FAFB;
      border-bottom: 1px solid #E5E7EB;
    }
    QFrame QLabel {
      color: #111827;
      font-weight: 600;
      background-color: transparent;
    }
    QFrame QPushButton {
      background-color: transparent;
      border: 1px solid #D1D5DB;
      border-radius: 4px;
      padding: 4px 12px;
      color: #374151;
      font-size: 12px;
      font-weight: 500;
    }
    QFrame QPushButton:hover {
      background-color: #F3F4F6;
      border-color: #9CA3AF;
    }
    QFrame QPushButton:pressed {
      background-color: #E5E7EB;
      border-color: #6B7280;
    }
  )");

  // Scroll area styling - Fix scrollbar issues
  scrollArea_->setStyleSheet(R"(
    QScrollArea {
      border: none;
      background-color: #FFFFFF;
    }
    QScrollBar:vertical {
      background: #F9FAFB;
      width: 12px;
      border: none;
      margin: 0px;
    }
    QScrollBar::handle:vertical {
      background: #D1D5DB;
      border-radius: 6px;
      min-height: 30px;
      margin: 2px;
    }
    QScrollBar::handle:vertical:hover {
      background: #9CA3AF;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      height: 0px;
      border: none;
    }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
      background: none;
    }
  )");

  // Input area styling - Enhanced footer
  inputFrame_->setStyleSheet(R"(
    QFrame {
      background-color: #FFFFFF;
      border-top: 1px solid #E5E7EB;
      padding: 0px;
    }
  )");

  // Send button styling
  sendButton_->setStyleSheet(R"(
    QPushButton {
      background-color: #2563EB;
      color: white;
      border: none;
      border-radius: 6px;
      padding: 8px 16px;
      font-weight: 500;
      font-size: 13px;
    }
    QPushButton:hover:enabled {
      background-color: #1D4ED8;
    }
    QPushButton:pressed:enabled {
      background-color: #1E40AF;
    }
    QPushButton:disabled {
      background-color: #9CA3AF;
      color: #E5E7EB;
    }
  )");

  // Regenerate button styling
  regenerateButton_->setStyleSheet(R"(
    QPushButton {
      background-color: transparent;
      border: 1px solid #D1D5DB;
      border-radius: 4px;
      padding: 6px 12px;
      color: #374151;
      font-size: 12px;
    }
    QPushButton:hover {
      background-color: #F3F4F6;
      border-color: #9CA3AF;
    }
    QPushButton:pressed {
      background-color: #E5E7EB;
    }
  )");
}

void ClaudePanel::connectSignals() {
  connect(sendButton_, &QPushButton::clicked, this, &ClaudePanel::onSendClicked);

  connect(clearButton_, &QPushButton::clicked, this, &ClaudePanel::onClearClicked);

  connect(regenerateButton_, &QPushButton::clicked, this, &ClaudePanel::onRegenerateClicked);

  connect(inputWidget_, &ChatInputWidget::sendRequested, this, &ClaudePanel::onSendClicked);

  connect(inputWidget_, &QTextEdit::textChanged, this, &ClaudePanel::onInputTextChanged);
}

void ClaudePanel::SetNodeRuntime(runtime::NodeRuntime* runtime) {
  node_runtime_ = runtime;
}

void ClaudePanel::SendMessage(const QString& message) {
  if (message.trimmed().isEmpty()) {
    return;
  }

  if (waiting_for_response_) {
    qWarning() << "[ClaudePanel] Already waiting for response";
    return;
  }

  // Add user message
  addMessage("user", message, true);

  // Show thinking indicator
  showThinkingIndicator(true);
  waiting_for_response_ = true;
  regenerateButton_->hide();

  // Check if node runtime is available
  if (!node_runtime_ || !node_runtime_->IsReady()) {
    qWarning() << "[ClaudePanel] Node runtime not available";
    showThinkingIndicator(false);
    addMessage("assistant",
               "❌ **Error:** Claude Agent is not available. Please ensure the Node.js runtime is "
               "running.",
               true);
    waiting_for_response_ = false;
    return;
  }

  qDebug() << "[ClaudePanel] Sending message to Claude:" << message;

  // Add an empty assistant message bubble that we'll update as chunks arrive
  addMessage("assistant", "", false);

  // Launch background thread for API call
  std::string message_copy = message.toStdString();
  runtime::NodeRuntime* node_runtime = node_runtime_;

  std::thread([this, message_copy, node_runtime]() {
    // Build JSON request
    std::string escaped_message = message_copy;
    size_t pos = 0;
    while ((pos = escaped_message.find("\"", pos)) != std::string::npos) {
      escaped_message.replace(pos, 1, "\\\"");
      pos += 2;
    }
    // Escape newlines
    pos = 0;
    while ((pos = escaped_message.find("\n", pos)) != std::string::npos) {
      escaped_message.replace(pos, 1, "\\n");
      pos += 2;
    }

    std::string json_body = "{\"message\":\"" + escaped_message + "\"}";

    // Call the Athena Agent streaming API
    auto response = node_runtime->Call("POST", "/v1/chat/stream", json_body);

    // Parse SSE streaming response
    if (!response.IsOk()) {
      QMetaObject::invokeMethod(
          this,
          [this, response]() {
            showThinkingIndicator(false);
            waiting_for_response_ = false;
            QString error_msg =
                QString::fromStdString("❌ **Error:** Failed to communicate with Claude Agent: " +
                                       response.GetError().Message());
            replaceLastAssistantMessage(error_msg);
          },
          Qt::QueuedConnection);
      return;
    }

    // Parse SSE events from response body
    std::string response_body = response.Value();
    qDebug() << "[ClaudePanel] SSE stream received (length=" << response_body.length() << ")";
    qDebug() << "[ClaudePanel] Raw SSE data:" << QString::fromStdString(response_body);

    std::string accumulated_text;
    bool had_error = false;
    std::string error_message;

    // Parse SSE format: lines starting with "data: " contain JSON chunks
    size_t parse_pos = 0;
    int chunk_count = 0;
    while (parse_pos < response_body.length()) {
      // Find "data: " prefix
      size_t data_start = response_body.find("data: ", parse_pos);
      if (data_start == std::string::npos)
        break;

      data_start += 6;  // Skip "data: "
      size_t data_end = response_body.find("\n", data_start);
      if (data_end == std::string::npos)
        data_end = response_body.length();

      std::string json_line = response_body.substr(data_start, data_end - data_start);
      parse_pos = data_end + 1;

      qDebug() << "[ClaudePanel] Parsing chunk" << ++chunk_count << ":"
               << QString::fromStdString(json_line);

      // Parse JSON chunk
      // Format: {"type":"chunk","content":"text"} or {"type":"done"} or
      // {"type":"error","error":"msg"}
      size_t type_pos = json_line.find("\"type\":\"");
      if (type_pos == std::string::npos)
        continue;

      size_t type_start = type_pos + 8;
      size_t type_end = json_line.find("\"", type_start);
      std::string chunk_type = json_line.substr(type_start, type_end - type_start);

      if (chunk_type == "chunk") {
        qDebug() << "[ClaudePanel] Found chunk type, extracting content...";
        // Extract content field
        size_t content_pos = json_line.find("\"content\":\"");
        if (content_pos != std::string::npos) {
          size_t content_start = content_pos + 11;
          size_t content_end = content_start;
          int escape_count = 0;

          // Find the end of the content string (handling escaped quotes)
          while (content_end < json_line.length()) {
            if (json_line[content_end] == '\\') {
              escape_count++;
              content_end++;
              continue;
            }
            if (json_line[content_end] == '\"' && escape_count % 2 == 0) {
              break;
            }
            escape_count = 0;
            content_end++;
          }

          std::string chunk_content = json_line.substr(content_start, content_end - content_start);

          qDebug() << "[ClaudePanel] Extracted content (escaped):"
                   << QString::fromStdString(chunk_content);

          // Unescape JSON sequences
          size_t esc_pos = 0;
          while ((esc_pos = chunk_content.find("\\n", esc_pos)) != std::string::npos) {
            chunk_content.replace(esc_pos, 2, "\n");
            esc_pos += 1;
          }
          esc_pos = 0;
          while ((esc_pos = chunk_content.find("\\\"", esc_pos)) != std::string::npos) {
            chunk_content.replace(esc_pos, 2, "\"");
            esc_pos += 1;
          }
          esc_pos = 0;
          while ((esc_pos = chunk_content.find("\\\\", esc_pos)) != std::string::npos) {
            chunk_content.replace(esc_pos, 2, "\\");
            esc_pos += 1;
          }

          accumulated_text += chunk_content;

          qDebug() << "[ClaudePanel] Accumulated text length:" << accumulated_text.length();

          // Update UI with accumulated text so far
          QMetaObject::invokeMethod(
              this,
              [this, accumulated_text]() {
                qDebug() << "[ClaudePanel] Updating UI with text (length="
                         << accumulated_text.length() << ")";
                replaceLastAssistantMessage(QString::fromStdString(accumulated_text));
              },
              Qt::QueuedConnection);
        } else {
          qDebug() << "[ClaudePanel] ERROR: content field not found in chunk!";
        }
      } else if (chunk_type == "error") {
        // Extract error message
        size_t error_pos = json_line.find("\"error\":\"");
        if (error_pos != std::string::npos) {
          size_t err_start = error_pos + 9;
          size_t err_end = json_line.find("\"", err_start);
          error_message = json_line.substr(err_start, err_end - err_start);
          had_error = true;
        }
      } else if (chunk_type == "done") {
        // Streaming complete
        qDebug() << "[ClaudePanel] Streaming complete";
      }
    }

    // Final UI update
    QMetaObject::invokeMethod(
        this,
        [this, accumulated_text, had_error, error_message]() {
          showThinkingIndicator(false);
          waiting_for_response_ = false;

          if (had_error) {
            QString error_msg = QString::fromStdString("❌ **Error:** " + error_message);
            replaceLastAssistantMessage(error_msg);
          } else if (!accumulated_text.empty()) {
            replaceLastAssistantMessage(QString::fromStdString(accumulated_text));
          } else {
            replaceLastAssistantMessage("❌ **Error:** No response received from Claude");
          }

          // Show regenerate button
          regenerateButton_->show();
        },
        Qt::QueuedConnection);
  }).detach();
}

void ClaudePanel::addMessage(const QString& role, const QString& message, bool animate) {
  ChatBubble::Role bubbleRole =
      (role == "user") ? ChatBubble::Role::User : ChatBubble::Role::Assistant;

  auto* bubble = new ChatBubble(bubbleRole, message, messagesContainer_);
  messageBubbles_.push_back(bubble);

  // Insert before the stretch spacer
  messagesLayout_->insertWidget(messagesLayout_->count() - 1, bubble);

  if (animate) {
    bubble->AnimateIn();
  }

  scrollToBottom(animate);

  // Trim history if needed
  trimHistory();
}

void ClaudePanel::replaceLastAssistantMessage(const QString& message) {
  qDebug() << "[ClaudePanel::replaceLastAssistantMessage] Called with message length:"
           << message.length();
  qDebug() << "[ClaudePanel::replaceLastAssistantMessage] Message preview:" << message.left(100);

  // Find the last assistant message
  for (auto it = messageBubbles_.rbegin(); it != messageBubbles_.rend(); ++it) {
    if ((*it)->GetRole() == ChatBubble::Role::Assistant) {
      qDebug()
          << "[ClaudePanel::replaceLastAssistantMessage] Found assistant bubble, updating message";
      (*it)->SetMessage(message);
      scrollToBottom(true);
      return;
    }
  }

  // If no assistant message found, add a new one
  qDebug()
      << "[ClaudePanel::replaceLastAssistantMessage] No assistant bubble found, adding new one";
  addMessage("assistant", message, true);
}

void ClaudePanel::showThinkingIndicator(bool show) {
  if (show) {
    thinkingIndicator_->show();
    thinkingIndicator_->Start();
  } else {
    thinkingIndicator_->Stop();
    thinkingIndicator_->hide();
  }
  scrollToBottom(true);
}

void ClaudePanel::scrollToBottom(bool animated) {
  QScrollBar* scrollBar = scrollArea_->verticalScrollBar();

  if (animated) {
    // Smooth scroll animation
    int targetValue = scrollBar->maximum();
    int currentValue = scrollBar->value();

    if (targetValue != currentValue) {
      auto* animation = new QPropertyAnimation(scrollBar, "value", this);
      animation->setDuration(300);
      animation->setStartValue(currentValue);
      animation->setEndValue(targetValue);
      animation->setEasingCurve(QEasingCurve::OutCubic);
      animation->start(QPropertyAnimation::DeleteWhenStopped);
    }
  } else {
    scrollBar->setValue(scrollBar->maximum());
  }
}

void ClaudePanel::trimHistory() {
  while (messageBubbles_.size() > MAX_MESSAGES) {
    auto* bubble = messageBubbles_.front();
    messageBubbles_.pop_front();
    messagesLayout_->removeWidget(bubble);
    bubble->deleteLater();
  }
}

void ClaudePanel::ClearHistory() {
  // Remove all message bubbles
  for (auto* bubble : messageBubbles_) {
    messagesLayout_->removeWidget(bubble);
    bubble->deleteLater();
  }
  messageBubbles_.clear();

  regenerateButton_->hide();

  qDebug() << "[ClaudePanel] Chat history cleared";
}

void ClaudePanel::ToggleVisibility() {
  panel_visible_ = !panel_visible_;
  setVisible(panel_visible_);
  emit visibilityChanged(panel_visible_);

  if (panel_visible_) {
    inputWidget_->setFocus();
  }
}

void ClaudePanel::onSendClicked() {
  QString message = inputWidget_->GetText().trimmed();
  if (!message.isEmpty()) {
    SendMessage(message);
    inputWidget_->Clear();
  }
}

void ClaudePanel::onClearClicked() {
  ClearHistory();
}

void ClaudePanel::onInputTextChanged() {
  bool hasText = !inputWidget_->toPlainText().trimmed().isEmpty();
  sendButton_->setEnabled(hasText);
}

void ClaudePanel::onRegenerateClicked() {
  // Find the last user message and resend it
  for (auto it = messageBubbles_.rbegin(); it != messageBubbles_.rend(); ++it) {
    if ((*it)->GetRole() == ChatBubble::Role::User) {
      QString lastUserMessage = (*it)->GetMessage();

      // Remove the last assistant message if it exists
      for (auto it2 = messageBubbles_.rbegin(); it2 != messageBubbles_.rend(); ++it2) {
        if ((*it2)->GetRole() == ChatBubble::Role::Assistant) {
          messagesLayout_->removeWidget(*it2);
          (*it2)->deleteLater();
          messageBubbles_.erase(std::next(it2).base());
          break;
        }
      }

      // Resend the message
      SendMessage(lastUserMessage);
      return;
    }
  }
}

// ============================================================================
// ChatInputWidget Implementation
// ============================================================================

ChatInputWidget::ChatInputWidget(QWidget* parent) : QTextEdit(parent) {
  setupUI();
  connect(this, &QTextEdit::textChanged, this, &ChatInputWidget::adjustHeight);
}

void ChatInputWidget::setupUI() {
  setAcceptRichText(false);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  // Styling
  setStyleSheet(R"(
    QTextEdit {
      background-color: #FFFFFF;
      border: 1px solid #D1D5DB;
      border-radius: 6px;
      padding: 10px 14px;
      font-size: 14px;
      color: #111827;
    }
    QTextEdit:focus {
      border: 2px solid #2563EB;
      padding: 9px 13px;
    }
  )");

  setMinimumHeight(MIN_HEIGHT);
  setMaximumHeight(MAX_HEIGHT);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  adjustHeight();
}

void ChatInputWidget::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    if (event->modifiers() & Qt::ShiftModifier) {
      // Shift+Enter: Insert newline
      QTextEdit::keyPressEvent(event);
    } else {
      // Enter: Send message
      emit sendRequested();
      event->accept();
      return;
    }
  } else {
    QTextEdit::keyPressEvent(event);
  }
}

void ChatInputWidget::focusInEvent(QFocusEvent* event) {
  QTextEdit::focusInEvent(event);
}

void ChatInputWidget::focusOutEvent(QFocusEvent* event) {
  QTextEdit::focusOutEvent(event);
}

void ChatInputWidget::adjustHeight() {
  int idealHeight = calculateIdealHeight();
  setFixedHeight(idealHeight);
}

int ChatInputWidget::calculateIdealHeight() {
  QFontMetrics fm(font());
  int lineHeight = fm.lineSpacing();
  int lines = document()->lineCount();

  // Add padding (8px top + 8px bottom + 2px for border)
  int padding = 18;
  int height = (lines * lineHeight) + padding;

  // Clamp between min and max
  return qBound(MIN_HEIGHT, height, MAX_HEIGHT);
}

QString ChatInputWidget::GetText() const {
  return toPlainText();
}

void ChatInputWidget::Clear() {
  clear();
}

// ============================================================================
// ChatBubble Implementation
// ============================================================================

ChatBubble::ChatBubble(Role role, const QString& message, QWidget* parent)
    : QFrame(parent), role_(role), message_(message) {
  setupUI();
  setupStyles();
  renderMarkdown(message);

  // Setup fade-in animation
  opacityEffect_ = new QGraphicsOpacityEffect(this);
  opacityEffect_->setOpacity(0.0);
  setGraphicsEffect(opacityEffect_);

  fadeInAnimation_ = new QPropertyAnimation(opacityEffect_, "opacity", this);
  fadeInAnimation_->setDuration(200);
  fadeInAnimation_->setStartValue(0.0);
  fadeInAnimation_->setEndValue(1.0);
  fadeInAnimation_->setEasingCurve(QEasingCurve::OutCubic);
}

void ChatBubble::setupUI() {
  layout_ = new QVBoxLayout(this);
  layout_->setContentsMargins(16, 12, 16, 12);
  layout_->setSpacing(12);

  // Role label
  roleLabel_ = new QLabel(this);
  QFont labelFont = roleLabel_->font();
  labelFont.setPointSize(11);
  labelFont.setBold(true);
  roleLabel_->setFont(labelFont);
  roleLabel_->setText(role_ == Role::User ? "You" : "Claude");

  layout_->addWidget(roleLabel_);

  // Content widget (read-only text edit for rich text support)
  contentWidget_ = new QTextEdit(this);
  contentWidget_->setReadOnly(true);
  contentWidget_->setFrameShape(QFrame::NoFrame);
  contentWidget_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  contentWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  contentWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  // Set explicit font size for consistent rendering
  QFont contentFont = contentWidget_->font();
  contentFont.setPointSize(13);
  contentWidget_->setFont(contentFont);

  // Enable word wrapping
  contentWidget_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  contentWidget_->setLineWrapMode(QTextEdit::WidgetWidth);

  layout_->addWidget(contentWidget_);

  // Copy button
  copyButton_ = new QPushButton("Copy", this);
  copyButton_->setCursor(Qt::PointingHandCursor);
  copyButton_->setFixedSize(70, 28);  // Fixed size for better visibility

  connect(copyButton_, &QPushButton::clicked, this, &ChatBubble::onCopyClicked);

  layout_->addWidget(copyButton_, 0, Qt::AlignRight);

  setFrameShape(QFrame::StyledPanel);
}

void ChatBubble::setupStyles() {
  if (role_ == Role::User) {
    // User bubble: Blue accent with better contrast
    setStyleSheet(R"(
      ChatBubble {
        background-color: #EFF6FF;
        border: 1px solid #BFDBFE;
        border-radius: 8px;
      }
      ChatBubble QLabel {
        color: #1E40AF;
        background-color: transparent;
        font-size: 11pt;
        font-weight: bold;
      }
      ChatBubble QTextEdit {
        background-color: transparent;
        color: #1E3A8A;
        font-size: 13pt;
        selection-background-color: #2563EB;
        selection-color: white;
      }
      ChatBubble QPushButton {
        background-color: #2563EB;
        border: 1px solid #1D4ED8;
        border-radius: 4px;
        padding: 4px 8px;
        color: white;
        font-size: 11pt;
        font-weight: 600;
      }
      ChatBubble QPushButton:hover {
        background-color: #1D4ED8;
      }
      ChatBubble QPushButton:pressed {
        background-color: #1E40AF;
      }
    )");
  } else {
    // Assistant bubble: Gray/neutral with better contrast
    setStyleSheet(R"(
      ChatBubble {
        background-color: #F9FAFB;
        border: 1px solid #D1D5DB;
        border-radius: 8px;
      }
      ChatBubble QLabel {
        color: #374151;
        background-color: transparent;
        font-size: 11pt;
        font-weight: bold;
      }
      ChatBubble QTextEdit {
        background-color: transparent;
        color: #111827;
        font-size: 13pt;
        selection-background-color: #2563EB;
        selection-color: white;
      }
      ChatBubble QPushButton {
        background-color: #6B7280;
        border: 1px solid #4B5563;
        border-radius: 4px;
        padding: 4px 8px;
        color: white;
        font-size: 11pt;
        font-weight: 600;
      }
      ChatBubble QPushButton:hover {
        background-color: #4B5563;
      }
      ChatBubble QPushButton:pressed {
        background-color: #374151;
      }
    )");
  }
}

void ChatBubble::renderMarkdown(const QString& markdown) {
  // Simple markdown rendering with explicit text color
  // For now, just use plain text with basic formatting
  // TODO: Implement proper markdown parser with code highlighting

  QString html = markdown;

  // Escape HTML special characters first
  html.replace("&", "&amp;");
  html.replace("<", "&lt;");
  html.replace(">", "&gt;");

  // Bold: **text** -> <b>text</b>
  html.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "<b>\\1</b>");

  // Italic: *text* -> <i>text</i>
  html.replace(QRegularExpression("(?<!\\*)\\*([^*]+)\\*(?!\\*)"), "<i>\\1</i>");

  // Code: `code` -> <code>code</code>
  html.replace(QRegularExpression("`([^`]+)`"),
               "<code style='background-color: #F3F4F6; padding: 2px 4px; border-radius: 3px; "
               "font-family: monospace;'>\\1</code>");

  // Preserve newlines
  html.replace("\n", "<br>");

  // Wrap in HTML with explicit text color based on role
  QString textColor = (role_ == Role::User) ? "#1E3A8A" : "#111827";
  QString fullHtml = QString("<!DOCTYPE html>"
                             "<html>"
                             "<head>"
                             "<style>"
                             "body { "
                             "  color: %1; "
                             "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', "
                             "Roboto, 'Helvetica Neue', Arial, sans-serif; "
                             "  font-size: 14px; "
                             "  line-height: 1.6; "
                             "  margin: 4px; "
                             "  padding: 0; "
                             "  word-wrap: break-word; "
                             "  overflow-wrap: break-word; "
                             "}"
                             "p { margin: 0 0 8px 0; }"
                             "code { font-size: 13px; }"
                             "</style>"
                             "</head>"
                             "<body>%2</body>"
                             "</html>")
                         .arg(textColor, html);

  contentWidget_->setHtml(fullHtml);

  // Adjust height to fit content properly
  // Use document()->size() which accounts for word wrapping and margins
  contentWidget_->document()->setTextWidth(contentWidget_->viewport()->width());
  QSizeF docSize = contentWidget_->document()->size();
  int idealHeight = static_cast<int>(docSize.height()) + 20;  // Add padding for safety

  // Set minimum height to ensure visibility
  idealHeight = qMax(idealHeight, 40);

  contentWidget_->setMinimumHeight(idealHeight);
  contentWidget_->setMaximumHeight(idealHeight);
}

void ChatBubble::SetMessage(const QString& message) {
  message_ = message;
  renderMarkdown(message);
}

QString ChatBubble::GetMessage() const {
  return message_;
}

void ChatBubble::AnimateIn() {
  fadeInAnimation_->start();
}

void ChatBubble::onCopyClicked() {
  QApplication::clipboard()->setText(message_);
  copyButton_->setText("Copied!");

  // Reset button text after 1 second
  QTimer::singleShot(1000, this, [this]() { copyButton_->setText("Copy"); });
}

// ============================================================================
// ThinkingIndicator Implementation
// ============================================================================

ThinkingIndicator::ThinkingIndicator(QWidget* parent)
    : QWidget(parent), animationFrame_(0), text_("Claude is thinking") {
  animationTimer_ = new QTimer(this);
  connect(animationTimer_, &QTimer::timeout, this, &ThinkingIndicator::updateAnimation);

  setFixedHeight(50);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  // Styling
  setStyleSheet(R"(
    ThinkingIndicator {
      background-color: #F9FAFB;
      border: 1px solid #D1D5DB;
      border-radius: 8px;
    }
  )");
}

void ThinkingIndicator::Start() {
  animationFrame_ = 0;
  animationTimer_->start(500);  // Update every 500ms
  update();
}

void ThinkingIndicator::Stop() {
  animationTimer_->stop();
}

void ThinkingIndicator::updateAnimation() {
  animationFrame_ = (animationFrame_ + 1) % ANIMATION_FRAMES;
  update();
}

void ThinkingIndicator::paintEvent(QPaintEvent* event) {
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw text with animated dots
  QString dots;
  for (int i = 0; i <= animationFrame_; ++i) {
    dots += ".";
  }

  QString fullText = text_ + dots;

  QFont font = painter.font();
  font.setItalic(true);
  font.setPointSize(13);
  painter.setFont(font);
  painter.setPen(QColor("#6B7280"));

  QRect textRect = rect().adjusted(16, 0, -16, 0);
  painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, fullText);
}

}  // namespace platform
}  // namespace athena

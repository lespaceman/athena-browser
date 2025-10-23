#include "qt_claude_panel.h"

#include "qt_mainwindow.h"
#include "runtime/node_runtime.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QThread>
#include <chrono>
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
      waiting_for_response_(false),
      streaming_socket_(nullptr),
      headers_received_(false) {
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
  headerLayout->setContentsMargins(20, 16, 20, 16);  // More airy padding (was 16, 12)

  // Claude logo/title
  headerLabel_ = new QLabel("Claude", headerFrame_);
  QFont headerFont = headerLabel_->font();
  headerFont.setPixelSize(16);  // Use pixels, not points - slightly larger for header
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
  messagesLayout_->setContentsMargins(16, 12, 16, 12);  // Minimal padding all around
  messagesLayout_->setSpacing(0);                       // Spacing handled dynamically per message
  messagesLayout_->setAlignment(Qt::AlignTop | Qt::AlignLeft);  // Align bubbles to left
  messagesLayout_->addStretch();                        // Push messages to top

  scrollArea_->setWidget(messagesContainer_);
  mainLayout_->addWidget(scrollArea_, 1);  // Stretch factor 1

  // ============================================================================
  // Thinking Indicator
  // ============================================================================

  thinkingIndicator_ = new ThinkingIndicator(messagesContainer_);
  thinkingIndicator_->hide();
  // Don't add to layout yet - we'll dynamically add it before stretch when showing

  // ============================================================================
  // Input Area (Footer)
  // ============================================================================

  inputFrame_ = new QFrame(this);
  auto* inputLayout = new QVBoxLayout(inputFrame_);
  inputLayout->setContentsMargins(20, 16, 20, 16);  // More airy padding (was 16, 12)
  inputLayout->setSpacing(12);  // More breathing room (was 8)

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

  // Keyboard shortcut hint (shown when input is focused)
  keyboardHintLabel_ = new QLabel("Press Enter to send • Shift+Enter for new line", inputFrame_);
  keyboardHintLabel_->setAlignment(Qt::AlignRight);
  keyboardHintLabel_->setStyleSheet("color: #9AA0A6; font-size: 12px; padding: 4px 0; font-weight: 400;");
  keyboardHintLabel_->hide();  // Hidden by default, shown on focus
  inputLayout->addWidget(keyboardHintLabel_);

  mainLayout_->addWidget(inputFrame_);

  // Set size policy
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  setMinimumWidth(300);
}

void ClaudePanel::setupStyles() {
  // Force light theme colors globally for the panel
  // This overrides any dark theme that might be applied by the system
  // NOTE: We don't set color on all children (*) to avoid conflicts with ChatBubble HTML rendering
  setStyleSheet(R"(
    ClaudePanel {
      background-color: #FFFFFF;
      border: none;
    }
  )");

  // Header styling - Modern clean design with better spacing
  headerFrame_->setStyleSheet(R"(
    QFrame {
      background-color: #FFFFFF;
      border: none;
      border-bottom: 1px solid #F0F0F0;
      padding-bottom: 12px;
    }
    QFrame QLabel {
      color: #202124;
      font-weight: 600;
      background-color: transparent;
    }
    QFrame QPushButton {
      background-color: transparent;
      border: none;
      border-radius: 8px;
      padding: 8px 16px;
      color: #5F6368;
      font-size: 13px;
      font-weight: 500;
    }
    QFrame QPushButton:hover {
      background-color: #F1F3F4;
      color: #202124;
    }
    QFrame QPushButton:pressed {
      background-color: #E8EAED;
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

  // Input area styling - Clean modern footer with subtle separator
  inputFrame_->setStyleSheet(R"(
    QFrame {
      background-color: #FFFFFF;
      border: none;
      border-top: 1px solid #F0F0F0;
      padding: 0px;
    }
  )");

  // Send button styling - Modern with better visual feedback and polish
  sendButton_->setStyleSheet(R"(
    QPushButton {
      background-color: #4285F4;
      color: white;
      border: none;
      border-radius: 10px;
      padding: 10px 20px;
      font-weight: 600;
      font-size: 14px;
      min-width: 70px;
    }
    QPushButton:hover:enabled {
      background-color: #3367D6;
    }
    QPushButton:pressed:enabled {
      background-color: #2B5DBA;
      transform: scale(0.98);
    }
    QPushButton:disabled {
      background-color: #E8EAED;
      color: #9AA0A6;
    }
  )");

  // Regenerate button styling - Modern clean design with better visual hierarchy
  regenerateButton_->setStyleSheet(R"(
    QPushButton {
      background-color: #F8F9FA;
      border: 1px solid #E8EAED;
      border-radius: 10px;
      padding: 10px 18px;
      color: #5F6368;
      font-size: 13px;
      font-weight: 500;
    }
    QPushButton:hover {
      background-color: #F1F3F4;
      border-color: #DADCE0;
      color: #202124;
    }
    QPushButton:pressed {
      background-color: #E8EAED;
      border-color: #DADCE0;
    }
  )");
}

void ClaudePanel::connectSignals() {
  connect(sendButton_, &QPushButton::clicked, this, &ClaudePanel::onSendClicked);

  connect(clearButton_, &QPushButton::clicked, this, &ClaudePanel::onClearClicked);

  connect(regenerateButton_, &QPushButton::clicked, this, &ClaudePanel::onRegenerateClicked);

  connect(inputWidget_, &ChatInputWidget::sendRequested, this, &ClaudePanel::onSendClicked);

  connect(inputWidget_, &QTextEdit::textChanged, this, &ClaudePanel::onInputTextChanged);

  // Show/hide keyboard hint based on input focus
  connect(inputWidget_, &ChatInputWidget::focusChanged, keyboardHintLabel_, &QLabel::setVisible);
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
  // IMPORTANT: Don't animate for streaming responses (would interfere with real-time updates)
  addMessage("assistant", "", false);

  // Manually set opacity to 1.0 immediately (skip animation for streaming)
  if (!messageBubbles_.empty()) {
    ChatBubble* lastBubble = messageBubbles_.back();
    if (lastBubble->GetRole() == ChatBubble::Role::Assistant) {
      // Get the opacity effect and set it to fully visible immediately
      auto* opacityEffect = qobject_cast<QGraphicsOpacityEffect*>(lastBubble->graphicsEffect());
      if (opacityEffect) {
        opacityEffect->setOpacity(1.0);
      }
    }
  }

  // Build JSON request body
  QString escaped_message = message;
  escaped_message.replace("\\", "\\\\");  // Escape backslashes first
  escaped_message.replace("\"", "\\\"");  // Escape quotes
  escaped_message.replace("\n", "\\n");   // Escape newlines

  // Include sessionId if we have one for conversation continuity
  QString json_body;
  if (!current_session_id_.isEmpty()) {
    json_body = QString("{\"message\":\"%1\",\"sessionId\":\"%2\"}")
                    .arg(escaped_message, current_session_id_);
    qDebug() << "[ClaudePanel] Sending message with session ID:" << current_session_id_;
  } else {
    json_body = QString("{\"message\":\"%1\"}").arg(escaped_message);
    qDebug() << "[ClaudePanel] Sending message (new session)";
  }

  // Create streaming socket connection
  if (streaming_socket_) {
    streaming_socket_->deleteLater();
  }

  streaming_socket_ = new QLocalSocket(this);
  response_buffer_.clear();
  accumulated_text_.clear();
  headers_received_ = false;

  // Connect socket signals
  connect(streaming_socket_, &QLocalSocket::connected, this, &ClaudePanel::onSocketConnected);
  connect(streaming_socket_, &QLocalSocket::readyRead, this, &ClaudePanel::onSocketReadyRead);
  connect(streaming_socket_, &QLocalSocket::errorOccurred, this, &ClaudePanel::onSocketError);
  connect(streaming_socket_, &QLocalSocket::disconnected, this, &ClaudePanel::onSocketDisconnected);

  // Store JSON body for sending after connection
  streaming_socket_->setProperty("json_body", json_body);

  // Connect to Unix socket
  QString socket_path = QString::fromStdString(node_runtime_->GetSocketPath());
  qDebug() << "[ClaudePanel] Connecting to socket:" << socket_path;
  streaming_socket_->connectToServer(socket_path);
}

void ClaudePanel::addMessage(const QString& role, const QString& message, bool animate) {
  ChatBubble::Role bubbleRole =
      (role == "user") ? ChatBubble::Role::User : ChatBubble::Role::Assistant;

  auto* bubble = new ChatBubble(bubbleRole, message, messagesContainer_);

  // Insert before the stretch spacer
  int insertIndex = messagesLayout_->count() - 1;

  // Add spacing before the bubble based on previous message
  if (!messageBubbles_.empty()) {
    ChatBubble* lastBubble = messageBubbles_.back();
    bool sameRole = (lastBubble->GetRole() == bubbleRole);

    // Very tight spacing - minimal gaps between messages
    int spacing = sameRole ? 2 : 4;
    messagesLayout_->insertSpacing(insertIndex, spacing);
    insertIndex++;
  }

  messagesLayout_->insertWidget(insertIndex, bubble);
  messageBubbles_.push_back(bubble);

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
    // Add thinking indicator before the stretch spacer (at the bottom)
    int insertIndex = messagesLayout_->count() - 1;

    // Add spacing before thinking indicator if there are messages
    if (!messageBubbles_.empty()) {
      messagesLayout_->insertSpacing(insertIndex, 4);  // Minimal spacing before thinking indicator
      insertIndex++;
    }

    messagesLayout_->insertWidget(insertIndex, thinkingIndicator_);
    thinkingIndicator_->show();
    thinkingIndicator_->Start();
  } else {
    thinkingIndicator_->Stop();
    thinkingIndicator_->hide();

    // Remove thinking indicator from layout
    messagesLayout_->removeWidget(thinkingIndicator_);

    // Remove the spacing that was added before it
    if (messagesLayout_->count() > 1) {
      QLayoutItem* item = messagesLayout_->itemAt(messagesLayout_->count() - 2);
      if (item && item->spacerItem()) {
        messagesLayout_->removeItem(item);
        delete item;
      }
    }
  }
  scrollToBottom(true);
}

void ClaudePanel::scrollToBottom(bool animated) {
  QScrollBar* scrollBar = scrollArea_->verticalScrollBar();

  // Smart scroll: Only auto-scroll if user is already near the bottom
  // This allows users to read history without being interrupted by new messages
  int currentValue = scrollBar->value();
  int maxValue = scrollBar->maximum();
  int threshold = 50;  // Within 50px of bottom = "at bottom"

  bool isNearBottom = (maxValue - currentValue) <= threshold;

  // Only scroll if user is already at bottom (or force scroll for first message)
  if (!isNearBottom && !messageBubbles_.empty()) {
    return;  // User has scrolled up, don't interrupt them
  }

  if (animated) {
    // Smooth scroll animation
    int targetValue = maxValue;

    if (targetValue != currentValue) {
      auto* animation = new QPropertyAnimation(scrollBar, "value", this);
      animation->setDuration(300);
      animation->setStartValue(currentValue);
      animation->setEndValue(targetValue);
      animation->setEasingCurve(QEasingCurve::OutCubic);
      animation->start(QPropertyAnimation::DeleteWhenStopped);
    }
  } else {
    scrollBar->setValue(maxValue);
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
  // Clear UI
  ClearHistory();

  // Clear session ID to start fresh
  current_session_id_.clear();
  qDebug() << "[ClaudePanel] Cleared session ID";

  // Also clear the server-side session
  if (node_runtime_ && node_runtime_->IsReady()) {
    qDebug() << "[ClaudePanel] Clearing server-side session";

    // Create socket for clear request
    auto* clearSocket = new QLocalSocket(this);

    // Build HTTP request to clear endpoint
    QString http_request = QString(
        "POST /v1/chat/clear HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "User-Agent: Athena-Browser/1.0\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
    );

    // Connect and send
    connect(clearSocket, &QLocalSocket::connected, this, [clearSocket, http_request]() {
      qDebug() << "[ClaudePanel] Sending clear request";
      clearSocket->write(http_request.toUtf8());
      clearSocket->flush();
      clearSocket->disconnectFromServer();
    });

    connect(clearSocket, &QLocalSocket::disconnected, clearSocket, &QLocalSocket::deleteLater);
    connect(clearSocket, &QLocalSocket::errorOccurred, this, [clearSocket](QLocalSocket::LocalSocketError error) {
      qWarning() << "[ClaudePanel] Clear request failed:" << error << clearSocket->errorString();
      clearSocket->deleteLater();
    });

    QString socket_path = QString::fromStdString(node_runtime_->GetSocketPath());
    clearSocket->connectToServer(socket_path);
  }
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
// Streaming Socket Handlers
// ============================================================================

void ClaudePanel::onSocketConnected() {
  qDebug() << "[ClaudePanel] Socket connected, sending HTTP request";

  // Get JSON body from socket property
  QString json_body = streaming_socket_->property("json_body").toString();

  // Build HTTP request
  QString http_request = QString(
      "POST /v1/chat/stream HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "User-Agent: Athena-Browser/1.0\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %1\r\n"
      "\r\n"
      "%2"
  ).arg(json_body.toUtf8().size()).arg(json_body);

  qDebug() << "[ClaudePanel] Sending HTTP request:" << http_request;

  // Send request
  streaming_socket_->write(http_request.toUtf8());
  streaming_socket_->flush();
}

void ClaudePanel::onSocketReadyRead() {
  // Read all available data
  QByteArray data = streaming_socket_->readAll();
  response_buffer_.append(QString::fromUtf8(data));

  qDebug() << "[ClaudePanel] Received" << data.size() << "bytes (total buffer size:"
           << response_buffer_.size() << ")";

  // Parse HTTP headers if not yet received
  if (!headers_received_) {
    int header_end = response_buffer_.indexOf("\r\n\r\n");
    if (header_end != -1) {
      headers_received_ = true;
      qDebug() << "[ClaudePanel] HTTP headers received, body starts at position" << (header_end + 4);

      // Extract body and start parsing SSE
      QString body = response_buffer_.mid(header_end + 4);
      response_buffer_ = body;  // Keep only body in buffer

      // Hide thinking indicator now that we're receiving data
      showThinkingIndicator(false);

      // Parse any SSE chunks in the initial body
      parseSSEChunks(body);
    }
  } else {
    // Headers already received, parse new SSE chunks
    parseSSEChunks(data);
  }
}

void ClaudePanel::onSocketError(QLocalSocket::LocalSocketError error) {
  QString error_msg = streaming_socket_->errorString();

  // PeerClosedError is NOT an error - it's the normal way HTTP connections close
  // after the server finishes sending the response (res.end() in Node.js)
  if (error == QLocalSocket::PeerClosedError) {
    qDebug() << "[ClaudePanel] Socket closed by peer (normal completion)";
    return;  // Let onSocketDisconnected handle cleanup
  }

  qWarning() << "[ClaudePanel] Socket error:" << error << "-" << error_msg;

  showThinkingIndicator(false);
  waiting_for_response_ = false;

  replaceLastAssistantMessage(
      QString("❌ **Error:** Failed to communicate with Claude Agent: %1").arg(error_msg));

  regenerateButton_->show();

  // Clean up socket
  if (streaming_socket_) {
    streaming_socket_->deleteLater();
    streaming_socket_ = nullptr;
  }
}

void ClaudePanel::onSocketDisconnected() {
  qDebug() << "[ClaudePanel] Socket disconnected";

  waiting_for_response_ = false;

  // If we have no accumulated text, show error
  if (accumulated_text_.isEmpty()) {
    replaceLastAssistantMessage("❌ **Error:** No response received from Claude");
  } else {
    // Ensure final text is displayed
    replaceLastAssistantMessage(accumulated_text_);
  }

  regenerateButton_->show();

  // Clean up socket
  if (streaming_socket_) {
    streaming_socket_->deleteLater();
    streaming_socket_ = nullptr;
  }
}

void ClaudePanel::parseSSEChunks(const QString& data) {
  // Parse SSE format: lines starting with "data: " contain JSON chunks
  QStringList lines = data.split('\n');

  for (const QString& line : lines) {
    if (!line.startsWith("data: ")) {
      continue;
    }

    // Extract JSON from "data: {...}"
    QString json_str = line.mid(6).trimmed();  // Skip "data: "

    if (json_str.isEmpty()) {
      continue;
    }

    qDebug() << "[ClaudePanel] Parsing SSE chunk:" << json_str;

    // Parse JSON manually (simple parser for our known format)
    // Format: {"type":"chunk","content":"text"} or {"type":"done"} or {"type":"error","error":"msg"}

    // Extract type
    int type_start = json_str.indexOf("\"type\":\"") + 8;
    int type_end = json_str.indexOf("\"", type_start);
    if (type_start < 8 || type_end == -1) {
      continue;
    }
    QString chunk_type = json_str.mid(type_start, type_end - type_start);

    if (chunk_type == "chunk") {
      // Extract content
      int content_start = json_str.indexOf("\"content\":\"");
      if (content_start == -1) {
        continue;
      }
      content_start += 11;  // Skip "content":""

      // Find end of content (handling escaped quotes)
      int content_end = content_start;
      while (content_end < json_str.length()) {
        if (json_str[content_end] == '\\' && content_end + 1 < json_str.length()) {
          content_end += 2;  // Skip escaped character
          continue;
        }
        if (json_str[content_end] == '\"') {
          break;
        }
        content_end++;
      }

      QString chunk_content = json_str.mid(content_start, content_end - content_start);

      // Unescape JSON sequences
      chunk_content.replace("\\n", "\n");
      chunk_content.replace("\\\"", "\"");
      chunk_content.replace("\\\\", "\\");

      qDebug() << "[ClaudePanel] Chunk content:" << chunk_content;

      // Accumulate text
      accumulated_text_ += chunk_content;

      // Update UI immediately
      replaceLastAssistantMessage(accumulated_text_);

      // Extract session ID from chunk if present (for session continuity)
      int session_id_start = json_str.indexOf("\"sessionId\":\"");
      if (session_id_start != -1) {
        session_id_start += 13;  // Skip "sessionId":"
        int session_id_end = json_str.indexOf("\"", session_id_start);
        QString session_id = json_str.mid(session_id_start, session_id_end - session_id_start);

        // Only update if we don't have a session ID yet or if it changed
        if (current_session_id_ != session_id) {
          current_session_id_ = session_id;
          qDebug() << "[ClaudePanel] Received session ID from chunk:" << session_id;
        }
      }

    } else if (chunk_type == "error") {
      // Extract error message
      int error_start = json_str.indexOf("\"error\":\"");
      if (error_start != -1) {
        error_start += 9;
        int error_end = json_str.indexOf("\"", error_start);
        QString error_msg = json_str.mid(error_start, error_end - error_start);

        qWarning() << "[ClaudePanel] Received error:" << error_msg;
        replaceLastAssistantMessage(QString("❌ **Error:** %1").arg(error_msg));
      }
    } else if (chunk_type == "done") {
      qDebug() << "[ClaudePanel] Stream complete";
    }
  }
}

// ============================================================================
// ChatInputWidget Implementation
// ============================================================================

ChatInputWidget::ChatInputWidget(QWidget* parent) : QTextEdit(parent) {
  setupUI();
  // Use contentsChanged for better responsiveness with wrapped lines
  connect(document(), &QTextDocument::contentsChanged, this, &ChatInputWidget::adjustHeight);
}

void ChatInputWidget::setupUI() {
  setAcceptRichText(false);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  // Styling - Modern clean design with subtle background and smooth transitions
  setStyleSheet(R"(
    QTextEdit {
      background-color: #F8F9FA;
      border: 2px solid #E8EAED;
      border-radius: 12px;
      padding: 12px 16px;
      font-size: 14px;
      color: #202124;
    }
    QTextEdit:focus {
      border-color: #4285F4;
      background-color: #FFFFFF;
      /* Padding stays the same - no content shift */
    }
  )");

  // Set document margin to prevent text from touching edges
  document()->setDocumentMargin(2);

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
  emit focusChanged(true);
}

void ChatInputWidget::focusOutEvent(QFocusEvent* event) {
  QTextEdit::focusOutEvent(event);
  emit focusChanged(false);
}

void ChatInputWidget::adjustHeight() {
  int idealHeight = calculateIdealHeight();
  setFixedHeight(idealHeight);
}

int ChatInputWidget::calculateIdealHeight() {
  // Fixed: Use document size instead of line count to handle wrapped lines
  // Set text width to account for padding and borders
  int availableWidth = viewport()->width() - 4;  // Account for margins
  document()->setTextWidth(availableWidth);

  // Get actual document height (handles word wrapping)
  QSizeF docSize = document()->size();
  int contentHeight = static_cast<int>(docSize.height());

  // Add padding (10px top + 10px bottom) + borders (2px top + 2px bottom) + margin
  int padding = 28;
  int totalHeight = contentHeight + padding;

  // Clamp between min and max
  return qBound(MIN_HEIGHT, totalHeight, MAX_HEIGHT);
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
  layout_->setContentsMargins(10, 6, 10, 6);  // Balanced padding - readable but compact
  layout_->setSpacing(4);  // Tight spacing between role and content

  // Role label
  roleLabel_ = new QLabel(this);
  QFont labelFont = roleLabel_->font();
  labelFont.setPixelSize(12);  // Use pixels, not points
  labelFont.setBold(true);
  roleLabel_->setFont(labelFont);
  roleLabel_->setText(role_ == Role::User ? "You" : "Claude");

  layout_->addWidget(roleLabel_);

  // Content widget (use QTextBrowser for better HTML rendering)
  contentWidget_ = new QTextEdit(this);
  contentWidget_->setReadOnly(true);
  contentWidget_->setFrameShape(QFrame::NoFrame);
  contentWidget_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  contentWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  contentWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  // Don't set maximum width - let it adapt to container width

  // CRITICAL FIX: Override system dark theme with explicit colors
  // System may use dark theme (white text), but we need dark text on light bubble
  QString textColorHex = (role_ == Role::User) ? "#1565C0" : "#202124";
  QString bgColorHex = (role_ == Role::User) ? "#D6EAF8" : "#ECF0F1";

  // Method 1: Set palette (for plain text rendering)
  QPalette lightPalette;
  lightPalette.setColor(QPalette::Text, QColor(textColorHex));
  lightPalette.setColor(QPalette::Base, QColor(bgColorHex));
  contentWidget_->setPalette(lightPalette);

  // Method 2: Set default stylesheet on document (for HTML rendering)
  // This persists even when setHtml() is called
  QString defaultCSS = QString("body { color: %1; background-color: %2; }")
                          .arg(textColorHex, bgColorHex);
  contentWidget_->document()->setDefaultStyleSheet(defaultCSS);

  // FORCE text color via widget stylesheet (nuclear option)
  QString widgetStyleSheet = QString(
      "QTextEdit { "
      "  background: transparent; "
      "  border: none; "
      "  color: %1; "
      "}"
  ).arg(textColorHex);
  contentWidget_->setStyleSheet(widgetStyleSheet);

  contentWidget_->setAutoFillBackground(false);

  // Set explicit font size for consistent rendering - standard readable size
  QFont contentFont = contentWidget_->font();
  contentFont.setPixelSize(14);  // Use pixels, not points - matches input field
  contentWidget_->setFont(contentFont);

  // Enable word wrapping
  contentWidget_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  contentWidget_->setLineWrapMode(QTextEdit::WidgetWidth);

  layout_->addWidget(contentWidget_);

  // Use Box frame with no border for clean background
  setFrameShape(QFrame::Box);
  setFrameShadow(QFrame::Plain);
  setLineWidth(0);  // No border line

  // Adaptive width - expand to fill container while maintaining readability
  // Maximum width constraint removed to prevent horizontal scrolling in narrow panels
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void ChatBubble::setupStyles() {
  // Determine colors based on role
  QString bgColorHex = (role_ == Role::User) ? "#D6EAF8" : "#ECF0F1";
  QString labelColorHex = (role_ == Role::User) ? "#1565C0" : "#3C4043";

  // Set background using palette (more reliable than stylesheet for QFrame)
  QPalette bubblePalette = palette();
  bubblePalette.setColor(QPalette::Window, QColor(bgColorHex));
  setPalette(bubblePalette);
  setAutoFillBackground(true);

  // Use QFrame stylesheet syntax with border-radius
  setStyleSheet(QString(R"(
    QFrame {
      background-color: %1;
      border: none;
      border-radius: 12px;
    }
    QLabel {
      color: %2;
      background-color: transparent;
      font-weight: 600;
    }
  )").arg(bgColorHex, labelColorHex));
}

void ChatBubble::renderMarkdown(const QString& markdown) {
  // Simple markdown rendering with proper color handling
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
               "<code style='background-color: rgba(0,0,0,0.05); padding: 2px 4px; "
               "border-radius: 3px; font-family: monospace;'>\\1</code>");

  // Preserve newlines
  html.replace("\n", "<br>");

  // Modern HTML wrapper with better typography - colors come from widget stylesheet and document default CSS
  QString wrappedHtml = QString(
      "<div style='line-height: 1.7; word-wrap: break-word; overflow-wrap: break-word;'>%1</div>"
  ).arg(html);

  contentWidget_->setHtml(wrappedHtml);

  // Adjust height to fit content with dynamic width calculation
  // Get the actual available width from the widget, not a hardcoded value
  int padding = 24 * 2;  // Left + right padding from layout
  int availableWidth = qMax(200, width() - padding);  // Use actual bubble width, min 200px

  // Set text width to available width to ensure proper wrapping
  contentWidget_->document()->setTextWidth(availableWidth);

  QSizeF docSize = contentWidget_->document()->size();
  int idealHeight = static_cast<int>(docSize.height()) + 20;
  idealHeight = qMax(idealHeight, 40);

  contentWidget_->setMinimumHeight(idealHeight);
  contentWidget_->setMaximumHeight(idealHeight);

  contentWidget_->updateGeometry();
  updateGeometry();
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


// ============================================================================
// ThinkingIndicator Implementation
// ============================================================================

ThinkingIndicator::ThinkingIndicator(QWidget* parent)
    : QWidget(parent), animationFrame_(0), text_("Claude is thinking") {
  animationTimer_ = new QTimer(this);
  connect(animationTimer_, &QTimer::timeout, this, &ThinkingIndicator::updateAnimation);

  setFixedHeight(36);  // Compact height to match bubble sizing
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  // Set background using palette for reliability
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor("#F8F9FA"));
  setPalette(pal);
  setAutoFillBackground(true);

  // Styling - Modern, clean, subtle background
  setStyleSheet(R"(
    QWidget {
      background-color: #F8F9FA;
      border: none;
      border-radius: 12px;
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
  // Don't call QWidget::paintEvent - we're doing all painting ourselves
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  // Draw text with animated dots
  QString dots;
  for (int i = 0; i <= animationFrame_; ++i) {
    dots += ".";
  }

  QString fullText = text_ + dots;

  QFont font = painter.font();
  font.setItalic(true);
  font.setPixelSize(14);  // Use pixels, not points
  font.setWeight(QFont::Medium);
  painter.setFont(font);
  painter.setPen(QColor("#5F6368"));

  // Left-align with padding matching bubble style
  QRect textRect = rect().adjusted(10, 0, -10, 0);
  painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, fullText);
}

}  // namespace platform
}  // namespace athena

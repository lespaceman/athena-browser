#include "qt_agent_panel.h"

#include "qt_mainwindow.h"
#include "runtime/node_runtime.h"

#include <chrono>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QObject>
#include <QRegularExpression>
#include <QScrollBar>
#include <QThread>
#include <thread>

namespace athena {
namespace platform {

namespace {

QString colorToCss(const QColor& color) {
  return color.alpha() == 255 ? color.name(QColor::HexRgb) : color.name(QColor::HexArgb);
}

QColor lighten(const QColor& color, int percentage) {
  QColor c = color;
  return c.lighter(percentage);
}

QColor darken(const QColor& color, int percentage) {
  QColor c = color;
  return c.darker(percentage);
}

QIcon createSendIcon(const QColor& color, qreal devicePixelRatio = 1.0) {
  const int baseSize = 24;
  QPixmap pixmap(static_cast<int>(baseSize * devicePixelRatio),
                 static_cast<int>(baseSize * devicePixelRatio));
  pixmap.fill(Qt::transparent);
  pixmap.setDevicePixelRatio(devicePixelRatio);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setBrush(color);
  painter.setPen(Qt::NoPen);

  QPainterPath plane;
  plane.moveTo(5, 12);
  plane.lineTo(5, 5);
  plane.lineTo(21, 12);
  plane.lineTo(5, 19);
  plane.lineTo(5, 14.5);
  plane.lineTo(13, 12);
  plane.lineTo(5, 9.5);
  plane.closeSubpath();
  painter.drawPath(plane);

  return QIcon(pixmap);
}

QIcon createStopIcon(const QColor& color, qreal devicePixelRatio = 1.0) {
  const int baseSize = 24;
  QPixmap pixmap(static_cast<int>(baseSize * devicePixelRatio),
                 static_cast<int>(baseSize * devicePixelRatio));
  pixmap.fill(Qt::transparent);
  pixmap.setDevicePixelRatio(devicePixelRatio);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setBrush(color);
  painter.setPen(Qt::NoPen);

  QRectF square(7, 7, 10, 10);
  painter.drawRoundedRect(square, 3, 3);

  return QIcon(pixmap);
}

}  // namespace

// ============================================================================
// AgentPanel Implementation
// ============================================================================

AgentPanel::AgentPanel(QtMainWindow* window, QWidget* parent)
    : QWidget(parent),
      window_(window),
      node_runtime_(nullptr),
      panel_visible_(true),
      waiting_for_response_(false),
      userCanceledResponse_(false),
      streaming_socket_(nullptr),
      headers_received_(false) {
  setupUI();
  setupStyles();
  connectSignals();
}

AgentPanel::~AgentPanel() = default;

void AgentPanel::setupUI() {
  mainLayout_ = new QVBoxLayout(this);
  mainLayout_->setContentsMargins(0, 12, 0, 0);
  mainLayout_->setSpacing(0);

  // ============================================================================
  // Header
  // ============================================================================

  scrollArea_ = new QScrollArea(this);
  scrollArea_->setWidgetResizable(true);
  scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollArea_->setFrameShape(QFrame::NoFrame);  // Remove frame border

  messagesContainer_ = new QWidget();
  messagesLayout_ = new QVBoxLayout(messagesContainer_);
  messagesLayout_->setContentsMargins(24, 12, 24, 16);
  messagesLayout_->setSpacing(10);  // Spacing handled dynamically per message
  messagesLayout_->setAlignment(Qt::AlignTop | Qt::AlignLeft);  // Align bubbles to left
  messagesLayout_->addStretch();                                // Push messages to top

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
  inputLayout->setContentsMargins(0, 10, 0, 0);
  inputLayout->setSpacing(0);

  inputCard_ = new QFrame(inputFrame_);
  auto* cardLayout = new QVBoxLayout(inputCard_);
  cardLayout->setContentsMargins(12, 12, 12, 12);
  cardLayout->setSpacing(6);
  inputShadow_ = new QGraphicsDropShadowEffect(inputCard_);
  inputShadow_->setBlurRadius(18);
  inputShadow_->setOffset(0, 3);
  inputShadow_->setColor(QColor(0, 0, 0, 32));
  inputCard_->setGraphicsEffect(inputShadow_);

  // Input text area + send button row
  auto* inputRowWidget = new QWidget(inputCard_);
  auto* inputRowLayout = new QHBoxLayout(inputRowWidget);
  inputRowLayout->setContentsMargins(0, 0, 0, 0);
  inputRowLayout->setSpacing(12);

  inputWidget_ = new ChatInputWidget(inputRowWidget);
  inputWidget_->setPlaceholderText(tr("Follow up..."));

  stopButton_ = new QPushButton(inputRowWidget);
  stopButton_->setCursor(Qt::PointingHandCursor);
  stopButton_->setVisible(false);
  stopButton_->setEnabled(false);
  stopButton_->setFlat(true);
  stopButton_->setIconSize(QSize(22, 22));
  stopButton_->setToolTip(tr("Stop response"));
  stopButton_->setMinimumSize(44, 44);
  stopButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  sendButton_ = new QPushButton(inputRowWidget);
  sendButton_->setCursor(Qt::PointingHandCursor);
  sendButton_->setEnabled(false);  // Disabled until text is entered
  sendButton_->setFlat(true);
  sendButton_->setIconSize(QSize(22, 22));
  sendButton_->setToolTip(tr("Send message"));
  sendButton_->setMinimumSize(44, 44);
  sendButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  inputRowLayout->addWidget(inputWidget_, 1);
  inputRowLayout->addWidget(stopButton_);
  inputRowLayout->addWidget(sendButton_);

  cardLayout->addWidget(inputRowWidget);
  inputLayout->addWidget(inputCard_);

  mainLayout_->addWidget(inputFrame_);

  // Set size policy
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  setMinimumWidth(300);
}

void AgentPanel::setupStyles() {
  palette_ = buildPalette(detectDarkMode());
  applyPalette();
  updateActionButtons();
}

AgentPanelPalette AgentPanel::buildPalette(bool darkMode) const {
  AgentPanelPalette palette;
  palette.dark = darkMode;

  const QPalette systemPalette = QApplication::palette();
  QColor window = systemPalette.color(QPalette::Window);
  QColor base = systemPalette.color(QPalette::Base);
  QColor text = systemPalette.color(QPalette::WindowText);
  QColor placeholder = systemPalette.color(QPalette::PlaceholderText);
  QColor highlight = systemPalette.color(QPalette::Highlight);
  QColor highlightedText = systemPalette.color(QPalette::HighlightedText);

  if (!highlight.isValid() || highlight.alpha() == 0) {
    highlight = darkMode ? QColor("#3B82F6") : QColor("#2563EB");
  }
  if (!highlightedText.isValid() || highlightedText.alpha() == 0) {
    highlightedText = darkMode ? QColor("#0F172A") : QColor("#FFFFFF");
  }
  if (!placeholder.isValid() || placeholder == text) {
    placeholder = darkMode ? lighten(text, 180) : darken(text, 130);
  }

  palette.panelBackground = window;
  palette.panelBorder = darkMode ? lighten(window, 130) : darken(window, 110);
  palette.messageAreaBackground = window;
  palette.keyboardHintText = placeholder;
  palette.thinkingBackground = darkMode ? darken(window, 120) : lighten(window, 108);
  palette.thinkingText = darkMode ? lighten(text, 140) : darken(text, 120);
  palette.secondaryText = palette.keyboardHintText;
  palette.accent = highlight;

  palette.scrollbar.track = darkMode ? darken(window, 130) : lighten(window, 115);
  palette.scrollbar.thumb = darkMode ? darken(highlight, 130) : darken(highlight, 110);
  palette.scrollbar.thumbHover = darkMode ? darken(highlight, 110) : darken(highlight, 130);

  BubblePalette userBubble;
  userBubble.background = highlight;
  userBubble.text = highlightedText;
  userBubble.label = highlightedText;
  QColor userCodeBg = highlight;
  userCodeBg.setAlphaF(darkMode ? 0.25 : 0.18);
  userBubble.codeBackground = userCodeBg;
  userBubble.codeText = highlightedText;
  palette.userBubble = userBubble;

  BubblePalette assistantBubble;
  assistantBubble.background = darkMode ? darken(window, 130) : lighten(window, 112);
  assistantBubble.text = darkMode ? lighten(text, 150) : text;
  assistantBubble.label = darkMode ? lighten(text, 130) : darken(text, 120);
  QColor assistantCodeBg = darkMode ? darken(window, 120) : lighten(window, 120);
  assistantBubble.codeBackground = assistantCodeBg;
  assistantBubble.codeText = assistantBubble.text;
  palette.assistantBubble = assistantBubble;

  QColor inputBackground = darkMode ? darken(window, 140) : lighten(base, 108);
  palette.input = {inputBackground,
                   darkMode ? darken(window, 110) : darken(inputBackground, 110),
                   highlight,
                   darkMode ? lighten(text, 150) : text,
                   placeholder,
                   highlight};

  palette.sendButton = {highlight,
                        darken(highlight, 110),
                        darken(highlight, 130),
                        darkMode ? darken(window, 120) : lighten(window, 120),
                        highlightedText,
                        darkMode ? lighten(window, 170) : darken(window, 150)};

  QColor stopBackground = darkMode ? darken(window, 120) : lighten(window, 114);
  palette.stopButton = {stopBackground,
                        darken(stopBackground, 110),
                        darken(stopBackground, 125),
                        darkMode ? darken(window, 110) : lighten(window, 125),
                        darkMode ? lighten(text, 160) : darken(text, 110),
                        darkMode ? lighten(window, 180) : darken(window, 140)};

  palette.chip = {darkMode ? darken(window, 130) : lighten(window, 120),
                  darkMode ? lighten(text, 160) : darken(text, 110),
                  darkMode ? darken(window, 110) : lighten(window, 130)};

  return palette;
}

bool AgentPanel::detectDarkMode() const {
  const QPalette systemPalette = QApplication::palette();
  const QColor windowColor = systemPalette.color(QPalette::Window);
  const QColor textColor = systemPalette.color(QPalette::WindowText);
  return windowColor.lightness() < textColor.lightness();
}

void AgentPanel::applyPalette() {
  setAutoFillBackground(true);
  setStyleSheet(QStringLiteral("AgentPanel { background-color: %1; border-left: 1px solid %2; }")
                    .arg(colorToCss(palette_.panelBackground), colorToCss(palette_.panelBorder)));

  applyPaletteToScrollArea();
  applyPaletteToInput();
  applyPaletteToButtons();
  applyPaletteToMessages();
  applyPaletteToThinkingIndicator();
  refreshSendStopIcons();
}

void AgentPanel::applyPaletteToScrollArea() {
  messagesContainer_->setStyleSheet(
      QStringLiteral("QWidget { background-color: %1; }")
          .arg(colorToCss(palette_.messageAreaBackground)));

  scrollArea_->setStyleSheet(QStringLiteral(R"(
    QScrollArea {
      border: none;
      background-color: %1;
    }
    QScrollBar:vertical {
      background: %2;
      width: 12px;
      border: none;
      margin: 0px;
    }
    QScrollBar::handle:vertical {
      background: %3;
      border-radius: 6px;
      min-height: 40px;
      margin: 2px;
    }
    QScrollBar::handle:vertical:hover {
      background: %4;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      height: 0px;
      border: none;
    }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
      background: none;
    }
  )")
                             .arg(colorToCss(palette_.messageAreaBackground),
                                  colorToCss(palette_.scrollbar.track),
                                  colorToCss(palette_.scrollbar.thumb),
                                  colorToCss(palette_.scrollbar.thumbHover)));
}

void AgentPanel::applyPaletteToInput() {
  inputFrame_->setStyleSheet(
      QStringLiteral("QFrame { background-color: %1; border: none; }")
          .arg(colorToCss(palette_.panelBackground)));
  inputCard_->setStyleSheet(
      QStringLiteral("QFrame { background-color: %1; border-radius: 0px; border: none; }")
          .arg(colorToCss(palette_.composerBackground)));

  if (inputShadow_) {
    inputShadow_->setColor(palette_.composerShadow);
    inputShadow_->setBlurRadius(palette_.dark ? 18 : 22);
    inputShadow_->setOffset(0, palette_.dark ? 3 : 4);
  }

  inputWidget_->ApplyTheme(palette_);
}

void AgentPanel::applyPaletteToButtons() {
  auto iconButtonStyle = [](const IconButtonPalette& colors) {
    return QStringLiteral(R"(
      QPushButton {
        background-color: %1;
        border: none;
        border-radius: 20px;
        padding: 0;
      }
      QPushButton:hover:enabled {
        background-color: %2;
      }
      QPushButton:pressed:enabled {
        background-color: %3;
      }
      QPushButton:disabled {
        background-color: %4;
      }
    )")
        .arg(colorToCss(colors.background),
             colorToCss(colors.backgroundHover),
             colorToCss(colors.backgroundPressed),
             colorToCss(colors.backgroundDisabled));
  };

  sendButton_->setStyleSheet(iconButtonStyle(palette_.sendButton));
  stopButton_->setStyleSheet(iconButtonStyle(palette_.stopButton));
}

void AgentPanel::applyPaletteToMessages() {
  for (auto* bubble : messageBubbles_) {
    if (bubble) {
      bubble->ApplyTheme(palette_);
    }
  }
}

void AgentPanel::applyPaletteToThinkingIndicator() {
  thinkingIndicator_->ApplyTheme(palette_);
}

void AgentPanel::refreshSendStopIcons() {
  const qreal devicePixelRatio = devicePixelRatioF();

  const QColor sendColor =
      sendButton_->isEnabled() ? palette_.sendButton.icon : palette_.sendButton.iconDisabled;
  sendButton_->setIcon(createSendIcon(sendColor, devicePixelRatio));

  const QColor stopColor =
      stopButton_->isEnabled() ? palette_.stopButton.icon : palette_.stopButton.iconDisabled;
  stopButton_->setIcon(createStopIcon(stopColor, devicePixelRatio));
}

void AgentPanel::updateActionButtons() {
  bool streaming = waiting_for_response_;
  bool hasText = !inputWidget_->toPlainText().trimmed().isEmpty();

  sendButton_->setVisible(!streaming);
  sendButton_->setEnabled(!streaming && hasText);

  stopButton_->setVisible(streaming);
  stopButton_->setEnabled(streaming);

  refreshSendStopIcons();
}

void AgentPanel::changeEvent(QEvent* event) {
  if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
    palette_ = buildPalette(detectDarkMode());
    applyPalette();
    updateActionButtons();
  }

  QWidget::changeEvent(event);
}
void AgentPanel::connectSignals() {
  connect(sendButton_, &QPushButton::clicked, this, &AgentPanel::onSendClicked);
  connect(stopButton_, &QPushButton::clicked, this, &AgentPanel::onStopClicked);

  connect(inputWidget_, &ChatInputWidget::sendRequested, this, &AgentPanel::onSendClicked);

  connect(inputWidget_, &QTextEdit::textChanged, this, &AgentPanel::onInputTextChanged);
}

void AgentPanel::SetNodeRuntime(runtime::NodeRuntime* runtime) {
  node_runtime_ = runtime;
}

void AgentPanel::SendMessage(const QString& message) {
  if (message.trimmed().isEmpty()) {
    return;
  }

  if (waiting_for_response_) {
    qWarning() << "[AgentPanel] Already waiting for response";
    return;
  }

  userCanceledResponse_ = false;

  // Add user message
  addMessage("user", message, true);

  // Show thinking indicator
  showThinkingIndicator(true);
  waiting_for_response_ = true;
  updateActionButtons();

  // Check if node runtime is available
  if (!node_runtime_ || !node_runtime_->IsReady()) {
    qWarning() << "[AgentPanel] Node runtime not available";
    showThinkingIndicator(false);
    addMessage("assistant",
               "❌ **Error:** Agent is not available. Please ensure the Node.js runtime is "
               "running.",
               true);
    waiting_for_response_ = false;
    updateActionButtons();
    return;
  }

  qDebug() << "[AgentPanel] Sending message to Agent:" << message;

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
    qDebug() << "[AgentPanel] Sending message with session ID:" << current_session_id_;
  } else {
    json_body = QString("{\"message\":\"%1\"}").arg(escaped_message);
    qDebug() << "[AgentPanel] Sending message (new session)";
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
  connect(streaming_socket_, &QLocalSocket::connected, this, &AgentPanel::onSocketConnected);
  connect(streaming_socket_, &QLocalSocket::readyRead, this, &AgentPanel::onSocketReadyRead);
  connect(streaming_socket_, &QLocalSocket::errorOccurred, this, &AgentPanel::onSocketError);
  connect(streaming_socket_, &QLocalSocket::disconnected, this, &AgentPanel::onSocketDisconnected);

  // Store JSON body for sending after connection
  streaming_socket_->setProperty("json_body", json_body);

  // Connect to Unix socket
  QString socket_path = QString::fromStdString(node_runtime_->GetSocketPath());
  qDebug() << "[AgentPanel] Connecting to socket:" << socket_path;
  streaming_socket_->connectToServer(socket_path);
}

void AgentPanel::addMessage(const QString& role, const QString& message, bool animate) {
  ChatBubble::Role bubbleRole =
      (role == "user") ? ChatBubble::Role::User : ChatBubble::Role::Assistant;

  auto* bubble = new ChatBubble(bubbleRole, message, palette_, messagesContainer_);

  // Insert before the stretch spacer
  int insertIndex = messagesLayout_->count() - 1;

  // Add spacing before the bubble based on previous message
  if (!messageBubbles_.empty()) {
    ChatBubble* lastBubble = messageBubbles_.back();
    bool sameRole = (lastBubble->GetRole() == bubbleRole);

    int spacing = sameRole ? 4 : 14;
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

void AgentPanel::replaceLastAssistantMessage(const QString& message) {
  qDebug() << "[AgentPanel::replaceLastAssistantMessage] Called with message length:"
           << message.length();
  qDebug() << "[AgentPanel::replaceLastAssistantMessage] Message preview:" << message.left(100);

  // Find the last assistant message
  for (auto it = messageBubbles_.rbegin(); it != messageBubbles_.rend(); ++it) {
    if ((*it)->GetRole() == ChatBubble::Role::Assistant) {
      qDebug()
          << "[AgentPanel::replaceLastAssistantMessage] Found assistant bubble, updating message";
      (*it)->SetMessage(message);
      scrollToBottom(true);
      return;
    }
  }

  // If no assistant message found, add a new one
  qDebug() << "[AgentPanel::replaceLastAssistantMessage] No assistant bubble found, adding new one";
  addMessage("assistant", message, true);
}

void AgentPanel::showThinkingIndicator(bool show) {
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

void AgentPanel::scrollToBottom(bool animated) {
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

void AgentPanel::trimHistory() {
  while (messageBubbles_.size() > MAX_MESSAGES) {
    auto* bubble = messageBubbles_.front();
    messageBubbles_.pop_front();
    messagesLayout_->removeWidget(bubble);
    bubble->deleteLater();
  }
}

void AgentPanel::ClearHistory() {
  // Remove all message bubbles
  for (auto* bubble : messageBubbles_) {
    messagesLayout_->removeWidget(bubble);
    bubble->deleteLater();
  }
  messageBubbles_.clear();

  qDebug() << "[AgentPanel] Chat history cleared";

  updateActionButtons();
}

void AgentPanel::ToggleVisibility() {
  panel_visible_ = !panel_visible_;
  setVisible(panel_visible_);
  emit visibilityChanged(panel_visible_);

  if (panel_visible_) {
    inputWidget_->setFocus();
  }
}

void AgentPanel::onSendClicked() {
  QString message = inputWidget_->GetText().trimmed();
  if (!message.isEmpty()) {
    SendMessage(message);
    inputWidget_->Clear();
  }
}

void AgentPanel::onStopClicked() {
  if (!waiting_for_response_) {
    return;
  }

  qDebug() << "[AgentPanel] Stop requested by user";

  userCanceledResponse_ = true;
  waiting_for_response_ = false;
  showThinkingIndicator(false);

  if (streaming_socket_) {
    disconnect(streaming_socket_, nullptr, this, nullptr);
    streaming_socket_->abort();
    streaming_socket_->deleteLater();
    streaming_socket_ = nullptr;
  }

  if (accumulated_text_.isEmpty()) {
    replaceLastAssistantMessage("Response stopped.");
  } else {
    replaceLastAssistantMessage(accumulated_text_);
  }

  updateActionButtons();
}

void AgentPanel::onInputTextChanged() {
  updateActionButtons();
}

// ============================================================================
// Streaming Socket Handlers
// ============================================================================

void AgentPanel::onSocketConnected() {
  qDebug() << "[AgentPanel] Socket connected, sending HTTP request";

  // Get JSON body from socket property
  QString json_body = streaming_socket_->property("json_body").toString();

  // Build HTTP request
  QString http_request = QString("POST /v1/chat/stream HTTP/1.1\r\n"
                                 "Host: localhost\r\n"
                                 "User-Agent: Athena-Browser/1.0\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: %1\r\n"
                                 "\r\n"
                                 "%2")
                             .arg(json_body.toUtf8().size())
                             .arg(json_body);

  qDebug() << "[AgentPanel] Sending HTTP request:" << http_request;

  // Send request
  streaming_socket_->write(http_request.toUtf8());
  streaming_socket_->flush();
}

void AgentPanel::onSocketReadyRead() {
  // Read all available data
  QByteArray data = streaming_socket_->readAll();
  response_buffer_.append(QString::fromUtf8(data));

  qDebug() << "[AgentPanel] Received" << data.size()
           << "bytes (total buffer size:" << response_buffer_.size() << ")";

  // Parse HTTP headers if not yet received
  if (!headers_received_) {
    int header_end = response_buffer_.indexOf("\r\n\r\n");
    if (header_end != -1) {
      headers_received_ = true;
      qDebug() << "[AgentPanel] HTTP headers received, body starts at position" << (header_end + 4);

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

void AgentPanel::onSocketError(QLocalSocket::LocalSocketError error) {
  QString error_msg = streaming_socket_->errorString();

  // PeerClosedError is NOT an error - it's the normal way HTTP connections close
  // after the server finishes sending the response (res.end() in Node.js)
  if (error == QLocalSocket::PeerClosedError) {
    qDebug() << "[AgentPanel] Socket closed by peer (normal completion)";
    return;  // Let onSocketDisconnected handle cleanup
  }

  qWarning() << "[AgentPanel] Socket error:" << error << "-" << error_msg;

  showThinkingIndicator(false);
  waiting_for_response_ = false;
  userCanceledResponse_ = false;
  updateActionButtons();

  replaceLastAssistantMessage(
      QString("❌ **Error:** Failed to communicate with Agent: %1").arg(error_msg));

  // Clean up socket
  if (streaming_socket_) {
    streaming_socket_->deleteLater();
    streaming_socket_ = nullptr;
  }
}

void AgentPanel::onSocketDisconnected() {
  qDebug() << "[AgentPanel] Socket disconnected";

  waiting_for_response_ = false;
  updateActionButtons();

  if (userCanceledResponse_ && accumulated_text_.isEmpty()) {
    replaceLastAssistantMessage("Response stopped.");
  } else if (accumulated_text_.isEmpty()) {
    replaceLastAssistantMessage("❌ **Error:** No response received from Agent");
  } else {
    // Ensure final text is displayed
    replaceLastAssistantMessage(accumulated_text_);
  }
  userCanceledResponse_ = false;

  // Clean up socket
  if (streaming_socket_) {
    streaming_socket_->deleteLater();
    streaming_socket_ = nullptr;
  }
}

void AgentPanel::parseSSEChunks(const QString& data) {
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

    qDebug() << "[AgentPanel] Parsing SSE chunk:" << json_str;

    // Parse JSON manually (simple parser for our known format)
    // Format: {"type":"chunk","content":"text"} or {"type":"done"} or
    // {"type":"error","error":"msg"}

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

      qDebug() << "[AgentPanel] Chunk content:" << chunk_content;

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
          qDebug() << "[AgentPanel] Received session ID from chunk:" << session_id;
        }
      }

    } else if (chunk_type == "error") {
      // Extract error message
      int error_start = json_str.indexOf("\"error\":\"");
      if (error_start != -1) {
        error_start += 9;
        int error_end = json_str.indexOf("\"", error_start);
        QString error_msg = json_str.mid(error_start, error_end - error_start);

        qWarning() << "[AgentPanel] Received error:" << error_msg;
        replaceLastAssistantMessage(QString("❌ **Error:** %1").arg(error_msg));
      }
    } else if (chunk_type == "done") {
      qDebug() << "[AgentPanel] Stream complete";
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

  // Set document margin to prevent text from touching edges
  document()->setDocumentMargin(2);

  setMinimumHeight(MIN_HEIGHT);
  setMaximumHeight(MAX_HEIGHT);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  adjustHeight();
}

void ChatInputWidget::ApplyTheme(const AgentPanelPalette& palette) {
  currentPalette_ = palette;
  applyPalette(palette);
}

void ChatInputWidget::applyPalette(const AgentPanelPalette& palette) {
  QColor focusBackground = palette.dark ? darken(palette.input.background, 90) : QColor("#FFFFFF");

  QString style = QStringLiteral(R"(
    QTextEdit {
      background-color: %1;
      border: 1px solid %2;
      border-radius: 6px;
      padding: 10px 14px;
      font-size: 14px;
      color: %3;
      caret-color: %6;
    }
    QTextEdit:focus {
      border: 1px solid %4;
      background-color: %5;
      caret-color: %6;
    }
  )");
  style = style.arg(colorToCss(palette.input.background),
                    colorToCss(palette.input.border),
                    colorToCss(palette.input.text),
                    colorToCss(palette.input.borderFocused),
                    colorToCss(focusBackground),
                    colorToCss(palette.input.caret));
  setStyleSheet(style);

  QPalette widgetPalette = this->palette();
  widgetPalette.setColor(QPalette::Base, palette.input.background);
  widgetPalette.setColor(QPalette::Text, palette.input.text);
  widgetPalette.setColor(QPalette::Highlight, palette.accent);
  widgetPalette.setColor(QPalette::HighlightedText,
                         palette.dark ? QColor("#0F172A") : QColor("#FFFFFF"));
  widgetPalette.setColor(QPalette::PlaceholderText, palette.input.placeholder);
  setPalette(widgetPalette);

  QPalette viewportPalette = viewport()->palette();
  viewportPalette.setColor(QPalette::Base, palette.input.background);
  viewportPalette.setColor(QPalette::Text, palette.input.text);
  viewport()->setPalette(viewportPalette);

  document()->setDefaultStyleSheet(
      QStringLiteral("body { color: %1; }").arg(colorToCss(palette.input.text)));

  setCursorWidth(2);
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

ChatBubble::ChatBubble(Role role,
                       const QString& message,
                       const AgentPanelPalette& palette,
                       QWidget* parent)
    : QFrame(parent), role_(role), message_(message) {
  setupUI();
  ApplyTheme(palette);

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
  layout_->setContentsMargins(16, 10, 16, 10);
  layout_->setSpacing(4);

  roleLabel_ = new QLabel(this);
  QFont labelFont = roleLabel_->font();
  labelFont.setPixelSize(12);
  labelFont.setBold(true);
  roleLabel_->setFont(labelFont);
  roleLabel_->setText(role_ == Role::User ? QObject::tr("You") : QObject::tr("Agent"));
  layout_->addWidget(roleLabel_);

  contentWidget_ = new QTextEdit(this);
  contentWidget_->setReadOnly(true);
  contentWidget_->setFrameShape(QFrame::NoFrame);
  contentWidget_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  contentWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  contentWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  contentWidget_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  contentWidget_->setLineWrapMode(QTextEdit::WidgetWidth);
  contentWidget_->setAutoFillBackground(false);

  QFont contentFont = contentWidget_->font();
  contentFont.setPixelSize(14);
  contentWidget_->setFont(contentFont);

  layout_->addWidget(contentWidget_);

  setFrameShape(QFrame::NoFrame);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void ChatBubble::ApplyTheme(const AgentPanelPalette& palette) {
  bubblePalette_ = (role_ == Role::User) ? palette.userBubble : palette.assistantBubble;
  applyPalette(palette);
  renderMarkdown(message_);
}

void ChatBubble::applyPalette(const AgentPanelPalette& palette) {
  setAutoFillBackground(true);

  QPalette framePalette = this->palette();
  framePalette.setColor(QPalette::Window, bubblePalette_.background);
  setPalette(framePalette);

  setStyleSheet(QStringLiteral(R"(
    QFrame {
      background-color: %1;
      border: none;
      border-radius: 20px;
    }
    QLabel {
      color: %2;
      background-color: transparent;
      font-weight: 600;
    }
  )")
                    .arg(colorToCss(bubblePalette_.background),
                         colorToCss(bubblePalette_.label)));

  QPalette textPalette = contentWidget_->palette();
  textPalette.setColor(QPalette::Base, bubblePalette_.background);
  textPalette.setColor(QPalette::Text, bubblePalette_.text);
  textPalette.setColor(QPalette::Highlight, palette.accent);
  textPalette.setColor(QPalette::HighlightedText,
                       palette.dark ? QColor("#0F172A") : QColor("#FFFFFF"));
  contentWidget_->setPalette(textPalette);

  QString defaultCSS = QStringLiteral(
      "body { color: %1; background-color: %2; font-size: 14px; } "
      "code { background-color: %3; color: %4; padding: 2px 4px; border-radius: 4px; "
      "font-family: 'Fira Code', 'JetBrains Mono', monospace; } "
      "a { color: %5; text-decoration: none; font-weight: 600; } "
      "a:hover { text-decoration: underline; } "
      "strong { font-weight: 600; } "
      "em { font-style: italic; } "
      "ul { padding-left: 20px; margin: 12px 0; } "
      "li { margin-bottom: 6px; }")
                             .arg(colorToCss(bubblePalette_.text),
                                  colorToCss(bubblePalette_.background),
                                  colorToCss(bubblePalette_.codeBackground),
                                  colorToCss(bubblePalette_.codeText),
                                  colorToCss(palette.accent));

  contentWidget_->document()->setDefaultStyleSheet(defaultCSS);
}

void ChatBubble::renderMarkdown(const QString& markdown) {
  QString html = markdown;

  html.replace("&", "&amp;");
  html.replace("<", "&lt;");
  html.replace(">", "&gt;");

  html.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "<b>\\1</b>");
  html.replace(QRegularExpression("(?<!\\*)\\*([^*]+)\\*(?!\\*)"), "<i>\\1</i>");
  html.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");

  html.replace("\n", "<br>");

  QString wrappedHtml = QStringLiteral(
      "<div style='line-height:1.7; word-wrap:break-word; white-space:pre-wrap;'>%1</div>")
                               .arg(html);
  contentWidget_->setHtml(wrappedHtml);

  int availableWidth = contentWidget_->viewport()->width();
  if (availableWidth <= 0) {
    availableWidth = qMax(220, width() - 36);
  }
  contentWidget_->document()->setTextWidth(availableWidth);

  QSizeF docSize = contentWidget_->document()->size();
  int idealHeight = static_cast<int>(docSize.height()) + 12;
  idealHeight = qBound(36, idealHeight, 600);

  contentWidget_->setMinimumHeight(idealHeight);
  contentWidget_->setMaximumHeight(idealHeight);

  contentWidget_->updateGeometry();
  updateGeometry();
}

void ChatBubble::SetMessage(const QString& message) {
  message_ = message;
  renderMarkdown(message_);
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
    : QWidget(parent),
      animationFrame_(0),
      text_("Agent is thinking"),
      textColor_(QColor("#5F6368")),
      backgroundColor_(QColor("#F8F9FA")) {
  animationTimer_ = new QTimer(this);
  connect(animationTimer_, &QTimer::timeout, this, &ThinkingIndicator::updateAnimation);

  setFixedHeight(36);  // Compact height to match bubble sizing
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setAttribute(Qt::WA_TranslucentBackground, true);
}

void ThinkingIndicator::Start() {
  animationFrame_ = 0;
  animationTimer_->start(500);  // Update every 500ms
  update();
}

void ThinkingIndicator::Stop() {
  animationTimer_->stop();
}

void ThinkingIndicator::ApplyTheme(const AgentPanelPalette& palette) {
  backgroundColor_ = palette.thinkingBackground;
  textColor_ = palette.thinkingText;
  update();
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

  QRectF bubbleRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setBrush(backgroundColor_);
  painter.setPen(Qt::NoPen);
  painter.drawRoundedRect(bubbleRect, 18, 18);

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
  painter.setPen(textColor_);

  // Left-align with padding matching bubble style
  QRect textRect = rect().adjusted(10, 0, -10, 0);
  painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, fullText);
}

}  // namespace platform
}  // namespace athena

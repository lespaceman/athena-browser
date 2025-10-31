#include "qt_agent_panel.h"

#include "qt_mainwindow.h"
#include "runtime/node_runtime.h"

#include <QApplication>
#include <QAbstractSlider>
#include <QDebug>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>

namespace athena {
namespace platform {

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
      headers_received_(false),
      current_session_id_(),
      palette_(),
      autoScrollEnabled_(true),
      suppressScrollEvents_(false),
      pendingScrollToBottom_(false),
      pendingScrollAnimated_(false) {
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
  messagesContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  messagesLayout_ = new QVBoxLayout(messagesContainer_);
  messagesLayout_->setContentsMargins(12, 12, 12, 16);
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
    placeholder = darkMode ? Lighten(text, 180) : Darken(text, 130);
  }

  palette.panelBackground = window;
  palette.panelBorder = darkMode ? Lighten(window, 130) : Darken(window, 110);
  palette.messageAreaBackground = window;
  palette.keyboardHintText = placeholder;
  palette.thinkingBackground = darkMode ? Darken(window, 120) : Lighten(window, 108);
  palette.thinkingText = darkMode ? Lighten(text, 140) : Darken(text, 120);
  palette.secondaryText = palette.keyboardHintText;
  palette.accent = highlight;

  palette.scrollbar.track = darkMode ? Darken(window, 130) : Lighten(window, 115);
  palette.scrollbar.thumb = darkMode ? Darken(highlight, 130) : Darken(highlight, 110);
  palette.scrollbar.thumbHover = darkMode ? Darken(highlight, 110) : Darken(highlight, 130);

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
  assistantBubble.background = darkMode ? Darken(window, 130) : Lighten(window, 112);
  assistantBubble.text = darkMode ? Lighten(text, 150) : text;
  assistantBubble.label = darkMode ? Lighten(text, 130) : Darken(text, 120);
  QColor assistantCodeBg = darkMode ? Darken(window, 120) : Lighten(window, 120);
  assistantBubble.codeBackground = assistantCodeBg;
  assistantBubble.codeText = assistantBubble.text;
  palette.assistantBubble = assistantBubble;

  QColor inputBackground = darkMode ? Darken(window, 140) : Lighten(base, 108);
  palette.input = {inputBackground,
                   darkMode ? Darken(window, 110) : Darken(inputBackground, 110),
                   highlight,
                   darkMode ? Lighten(text, 150) : text,
                   placeholder,
                   highlight};

  // Composer (input card) background should be slightly darker than input field
  palette.composerBackground =
      darkMode ? Darken(inputBackground, 105) : Darken(inputBackground, 103);
  palette.composerBorder = darkMode ? Darken(window, 110) : Lighten(window, 110);
  palette.composerShadow = QColor(0, 0, 0, darkMode ? 32 : 40);

  palette.sendButton = {highlight,
                        Darken(highlight, 110),
                        Darken(highlight, 130),
                        darkMode ? Darken(window, 120) : Lighten(window, 120),
                        highlightedText,
                        darkMode ? Lighten(window, 170) : Darken(window, 150)};

  QColor stopBackground = darkMode ? Darken(window, 120) : Lighten(window, 114);
  palette.stopButton = {stopBackground,
                        Darken(stopBackground, 110),
                        Darken(stopBackground, 125),
                        darkMode ? Darken(window, 110) : Lighten(window, 125),
                        darkMode ? Lighten(text, 160) : Darken(text, 110),
                        darkMode ? Lighten(window, 180) : Darken(window, 140)};

  palette.chip = {darkMode ? Darken(window, 130) : Lighten(window, 120),
                  darkMode ? Lighten(text, 160) : Darken(text, 110),
                  darkMode ? Darken(window, 110) : Lighten(window, 130)};

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
                    .arg(ColorToCss(palette_.panelBackground), ColorToCss(palette_.panelBorder)));

  applyPaletteToScrollArea();
  applyPaletteToInput();
  applyPaletteToButtons();
  applyPaletteToMessages();
  applyPaletteToThinkingIndicator();
  refreshSendStopIcons();
}

void AgentPanel::applyPaletteToScrollArea() {
  messagesContainer_->setStyleSheet(QStringLiteral("QWidget { background-color: %1; }")
                                        .arg(ColorToCss(palette_.messageAreaBackground)));

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
                                 .arg(ColorToCss(palette_.messageAreaBackground),
                                      ColorToCss(palette_.scrollbar.track),
                                      ColorToCss(palette_.scrollbar.thumb),
                                      ColorToCss(palette_.scrollbar.thumbHover)));
}

void AgentPanel::applyPaletteToInput() {
  inputFrame_->setStyleSheet(QStringLiteral("QFrame { background-color: %1; border: none; }")
                                 .arg(ColorToCss(palette_.panelBackground)));
  inputCard_->setStyleSheet(
      QStringLiteral("QFrame { background-color: %1; border-radius: 0px; border: none; }")
          .arg(ColorToCss(palette_.composerBackground)));

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
        .arg(ColorToCss(colors.background),
             ColorToCss(colors.backgroundHover),
             ColorToCss(colors.backgroundPressed),
             ColorToCss(colors.backgroundDisabled));
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
  sendButton_->setIcon(CreateSendIcon(sendColor, devicePixelRatio));

  const QColor stopColor =
      stopButton_->isEnabled() ? palette_.stopButton.icon : palette_.stopButton.iconDisabled;
  stopButton_->setIcon(CreateStopIcon(stopColor, devicePixelRatio));
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

void AgentPanel::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);

  // Update maximum width constraints for all chat bubbles when panel is resized
  // This ensures text wraps properly at any panel width
  if (scrollArea_ && scrollArea_->viewport()) {
    // Calculate available width: viewport width minus container margins
    int viewportWidth = scrollArea_->viewport()->width();
    int containerMargins = 24;  // 12px left + 12px right from messagesLayout_
    int maxBubbleWidth = viewportWidth - containerMargins;

    // Update all existing chat bubbles
    for (int i = 0; i < messagesLayout_->count(); ++i) {
      QLayoutItem* item = messagesLayout_->itemAt(i);
      if (item && item->widget()) {
        ChatBubble* bubble = qobject_cast<ChatBubble*>(item->widget());
        if (bubble) {
          bubble->setMaximumWidth(maxBubbleWidth);
          bubble->updateGeometry();
        }
      }
    }
  }
}

void AgentPanel::connectSignals() {
  connect(sendButton_, &QPushButton::clicked, this, &AgentPanel::onSendClicked);
  connect(stopButton_, &QPushButton::clicked, this, &AgentPanel::onStopClicked);

  connect(inputWidget_, &ChatInputWidget::sendRequested, this, &AgentPanel::onSendClicked);

  connect(inputWidget_, &QTextEdit::textChanged, this, &AgentPanel::onInputTextChanged);

  if (scrollArea_) {
    QScrollBar* scrollBar = scrollArea_->verticalScrollBar();
    connect(scrollBar, &QScrollBar::valueChanged, this, &AgentPanel::onScrollValueChanged);
    connect(scrollBar,
            &QAbstractSlider::actionTriggered,
            this,
            &AgentPanel::onScrollActionTriggered);
    connect(scrollBar, &QAbstractSlider::sliderPressed, this, &AgentPanel::onScrollSliderPressed);
    connect(scrollBar, &QAbstractSlider::sliderReleased, this, &AgentPanel::onScrollSliderReleased);
  }
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

  // Set maximum width constraint to prevent overflow
  // This ensures text wraps properly within the panel
  if (scrollArea_ && scrollArea_->viewport()) {
    int viewportWidth = scrollArea_->viewport()->width();
    int containerMargins = 24;  // 12px left + 12px right from messagesLayout_
    int maxBubbleWidth = viewportWidth - containerMargins;
    bubble->setMaximumWidth(maxBubbleWidth);
  }

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
  if (!thinkingIndicator_) {
    return;
  }

  // Thinking bubble is disabled; ensure the indicator stays hidden regardless of state.
  thinkingIndicator_->Stop();
  thinkingIndicator_->hide();

  if (!show) {
    scrollToBottom(true);
  }
}

void AgentPanel::scrollToBottom(bool animated) {
  scheduleScrollToBottom(animated);
}

void AgentPanel::scheduleScrollToBottom(bool animated) {
  pendingScrollAnimated_ = pendingScrollAnimated_ || animated;

  if (pendingScrollToBottom_) {
    return;
  }

  pendingScrollToBottom_ = true;
  QTimer::singleShot(0, this, &AgentPanel::flushPendingScroll);
}

void AgentPanel::flushPendingScroll() {
  if (!pendingScrollToBottom_) {
    return;
  }

  pendingScrollToBottom_ = false;
  bool animated = pendingScrollAnimated_;
  pendingScrollAnimated_ = false;

  performScrollToBottom(animated);
}

void AgentPanel::performScrollToBottom(bool animated) {
  if (!scrollArea_) {
    return;
  }

  QScrollBar* scrollBar = scrollArea_->verticalScrollBar();
  if (!scrollBar) {
    return;
  }

  // Only auto-scroll when the user hasn't intentionally scrolled away
  if (!autoScrollEnabled_ && !isNearBottom()) {
    return;
  }

  const int currentValue = scrollBar->value();
  const int maxValue = scrollBar->maximum();

  if (maxValue <= 0 || currentValue == maxValue) {
    updateAutoScrollStateFromPosition();
    return;
  }

  if (animated && (maxValue - currentValue) > 4) {
    if (scrollAnimation_) {
      scrollAnimation_->stop();
    }

    auto* animation = new QPropertyAnimation(scrollBar, "value", this);
    scrollAnimation_ = animation;
    animation->setDuration(260);
    animation->setStartValue(currentValue);
    animation->setEndValue(maxValue);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    suppressScrollEvents_ = true;
    connect(animation, &QPropertyAnimation::finished, this, [this, animation]() {
      suppressScrollEvents_ = false;
      updateAutoScrollStateFromPosition();
      if (scrollAnimation_ == animation) {
        scrollAnimation_.clear();
      }
      animation->deleteLater();
    });

    animation->start();
  } else {
    if (scrollAnimation_) {
      scrollAnimation_->stop();
    }

    suppressScrollEvents_ = true;
    scrollBar->setValue(maxValue);
    suppressScrollEvents_ = false;
    updateAutoScrollStateFromPosition();
  }
}

void AgentPanel::onScrollValueChanged(int /*value*/) {
  if (suppressScrollEvents_) {
    return;
  }

  updateAutoScrollStateFromPosition();
}

void AgentPanel::onScrollActionTriggered(int action) {
  if (suppressScrollEvents_) {
    return;
  }

  if (action != QAbstractSlider::SliderToMaximum) {
    autoScrollEnabled_ = false;
  }

  updateAutoScrollStateFromPosition();
}

void AgentPanel::onScrollSliderPressed() {
  if (suppressScrollEvents_) {
    return;
  }

  autoScrollEnabled_ = false;
}

void AgentPanel::onScrollSliderReleased() {
  if (suppressScrollEvents_) {
    return;
  }

  updateAutoScrollStateFromPosition();
}

void AgentPanel::updateAutoScrollStateFromPosition() {
  if (!scrollArea_) {
    autoScrollEnabled_ = true;
    return;
  }

  QScrollBar* scrollBar = scrollArea_->verticalScrollBar();
  if (!scrollBar) {
    autoScrollEnabled_ = true;
    return;
  }

  autoScrollEnabled_ = isNearBottom();
}

bool AgentPanel::isNearBottom() const {
  if (!scrollArea_) {
    return true;
  }

  QScrollBar* scrollBar = scrollArea_->verticalScrollBar();
  if (!scrollBar) {
    return true;
  }

  const int maxValue = scrollBar->maximum();
  const int currentValue = scrollBar->value();
  return (maxValue - currentValue) <= kAutoScrollLockThresholdPx;
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

      // Unescape JSON sequences (order matters: backslashes must be first!)
      chunk_content.replace("\\\\", "\x01");  // Temp placeholder for escaped backslash
      chunk_content.replace("\\n", "\n");
      chunk_content.replace("\\r", "\r");
      chunk_content.replace("\\t", "\t");
      chunk_content.replace("\\\"", "\"");
      chunk_content.replace("\x01", "\\");  // Restore escaped backslashes

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

}  // namespace platform
}  // namespace athena

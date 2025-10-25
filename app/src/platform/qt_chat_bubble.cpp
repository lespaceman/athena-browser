#include "qt_chat_bubble.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QTextEdit>
#include <QTextOption>
#include <QVBoxLayout>

namespace athena {
namespace platform {

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
  layout_->setContentsMargins(14, 6, 14, 6);
  layout_->setSpacing(1);

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
      border-radius: 6px;
    }
    QLabel {
      color: %2;
      background-color: transparent;
      font-weight: 600;
    }
  )")
                    .arg(ColorToCss(bubblePalette_.background), ColorToCss(bubblePalette_.label)));

  QPalette textPalette = contentWidget_->palette();
  textPalette.setColor(QPalette::Base, bubblePalette_.background);
  textPalette.setColor(QPalette::Text, bubblePalette_.text);
  textPalette.setColor(QPalette::Highlight, palette.accent);
  textPalette.setColor(QPalette::HighlightedText,
                       palette.dark ? QColor("#0F172A") : QColor("#FFFFFF"));
  contentWidget_->setPalette(textPalette);

  QString defaultCSS =
      QStringLiteral(
          "body { color: %1; background-color: %2; font-size: 14px; } "
          "code { background-color: %3; color: %4; padding: 2px 4px; border-radius: 4px; "
          "font-family: 'Fira Code', 'JetBrains Mono', monospace; } "
          "a { color: %5; text-decoration: none; font-weight: 600; } "
          "a:hover { text-decoration: underline; } "
          "strong { font-weight: 600; } "
          "em { font-style: italic; } "
          "ul { padding-left: 20px; margin: 12px 0; } "
          "li { margin-bottom: 6px; }")
          .arg(ColorToCss(bubblePalette_.text),
               ColorToCss(bubblePalette_.background),
               ColorToCss(bubblePalette_.codeBackground),
               ColorToCss(bubblePalette_.codeText),
               ColorToCss(palette.accent));

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

  QString wrappedHtml =
      QStringLiteral(
          "<div style='line-height:1.4; word-wrap:break-word; white-space:pre-wrap;'>%1</div>")
          .arg(html);
  contentWidget_->setHtml(wrappedHtml);

  int availableWidth = contentWidget_->viewport()->width();
  if (availableWidth <= 0) {
    availableWidth = qMax(220, width() - 36);
  }
  contentWidget_->document()->setTextWidth(availableWidth);

  QSizeF docSize = contentWidget_->document()->size();
  int idealHeight = static_cast<int>(docSize.height());
  idealHeight = qMin(idealHeight, 600);  // Only enforce maximum, no minimum

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

}  // namespace platform
}  // namespace athena

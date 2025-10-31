#include "qt_chat_bubble.h"

#include <cmath>
#include <QAbstractTextDocumentLayout>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMargins>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSize>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextOption>
#include <QTimer>
#include <QVBoxLayout>

namespace athena {
namespace platform {

namespace {
// Helper function to normalize markdown spacing
// Ensures headers and sections have proper line breaks
QString normalizeMarkdownSpacing(const QString& markdown) {
  QString result = markdown;

  // Ensure headers (lines starting with #) have a blank line before them
  // But only if they're not at the start and don't already have spacing
  // Pattern: (non-newline characters)(no newlines or single newline)(## header)
  // Replace with: (text)\n\n(## header)
  result.replace(QRegularExpression("([^\n])(\n?)(#{1,6} )"), "\\1\n\n\\3");

  return result;
}
}  // namespace

ChatBubble::ChatBubble(Role role,
                       const QString& message,
                       const AgentPanelPalette& palette,
                       QWidget* parent)
    : QFrame(parent), role_(role), message_(message), geometryUpdateScheduled_(false) {
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
  contentWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  contentWidget_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  contentWidget_->setLineWrapMode(QTextEdit::WidgetWidth);
  contentWidget_->setAutoFillBackground(false);

  QFont contentFont = contentWidget_->font();
  contentFont.setPixelSize(14);
  contentWidget_->setFont(contentFont);

  layout_->addWidget(contentWidget_);

  setFrameShape(QFrame::NoFrame);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
  // Normalize markdown spacing before rendering
  // Fix: Ensure headers have proper line breaks before them
  QString normalized = normalizeMarkdownSpacing(markdown);

  // Use Qt's native markdown support (available since Qt 5.14, we're on Qt 6.6.2)
  // This is much more robust than manual HTML conversion
  contentWidget_->setMarkdown(normalized);

  // Apply proper spacing for readability
  // Add document margin for breathing room (default is 4, we increase to 8)
  contentWidget_->document()->setDocumentMargin(8);

  // Apply block formatting for all paragraphs
  QTextCursor cursor(contentWidget_->document());
  cursor.select(QTextCursor::Document);

  QTextBlockFormat blockFormat;
  // Add spacing between paragraphs (12px bottom margin)
  blockFormat.setBottomMargin(12);
  // Add comfortable line height within paragraphs (140% = 1.4x normal height)
  blockFormat.setLineHeight(140, QTextBlockFormat::ProportionalHeight);

  cursor.mergeBlockFormat(blockFormat);

  // Update geometry - will set proper text width based on available space
  updateContentGeometry();
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

void ChatBubble::resizeEvent(QResizeEvent* event) {
  QFrame::resizeEvent(event);
  updateContentGeometry();
}

void ChatBubble::showEvent(QShowEvent* event) {
  QFrame::showEvent(event);
  // When widget is first shown, ensure content geometry is updated with valid dimensions
  updateContentGeometry();
}

void ChatBubble::updateContentGeometry() {
  if (!contentWidget_) {
    return;
  }

  QTextDocument* document = contentWidget_->document();
  if (!document) {
    return;
  }

  // Get the actual width available for content
  // The parent (AgentPanel) sets our maximum width via resizeEvent,
  // so we use our current width minus internal margins
  const QMargins margins = layout_ ? layout_->contentsMargins() : QMargins();
  int availableWidth = width() - margins.left() - margins.right();

  // If width is not yet available (widget not fully laid out), defer update
  if (availableWidth <= 0) {
    if (!geometryUpdateScheduled_) {
      geometryUpdateScheduled_ = true;
      QTimer::singleShot(0, this, [this]() {
        geometryUpdateScheduled_ = false;
        updateContentGeometry();
      });
    }
    return;
  }

  // Set the document's text width to match the available width
  // This ensures text wraps properly within the visible area
  document->setTextWidth(availableWidth);
  document->adjustSize();

  const qreal docHeight = document->documentLayout()->documentSize().height();
  const int frameWidth = contentWidget_->frameWidth();
  const int margin = static_cast<int>(std::ceil(document->documentMargin()));
  const int totalHeight = static_cast<int>(std::ceil(docHeight)) + 2 * (margin + frameWidth);
  const int adjustedHeight = qMax(totalHeight, 0);
  contentWidget_->setFixedHeight(adjustedHeight);

  const int layoutMarginsTB =
      layout_ ? layout_->contentsMargins().top() + layout_->contentsMargins().bottom() : 0;
  const int spacing = layout_ ? layout_->spacing() : 0;
  const int roleHeight = roleLabel_ ? roleLabel_->sizeHint().height() : 0;

  const int bubbleHeight = adjustedHeight + layoutMarginsTB + spacing + roleHeight;

  setMinimumHeight(bubbleHeight);
  setMaximumHeight(bubbleHeight);

  contentWidget_->updateGeometry();
  updateGeometry();
}

QSize ChatBubble::sizeHint() const {
  if (!contentWidget_ || !layout_) {
    return QFrame::sizeHint();
  }

  // Calculate preferred width based on parent container
  int preferredWidth = 400;  // Default preference
  if (parentWidget()) {
    // Prefer to use parent's width
    preferredWidth = parentWidget()->width();
  }

  // Calculate height based on current content
  int totalHeight = minimumHeight();
  if (totalHeight <= 0) {
    totalHeight = 100;  // Reasonable default
  }

  return QSize(preferredWidth, totalHeight);
}

QSize ChatBubble::minimumSizeHint() const {
  if (!contentWidget_ || !layout_) {
    return QFrame::minimumSizeHint();
  }

  // Minimum width should be quite narrow to allow flexible layouts
  // This ensures the bubble can shrink when the panel is narrow
  const int minWidth = 200;

  // Minimum height based on content
  int minHeight = minimumHeight();
  if (minHeight <= 0) {
    minHeight = 50;  // Reasonable default
  }

  return QSize(minWidth, minHeight);
}

}  // namespace platform
}  // namespace athena

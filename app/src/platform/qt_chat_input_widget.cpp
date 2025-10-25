#include "qt_chat_input_widget.h"

#include <QKeyEvent>
#include <QTextDocument>

namespace athena {
namespace platform {

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
  QColor focusBackground = palette.dark ? Darken(palette.input.background, 90) : QColor("#FFFFFF");

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
  style = style.arg(ColorToCss(palette.input.background),
                    ColorToCss(palette.input.border),
                    ColorToCss(palette.input.text),
                    ColorToCss(palette.input.borderFocused),
                    ColorToCss(focusBackground),
                    ColorToCss(palette.input.caret));
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
      QStringLiteral("body { color: %1; }").arg(ColorToCss(palette.input.text)));

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

}  // namespace platform
}  // namespace athena

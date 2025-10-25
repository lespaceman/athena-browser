#include "qt_thinking_indicator.h"

#include <QPainter>
#include <QTimer>

namespace athena {
namespace platform {

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
  painter.drawRoundedRect(bubbleRect, 6, 6);

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

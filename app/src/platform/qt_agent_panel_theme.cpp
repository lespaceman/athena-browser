#include "qt_agent_panel_theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace athena {
namespace platform {

QString ColorToCss(const QColor& color) {
  return color.alpha() == 255 ? color.name(QColor::HexRgb) : color.name(QColor::HexArgb);
}

QColor Lighten(const QColor& color, int percentage) {
  QColor c = color;
  return c.lighter(percentage);
}

QColor Darken(const QColor& color, int percentage) {
  QColor c = color;
  return c.darker(percentage);
}

QIcon CreateSendIcon(const QColor& color, qreal devicePixelRatio) {
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

QIcon CreateStopIcon(const QColor& color, qreal devicePixelRatio) {
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

}  // namespace platform
}  // namespace athena

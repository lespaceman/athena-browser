#ifndef ATHENA_PLATFORM_QT_THINKING_INDICATOR_H_
#define ATHENA_PLATFORM_QT_THINKING_INDICATOR_H_

#include "qt_agent_panel_theme.h"

#include <QWidget>

class QPaintEvent;
class QTimer;

namespace athena {
namespace platform {

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

#endif  // ATHENA_PLATFORM_QT_THINKING_INDICATOR_H_

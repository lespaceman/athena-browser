#ifndef ATHENA_PLATFORM_QT_CHAT_INPUT_WIDGET_H_
#define ATHENA_PLATFORM_QT_CHAT_INPUT_WIDGET_H_

#include "qt_agent_panel_theme.h"

#include <QTextEdit>

class QKeyEvent;
class QFocusEvent;

namespace athena {
namespace platform {

/**
 * Custom chat input widget with multiline support.
 *
 * Features:
 * - Auto-expanding height (1-5 lines)
 * - Enter to send, Shift+Enter for newline
 * - Placeholder text
 * - Focus styling
 */
class ChatInputWidget : public QTextEdit {
  Q_OBJECT

 public:
  explicit ChatInputWidget(QWidget* parent = nullptr);

  /**
   * Get the input text.
   */
  QString GetText() const;

  /**
   * Clear the input text.
   */
  void Clear();

  /**
   * Apply themed styling.
   */
  void ApplyTheme(const AgentPanelPalette& palette);

 signals:
  /**
   * Emitted when user presses Enter (without Shift).
   */
  void sendRequested();

  /**
   * Emitted when focus state changes.
   */
  void focusChanged(bool focused);

 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;

 private slots:
  void adjustHeight();

 private:
  void setupUI();
  int calculateIdealHeight();
  void applyPalette(const AgentPanelPalette& palette);

  static constexpr int MIN_HEIGHT = 40;
  static constexpr int MAX_HEIGHT = 120;
  AgentPanelPalette currentPalette_;
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_CHAT_INPUT_WIDGET_H_

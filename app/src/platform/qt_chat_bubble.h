#ifndef ATHENA_PLATFORM_QT_CHAT_BUBBLE_H_
#define ATHENA_PLATFORM_QT_CHAT_BUBBLE_H_

#include "qt_agent_panel_theme.h"

#include <QFrame>
#include <QString>

class QGraphicsOpacityEffect;
class QLabel;
class QPropertyAnimation;
class QTextEdit;
class QVBoxLayout;

namespace athena {
namespace platform {

/**
 * Chat message bubble with role-specific styling.
 *
 * Features:
 * - Different colors for user vs assistant
 * - Markdown rendering
 * - Code syntax highlighting
 * - Copy button
 * - Smooth fade-in animation
 */
class ChatBubble : public QFrame {
  Q_OBJECT

 public:
  enum class Role { User, Assistant };

  explicit ChatBubble(Role role,
                      const QString& message,
                      const AgentPanelPalette& palette,
                      QWidget* parent = nullptr);

  /**
   * Update the message content.
   * Used for replacing thinking indicator with actual response.
   */
  void SetMessage(const QString& message);

  /**
   * Get the message content.
   */
  QString GetMessage() const;

  /**
   * Get the role.
   */
  Role GetRole() const { return role_; }

  /**
   * Animate the bubble appearing.
   */
  void AnimateIn();

  /**
   * Apply theme colors to the bubble.
   */
  void ApplyTheme(const AgentPanelPalette& palette);

 private:
  void setupUI();
  void renderMarkdown(const QString& markdown);
  void applyPalette(const AgentPanelPalette& palette);

  Role role_;
  QString message_;
  BubblePalette bubblePalette_;

  // UI Components
  QVBoxLayout* layout_;
  QLabel* roleLabel_;         // "You" or "Agent"
  QTextEdit* contentWidget_;  // Message content (read-only)

  // Animation
  QGraphicsOpacityEffect* opacityEffect_;
  QPropertyAnimation* fadeInAnimation_;
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_CHAT_BUBBLE_H_

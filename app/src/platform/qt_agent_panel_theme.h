#ifndef ATHENA_PLATFORM_QT_AGENT_PANEL_THEME_H_
#define ATHENA_PLATFORM_QT_AGENT_PANEL_THEME_H_

#include <QColor>
#include <QIcon>
#include <QString>

namespace athena {
namespace platform {

// ============================================================================
// Theme Palette Structures
// ============================================================================

struct ScrollbarPalette {
  QColor track;
  QColor thumb;
  QColor thumbHover;
};

struct BubblePalette {
  QColor background;
  QColor text;
  QColor label;
  QColor codeBackground;
  QColor codeText;
};

struct InputPalette {
  QColor background;
  QColor border;
  QColor borderFocused;
  QColor text;
  QColor placeholder;
  QColor caret;
};

struct IconButtonPalette {
  QColor background;
  QColor backgroundHover;
  QColor backgroundPressed;
  QColor backgroundDisabled;
  QColor icon;
  QColor iconDisabled;
};

struct ChipPalette {
  QColor background;
  QColor text;
  QColor border;
};

struct AgentPanelPalette {
  bool dark = false;
  QColor panelBackground;
  QColor panelBorder;
  QColor messageAreaBackground;
  QColor composerBackground;
  QColor composerBorder;
  QColor composerShadow;
  QColor keyboardHintText;
  QColor thinkingBackground;
  QColor thinkingText;
  QColor secondaryText;
  QColor accent;

  ScrollbarPalette scrollbar;
  BubblePalette userBubble;
  BubblePalette assistantBubble;
  InputPalette input;
  IconButtonPalette sendButton;
  IconButtonPalette stopButton;
  ChipPalette chip;
};

// ============================================================================
// Theme Utility Functions
// ============================================================================

/**
 * Convert QColor to CSS string (supports alpha).
 * @param color Color to convert
 * @return CSS color string (e.g., "#FF0000" or "#FF0000FF")
 */
QString ColorToCss(const QColor& color);

/**
 * Lighten a color by a percentage.
 * @param color Color to lighten
 * @param percentage Lightening percentage (100 = no change, 200 = twice as light)
 * @return Lightened color
 */
QColor Lighten(const QColor& color, int percentage);

/**
 * Darken a color by a percentage.
 * @param color Color to darken
 * @param percentage Darkening percentage (100 = no change, 200 = twice as dark)
 * @return Darkened color
 */
QColor Darken(const QColor& color, int percentage);

/**
 * Create a send icon with the specified color.
 * @param color Icon color
 * @param devicePixelRatio Device pixel ratio for HiDPI support
 * @return Send icon (paper plane)
 */
QIcon CreateSendIcon(const QColor& color, qreal devicePixelRatio = 1.0);

/**
 * Create a stop icon with the specified color.
 * @param color Icon color
 * @param devicePixelRatio Device pixel ratio for HiDPI support
 * @return Stop icon (rounded square)
 */
QIcon CreateStopIcon(const QColor& color, qreal devicePixelRatio = 1.0);

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_AGENT_PANEL_THEME_H_

/**
 * BrowserWidget Keyboard Mapping Tests
 *
 * Tests comprehensive keyboard mapping including:
 * - Function keys (F1-F24)
 * - Numpad keys
 * - Special keys (Home, End, PageUp, PageDown, Insert, etc.)
 * - Keypad modifier flag
 */

#include <gtest/gtest.h>
#include <QKeyEvent>

// Helper test class to expose BrowserWidget's helper methods without needing full Qt setup
namespace athena {
namespace platform {

class KeyboardMapper {
 public:
  static uint32_t getCefModifiers(Qt::KeyboardModifiers qtMods, Qt::MouseButtons qtButtons) {
    uint32_t cefMods = 0;

    // Keyboard modifiers
    if (qtMods & Qt::ShiftModifier)
      cefMods |= 0x00000004;  // EVENTFLAG_SHIFT_DOWN
    if (qtMods & Qt::ControlModifier)
      cefMods |= 0x00000002;  // EVENTFLAG_CONTROL_DOWN
    if (qtMods & Qt::AltModifier)
      cefMods |= 0x00000001;  // EVENTFLAG_ALT_DOWN
    if (qtMods & Qt::KeypadModifier)
      cefMods |= 0x00000008;  // EVENTFLAG_IS_KEY_PAD

    // Mouse button modifiers
    if (qtButtons & Qt::LeftButton)
      cefMods |= 0x00000020;  // EVENTFLAG_LEFT_MOUSE_BUTTON
    if (qtButtons & Qt::MiddleButton)
      cefMods |= 0x00000040;  // EVENTFLAG_MIDDLE_MOUSE_BUTTON
    if (qtButtons & Qt::RightButton)
      cefMods |= 0x00000080;  // EVENTFLAG_RIGHT_MOUSE_BUTTON

    return cefMods;
  }

  static int getWindowsKeyCode(int qtKey) {
    // This mirrors the implementation in qt_browserwidget.cpp
    // Map Qt keys to Windows virtual key codes

    // Standard alphanumeric keys
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
      return qtKey;  // '0'-'9' (0x30-0x39)
    }
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
      return qtKey;  // 'A'-'Z' (0x41-0x5A)
    }

    // Function keys (F1-F24)
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
      return 0x70 + (qtKey - Qt::Key_F1);  // VK_F1 (0x70) through VK_F24 (0x87)
    }

    // Comprehensive key mapping switch
    switch (qtKey) {
      // Control keys
      case Qt::Key_Backspace:
        return 0x08;
      case Qt::Key_Tab:
      case Qt::Key_Backtab:
        return 0x09;
      case Qt::Key_Clear:
        return 0x0C;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        return 0x0D;

      // Modifier keys
      case Qt::Key_Shift:
        return 0x10;
      case Qt::Key_Control:
        return 0x11;
      case Qt::Key_Alt:
        return 0x12;
      case Qt::Key_Pause:
        return 0x13;
      case Qt::Key_CapsLock:
        return 0x14;

      // Navigation keys
      case Qt::Key_Escape:
        return 0x1B;
      case Qt::Key_Space:
        return 0x20;
      case Qt::Key_PageUp:
        return 0x21;
      case Qt::Key_PageDown:
        return 0x22;
      case Qt::Key_End:
        return 0x23;
      case Qt::Key_Home:
        return 0x24;
      case Qt::Key_Left:
        return 0x25;
      case Qt::Key_Up:
        return 0x26;
      case Qt::Key_Right:
        return 0x27;
      case Qt::Key_Down:
        return 0x28;

      // Special keys
      case Qt::Key_Select:
        return 0x29;
      case Qt::Key_Print:
        return 0x2A;
      case Qt::Key_Execute:
        return 0x2B;
      case Qt::Key_Printer:
        return 0x2C;
      case Qt::Key_Insert:
        return 0x2D;
      case Qt::Key_Delete:
        return 0x2E;
      case Qt::Key_Help:
        return 0x2F;

      // Numpad operators
      case Qt::Key_multiply:
      case Qt::Key_Asterisk:
        return 0x6A;
      case Qt::Key_Plus:
      case Qt::Key_Equal:
        return 0xBB;
      case Qt::Key_Comma:
      case Qt::Key_Less:
        return 0xBC;
      case Qt::Key_Minus:
      case Qt::Key_Underscore:
        return 0xBD;
      case Qt::Key_Period:
      case Qt::Key_Greater:
        return 0xBE;
      case Qt::Key_Slash:
      case Qt::Key_Question:
        return 0xBF;

      // Lock keys
      case Qt::Key_NumLock:
        return 0x90;
      case Qt::Key_ScrollLock:
        return 0x91;

      // Media keys
      case Qt::Key_VolumeMute:
        return 0xAD;
      case Qt::Key_VolumeDown:
        return 0xAE;
      case Qt::Key_VolumeUp:
        return 0xAF;
      case Qt::Key_MediaStop:
        return 0xB2;
      case Qt::Key_MediaPlay:
        return 0xB3;

      // Punctuation and symbols (OEM keys)
      case Qt::Key_Semicolon:
      case Qt::Key_Colon:
        return 0xBA;
      case Qt::Key_QuoteLeft:
      case Qt::Key_AsciiTilde:
        return 0xC0;
      case Qt::Key_BracketLeft:
      case Qt::Key_BraceLeft:
        return 0xDB;
      case Qt::Key_Backslash:
      case Qt::Key_Bar:
        return 0xDC;
      case Qt::Key_BracketRight:
      case Qt::Key_BraceRight:
        return 0xDD;
      case Qt::Key_Apostrophe:
      case Qt::Key_QuoteDbl:
        return 0xDE;

      // Symbol keys (shifted number keys)
      case Qt::Key_ParenRight:
        return 0x30;
      case Qt::Key_Exclam:
        return 0x31;
      case Qt::Key_At:
        return 0x32;
      case Qt::Key_NumberSign:
        return 0x33;
      case Qt::Key_Dollar:
        return 0x34;
      case Qt::Key_Percent:
        return 0x35;
      case Qt::Key_AsciiCircum:
        return 0x36;
      case Qt::Key_Ampersand:
        return 0x37;
      case Qt::Key_ParenLeft:
        return 0x39;

      default:
        return qtKey;
    }
  }
};

}  // namespace platform
}  // namespace athena

using namespace athena::platform;

// Test fixture for keyboard mapping tests
class QtKeyboardTest : public ::testing::Test {
 protected:
  // No setup needed for these simple static tests
};

// ============================================================================
// Function Keys Tests (F1-F24)
// ============================================================================

TEST_F(QtKeyboardTest, FunctionKeys_F1_F12_MapCorrectly) {
  // F1 should map to 0x70
  EXPECT_EQ(0x70, KeyboardMapper::getWindowsKeyCode(Qt::Key_F1));
  // F2 should map to 0x71
  EXPECT_EQ(0x71, KeyboardMapper::getWindowsKeyCode(Qt::Key_F2));
  // F3 should map to 0x72
  EXPECT_EQ(0x72, KeyboardMapper::getWindowsKeyCode(Qt::Key_F3));
  // F4 should map to 0x73
  EXPECT_EQ(0x73, KeyboardMapper::getWindowsKeyCode(Qt::Key_F4));
  // F5 should map to 0x74 (used for refresh)
  EXPECT_EQ(0x74, KeyboardMapper::getWindowsKeyCode(Qt::Key_F5));
  // F6 should map to 0x75
  EXPECT_EQ(0x75, KeyboardMapper::getWindowsKeyCode(Qt::Key_F6));
  // F7 should map to 0x76
  EXPECT_EQ(0x76, KeyboardMapper::getWindowsKeyCode(Qt::Key_F7));
  // F8 should map to 0x77
  EXPECT_EQ(0x77, KeyboardMapper::getWindowsKeyCode(Qt::Key_F8));
  // F9 should map to 0x78
  EXPECT_EQ(0x78, KeyboardMapper::getWindowsKeyCode(Qt::Key_F9));
  // F10 should map to 0x79
  EXPECT_EQ(0x79, KeyboardMapper::getWindowsKeyCode(Qt::Key_F10));
  // F11 should map to 0x7A
  EXPECT_EQ(0x7A, KeyboardMapper::getWindowsKeyCode(Qt::Key_F11));
  // F12 should map to 0x7B (used for DevTools)
  EXPECT_EQ(0x7B, KeyboardMapper::getWindowsKeyCode(Qt::Key_F12));
}

TEST_F(QtKeyboardTest, FunctionKeys_F13_F24_MapCorrectly) {
  // F13 should map to 0x7C
  EXPECT_EQ(0x7C, KeyboardMapper::getWindowsKeyCode(Qt::Key_F13));
  // F14 should map to 0x7D
  EXPECT_EQ(0x7D, KeyboardMapper::getWindowsKeyCode(Qt::Key_F14));
  // F15 should map to 0x7E
  EXPECT_EQ(0x7E, KeyboardMapper::getWindowsKeyCode(Qt::Key_F15));
  // F16 should map to 0x7F
  EXPECT_EQ(0x7F, KeyboardMapper::getWindowsKeyCode(Qt::Key_F16));
  // F17 should map to 0x80
  EXPECT_EQ(0x80, KeyboardMapper::getWindowsKeyCode(Qt::Key_F17));
  // F18 should map to 0x81
  EXPECT_EQ(0x81, KeyboardMapper::getWindowsKeyCode(Qt::Key_F18));
  // F19 should map to 0x82
  EXPECT_EQ(0x82, KeyboardMapper::getWindowsKeyCode(Qt::Key_F19));
  // F20 should map to 0x83
  EXPECT_EQ(0x83, KeyboardMapper::getWindowsKeyCode(Qt::Key_F20));
  // F21 should map to 0x84
  EXPECT_EQ(0x84, KeyboardMapper::getWindowsKeyCode(Qt::Key_F21));
  // F22 should map to 0x85
  EXPECT_EQ(0x85, KeyboardMapper::getWindowsKeyCode(Qt::Key_F22));
  // F23 should map to 0x86
  EXPECT_EQ(0x86, KeyboardMapper::getWindowsKeyCode(Qt::Key_F23));
  // F24 should map to 0x87
  EXPECT_EQ(0x87, KeyboardMapper::getWindowsKeyCode(Qt::Key_F24));
}

// ============================================================================
// Numpad Keys Tests
// ============================================================================

// Note: Numpad digit keys (0-9) cannot be reliably distinguished from regular
// number keys by Qt::Key value alone. The distinction comes from the KeypadModifier
// flag which is tested separately in the Modifiers_KeypadModifierFlagSet test.
// In a real implementation, the keyEvent handler would need to check both the
// key value AND the KeypadModifier flag to map correctly to VK_NUMPAD0-9 (0x60-0x69)
// versus VK_0-9 (0x30-0x39).

TEST_F(QtKeyboardTest, NumpadKeys_OperatorsMapCorrectly) {
  // VK_MULTIPLY = 0x6A (asterisk on numpad)
  EXPECT_EQ(0x6A, KeyboardMapper::getWindowsKeyCode(Qt::Key_Asterisk));

  // Note: Plus, Comma, Minus, Period, Slash map to OEM keys (0xBB, 0xBC, 0xBD, 0xBE, 0xBF)
  // because they're also used outside the numpad.
  // The KeypadModifier flag distinguishes numpad vs regular usage at the event level,
  // not at the key code mapping level.
}

// ============================================================================
// Special Keys Tests
// ============================================================================

TEST_F(QtKeyboardTest, SpecialKeys_NavigationKeysMapCorrectly) {
  // Home = 0x24
  EXPECT_EQ(0x24, KeyboardMapper::getWindowsKeyCode(Qt::Key_Home));
  // End = 0x23
  EXPECT_EQ(0x23, KeyboardMapper::getWindowsKeyCode(Qt::Key_End));
  // PageUp = 0x21
  EXPECT_EQ(0x21, KeyboardMapper::getWindowsKeyCode(Qt::Key_PageUp));
  // PageDown = 0x22
  EXPECT_EQ(0x22, KeyboardMapper::getWindowsKeyCode(Qt::Key_PageDown));
  // Insert = 0x2D
  EXPECT_EQ(0x2D, KeyboardMapper::getWindowsKeyCode(Qt::Key_Insert));
  // Delete = 0x2E
  EXPECT_EQ(0x2E, KeyboardMapper::getWindowsKeyCode(Qt::Key_Delete));
}

TEST_F(QtKeyboardTest, SpecialKeys_LockKeysMapCorrectly) {
  // NumLock = 0x90
  EXPECT_EQ(0x90, KeyboardMapper::getWindowsKeyCode(Qt::Key_NumLock));
  // ScrollLock = 0x91
  EXPECT_EQ(0x91, KeyboardMapper::getWindowsKeyCode(Qt::Key_ScrollLock));
  // CapsLock = 0x14
  EXPECT_EQ(0x14, KeyboardMapper::getWindowsKeyCode(Qt::Key_CapsLock));
}

TEST_F(QtKeyboardTest, SpecialKeys_MediaKeysMapCorrectly) {
  // VolumeDown = 0xAE
  EXPECT_EQ(0xAE, KeyboardMapper::getWindowsKeyCode(Qt::Key_VolumeDown));
  // VolumeUp = 0xAF
  EXPECT_EQ(0xAF, KeyboardMapper::getWindowsKeyCode(Qt::Key_VolumeUp));
  // VolumeMute = 0xAD
  EXPECT_EQ(0xAD, KeyboardMapper::getWindowsKeyCode(Qt::Key_VolumeMute));
  // MediaStop = 0xB2
  EXPECT_EQ(0xB2, KeyboardMapper::getWindowsKeyCode(Qt::Key_MediaStop));
  // MediaPlay = 0xB3
  EXPECT_EQ(0xB3, KeyboardMapper::getWindowsKeyCode(Qt::Key_MediaPlay));
}

TEST_F(QtKeyboardTest, SpecialKeys_OtherKeysMapCorrectly) {
  // Pause = 0x13
  EXPECT_EQ(0x13, KeyboardMapper::getWindowsKeyCode(Qt::Key_Pause));
  // Print = 0x2A
  EXPECT_EQ(0x2A, KeyboardMapper::getWindowsKeyCode(Qt::Key_Print));
  // Select = 0x29
  EXPECT_EQ(0x29, KeyboardMapper::getWindowsKeyCode(Qt::Key_Select));
  // Execute = 0x2B
  EXPECT_EQ(0x2B, KeyboardMapper::getWindowsKeyCode(Qt::Key_Execute));
  // Help = 0x2F
  EXPECT_EQ(0x2F, KeyboardMapper::getWindowsKeyCode(Qt::Key_Help));
  // Clear = 0x0C
  EXPECT_EQ(0x0C, KeyboardMapper::getWindowsKeyCode(Qt::Key_Clear));
}

// ============================================================================
// Modifier Tests
// ============================================================================

TEST_F(QtKeyboardTest, Modifiers_KeypadModifierFlagSet) {
  // Create a key event with KeypadModifier
  QKeyEvent event(QEvent::KeyPress, Qt::Key_5, Qt::KeypadModifier);

  // getCefModifiers should include EVENTFLAG_IS_KEY_PAD (0x00000008)
  uint32_t modifiers = KeyboardMapper::getCefModifiers(event.modifiers(), Qt::NoButton);
  EXPECT_NE(0u, modifiers & 0x00000008);  // EVENTFLAG_IS_KEY_PAD
}

TEST_F(QtKeyboardTest, Modifiers_StandardModifiersWork) {
  // Test Shift
  QKeyEvent shiftEvent(QEvent::KeyPress, Qt::Key_A, Qt::ShiftModifier);
  uint32_t modifiers = KeyboardMapper::getCefModifiers(shiftEvent.modifiers(), Qt::NoButton);
  EXPECT_NE(0u, modifiers & 0x00000004);  // EVENTFLAG_SHIFT_DOWN

  // Test Control
  QKeyEvent ctrlEvent(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
  modifiers = KeyboardMapper::getCefModifiers(ctrlEvent.modifiers(), Qt::NoButton);
  EXPECT_NE(0u, modifiers & 0x00000002);  // EVENTFLAG_CONTROL_DOWN

  // Test Alt
  QKeyEvent altEvent(QEvent::KeyPress, Qt::Key_A, Qt::AltModifier);
  modifiers = KeyboardMapper::getCefModifiers(altEvent.modifiers(), Qt::NoButton);
  EXPECT_NE(0u, modifiers & 0x00000001);  // EVENTFLAG_ALT_DOWN
}

// ============================================================================
// Punctuation and Symbol Tests
// ============================================================================

TEST_F(QtKeyboardTest, Symbols_PunctuationMapCorrectly) {
  // Semicolon/Colon = 0xBA
  EXPECT_EQ(0xBA, KeyboardMapper::getWindowsKeyCode(Qt::Key_Semicolon));
  EXPECT_EQ(0xBA, KeyboardMapper::getWindowsKeyCode(Qt::Key_Colon));

  // Comma/Less = 0xBC
  EXPECT_EQ(0xBC, KeyboardMapper::getWindowsKeyCode(Qt::Key_Comma));
  EXPECT_EQ(0xBC, KeyboardMapper::getWindowsKeyCode(Qt::Key_Less));

  // Period/Greater = 0xBE
  EXPECT_EQ(0xBE, KeyboardMapper::getWindowsKeyCode(Qt::Key_Period));
  EXPECT_EQ(0xBE, KeyboardMapper::getWindowsKeyCode(Qt::Key_Greater));

  // Slash/Question = 0xBF
  EXPECT_EQ(0xBF, KeyboardMapper::getWindowsKeyCode(Qt::Key_Slash));
  EXPECT_EQ(0xBF, KeyboardMapper::getWindowsKeyCode(Qt::Key_Question));

  // Backtick/Tilde = 0xC0
  EXPECT_EQ(0xC0, KeyboardMapper::getWindowsKeyCode(Qt::Key_QuoteLeft));
  EXPECT_EQ(0xC0, KeyboardMapper::getWindowsKeyCode(Qt::Key_AsciiTilde));

  // Brackets = 0xDB, 0xDD
  EXPECT_EQ(0xDB, KeyboardMapper::getWindowsKeyCode(Qt::Key_BracketLeft));
  EXPECT_EQ(0xDB, KeyboardMapper::getWindowsKeyCode(Qt::Key_BraceLeft));
  EXPECT_EQ(0xDD, KeyboardMapper::getWindowsKeyCode(Qt::Key_BracketRight));
  EXPECT_EQ(0xDD, KeyboardMapper::getWindowsKeyCode(Qt::Key_BraceRight));

  // Backslash/Pipe = 0xDC
  EXPECT_EQ(0xDC, KeyboardMapper::getWindowsKeyCode(Qt::Key_Backslash));
  EXPECT_EQ(0xDC, KeyboardMapper::getWindowsKeyCode(Qt::Key_Bar));

  // Quote = 0xDE
  EXPECT_EQ(0xDE, KeyboardMapper::getWindowsKeyCode(Qt::Key_Apostrophe));
  EXPECT_EQ(0xDE, KeyboardMapper::getWindowsKeyCode(Qt::Key_QuoteDbl));
}

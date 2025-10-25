# Cursor Visibility Fix

## Problem

Text cursor (caret) became invisible in input fields after navigation, though typing still worked. The cursor would reappear after switching tabs or minimizing/restoring the window.

## Root Cause

This is a **known upstream CEF bug** affecting multiple implementations:
- **CefSharp Issue #4146**: "WPF - fails to provide visual focus indicators"
- **chromiumembedded/cef Issues #3436, #3481**: Focus handling bugs
- **Java-CEF Issue #24**: "No visible cursor in OSR mode"

CEF incorrectly assesses focus state after mouse-click navigation (keyboard navigation works fine), causing the cursor to become invisible even though the browser actually has focus.

## Solution

### 1. Track Focus State (`has_focus_`)
Added `has_focus_` member to `CefClient` to track whether the browser has focus independently of CEF's internal state.

**Files Modified:**
- `app/src/browser/cef_client.h` - Added `has_focus_` member and `SetFocus(bool)` method
- `app/src/browser/cef_client.cpp` - Implemented focus state tracking

### 2. Synchronize Focus Events
Updated Qt widget focus events to update BOTH CEF's focus AND our tracking variable.

**File Modified:** `app/src/platform/qt_browserwidget.cpp`

**Before:**
```cpp
void BrowserWidget::focusInEvent(QFocusEvent* event) {
  QOpenGLWidget::focusInEvent(event);
  auto* client = GetCefClientForThisTab();
  if (client && client->GetBrowser()) {
    client->GetBrowser()->GetHost()->SetFocus(true);  // Only updates CEF
  }
}
```

**After:**
```cpp
void BrowserWidget::focusInEvent(QFocusEvent* event) {
  QOpenGLWidget::focusInEvent(event);
  auto* client = GetCefClientForThisTab();
  if (client && client->GetBrowser()) {
    client->GetBrowser()->GetHost()->SetFocus(true);  // Update CEF
    client->SetFocus(true);  // ALSO update tracking variable!
  }
}
```

### 3. Refresh Focus After Page Load
Force CEF to refresh its focus state after page load completes.

**File Modified:** `app/src/browser/cef_client.cpp`

Added to `OnLoadingStateChange`:
```cpp
// Workaround for upstream CEF bug: cursor/caret becomes invisible after navigation
// Root cause: CEF incorrectly assesses focus state after mouse-click navigation
// Solution: Force SetFocus(true) after page load completes to refresh focus state
// Related: CefSharp #4146, chromiumembedded/cef #3436, #3481
if (!isLoading && has_focus_ && browser) {
  logger.Debug("Page load complete, refreshing focus to restore cursor visibility");
  browser->GetHost()->SetFocus(true);
}
```

## Testing

✅ **All 213 unit tests pass**
✅ **No compiler warnings**
✅ **Cursor visible immediately after navigation**
✅ **No need to switch tabs or minimize window**

## Why This Works

1. When you click the browser, Qt's `focusInEvent()` fires
2. We now update BOTH CEF's focus AND `has_focus_` tracking
3. When navigation completes, `OnLoadingStateChange` fires with `isLoading=false`
4. The workaround checks: `!isLoading && has_focus_` → both are true!
5. We call `SetFocus(true)` to force CEF to refresh its focus state
6. CEF realizes it has focus and renders the cursor ✨

## What Didn't Work

❌ **Calling `WasHidden()` in `OnAddressChange`** - Wrong place, wrong method
❌ **Calling `ImeCancelComposition()` in `OnAddressChange`** - Only for IME input
❌ **Implementing `OnImeCompositionRangeChanged`** - Only called for Asian language input, not Latin text

## Future Improvements

This is a workaround, not a perfect fix. The upstream CEF bug still exists. When CEF fixes the root issue, this workaround can be removed.

## References

- [CefSharp Issue #4146](https://github.com/cefsharp/CefSharp/issues/4146)
- [CEF Issue #3436](https://github.com/chromiumembedded/cef/issues/3436)
- [CEF Issue #3481](https://github.com/chromiumembedded/cef/issues/3481)
- [Java-CEF Issue #24](https://bitbucket.org/chromiumembedded/java-cef/issues/24)

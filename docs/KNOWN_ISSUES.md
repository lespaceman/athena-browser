# Known Issues Playbook

This document catalogs known issues, quirks, and workarounds in CEF OSR (Off-Screen Rendering) integration and platform-specific behaviors.

## Table of Contents
- [CEF Core Issues](#cef-core-issues)
- [Platform-Specific Issues](#platform-specific-issues)
- [Performance Gotchas](#performance-gotchas)
- [Input Handling](#input-handling)
- [Rendering Issues](#rendering-issues)

---

## CEF Core Issues

### 1. Focus Lost After Navigation (CEF Issue #3870) ✅ FIXED

**Status:** Fixed in Athena (app/src/browser/cef_client.cpp:91)

**Problem:**
CEF silently loses focus after navigation events (OnAddressChange), breaking keyboard input until user clicks the window again.

**Symptoms:**
- Keyboard input stops working after page navigation
- No visual indication that focus was lost
- Mouse input still works
- Clicking window restores keyboard functionality

**Root Cause:**
CEF's internal navigation logic calls SetFocus(false) and doesn't restore it.

**Solution:**
Track focus state in CefClient and restore it after address changes:

```cpp
void CefClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 const CefString& url) {
  // Restore focus if we had it before navigation
  if (has_focus_) {
    browser->GetHost()->SetFocus(true);
  }
}
```

**References:**
- https://github.com/chromiumembedded/cef/issues/3870
- QCefView workaround: CCefClientDelegate.cpp:onAddressChanged

---

### 2. Message Router Context Lifecycle

**Status:** Implemented (app/src/browser/message_router_handler.cpp)

**Problem:**
CefMessageRouter must be properly attached/detached for each browser instance, especially during renderer process crashes.

**Symptoms:**
- JS↔C++ bridge stops working after tab reload
- Promise-based cefQuery() calls never resolve
- Console errors: "cefQuery is not defined"

**Root Cause:**
Router not re-attached to new renderer context after crash/reload.

**Solution:**
Handle router lifecycle in OnContextCreated/OnContextReleased:

```cpp
void MessageRouterHandler::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
  message_router_->OnContextCreated(browser, frame, context);
}

void MessageRouterHandler::OnContextReleased(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
  message_router_->OnContextReleased(browser, frame, context);
}
```

**References:**
- https://magpcss.org/ceforum/apidocs3/projects/(default)/CefMessageRouterBrowserSide.html

---

### 3. Renderer Process Crashes

**Problem:**
CEF renderer processes can crash independently, requiring proper cleanup and restart.

**Symptoms:**
- Page turns blank/white
- No response to input
- Console shows "Aw, Snap!" or equivalent

**Detection:**
Implement OnRenderProcessTerminated callback:

```cpp
void CefClient::OnRenderProcessTerminated(
    CefRefPtr<CefBrowser> browser,
    TerminationStatus status) {
  logger_.Warn("Renderer process terminated with status {}", status);

  // Auto-reload if not abnormal termination
  if (status != TS_ABNORMAL_TERMINATION) {
    browser->Reload();
  }
}
```

**Best Practices:**
- Don't auto-reload on abnormal termination (may be malicious page)
- Show user-friendly error message
- Clean up MessageRouter state
- Log crash for debugging

**References:**
- https://magpcss.org/ceforum/viewtopic.php?f=6&t=13432

---

## Platform-Specific Issues

### 1. Linux: ANGLE/EGL Flags Required

**Status:** Fixed (app/src/browser/cef_engine.cpp:74)

**Problem:**
Recent CEF versions on Linux require ANGLE OpenGL ES backend for OSR stability.

**Symptoms:**
- Black screen on startup
- OpenGL context creation failures
- GPU process crashes

**Solution:**
Use platform-specific command-line flags:

```cpp
// Linux-specific flags
command_line->AppendSwitch("use-angle");
command_line->AppendSwitchWithValue("use-angle", "gl-egl");
command_line->AppendSwitch("enable-features", "VaapiVideoDecoder");
```

**Platform Detection:**
```cpp
#ifdef __linux__
  // Linux-specific flags
#elif defined(_WIN32)
  // Windows-specific flags
#elif defined(__APPLE__)
  // macOS-specific flags
#endif
```

**References:**
- https://github.com/chromiumembedded/cef/issues/3953
- QCefView: src/CefViewBrowserApp.cpp

---

### 2. Windows: DPI Scaling Issues

**Problem:**
Non-100% Windows display scaling (125%, 150%, 200%) can cause paint glitches.

**Symptoms:**
- Blurry text rendering
- Misaligned click coordinates
- Content appears too large/small
- Texture stretching artifacts

**Root Cause:**
Mismatch between OS DPI, Qt DPI, and CEF DPI calculations.

**Solution:**
- Track DPI changes via Qt's QWindow::devicePixelRatioChanged signal
- Call CefBrowserHost::WasResized() on DPI change
- Ensure consistent scale factor across all layers

```cpp
void BrowserWidget::onDevicePixelRatioChanged(qreal scale) {
  if (browser_) {
    browser_->GetHost()->WasResized();  // Forces CEF to recalculate sizes
  }
}
```

**Current Athena Status:**
- Scale factor tracking implemented (app/src/browser/cef_client.cpp:182)
- Monitor for edge cases on high-DPI Windows systems

**Known Problematic Scales:**
- 125% on Windows (most reports)
- 150% on Windows 10
- 250%+ on 4K displays

**References:**
- https://www.magpcss.org/ceforum/viewtopic.php?f=6&t=17089
- QCefView: QtBrowserWindow.cpp:onDpiChanged

---

### 3. Linux: X11 vs Wayland

**Problem:**
CEF has better support for X11 than Wayland on Linux.

**Symptoms (on Wayland):**
- Input lag or missed events
- Clipboard operations fail
- Drag-and-drop broken

**Workaround:**
Force X11 backend via environment variable:

```bash
export GDK_BACKEND=x11
./athena-browser
```

**Long-term Solution:**
- Monitor CEF Wayland support progress
- Consider contributing to CEF Wayland backend
- Document X11 requirement in README

**References:**
- https://github.com/chromiumembedded/cef/issues/2296

---

### 4. macOS: Retina Display Handling

**Problem:**
Retina displays (2x, 3x scaling) require special buffer size calculations.

**Solution:**
Always use physical pixels for CEF buffers:

```cpp
int physical_width = logical_width * device_scale_factor;
int physical_height = logical_height * device_scale_factor;
```

**Current Athena Status:**
- Implemented in ScalingManager (app/src/rendering/scaling_manager.cpp)
- Should work correctly on Retina displays

---

## Performance Gotchas

### 1. Full-Screen Repaints

**Status:** Optimized (app/src/rendering/gl_renderer.cpp uses dirty rects)

**Problem:**
Repainting entire frame every time is wasteful for incremental updates.

**Solution:**
CEF's OsrRenderer automatically uses dirty rectangles. Monitor logs:

```bash
LOG_LEVEL=debug ./athena-browser
# Look for: "GLRenderer: Partial texture update" vs "Full texture update"
```

**When Full Repaints Happen:**
- Window resize
- First paint after navigation
- Scroll of large regions
- Video playback

**When Partial Updates Happen:**
- Text input in forms
- Button hover states
- Small animations
- Cursor blinking

---

### 2. Cookie/Cache Bloat

**Status:** Mitigated (per-tab isolation available)

**Problem:**
Shared cache across all tabs can grow indefinitely.

**Solution:**
Use per-tab RequestContext with shared disk storage:

```cpp
BrowserConfig config;
config.url = "https://example.com";
config.isolate_cookies = true;  // Creates new in-memory context
auto result = engine->CreateBrowser(config);
```

**Trade-offs:**
- Isolated cookies: Better privacy, slower first load
- Shared cookies: Faster, but sessions leak across tabs

---

### 3. GPU Process Memory Leaks

**Problem:**
Long-running sessions can accumulate GPU memory (CEF bug).

**Symptoms:**
- Increasing RAM usage over hours
- Eventually GPU process crashes
- System becomes sluggish

**Workaround:**
- Restart browser periodically (not ideal)
- Monitor GPU memory: `chrome://gpu` in DevTools
- Disable GPU if critical: `--disable-gpu`

**References:**
- https://github.com/chromiumembedded/cef/issues/2834

---

## Input Handling

### 1. Keyboard Focus Timing

**Problem:**
Calling SetFocus(true) too early (before OnAfterCreated) fails silently.

**Solution:**
Wait for browser to be fully created:

```cpp
void CefClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  browser_ = browser;
  // NOW safe to set focus
  browser->GetHost()->SetFocus(true);
}
```

---

### 2. IME Input (CJK Languages)

**Problem:**
Input Method Editors (Chinese, Japanese, Korean) require special handling.

**Symptoms:**
- Composition text not visible
- Candidates window in wrong position
- Text commits incorrectly

**Solution:**
Implement IME callbacks in CefRenderHandler:

```cpp
void CefClient::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selected_range,
    const CefRenderHandler::RectList& character_bounds) {
  // Update IME candidate window position
}
```

**Current Athena Status:**
- Basic implementation in cef_client.cpp:234
- Needs testing with CJK input

---

### 3. Touch Input on Linux

**Problem:**
Qt touch events need conversion to CEF mouse events.

**Current Implementation:**
- Not yet implemented
- Touch events will be ignored

**TODO:**
- Convert QTouchEvent to SendTouchEvent() calls
- Handle multi-touch gestures (pinch-zoom, swipe)

---

## Rendering Issues

### 1. Transparent Backgrounds

**Problem:**
CEF OSR supports alpha channel, but Qt OpenGL may not composite correctly.

**Solution:**
- Use `browser_settings.background_color = 0` for transparency
- Enable alpha in OpenGL framebuffer config
- Use `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`

**Current Athena Status:**
- CEF renders with opaque white background
- Transparency not tested

---

### 2. V-Sync and Tearing

**Problem:**
Without V-Sync, screen tearing visible during scrolling.

**Solution:**
Enable V-Sync in Qt's OpenGL surface:

```cpp
QSurfaceFormat format;
format.setSwapInterval(1);  // Enable V-Sync
QSurfaceFormat::setDefaultFormat(format);
```

**Current Athena Status:**
- V-Sync enabled in main.cpp:72
- Should eliminate tearing

---

### 3. Black Flashes During Navigation

**Problem:**
Brief black screen when navigating between pages.

**Workaround:**
- Show previous frame until new frame ready
- Set `browser_settings.background_color` to page background

**Not Solvable:**
- Inherent to how CEF clears renderer between navigations

---

## Debugging Tips

### Enable CEF Debug Logging

```bash
export CEF_LOG_SEVERITY=info
export CEF_LOG_FILE=/tmp/cef.log
./athena-browser
```

### DevTools Remote Debugging

```bash
# Start browser with remote debugging
./athena-browser --remote-debugging-port=9222

# Open in another browser
chromium-browser http://localhost:9222
```

### Check GPU Status

Navigate to: `chrome://gpu` (via address bar or DevTools console)

### Monitor CEF Threads

```bash
# Attach GDB and show all threads
gdb -p $(pgrep athena-browser)
(gdb) info threads
```

---

## Quick Reference: Common Error Messages

| Error Message | Likely Cause | Solution |
|---------------|--------------|----------|
| "Check failed: texture_" | GL context destroyed | Ensure GLRenderer cleanup before browser close |
| "Aw, Snap!" page | Renderer crash | Check OnRenderProcessTerminated logs |
| "cefQuery is not defined" | MessageRouter not attached | Verify OnContextCreated called |
| "GPU process crashed" | GPU memory exhausted | Restart browser or disable GPU |
| Black screen on startup | ANGLE flags missing (Linux) | Add `--use-angle=gl-egl` |
| Keyboard doesn't work after navigation | Focus lost (CEF #3870) | Implemented fix in OnAddressChange |

---

## Contributing to This Playbook

When you encounter a new issue:

1. Document symptoms clearly
2. Identify root cause (use logs, debugger)
3. Describe solution/workaround
4. Add references (CEF forum, GitHub issues)
5. Update this document

This playbook is living documentation - keep it current as CEF evolves.

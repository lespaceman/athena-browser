# Athena Browser

A CEF-based desktop browser with GTK3 integration and React frontend.

## Prerequisites

- **Linux**: GTK3 development libraries, X11 development libraries
- CMake >= 3.21, C++17 compiler, and Ninja (recommended)
- Node.js >= 18 for the frontend tooling
- CEF binary distribution matching your platform

### Installing Dependencies (Ubuntu/Debian)

```bash
sudo apt-get install libgtk-3-dev libx11-dev pkg-config
```

## CEF Setup

CEF is expected at:

```
third_party/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}
```

The pinned version is set in `CMakeLists.txt` and `cmake/DownloadCEF.cmake`.
Download from the official CEF builds and extract into `third_party/`.

## Development (Quick Start)

Start the browser with hot module reloading:

```bash
./scripts/dev.sh
```

This script:
- Starts the Vite dev server for the React frontend
- Builds the native app if needed (debug mode)
- Launches the browser with `GDK_BACKEND=x11` for proper window embedding

## Build (Release)

```bash
./scripts/build.sh
```

The script:
- Builds the React app with Vite
- Configures and builds the native GTK app
- Copies web assets into `resources/web`

Binary output: `build/release/athena-browser`

To run the release build:

```bash
GDK_BACKEND=x11 build/release/athena-browser
```

## Architecture

- **GTK3 Integration**: Uses GTK3 for proper CEF window embedding on Linux
- **X11 Backend**: Forces X11 backend for reliable child window embedding (works on both X11 and Wayland)
- **Custom Scheme**: `app://` serves local resources with CSP
- **IPC Bridge**: Message Router provides `window.Native` API for browser-app communication

## Notes

- The browser uses GTK3 with X11 backend for proper CEF integration
- On Wayland desktops, `GDK_BACKEND=x11` forces XWayland for compatibility
- Resource paths are configured for ICU data and locale files
- Navigation guards block unsafe schemes (`file://`, `chrome://`, etc.)

## Next Steps

- Add more IPC methods to the `window.Native` bridge
- Implement multi-window support
- Add CI and platform packaging scripts

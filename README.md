# Athena Browser

A CEF-based desktop browser with Qt6 integration and React frontend.

## Prerequisites

- **Linux**: Qt6 development libraries, X11 development libraries
- CMake >= 3.21, C++17 compiler, and Ninja (recommended)
- Node.js >= 18 for the frontend tooling
- CEF binary distribution matching your platform

### Installing Dependencies (Ubuntu/Debian)

```bash
sudo apt-get install qt6-base-dev qt6-tools-dev libqt6opengl6-dev libgtk-3-dev libx11-dev pkg-config
```

Note: GTK3 headers are still needed as CEF has GTK dependencies.

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
- Launches the browser with Qt6 window system

## Build (Release)

```bash
./scripts/build.sh
```

The script:
- Builds the React app with Vite
- Configures and builds the native Qt app
- Copies web assets into `resources/web`

Binary output: `build/release/app/athena-browser`

To run the release build:

```bash
build/release/app/athena-browser
```

## Architecture

- **Qt6 Integration**: Uses Qt6 for cross-platform window management and UI
- **CEF Integration**: Chromium Embedded Framework for web rendering
- **Custom Scheme**: `app://` serves local resources with CSP
- **IPC Bridge**: Message Router provides `window.Native` API for browser-app communication
- **OpenGL Rendering**: Hardware-accelerated rendering using CEF's OSR mode

## Notes

- The browser uses Qt6 for the platform layer
- CEF runs in off-screen rendering (OSR) mode with OpenGL
- Resource paths are configured for ICU data and locale files
- Navigation guards block unsafe schemes (`file://`, `chrome://`, etc.)

## Next Steps

- Add more IPC methods to the `window.Native` bridge
- Implement multi-window support
- Add CI and platform packaging scripts

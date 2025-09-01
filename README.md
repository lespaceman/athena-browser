# Athena Browser — Scaffold

This repository contains a minimal scaffold for a CEF-based desktop browser with a React frontend.

## Prerequisites

- CMake >= 3.21, a C++17 compiler, and Ninja (recommended)
- Node.js >= 18 for the frontend tooling
- Download a pinned CEF binary distribution matching your platform

## CEF Setup

By default, CEF is expected at:

```
third_party/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}
```

The pinned version is set in `CMakeLists.txt` and `cmake/DownloadCEF.cmake`.
Download from the official CEF builds and extract into `third_party/`.

## Build (Release)

```
./scripts/build.sh
```

The script:
- builds the React app with Vite
- configures and builds the native app via `CMakePresets.json`
- copies the built web assets into `resources/web`

Binary output: `build/release/athena-browser`

## Dev (HMR)

In one terminal:

```
cd frontend && npm install && npm run dev
```

In another terminal, run the native app pointing to the dev server:

```
DEV_URL=http://localhost:5173 build/release/athena-browser
```

Or use the helper:

```
./scripts/dev.sh
```

## Notes & Next Steps

- Resource paths are set in code so ICU (`icudtl.dat`) and `.pak` files load reliably. Ensure runtime layout includes `locales/` next to the executable.
- `app://` custom scheme is registered and serves `resources/web` with CSP. In dev, CSP permits Vite HMR (`localhost:5173`). In prod, CSP is strict.
- A basic navigation guard blocks unsafe schemes like `file://` and `chrome://`.

Next improvements:
- Introduce IPC via CEF Message Router and a minimal `window.Native` bridge.
- Factor `CefApp` implementations for browser and renderer processes.
- Add CI and platform packaging scripts.

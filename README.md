# Athena Browser

A CEF-based desktop browser with Qt6 integration, React homepage, and Claude AI integration.

## Prerequisites

- **Linux**: Qt6 development libraries, X11 development libraries
- CMake >= 3.21, C++17 compiler, and Ninja (recommended)
- Node.js >= 18 for the frontend tooling
- CEF binary distribution matching your platform

### Installing Dependencies (Ubuntu/Debian)

```bash
sudo apt-get install qt6-base-dev qt6-tools-dev libqt6opengl6-dev libgtk-3-dev libx11-dev pkg-config
```

**Note:** Athena uses Qt6 for its window system. GTK3 headers are required only because CEF (Chromium Embedded Framework) has internal GTK dependencies on Linux. Your application code uses Qt exclusively.

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
- Starts the Vite dev server for the React homepage
- Builds the native app if needed (debug mode)
- Builds the Claude integration agent
- Launches the browser with Qt6 window system

## Build (Release)

```bash
./scripts/build.sh
```

The script:
- Builds the homepage with Vite
- Builds the Claude integration agent
- Configures and builds the native Qt app
- Copies homepage assets into `resources/homepage/`

Binary output: `build/release/app/athena-browser`

To run the release build:

```bash
./scripts/run.sh
# Or directly:
build/release/app/athena-browser
```

## MCP Integration (Browser Automation)

Athena Browser supports **Model Context Protocol (MCP)** for AI-powered browser automation with 40+ tools.

### Quick Start

1. **Install athena-browser-mcp package:**
```bash
npm install athena-browser-mcp
```

2. **Start Athena Browser** (CDP enabled by default on port 9222):
```bash
./scripts/run.sh
```

3. **Test CDP connection:**
```bash
./scripts/test-cdp-connection.sh
```

4. **Use with MCP Inspector:**
```bash
CEF_BRIDGE_PORT=9222 npx @modelcontextprotocol/inspector npx athena-browser-mcp
```

### Claude Desktop Integration

Configure Claude Desktop to control Athena Browser with 40+ automation tools:

**Add to `~/.config/Claude/claude_desktop_config.json`:**
```json
{
  "mcpServers": {
    "athena-browser": {
      "command": "npx",
      "args": ["-y", "athena-browser-mcp"],
      "env": {
        "CEF_BRIDGE_HOST": "127.0.0.1",
        "CEF_BRIDGE_PORT": "9222"
      }
    }
  }
}
```

**Available Tools (40+):**
- **Perception:** DOM tree, accessibility tree, element discovery, OCR, network monitoring
- **Interaction:** Click, type, scroll, form filling, file upload, navigation
- **Session:** Save/restore sessions, learn selectors, safety policies, audit logging

**Example Prompts:**
- "Navigate to google.com and search for 'web scraping'"
- "Find all forms on this page and fill them out"
- "Extract the main content from this article"
- "Monitor network requests and show me the API calls"

See [docs/MCP_INTEGRATION.md](docs/MCP_INTEGRATION.md) for complete documentation.

## Distribution Packaging

Create a distributable Linux bundle:

```bash
# Build and package in one command
PACKAGE=1 ./scripts/build.sh

# Or package separately
./scripts/package-linux.sh
```

The bundle will be created at `dist/linux/athena-browser/` and includes:
- Athena Browser binary
- CEF libraries and resources
- Qt6 libraries and plugins
- Claude integration agent with dependencies
- Homepage resources
- Launcher script (`athena-browser.sh`)

To run the bundle:

```bash
cd dist/linux/athena-browser
./athena-browser.sh
```

**Note:** The bundle requires Node.js 18+ to be installed on the target system.

## Architecture

- **Qt6 Integration**: Uses Qt6 for cross-platform window management and UI
- **CEF Integration**: Chromium Embedded Framework for web rendering
- **Claude Agent**: Node.js runtime providing Claude AI chat integration
- **Custom Scheme**: `app://` serves local resources with CSP
- **IPC Bridge**: Message Router provides `window.Native` API for browser-app communication
- **OpenGL Rendering**: Hardware-accelerated rendering using CEF's OSR mode

## Notes

- The browser uses Qt6 for the platform layer
- CEF runs in off-screen rendering (OSR) mode with OpenGL
- Resource paths are configured for ICU data and locale files
- Navigation guards block unsafe schemes (`file://`, `chrome://`, etc.)

## Project Structure

```
athena-browser/
├── app/                    # C++ browser engine (Qt6 + CEF)
├── homepage/               # Default homepage webapp (React + Vite)
├── agent/                  # Node.js Claude integration
├── resources/homepage/     # Built homepage assets (auto-generated)
├── dist/                   # Distribution bundles (Linux, macOS, Windows)
├── scripts/                # Build and packaging scripts
└── third_party/            # CEF binary distribution
```

## Documentation

- [CLAUDE.md](CLAUDE.md) - Comprehensive development guide for Claude Code
- [docs/MCP_INTEGRATION.md](docs/MCP_INTEGRATION.md) - MCP browser automation integration guide
- [REORGANIZATION_PLAN.md](REORGANIZATION_PLAN.md) - Packaging and distribution implementation plan
- [app/tests/README.md](app/tests/README.md) - Testing guide

## Next Steps

- Add more IPC methods to the `window.Native` bridge
- ✅ ~~Implement multi-window support~~ (Completed)
- ✅ ~~Add platform packaging scripts~~ (Linux packaging completed)
- Add CI/CD workflows (GitHub Actions)
- Create release packages (AppImage, DMG)

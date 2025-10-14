# Athena Agent

AI-powered agent for Athena Browser with Claude Agent SDK and MCP tool integration.

## Overview

Athena Agent is a Node.js service that provides:

1. **Claude Chat Integration** - AI assistant powered by Claude 3.5 Sonnet
2. **MCP Browser Control Tools** - 11 tools for controlling the browser via Claude
3. **HTTP API** - RESTful endpoints for browser control and chat
4. **Unix Socket IPC** - Efficient communication with the C++ GTK application

## Quick Start

```bash
cd athena-agent
npm install
npm run build

# Run the server
npm start

# Or with custom socket
ATHENA_SOCKET_PATH=/tmp/test.sock npm start
```

## Testing

```bash
# Run integration tests
npm run test:integration

# Manual testing with curl
curl --unix-socket /tmp/athena-$(id -u).sock http://localhost/health
```

See [TESTING.md](./TESTING.md) for detailed testing guide.

## API Endpoints

### Browser Control (11 endpoints)

**Navigation:**
- `POST /v1/browser/navigate` - Navigate to URL
- `POST /v1/browser/back` - Go back
- `POST /v1/browser/forward` - Go forward
- `POST /v1/browser/reload` - Reload page

**Information:**
- `GET /v1/browser/get_url` - Get current URL
- `GET /v1/browser/get_html` - Get page HTML

**Interaction:**
- `POST /v1/browser/execute_js` - Execute JavaScript
- `POST /v1/browser/screenshot` - Capture screenshot

**Tabs:**
- `POST /v1/window/create_tab` - Create tab
- `POST /v1/window/close_tab` - Close tab
- `POST /v1/window/switch_tab` - Switch tab

**Info:**
- `GET /v1/window/tab_info` - Tab count and active index

### Chat
- `POST /v1/chat/send` - Send message to Claude
- `POST /v1/chat/continue` - Continue conversation
- `POST /v1/chat/clear` - Clear history

### System
- `GET /health` - Health check
- `GET /v1/capabilities` - Server capabilities

## Architecture

```
Claude SDK → MCP Tools → HTTP (Unix Socket) → Express Routes → BrowserController → C++ GtkWindow
```

See [BROWSER_CONTROL_IMPLEMENTATION.md](./BROWSER_CONTROL_IMPLEMENTATION.md) for details.

## Current Status

✅ Complete TypeScript implementation
✅ All 11 browser endpoints working
✅ Real HTTP client (no mocks)
✅ Integration tests
✅ Winston logging to stderr
⏳ C++ integration (next step)

## Documentation

- [TESTING.md](./TESTING.md) - How to test
- [BROWSER_CONTROL_IMPLEMENTATION.md](./BROWSER_CONTROL_IMPLEMENTATION.md) - Implementation details
- [src/routes/browser.ts](./src/routes/browser.ts) - API source

## License

MIT

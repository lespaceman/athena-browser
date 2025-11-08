# Athena Agent

AI-powered HTTP server for Athena Browser providing Claude AI chat integration.

## Overview

Athena Agent is a Node.js service that provides:

1. **Claude Chat Integration** - AI assistant powered by Claude Agent SDK
2. **HTTP API** - RESTful endpoints for browser control and chat
3. **Unix Socket IPC** - Efficient communication with the C++ Qt application
4. **Session Management** - Persistent conversation storage

## Quick Start

```bash
cd agent
npm install
npm run build

# Run the server
npm start

# Or with custom socket
ATHENA_SOCKET_PATH=/tmp/test.sock npm start
```

## Architecture

Athena Agent provides an HTTP API over Unix sockets for communication with the C++ browser:

```
Claude Agent SDK (Sonnet 4.5)
    ↓
ClaudeClient
    ↓
Express HTTP Server
    ↓ (Unix socket)
C++ Qt Browser
```

**Key Components:**

- **`src/server/server.ts`** - Express HTTP server and application orchestration
- **`src/claude/client.ts`** - Claude Agent SDK integration with session management
- **`src/browser/api-client.ts`** - HTTP client for browser communication
- **`src/session/manager.ts`** - Persistent conversation storage

## Browser Automation (MCP)

For browser automation with 40+ tools, use the separate **`athena-browser-mcp`** package which connects directly to CEF via Chrome DevTools Protocol:

```bash
# Install globally or use npx
npm install -g athena-browser-mcp

# Run with MCP Inspector
CEF_BRIDGE_PORT=9222 npx @modelcontextprotocol/inspector npx athena-browser-mcp

# Or configure Claude Desktop (see docs/MCP_INTEGRATION.md)
```

See [../docs/MCP_INTEGRATION.md](../docs/MCP_INTEGRATION.md) for complete browser automation documentation.

## API Endpoints

### Browser Control
- `POST /v1/browser/navigate` - Navigate to URL
- `POST /v1/browser/back` - Go back
- `POST /v1/browser/forward` - Go forward
- `POST /v1/browser/reload` - Reload page
- `GET /v1/browser/url` - Get current URL
- `GET /v1/browser/html` - Get page HTML
- `POST /v1/browser/execute-js` - Execute JavaScript
- `POST /v1/browser/screenshot` - Capture screenshot

### Tab Management
- `POST /v1/window/create` - Create tab
- `POST /v1/window/close` - Close tab
- `POST /v1/window/switch` - Switch tab
- `GET /v1/window/tabs` - Tab count and active index

### Chat
- `POST /v1/chat/send` - Send message to Claude
- `POST /v1/chat/stream` - Stream response from Claude
- `POST /v1/chat/continue` - Continue conversation
- `POST /v1/chat/clear` - Clear history

### Sessions
- `GET /v1/sessions` - List all sessions
- `GET /v1/sessions/search` - Search sessions
- `GET /v1/sessions/stats` - Session statistics
- `POST /v1/sessions/prune` - Delete old sessions
- `GET /v1/sessions/:sessionId` - Get session details
- `GET /v1/sessions/:sessionId/messages` - Get session messages
- `PATCH /v1/sessions/:sessionId` - Update session
- `DELETE /v1/sessions/:sessionId` - Delete session

### System
- `GET /health` - Health check
- `GET /v1/capabilities` - Server capabilities

## Testing

```bash
# Run all tests
npm test

# Run integration tests
npm run test:integration

# Manual testing with curl
curl --unix-socket /tmp/athena-$(id -u).sock http://localhost/health
```

## Development

### Build

```bash
npm run build        # Build TypeScript
npm run type-check   # Type check without building
```

### Run in Development Mode

```bash
npm run dev          # Watch mode with hot reload
```

### Code Quality

```bash
npm run lint         # Run ESLint
```

## Configuration

### Environment Variables

**Server Configuration:**
- `ATHENA_SOCKET_PATH` - Socket for HTTP server (default: `/tmp/athena-UID.sock`)
- `ATHENA_CONTROL_SOCKET_PATH` - Socket for browser control (default: `/tmp/athena-UID-control.sock`)
- `LOG_LEVEL` - Logging level: debug, info, warn, error (default: `info`)

**AI Configuration:**
- `ANTHROPIC_API_KEY` - API key for Claude (optional, for chat features)
- `CLAUDE_MODEL` - Model to use (default: `claude-sonnet-4-5`)
- `PERMISSION_MODE` - Permission handling: `default`, `acceptEdits`, `bypassPermissions` (default: `default`)
- `MAX_THINKING_TOKENS` - Max tokens for extended thinking (default: `8000`)
- `MAX_TURNS` - Max conversation turns (default: `20`)

### Performance Tuning

**For cost optimization:**
```bash
export CLAUDE_MODEL=claude-haiku-4-5-20251001  # Faster, cheaper for simple tasks
export MAX_THINKING_TOKENS=2000                # Reduce thinking tokens
export MAX_TURNS=10                             # Limit conversation length
```

**For maximum capability:**
```bash
export CLAUDE_MODEL=claude-sonnet-4-5           # Best reasoning (default)
export MAX_THINKING_TOKENS=10000                # Extended reasoning
export MAX_TURNS=30                              # Longer conversations
```

## Logging

### Log Levels

Control verbosity with `LOG_LEVEL` environment variable:

```bash
# Debug logs (very verbose - shows all browser API calls)
LOG_LEVEL=debug ./scripts/run.sh

# Info logs (default - shows important events)
LOG_LEVEL=info ./scripts/run.sh

# Warnings and errors only
LOG_LEVEL=warn ./scripts/run.sh
```

## Current Status

✅ **Production Ready**

**Core Features:**
- Complete TypeScript implementation with modular architecture
- Express HTTP server with Unix socket communication
- Claude Agent SDK integration
- Session management with persistence
- Winston logging to stderr
- Type-safe with full TypeScript coverage

## Design Principles

1. **Single Responsibility** - Each module has one clear purpose
2. **DRY** - Shared components, no code duplication
3. **Type Safety** - Full TypeScript with strict mode
4. **Modularity** - Components can be used independently
5. **Standards Compliance** - Follows HTTP and REST best practices

## License

MIT

# Athena Agent Documentation

Complete documentation for Athena Agent - an AI-powered browser automation agent using Claude Agent SDK and MCP tools.

## Quick Links

- **[Main README](../README.md)** - Quick start, features, and configuration
- **[Architecture](./architecture/mcp-integration.md)** - Technical architecture and design
- **[Testing Guide](./guides/testing.md)** - Testing documentation and best practices

## Documentation Structure

### 📖 Main Documentation
**[README.md](../README.md)** - Start here!

The main entry point with:
- Quick start guide
- Performance features and metrics
- Complete API endpoint reference
- MCP tools listing
- Configuration options
- Status and roadmap

**Audience**: All users (developers, operators, contributors)

### 🏗️ Architecture
**[architecture/mcp-integration.md](./architecture/mcp-integration.md)**

Technical deep-dive into:
- System architecture and layers
- Two MCP server implementations
- Component responsibilities
- Design principles
- Sub-agent system
- Development workflows

**Audience**: Developers implementing features or integrations

### 🧪 Testing
**[guides/testing.md](./guides/testing.md)**

Comprehensive testing guide:
- Test structure and coverage
- Running tests (unit, MCP, integration)
- MCP Inspector usage
- Debugging tips
- Writing new tests

**Audience**: Developers writing tests or debugging issues

## Key Concepts

### MCP (Model Context Protocol)
Athena Agent uses MCP to expose 17 browser control tools that Claude can use:
- **Navigation** (4 tools): navigate, back, forward, reload
- **Information** (7 tools): HTML, summaries, accessibility tree, etc.
- **Interaction** (2 tools): JavaScript execution, screenshots
- **Tab Management** (4 tools): create, close, switch, info

### Claude Agent SDK Integration
The agent uses Claude's Agent SDK with:
- **4 specialized sub-agents** for different browser tasks
- **Dynamic tool selection** based on prompt analysis
- **Smart permissions** with allow/deny/ask callbacks
- **Session management** including forking

### Performance Optimizations
- **Model**: Claude Sonnet 4.5 (upgraded from Haiku)
- **Cost**: 20-40% reduction through dynamic tools
- **Speed**: 2-3x faster for read-only queries
- **Safety**: 100% blocking of dangerous operations

## Getting Started

```bash
# Install dependencies
npm install

# Build the project
npm run build

# Run the server
npm start

# Run tests
npm test

# Run MCP tests
npm run test:mcp

# Interactive MCP testing
npm run mcp:inspect
```

## Architecture Overview

```
┌─────────────────────────────────────────┐
│   Claude Agent SDK (Sonnet 4.5)        │
│   • 4 specialized sub-agents           │
│   • Dynamic tool selection             │
│   • Smart permissions                  │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│   mcp-agent-adapter.ts                  │
│   17 browser control tools              │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│   browser-api-client.ts                 │
│   HTTP client (Unix socket)             │
└──────────────┬──────────────────────────┘
               │
               ▼
       C++ Browser Control Server
```

See [architecture/mcp-integration.md](./architecture/mcp-integration.md) for detailed diagrams.

## Component Files

### Core Implementation
- `src/server.ts` - HTTP server and application orchestration
- `src/claude-client.ts` - Claude Agent SDK integration
- `src/mcp-agent-adapter.ts` - Agent SDK MCP tools (embedded)
- `src/mcp-server.ts` - Standard MCP server (stdio)
- `src/browser-api-client.ts` - HTTP client for browser control

### Configuration
- `src/config.ts` - Configuration management
- `src/types.ts` - TypeScript type definitions
- `src/logger.ts` - Winston logging

### Testing
- `test/mcp/` - MCP server tests
- `test/integration/` - Integration tests

## Environment Variables

**Server:**
- `ATHENA_SOCKET_PATH` - HTTP server socket (default: `/tmp/athena-UID.sock`)
- `ATHENA_CONTROL_SOCKET_PATH` - Browser control socket (default: `/tmp/athena-UID-control.sock`)
- `LOG_LEVEL` - Logging level (default: `info`)

**AI:**
- `ANTHROPIC_API_KEY` - Claude API key
- `CLAUDE_MODEL` - Model to use (default: `claude-sonnet-4-5`)
- `PERMISSION_MODE` - Permission handling (default: `default`)
- `MAX_THINKING_TOKENS` - Extended thinking (default: `8000`)
- `MAX_TURNS` - Conversation limit (default: `20`)

## Contributing

When contributing:

1. **Adding features**: Update architecture docs
2. **Adding MCP tools**: Follow development workflow in architecture docs
3. **Adding tests**: Follow testing guide
4. **Updating docs**: Keep all three docs in sync

## External Resources

- [MCP Specification](https://spec.modelcontextprotocol.io/)
- [Claude Agent SDK](https://github.com/anthropics/claude-agent-sdk-typescript)
- [MCP TypeScript SDK](https://github.com/modelcontextprotocol/typescript-sdk)
- [MCP Inspector](https://github.com/modelcontextprotocol/inspector)

## License

MIT

# Changelog

All notable changes to Athena Browser will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2024-11-08

### ⚠️ BREAKING CHANGES

- **MCP Integration Completely Rewritten:** The built-in MCP server has been removed from the agent folder. Browser automation now uses the external `athena-browser-mcp` npm package.
- **Agent API Changed:** `ClaudeClient` constructor no longer accepts `mcpServer` parameter.
- **Configuration Changed:** MCP configuration moved from `createSdkMcpServer()` to `options.mcpServers` in Claude SDK `query()` function.
- **Migration Required:** Existing Claude Desktop configurations and SDK integrations must be updated. See `docs/MIGRATION_V2.md` for detailed migration instructions.

### Added

- **External MCP Package Integration** - Integration with `athena-browser-mcp@1.0.3` package providing 40+ browser automation tools
- **CDP Protocol Support** - Browser automation now uses Chrome DevTools Protocol instead of custom HTTP API
- **Enhanced Tool Set** - Access to advanced features:
  - Form detection and intelligent filling
  - OCR for canvas/SVG elements
  - Network monitoring and HAR capture
  - Session management with saved selectors
  - Advanced interaction strategies (Accessibility, DOM, BBox)
- **Comprehensive Documentation**:
  - `docs/MCP_INTEGRATION.md` - Complete integration guide with all 40+ tools documented
  - `docs/MIGRATION_V2.md` - Step-by-step migration guide from v1.x
  - `docs/claude-desktop-config-mcp.json` - Ready-to-use Claude Desktop configuration
  - `.athena-browser-mcp.json` - Configuration reference with platform-specific examples
- **Testing Scripts**:
  - `scripts/test-cdp-connection.sh` - Automated CDP connection verification
- **Platform-Agnostic Paths** - File upload directories now use `os.homedir()` and `os.tmpdir()` instead of hardcoded `/home/user` paths
- **Environment Variable Support** - `ATHENA_ALLOWED_DIRS` for customizing file upload allowlist

### Changed

- **Simplified Agent Architecture** - Agent now focuses on HTTP API and Claude chat integration, browser automation delegated to external package
- **Query Builder Refactored** - `buildQueryOptions()` now takes `enableMcp: boolean` instead of `mcpServer` instance
- **Tool Selection Simplified** - `ToolSelector.selectTools()` uses boolean flag instead of server instance
- **Code Reduction** - Removed ~2,136 net lines of code (deleted 4,117, added 1,981)
- **Cleaner Dependencies** - Agent no longer depends on `@modelcontextprotocol/sdk` for MCP server creation

### Removed

- **agent/src/mcp/agent-adapter.ts** - Old 17-tool MCP implementation (346 lines)
- **agent/src/mcp/server.ts** - Old MCP server (1,065 lines)
- **agent/src/mcp/stdio-server.ts** - Old stdio server entry point (74 lines)
- **agent/test/mcp/** - Old MCP test suite (~900 lines)
- **agent/examples/** - Old MCP demo scripts (182 lines)
- **MCP_GUIDE.md** - Replaced by `docs/MCP_INTEGRATION.md`
- **scripts/test-mcp.sh** - Replaced by `scripts/test-cdp-connection.sh`
- **npm scripts**: `start:mcp`, `dev:mcp`, `mcp:inspect`, `test:mcp`

### Fixed

- **Cross-Platform Compatibility** - File paths now work on Linux, macOS, and Windows
- **Security Hardening** - Documented CDP security model and file upload restrictions
- **Test Coverage** - Updated 51 tests to use new MCP configuration API

### Security

- **CDP Port Binding** - Remote debugging binds to `127.0.0.1` (localhost only) for security
- **File Upload Restrictions** - `ATHENA_ALLOWED_DIRS` restricts file uploads to allowlisted directories
- **Documentation** - Added comprehensive security documentation in `docs/MCP_INTEGRATION.md`

### Performance

- **Lower Latency** - CDP direct TCP connection is faster than HTTP over Unix sockets
- **Better Resource Isolation** - Browser automation runs in separate process from agent

### Migration Guide

See `docs/MIGRATION_V2.md` for complete migration instructions including:
- Code updates for SDK users
- Claude Desktop configuration changes
- Tool mapping from v1.x to v2.0
- Troubleshooting common issues
- Rollback plan

### Upgrade Notes

**For SDK Users:**
```typescript
// Before (v1.x):
const mcpServer = createSdkMcpServer({...});

// After (v2.0):
options: {
  mcpServers: {
    'athena-browser': {
      command: 'npx',
      args: ['-y', 'athena-browser-mcp'],
      env: { CEF_BRIDGE_PORT: '9222' }
    }
  }
}
```

**For Claude Desktop Users:**
Update `~/.config/Claude/claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "athena-browser": {
      "command": "npx",
      "args": ["-y", "athena-browser-mcp"],
      "env": {
        "CEF_BRIDGE_PORT": "9222"
      }
    }
  }
}
```

### Dependencies

- **Added**: `athena-browser-mcp@^1.0.3` (external package)
- **Removed**: `@modelcontextprotocol/sdk` from agent dependencies

### Contributors

- Previous session contributors who implemented the initial MCP integration
- Current session for architectural refactoring and external package migration

---

## [1.0.0] - 2024-11-06 (Pre-MCP Refactor)

### Added

- Qt6-based windowing system
- CEF (Chromium Embedded Framework) integration
- Hardware-accelerated OpenGL rendering
- Multi-window support
- Complete input support (mouse, keyboard, focus)
- React-based homepage with Vite
- Node.js runtime integration
- Agent HTTP API over Unix sockets
- Built-in MCP server with 17 browser control tools
- Claude Agent SDK integration
- Session management
- Comprehensive test suite (200+ tests)
- Zero-global-state architecture
- Full RAII resource management
- Result<T> error handling throughout

### Features

- **Core Browser**:
  - Tab management
  - Navigation (back, forward, reload)
  - URL bar with navigation
  - Page loading indicators
  - Crash recovery
  - Popup handling

- **Input & Interaction**:
  - Full keyboard support (F-keys, numpad, special keys)
  - Mouse input (click, scroll, drag)
  - Focus management
  - Context menus

- **Developer Features**:
  - Remote debugging on port 9222
  - CDP protocol support
  - JavaScript execution
  - Console logging
  - Network monitoring

- **Agent Integration**:
  - HTTP API over Unix sockets
  - Browser control server (C++)
  - MCP tools for browser automation
  - Claude chat integration

### Documentation

- Comprehensive README.md
- Architecture documentation in CLAUDE.md
- Testing guide in app/tests/README.md
- Known issues documented

---

## Notes

### Version Numbering

- **Major** (X.0.0): Breaking changes, architectural changes, API incompatibilities
- **Minor** (1.X.0): New features, backwards-compatible changes
- **Patch** (1.0.X): Bug fixes, documentation updates

### Deprecation Policy

- **v1.x MCP**: No longer maintained, removed in v2.0
- **HTTP API**: Still supported, no breaking changes

### Future Roadmap

See `ROADMAP.md` for planned features and improvements.

---

## Links

- [Migration Guide](docs/MIGRATION_V2.md)
- [MCP Integration](docs/MCP_INTEGRATION.md)
- [GitHub Repository](https://github.com/lespaceman/athena-browser)
- [athena-browser-mcp Package](https://github.com/lespaceman/athena-browser-mcp)

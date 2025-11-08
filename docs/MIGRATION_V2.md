# Migration Guide: v1.x → v2.0 (athena-browser-mcp)

## Overview

Athena Browser v2.0 represents a major architectural change in how browser automation works. The built-in MCP server has been removed from the agent and replaced with a separate, more powerful `athena-browser-mcp` npm package.

## Breaking Changes

### 1. MCP Server Removed from Agent

**What Changed:**
- ❌ **Removed:** `agent/src/mcp/` - Built-in MCP server (17 tools)
- ❌ **Removed:** Agent-provided browser automation tools
- ✅ **Added:** Integration with external `athena-browser-mcp` package (40+ tools)

**Why This Change:**
- Better separation of concerns
- Industry-standard CDP protocol instead of custom HTTP API
- 2.3x more tools (40+ vs 17)
- Easier to maintain and extend
- Better compatibility with existing browser automation tools

### 2. Architecture Comparison

#### v1.x (Old)
```
Claude Desktop / SDK
    ↓
Agent MCP Server (built-in)
    ↓
HTTP API (Unix socket)
    ↓
C++ Browser Control Server
    ↓
Browser
```

**Tools:** 17 basic browser control tools

#### v2.0 (New)
```
Claude Desktop / SDK
    ↓
athena-browser-mcp (external package)
    ↓
Chrome DevTools Protocol (CDP)
    ↓
CEF Browser
```

**Tools:** 40+ advanced automation tools including:
- Form detection and intelligent filling
- OCR for canvas/SVG elements
- Network monitoring and HAR capture
- Session management with saved selectors
- Advanced interaction strategies (Accessibility, DOM, BBox)

### 3. What Still Works (No Changes)

The following features are **unchanged** and continue to work:

- ✅ Agent HTTP API (for custom integrations)
- ✅ Claude chat integration
- ✅ Session management
- ✅ C++ browser control server
- ✅ All existing browser features (tabs, navigation, etc.)

**Note:** The agent still provides browser control via HTTP API, but the MCP integration now uses CDP instead.

---

## Migration Steps

### Step 1: Update Your Code (If Using SDK)

If you're using the Claude Agent SDK programmatically:

**v1.x Code:**
```typescript
import { query, createSdkMcpServer, tool } from '@anthropic-ai/claude-agent-sdk';
import { BrowserApiClient } from './agent/src/browser/api-client';

// Old: Built-in MCP server
const mcpServer = createSdkMcpServer({
  name: 'athena-browser',
  tools: [/* ... */]
});

for await (const msg of query({
  prompt: 'Navigate to google.com',
  options: {
    mcpServers: {
      athena: mcpServer  // ❌ No longer works
    }
  }
})) {
  // ...
}
```

**v2.0 Code:**
```typescript
import { query } from '@anthropic-ai/claude-agent-sdk';

// New: External athena-browser-mcp package
for await (const msg of query({
  prompt: 'Navigate to google.com',
  options: {
    mcpServers: {
      'athena-browser': {
        command: 'npx',
        args: ['-y', 'athena-browser-mcp'],
        env: {
          CEF_BRIDGE_PORT: '9222',  // CDP port
          ATHENA_ALLOWED_DIRS: process.env.HOME + '/Downloads:' + process.env.TMPDIR
        }
      }
    }
  }
})) {
  // ...
}
```

### Step 2: Update Claude Desktop Configuration

**v1.x Configuration:**
```json
{
  "mcpServers": {
    "athena-browser": {
      "command": "node",
      "args": ["/path/to/athena-browser/agent/dist/mcp/stdio-server.js"]
    }
  }
}
```

**v2.0 Configuration:**
```json
{
  "mcpServers": {
    "athena-browser": {
      "command": "npx",
      "args": ["-y", "athena-browser-mcp"],
      "env": {
        "CEF_BRIDGE_PORT": "9222",
        "ATHENA_ALLOWED_DIRS": "/Users/yourname/Downloads:/tmp"
      }
    }
  }
}
```

**Configuration Notes:**
- `CEF_BRIDGE_PORT` must match the port in `app/src/browser/cef_engine.cpp` (default: 9222)
- `ATHENA_ALLOWED_DIRS` restricts file upload locations (colon-separated paths)
- Use platform-specific paths (see examples below)

**Platform-Specific Paths:**

Linux/macOS:
```json
"ATHENA_ALLOWED_DIRS": "/home/yourname/Downloads:/tmp"
```

Windows:
```json
"ATHENA_ALLOWED_DIRS": "C:\\Users\\yourname\\Downloads;C:\\Windows\\Temp"
```

### Step 3: Install athena-browser-mcp Package

The package is published on npm and will be automatically installed by `npx`:

```bash
# Optional: Install globally for faster startup
npm install -g athena-browser-mcp

# Verify installation
npx athena-browser-mcp --help
```

### Step 4: Test the Integration

#### Test with MCP Inspector

```bash
# 1. Start Athena Browser
./scripts/run.sh

# 2. In another terminal, test connection
./scripts/test-cdp-connection.sh

# 3. Launch MCP Inspector
CEF_BRIDGE_PORT=9222 npx @modelcontextprotocol/inspector npx athena-browser-mcp
```

This opens a web UI where you can:
- Browse all 40+ tools
- Test tools with custom parameters
- View real-time responses

#### Test with Claude Desktop

1. Update your `~/.config/Claude/claude_desktop_config.json` with v2.0 config (see Step 2)
2. Start Athena Browser: `./scripts/run.sh`
3. Restart Claude Desktop
4. Test with prompts:
   - "Navigate to google.com"
   - "Find all forms on this page"
   - "Extract the main content"

### Step 5: Update Your Tests (If You Have Custom Tests)

If you have tests that mock the old MCP server:

**v1.x Test Code:**
```typescript
const mockMcpServer = createSdkMcpServer({
  name: 'test-browser',
  tools: [/* ... */]
});

// Test code...
```

**v2.0 Test Code:**
```typescript
// Option 1: Use enableMcp flag
const result = await buildQueryOptions({
  prompt: 'test',
  enableMcp: true  // Boolean flag instead of server instance
});

// Option 2: Mock MCP config
const mockMcpConfig = {
  'athena-browser': {
    command: 'npx',
    args: ['-y', 'athena-browser-mcp'],
    env: { CEF_BRIDGE_PORT: '9222' }
  }
};
```

---

## Tool Mapping: v1.x → v2.0

Many v1.x tools have direct equivalents in v2.0, often with enhanced capabilities:

| v1.x Tool | v2.0 Equivalent | Enhancements |
|-----------|-----------------|--------------|
| `navigate` | `navigate` | Same functionality |
| `get_html` | `get_html` | Faster via CDP |
| `get_url` | `get_url` | Same functionality |
| `screenshot` | `screenshot` | Additional options |
| `execute_js` | `execute_js` | Better error handling |
| `page_summary` | `dom_tree_get` + `content_extract` | More detailed |
| `interactive_elements` | `interactive_elements_get` | More accurate |
| ❌ (not available) | `form_detect` | **New:** Auto-detect forms |
| ❌ (not available) | `ocr_element` | **New:** OCR for canvas/SVG |
| ❌ (not available) | `network_monitor_start` | **New:** Network capture |
| ❌ (not available) | `session_save` | **New:** Save/restore sessions |

**New Tool Categories in v2.0:**
- **Form Automation:** `form_detect`, `fill_form`
- **Vision:** `ocr_element`, `annotated_screenshot`
- **Network:** `network_monitor_start/stop`, `har_capture`
- **Session Management:** `session_save/restore`, `selector_learn`
- **Safety:** `safety_policy_set`, `action_audit_log`

---

## Troubleshooting

### Issue: "Connection refused on port 9222"

**Cause:** Athena Browser not running or CDP not enabled

**Fix:**
1. Verify browser is running: `ps aux | grep athena-browser`
2. Check CDP port is open: `./scripts/test-cdp-connection.sh`
3. Verify CDP enabled in `app/src/browser/cef_engine.cpp:56`

### Issue: "athena-browser-mcp command not found"

**Cause:** npm package not installed or not in PATH

**Fix:**
```bash
# Use npx (auto-installs):
npx -y athena-browser-mcp

# Or install globally:
npm install -g athena-browser-mcp
```

### Issue: "Permission denied for file upload"

**Cause:** File path not in `ATHENA_ALLOWED_DIRS`

**Fix:**
```bash
# Add your desired directory to allowlist
export ATHENA_ALLOWED_DIRS="/path/to/your/files:/tmp"
npx athena-browser-mcp
```

### Issue: "Old MCP tools not found"

**Cause:** Tool names changed or removed

**Fix:**
- Check tool mapping table above
- Browse available tools: `CEF_BRIDGE_PORT=9222 npx @modelcontextprotocol/inspector npx athena-browser-mcp`

---

## Rollback Plan

If you need to rollback to v1.x:

```bash
# 1. Checkout v1.x branch
git checkout v1.x  # Replace with your v1.x branch/tag

# 2. Rebuild agent
cd agent && npm install && npm run build

# 3. Update Claude Desktop config back to v1.x format
# (See v1.x documentation)

# 4. Restart browser and Claude Desktop
```

---

## FAQ

### Q: Do I need to change my C++ code?

**A:** No. The C++ browser code is unchanged. Only the MCP integration layer changed.

### Q: Can I use both v1.x HTTP API and v2.0 MCP at the same time?

**A:** Yes! The HTTP API still exists and works independently of the MCP integration. You can use:
- HTTP API for custom integrations
- athena-browser-mcp for Claude Desktop / SDK usage

### Q: Why use CDP instead of the custom HTTP API?

**A:** CDP offers:
- Industry-standard protocol used by Chrome DevTools, Puppeteer, Playwright
- More tools and capabilities (40+ vs 17)
- Better compatibility with existing automation tools
- Lower latency (direct TCP vs Unix socket HTTP)

### Q: Will v1.x MCP be maintained?

**A:** No. v1.x MCP code has been removed. Use `athena-browser-mcp` package for ongoing support and updates.

### Q: How do I report issues with athena-browser-mcp?

**A:** Report issues at the package repository: https://github.com/lespaceman/athena-browser-mcp/issues

---

## Getting Help

- **Documentation:** See `docs/MCP_INTEGRATION.md` for detailed integration guide
- **Examples:** Check `docs/claude-desktop-config-mcp.json` for working configurations
- **Testing:** Run `./scripts/test-cdp-connection.sh` to verify your setup
- **Issues:** Report bugs at https://github.com/lespaceman/athena-browser/issues

---

## Summary

**What You Need to Do:**
1. ✅ Update MCP configuration (Claude Desktop or SDK)
2. ✅ Use `athena-browser-mcp` package instead of built-in agent MCP
3. ✅ Set `CEF_BRIDGE_PORT=9222` environment variable
4. ✅ Configure `ATHENA_ALLOWED_DIRS` for file uploads

**What You DON'T Need to Do:**
- ❌ Change C++ code
- ❌ Modify HTTP API integrations
- ❌ Rebuild the browser (unless updating)

**Benefits:**
- 🚀 40+ tools (vs 17)
- 🎯 Industry-standard CDP protocol
- 🔧 Easier to maintain and extend
- 📊 Better performance and compatibility

Welcome to Athena Browser v2.0! 🎉

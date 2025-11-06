# Athena Browser MCP Integration Guide

**Status:** ✅ Fully Implemented and Production Ready

Athena Browser already has complete Model Context Protocol (MCP) integration with 17 browser control tools. This guide shows you how to use and extend the existing MCP functionality.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Available MCP Tools](#available-mcp-tools)
3. [Testing MCP Integration](#testing-mcp-integration)
4. [Using with Claude Desktop](#using-with-claude-desktop)
5. [Architecture Overview](#architecture-overview)
6. [Adding Custom MCP Tools](#adding-custom-mcp-tools)
7. [Troubleshooting](#troubleshooting)

---

## Quick Start

### 1. Test MCP Tools Interactively

```bash
# Build the agent
cd agent
npm install
npm run build

# Launch MCP Inspector to test tools interactively
npm run mcp:inspect
```

This opens an interactive web interface where you can:
- Browse all 17 available tools
- Test each tool with real browser instances
- View schemas and documentation
- Debug tool responses

### 2. Run the Full Agent Stack

```bash
# Terminal 1: Build and run the C++ browser
./scripts/build.sh
./scripts/run.sh

# Terminal 2: Start the agent server
cd agent
npm run dev
```

The agent automatically connects to the browser via Unix sockets and exposes all MCP tools.

---

## Available MCP Tools

### Navigation Tools (4 tools)

| Tool | Description | Parameters |
|------|-------------|------------|
| `browser_navigate` | Navigate to a URL | `url: string`, `tabIndex?: number` |
| `browser_back` | Go back in history | `tabIndex?: number` |
| `browser_forward` | Go forward in history | `tabIndex?: number` |
| `browser_reload` | Reload current page | `tabIndex?: number`, `ignoreCache?: boolean` |

**Example Usage:**
```typescript
// Navigate to Google
await tools.browser_navigate({ url: "https://google.com" });

// Go back
await tools.browser_back({});

// Hard reload (bypass cache)
await tools.browser_reload({ ignoreCache: true });
```

### Information Tools (7 tools)

| Tool | Description | Output Size | Use Case |
|------|-------------|-------------|----------|
| `browser_get_url` | Get current URL | ~50 bytes | Quick URL check |
| `browser_get_page_summary` | Page summary with title, headings, element counts | ~1-2 KB | Fast page overview |
| `browser_get_html` | Full HTML source | 100+ KB | Deep content analysis |
| `browser_get_interactive_elements` | All clickable elements with positions | ~5-10 KB | Element interaction |
| `browser_get_accessibility_tree` | Semantic page structure | ~10-20 KB | Accessibility analysis |
| `browser_query_content` | Query specific content types | Variable | Targeted extraction |
| `browser_get_annotated_screenshot` | Screenshot with element annotations | Image + metadata | Visual debugging |

**Content Query Types:**
- `forms` - All form elements, inputs, buttons
- `navigation` - Nav menus, breadcrumbs, links
- `article` - Main content, paragraphs, headings
- `tables` - Data tables with structure
- `media` - Images, videos, audio elements

**Example Usage:**
```typescript
// Get quick page overview (lightweight)
const summary = await tools.browser_get_page_summary({});
// Returns: { title, headings, forms, links, buttons, mainText }

// Get all form elements
const forms = await tools.browser_query_content({ queryType: "forms" });

// Get annotated screenshot for visual debugging
const screenshot = await tools.browser_get_annotated_screenshot({});
```

### Interaction Tools (2 tools)

| Tool | Description | Parameters |
|------|-------------|------------|
| `browser_execute_js` | Execute JavaScript in page context | `code: string`, `tabIndex?: number` |
| `browser_screenshot` | Capture page screenshot | `tabIndex?: number`, `fullPage?: boolean` |

**Example Usage:**
```typescript
// Click a button using JavaScript
await tools.browser_execute_js({
  code: "document.querySelector('#submit-btn').click()"
});

// Fill a form
await tools.browser_execute_js({
  code: `
    document.querySelector('#email').value = 'test@example.com';
    document.querySelector('#password').value = 'password123';
    document.querySelector('form').submit();
  `
});

// Capture full-page screenshot
const screenshot = await tools.browser_screenshot({ fullPage: true });
```

### Tab Management Tools (4 tools)

| Tool | Description | Parameters |
|------|-------------|------------|
| `window_create_tab` | Create new tab | `url: string` |
| `window_close_tab` | Close a tab | `tabIndex: number` |
| `window_switch_tab` | Switch to a tab | `tabIndex: number` |
| `window_get_tab_info` | Get tab count and active tab | None |

**Example Usage:**
```typescript
// Create new tab
const result = await tools.window_create_tab({ url: "https://example.com" });
// Returns: { tabIndex: 1 }

// Get all tabs
const info = await tools.window_get_tab_info({});
// Returns: { count: 2, activeTabIndex: 0 }

// Switch to second tab
await tools.window_switch_tab({ tabIndex: 1 });

// Close first tab
await tools.window_close_tab({ tabIndex: 0 });
```

---

## Testing MCP Integration

### Method 1: MCP Inspector (Recommended)

```bash
cd agent
npm run build
npm run mcp:inspect
```

Opens `http://localhost:5173` with interactive tool browser.

**Features:**
- Test each tool with custom parameters
- View full Zod schemas
- See real-time responses
- Debug tool errors

### Method 2: Manual Testing with curl

```bash
# Start the browser
./scripts/run.sh &

# Start the agent
cd agent && npm run dev &

# Test navigation
curl --unix-socket /tmp/athena-$(id -u).sock \
  -X POST http://localhost/internal/navigate \
  -H "Content-Type: application/json" \
  -d '{"url": "https://google.com"}'

# Get page summary
curl --unix-socket /tmp/athena-$(id -u).sock \
  -X GET "http://localhost/internal/get_page_summary?tabIndex=0"

# Take screenshot
curl --unix-socket /tmp/athena-$(id -u).sock \
  -X GET "http://localhost/internal/screenshot?tabIndex=0" \
  | jq -r '.screenshot' | base64 -d > screenshot.png
```

### Method 3: Unit Tests

```bash
cd agent
npm test                # Run all tests
npm run test:mcp        # Run MCP-specific tests
npm run test:integration # Run integration tests
```

---

## Using with Claude Desktop

### Configuration

Add to your Claude Desktop config (`~/Library/Application Support/Claude/claude_desktop_config.json` on macOS):

```json
{
  "mcpServers": {
    "athena-browser": {
      "command": "node",
      "args": ["/path/to/athena-browser/agent/dist/mcp/stdio-server.js"],
      "env": {
        "ATHENA_SOCKET_PATH": "/tmp/athena-1000.sock",
        "LOG_LEVEL": "info"
      }
    }
  }
}
```

### Usage

1. Start Athena Browser: `./scripts/run.sh`
2. Restart Claude Desktop
3. Claude now has access to all 17 browser tools!

**Example Prompts:**
- "Navigate to google.com and search for 'MCP protocol'"
- "Take a screenshot of the current page"
- "Get me all the forms on this page"
- "Open github.com in a new tab"

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                   Claude Agent SDK                       │
│                  (query() function)                      │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│         Agent SDK MCP Server (agent-adapter.ts)          │
│              17 Tools via createSdkMcpServer()           │
│                                                          │
│  Tools:                                                 │
│  ├─ browser_navigate, browser_back, browser_forward     │
│  ├─ browser_get_url, browser_get_page_summary           │
│  ├─ browser_execute_js, browser_screenshot              │
│  └─ window_create_tab, window_switch_tab, ...           │
└──────────────────────┬──────────────────────────────────┘
                       │ HTTP over Unix socket
                       ▼
┌─────────────────────────────────────────────────────────┐
│          BrowserApiClient (api-client.ts)                │
│         HTTP client for /tmp/athena-UID.sock            │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│        Express Server (server.ts on Unix socket)        │
│          Routes requests to Browser Control              │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│   C++ BrowserControlServer (browser_control_server.cpp)  │
│        HTTP server on /tmp/athena-UID-control.sock      │
│                                                          │
│  Handlers:                                              │
│  ├─ /internal/navigate → CEF Navigate()                │
│  ├─ /internal/screenshot → CEF CaptureScreenshot()     │
│  ├─ /internal/execute_js → CEF ExecuteJavaScript()     │
│  └─ /internal/tab/* → QtMainWindow tab operations      │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
              ┌─────────────────┐
              │  CEF Browser    │
              │  (Chromium)     │
              └─────────────────┘
```

**Key Files:**

| Component | File | Responsibility |
|-----------|------|----------------|
| **MCP Tools** | `agent/src/mcp/agent-adapter.ts` | 17 tool definitions using Claude SDK |
| **API Client** | `agent/src/browser/api-client.ts` | HTTP client for Unix socket communication |
| **Express Server** | `agent/src/server/server.ts` | Main agent HTTP server |
| **Browser Control** | `app/src/runtime/browser_control_server.cpp` | C++ HTTP server for browser operations |
| **Handlers** | `app/src/runtime/browser_control_handlers_*.cpp` | Navigation, tabs, content extraction |
| **Qt Integration** | `app/src/platform/qt_agent_panel.cpp` | Chat UI in browser window |

---

## Adding Custom MCP Tools

### Example: Add a "Find Text" Tool

#### Step 1: Add the tool to `agent/src/mcp/agent-adapter.ts`

```typescript
tool(
  'browser_find_text',
  'Search for text on the current page',
  {
    searchText: z.string().describe('Text to search for'),
    caseSensitive: z.boolean().optional().describe('Case-sensitive search'),
    tabIndex: z.number().optional().describe('Tab index (default: active tab)')
  },
  async (args) => {
    try {
      const result = await client.request('/internal/find_text', 'POST', {
        searchText: args.searchText,
        caseSensitive: args.caseSensitive,
        tabIndex: args.tabIndex
      });
      return {
        content: [{
          type: 'text',
          text: `Found ${result.count} matches for "${args.searchText}"`
        }]
      };
    } catch (error) {
      return {
        content: [{ type: 'text', text: `Search failed: ${error.message}` }],
        isError: true
      };
    }
  }
)
```

#### Step 2: Add API endpoint to `agent/src/browser/api-client.ts`

```typescript
async findText(searchText: string, caseSensitive = false, tabIndex?: number) {
  return this.request<{ count: number; matches: Array<{ index: number }> }>(
    '/internal/find_text',
    'POST',
    { searchText, caseSensitive, tabIndex }
  );
}
```

#### Step 3: Add C++ handler in `app/src/runtime/browser_control_handlers_content.cpp`

```cpp
static void HandleFindText(const HttpRequest& request, HttpResponse& response,
                          QtMainWindow* window) {
  auto json = nlohmann::json::parse(request.body);
  std::string search_text = json.value("searchText", "");
  bool case_sensitive = json.value("caseSensitive", false);
  int tab_index = json.value("tabIndex", -1);

  // Find text using CEF's Find API
  auto* tab = window->GetActiveOrSpecifiedTab(tab_index);
  if (!tab || !tab->browser) {
    response.status = 404;
    response.body = R"({"error": "Tab not found"})";
    return;
  }

  tab->browser->GetHost()->Find(
    search_text,
    /*forward=*/true,
    /*matchCase=*/case_sensitive,
    /*findNext=*/false
  );

  // CEF Find is async, so we return immediately
  // Results come via OnFindResult callback
  nlohmann::json result;
  result["status"] = "searching";
  result["searchText"] = search_text;

  response.status = 200;
  response.body = result.dump();
}
```

#### Step 4: Register the route in `app/src/runtime/browser_control_server_routing.cpp`

```cpp
if (request.path == "/internal/find_text" && request.method == "POST") {
  HandleFindText(request, response, window_);
  return;
}
```

#### Step 5: Test the new tool

```bash
# Rebuild
./scripts/build.sh
cd agent && npm run build

# Test with MCP Inspector
npm run mcp:inspect

# Or test directly
curl --unix-socket /tmp/athena-$(id -u).sock \
  -X POST http://localhost/internal/find_text \
  -H "Content-Type: application/json" \
  -d '{"searchText": "hello", "caseSensitive": false}'
```

---

## Performance Optimization Tips

### 1. Use Lightweight Tools First

**Preferred order for page analysis:**

1. `browser_get_page_summary` (~1-2 KB) - Quick overview
2. `browser_query_content` (~5-10 KB) - Targeted extraction
3. `browser_get_html` (100+ KB) - Full source only when needed

**Example:**
```typescript
// ❌ Bad: Always fetch full HTML
const html = await tools.browser_get_html({});
// Parse 200KB of HTML for just the title...

// ✅ Good: Use page summary first
const summary = await tools.browser_get_page_summary({});
console.log(summary.title); // Got what we needed with 1KB
```

### 2. Dynamic Tool Selection

The agent automatically selects tools based on query type. You can customize this in `agent/src/claude/config/tool-selection.ts`:

```typescript
export function selectToolsForQuery(query: string): string[] {
  if (query.includes('screenshot')) {
    return ['browser_screenshot', 'browser_get_url'];
  }
  if (query.includes('form')) {
    return ['browser_query_content', 'browser_execute_js'];
  }
  // Default: all tools
  return ['*'];
}
```

### 3. Screenshot Quality Tuning

```typescript
// High quality (large file, slower)
await tools.browser_screenshot({ fullPage: true });

// Quick viewport screenshot (faster)
await tools.browser_screenshot({ fullPage: false });
```

### 4. Caching Page Summaries

The agent caches page summaries to avoid redundant calls:

```typescript
// First call: fetches from browser
const summary1 = await tools.browser_get_page_summary({});

// Second call: returns cached result (if URL unchanged)
const summary2 = await tools.browser_get_page_summary({});
```

---

## Configuration

### Environment Variables

```bash
# Agent Configuration
ATHENA_SOCKET_PATH=/tmp/athena-$(id -u).sock       # Main agent socket
ATHENA_CONTROL_SOCKET_PATH=/tmp/athena-$(id -u)-control.sock  # Browser control
LOG_LEVEL=info                                     # debug|info|warn|error

# Claude API
ANTHROPIC_API_KEY=sk-...                           # Required for AI features
CLAUDE_MODEL=claude-sonnet-4-5                     # Model selection
MAX_THINKING_TOKENS=8000                           # Extended thinking budget
MAX_TURNS=20                                       # Max conversation turns

# Permissions
PERMISSION_MODE=default                            # default|acceptEdits|bypassPermissions

# Timeouts
SCREENSHOT_TIMEOUT_MS=90000                        # Screenshot capture timeout
```

### Permission Modes

```typescript
// agent/src/claude/config/permissions.ts

// Default: Ask user for each risky operation
PERMISSION_MODE=default

// Auto-accept code edits (for automation)
PERMISSION_MODE=acceptEdits

// Bypass all permissions (testing only)
PERMISSION_MODE=bypassPermissions
```

---

## Troubleshooting

### Problem: "Failed to connect to socket"

**Solution:**
```bash
# Check if browser is running
ps aux | grep athena-browser

# Check if socket exists
ls -la /tmp/athena-*.sock

# Restart browser
pkill athena-browser
./scripts/run.sh
```

### Problem: "Tool not found"

**Solution:**
```bash
# Rebuild agent
cd agent
npm run build

# Verify tools are registered
npm run mcp:inspect
# Should show all 17 tools
```

### Problem: "Screenshot timeout"

**Solution:**
```bash
# Increase timeout
SCREENSHOT_TIMEOUT_MS=120000 npm run dev

# Or use viewport screenshot instead of full page
await tools.browser_screenshot({ fullPage: false });
```

### Problem: "JavaScript execution failed"

**Causes:**
1. Page not fully loaded
2. Element not found
3. Script error

**Solution:**
```typescript
// Wait for page load first
await tools.browser_execute_js({
  code: `
    new Promise(resolve => {
      if (document.readyState === 'complete') {
        resolve();
      } else {
        window.addEventListener('load', resolve);
      }
    })
  `
});

// Then execute your script
await tools.browser_execute_js({
  code: "document.querySelector('#button').click()"
});
```

### Problem: "Tab index out of bounds"

**Solution:**
```typescript
// Always check tab count first
const info = await tools.window_get_tab_info({});
console.log(`Active tabs: ${info.count}`);

// Use valid tab index (0 to count-1)
if (tabIndex < info.count) {
  await tools.window_switch_tab({ tabIndex });
}
```

---

## Next Steps

1. **Test all tools:** `npm run mcp:inspect`
2. **Add custom tools:** Follow the "Adding Custom MCP Tools" guide above
3. **Integrate with Claude Desktop:** Configure MCP server in Claude Desktop settings
4. **Build automation workflows:** Combine multiple tools for complex tasks
5. **Monitor performance:** Use `LOG_LEVEL=debug` to see detailed logs

---

## Additional Resources

- **MCP Specification:** https://modelcontextprotocol.io
- **Claude Agent SDK Docs:** https://github.com/anthropics/claude-agent-sdk
- **Athena Browser Docs:** See `README.md` and `CLAUDE.md`
- **CEF API Reference:** https://bitbucket.org/chromiumembedded/cef/wiki/Home

---

## Summary

✅ **MCP is fully integrated** - 17 tools ready to use
✅ **Two MCP servers** - Agent SDK + standalone stdio server
✅ **Production ready** - Error handling, permissions, caching
✅ **Extensible** - Easy to add custom tools
✅ **Well-tested** - Unit tests, integration tests, MCP Inspector support

**You don't need to add MCP - it's already there!** Just start using it with the commands above.

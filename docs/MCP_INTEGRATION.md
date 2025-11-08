# Integrating athena-browser-mcp with Athena Browser

## Overview

The `athena-browser-mcp` npm package is a comprehensive MCP server with 40+ browser automation tools. It connects to browsers via **Chrome DevTools Protocol (CDP)** instead of the Unix socket HTTP API currently used by Athena's agent folder.

## Architecture Comparison

### Current Agent Implementation (agent/)
```
Claude Agent SDK
    ↓
Agent Adapter (createSdkMcpServer)
    ↓
BrowserApiClient (HTTP over Unix socket)
    ↓
Express Server (/tmp/athena-UID.sock)
    ↓
C++ BrowserControlServer
    ↓
Qt/CEF Browser
```

### New athena-browser-mcp Package
```
Claude Desktop / MCP Inspector
    ↓ (stdio MCP protocol)
athena-browser-mcp Server
    ↓ (Chrome DevTools Protocol)
CEF Remote Debugging Port :9222
    ↓
Chromium Engine (CEF)
```

## Key Differences

| Aspect | Current Agent | athena-browser-mcp |
|--------|---------------|-------------------|
| **Protocol** | HTTP over Unix sockets | Chrome DevTools Protocol (CDP) |
| **Connection** | /tmp/athena-UID.sock | localhost:9222 (TCP) |
| **Tools** | 17 browser control tools | 40+ advanced automation tools |
| **Library** | Custom HTTP client | chrome-remote-interface |
| **Integration** | Custom C++ HTTP server | CEF's built-in CDP support |

## What athena-browser-mcp Provides

### 40+ Tools in 4 Categories:

**1. Perception & Understanding (11 tools)**
- `dom_tree_get` - Get DOM tree structure
- `ax_tree_get` - Get accessibility tree
- `ui_discover` - Fuse DOM+AX+layout for element discovery
- `layout_get_bbox` - Get element bounding boxes
- `layout_check_visible` - Check element visibility
- `vision_ocr` - OCR for canvas/SVG elements
- `vision_find_by_text` - Find elements by text using vision
- `content_extract_main` - Extract main content (Readability)
- `net_observe` - Monitor network requests
- `net_get_response_body` - Get network response bodies
- `content_get_har` - Get HAR (HTTP Archive) file

**2. Interaction & Navigation (14 tools)**
- `targets_resolve` - Resolve semantic hints to elements
- `act_click` - Click elements (3 strategies: AX, DOM, BBox)
- `act_type` - Type text into elements
- `act_select` - Select dropdown options
- `act_scroll` - Scroll to elements
- `act_upload` - Upload files
- `nav_goto` - Navigate to URL
- `nav_wait` - Wait for conditions (load, idle, selector, etc.)
- `nav_switch_frame` - Switch to iframe
- `form_detect` - Detect form elements
- `form_fill` - Fill form fields
- `form_submit` - Submit forms
- `key_press` - Press key sequences
- `key_type` - Type text with keyboard

**3. Session & Memory (10 tools)**
- `session_save` - Save cookies and storage
- `session_restore` - Restore session
- `session_clear` - Clear session data
- `memory_learn_selector` - Learn stable selectors for sites
- `memory_get_selector` - Retrieve learned selectors
- `safety_set_policy` - Set domain allowlists and budgets
- `safety_get_policy` - Get current safety policy
- `audit_snapshot` - Capture screenshot + DOM + HAR
- `audit_get_log` - Get audit log entries
- `audit_clear_log` - Clear audit log

**4. Advanced Features**
- Multiple click strategies (Accessibility, DOM, BBox)
- Shadow DOM piercing
- Iframe handling
- OCR fallback for canvas elements
- Network request interception
- Session persistence
- Safety controls (allowlists, action budgets)
- Audit logging with screenshots

## Integration Requirements

### Step 1: Enable CDP in Athena Browser

CEF has built-in support for remote debugging via CDP. We need to enable it by adding a command-line flag:

**File:** `app/src/browser/cef_engine.cpp` (line 56)

**Current Status:** ✅ **Already Enabled!**

```cpp
// CEF settings in cef_engine.cpp
CefSettings settings;
// ...
settings.remote_debugging_port = 9222;  // ✅ Already configured
```

**No changes needed!** Athena Browser already has CDP enabled on port 9222. Just configure the MCP package to use port 9222:

```bash
export CEF_BRIDGE_PORT=9222
```

### Step 2: Configuration

**Environment Variables:**
```bash
# For athena-browser-mcp
export CEF_BRIDGE_HOST=127.0.0.1
export CEF_BRIDGE_PORT=9222
export ALLOWED_FILE_DIRS=/home/user/downloads,/tmp
export DEFAULT_TIMEOUT_MS=30000
```

**Claude Desktop Config** (`~/.config/Claude/claude_desktop_config.json`):
```json
{
  "mcpServers": {
    "athena-browser": {
      "command": "npx",
      "args": ["athena-browser-mcp"],
      "env": {
        "CEF_BRIDGE_HOST": "127.0.0.1",
        "CEF_BRIDGE_PORT": "9222"
      }
    }
  }
}
```

### Step 3: Test Connection

1. **Start Athena Browser with CDP enabled:**
```bash
# Build with CDP support
./scripts/build.sh

# Run browser (should expose port 9222)
./scripts/run.sh
```

2. **Verify CDP endpoint:**
```bash
# Should return JSON with browser info
curl http://localhost:9222/json
```

Expected response:
```json
[
  {
    "description": "",
    "devtoolsFrontendUrl": "/devtools/inspector.html?ws=localhost:9222/devtools/page/...",
    "id": "...",
    "title": "...",
    "type": "page",
    "url": "...",
    "webSocketDebuggerUrl": "ws://localhost:9222/devtools/page/..."
  }
]
```

3. **Test MCP connection:**
```bash
# Install the package globally or use npx
npx athena-browser-mcp

# Or with MCP Inspector
npx @modelcontextprotocol/inspector npx athena-browser-mcp
```

### Step 4: Security Considerations

**Local-only access:**
- CDP port should only listen on `127.0.0.1` (localhost)
- Never expose CDP to the network (security risk!)
- Consider adding `--remote-allow-origins=http://localhost:*` flag

**File uploads:**
- Set `ALLOWED_FILE_DIRS` to restrict file access
- Only allow uploads from safe directories

**Domain restrictions:**
- Use `safety_set_policy` tool to restrict navigation to approved domains

## Implementation Checklist

### Phase 1: Basic CDP Integration ✅
- [x] Install athena-browser-mcp package
- [x] CDP already enabled on port 9222 in CEF settings (no changes needed!)
- [x] Create test script for CDP connection (`scripts/test-cdp-connection.sh`)
- [x] Create Claude Desktop configuration (`docs/claude-desktop-config-mcp.json`)
- [x] Create comprehensive integration documentation
- [ ] Test CDP endpoint with curl (requires running browser)
- [ ] Test MCP connection with MCP Inspector

### Phase 2: Testing & Validation
- [ ] Run `./scripts/test-cdp-connection.sh` to verify CDP endpoint
- [ ] Test with MCP Inspector: `CEF_BRIDGE_PORT=9222 npx @modelcontextprotocol/inspector npx athena-browser-mcp`
- [ ] Test all 40+ MCP tools with Athena Browser
- [ ] Verify DOM tree extraction works
- [ ] Verify accessibility tree works
- [ ] Verify screenshot capture works
- [ ] Verify network monitoring works
- [ ] Test form detection and filling
- [ ] Test session save/restore

### Phase 3: Documentation & Cleanup
- [x] Update main README with CDP setup instructions
- [x] Create comprehensive MCP integration guide (`docs/MCP_INTEGRATION.md`)
- [x] Create Claude Desktop config example
- [ ] Add usage examples for common automation tasks
- [ ] Plan migration from old agent/ MCP code
- [ ] Document deprecation timeline for agent/src/mcp/ files

### Phase 4: Production Hardening
- [ ] Add safety policies for production use
- [ ] Configure domain allowlists
- [ ] Set up audit logging
- [ ] Add CDP connection health checks
- [ ] Document troubleshooting steps

## Migration Path

### What to Keep from agent/
- Express HTTP server (for backward compatibility if needed)
- BrowserControlServer in C++ (for custom endpoints)
- Logging infrastructure
- Session management

### What to Remove from agent/
- ❌ `agent/src/mcp/agent-adapter.ts` (replaced by athena-browser-mcp)
- ❌ `agent/src/mcp/server.ts` (replaced by athena-browser-mcp)
- ❌ `agent/src/mcp/stdio-server.ts` (replaced by athena-browser-mcp)
- ❌ 17 custom MCP tools (replaced by 40+ tools in athena-browser-mcp)

### What to Keep (Optional)
- ✅ `agent/src/claude/` (if you want Claude Agent SDK integration)
- ✅ `agent/src/server/` (if you want HTTP API for backward compat)
- ✅ Custom features not in athena-browser-mcp

## Benefits of athena-browser-mcp

1. **More Tools:** 40+ vs 17 tools (2.3x more capabilities)
2. **Better Automation:** Advanced features like form detection, OCR, network monitoring
3. **Standard Protocol:** Uses CDP (industry standard) instead of custom HTTP API
4. **Active Maintenance:** Published npm package with updates
5. **No Custom Code:** Leverage battle-tested CDP library
6. **Less Code to Maintain:** Remove custom MCP implementation

## Next Steps

1. **Enable CDP** in Athena Browser CEF settings
2. **Test connection** with `curl http://localhost:9222/json`
3. **Run MCP Inspector** to verify all tools work
4. **Update docs** with new setup instructions
5. **Clean up** old agent/ MCP code

## Example: Testing CDP Connection

```bash
# Terminal 1: Start Athena Browser with CDP
./scripts/run.sh

# Terminal 2: Check CDP endpoint
curl http://localhost:9222/json | jq

# Terminal 3: Test with MCP Inspector
npx @modelcontextprotocol/inspector npx athena-browser-mcp

# Terminal 4: Or use with Claude Desktop (after config)
# Just restart Claude Desktop and ask it to navigate to a website
```

## Troubleshooting

### CDP Connection Refused
```bash
# Check if port is open
lsof -i :9222

# Check CEF logs for remote debugging output
LOG_LEVEL=debug ./scripts/run.sh 2>&1 | grep -i "remote"
```

### MCP Server Can't Connect
```bash
# Verify environment variables
echo $CEF_BRIDGE_PORT  # Should be 9222

# Test direct CDP connection
npx chrome-remote-interface inspect --host=127.0.0.1 --port=9222
```

### Missing Tools
```bash
# Verify package installation
npm list athena-browser-mcp

# Check version
npx athena-browser-mcp --version
```

## References

- **athena-browser-mcp GitHub:** https://github.com/lespaceman/athena-browser-mcp
- **athena-browser-mcp npm:** https://www.npmjs.com/package/athena-browser-mcp
- **Chrome DevTools Protocol:** https://chromedevtools.github.io/devtools-protocol/
- **CEF Remote Debugging:** https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-remote-debugging

---

## Security Considerations

### CDP Port Exposure

Athena Browser exposes Chrome DevTools Protocol on **127.0.0.1:9222**:

**Security Model:**
- ✅ **Localhost binding** - Port 9222 is bound to `127.0.0.1` (localhost only), not `0.0.0.0`
- ✅ **Not exposed to network** - External machines cannot connect
- ⚠️ **No authentication** - Standard CDP behavior, no authentication layer
- ⚠️ **Full browser access** - Any local process can control the browser

**Configured in:** `app/src/browser/cef_engine.cpp:56`
```cpp
settings.remote_debugging_port = 9222;
```

**Threat Model:**

| Threat | Risk Level | Mitigation |
|--------|-----------|------------|
| Local process accesses CDP | Medium | Localhost binding prevents network access |
| Malware on same machine | High | Same risk as any local admin process |
| Network eavesdropping | None | Port not exposed to network |
| MITM attacks | None | Local TCP connection |

**Recommendations:**

1. **Firewall Rules** - Ensure firewall blocks port 9222 from external access:
   ```bash
   # Linux (ufw)
   sudo ufw deny 9222/tcp

   # Check port is not exposed
   sudo netstat -tlnp | grep 9222
   ```

2. **Production Deployments** - Consider adding authentication layer if exposing to network
3. **Trusted Environment** - Only run athena-browser-mcp on trusted machines
4. **Monitor Access** - Use audit logging tools to track CDP connections

### File Upload Security

`athena-browser-mcp` restricts file uploads to allowlisted directories via `ATHENA_ALLOWED_DIRS`:

**Default Allowlist:**
- `~/Downloads` - User's downloads folder (platform-specific)
- `/tmp` (Linux/macOS) or `%TEMP%` (Windows) - Temporary files

**Threat Model:**

| Threat | Risk Level | Mitigation |
|--------|-----------|------------|
| Arbitrary file system access | High | Allowlist prevents access outside configured dirs |
| `/tmp` world-writable exploits | Medium | Consider more restrictive defaults |
| Symlink attacks | Medium | athena-browser-mcp resolves symlinks |
| Path traversal (`../`) | Low | Library validates paths |

**Customization:**

**Linux/macOS:**
```bash
export ATHENA_ALLOWED_DIRS="/custom/path:/another/path"
npx athena-browser-mcp
```

**Windows:**
```cmd
set ATHENA_ALLOWED_DIRS=C:\custom\path;C:\another\path
npx athena-browser-mcp
```

**Claude Desktop Configuration:**
```json
{
  "mcpServers": {
    "athena-browser": {
      "command": "npx",
      "args": ["-y", "athena-browser-mcp"],
      "env": {
        "CEF_BRIDGE_PORT": "9222",
        "ATHENA_ALLOWED_DIRS": "/home/yourname/uploads:/tmp"
      }
    }
  }
}
```

**Production Recommendations:**

1. **Avoid `/tmp`** - Use dedicated upload directory:
   ```bash
   mkdir -p ~/athena-uploads
   chmod 700 ~/athena-uploads  # User-only access
   export ATHENA_ALLOWED_DIRS="$HOME/athena-uploads"
   ```

2. **Principle of Least Privilege** - Only allow directories actually needed
3. **Regular Cleanup** - Periodically clean upload directories
4. **Disk Quotas** - Consider setting disk quotas to prevent DoS

### Network Monitoring Privacy

Network monitoring tools (`net_observe`, `content_get_har`) can capture sensitive data:

**What Gets Captured:**
- HTTP headers (may include auth tokens)
- Request/response bodies
- Cookies
- API endpoints

**Recommendations:**
1. **Clear logs** - Use `audit_clear_log` after each session
2. **Avoid sensitive data** - Don't monitor login flows or payment pages
3. **Secure storage** - If storing HAR files, encrypt them
4. **Compliance** - Check privacy regulations (GDPR, CCPA) for your use case

### Session Persistence Security

`session_save` / `session_restore` preserve:
- Cookies (may include auth tokens)
- localStorage/sessionStorage
- IndexedDB

**Recommendations:**
1. **Encrypted storage** - Store session files encrypted
2. **Expire sessions** - Don't persist sessions indefinitely
3. **Access controls** - Restrict file permissions on session files
4. **Credential rotation** - Rotate auth tokens after restoring sessions

### Safety Controls

`athena-browser-mcp` provides built-in safety controls:

**Domain Allowlists:**
```bash
# Only allow specific domains
npx athena-browser-mcp --allowed-domains="example.com,trusted.org"
```

**Action Budgets:**
```bash
# Limit number of actions per session
npx athena-browser-mcp --action-budget=100
```

**Audit Logging:**
All actions are logged with timestamps, screenshots, and DOM snapshots for accountability.

### Best Practices Summary

| Practice | Rationale |
|----------|-----------|
| ✅ Bind CDP to localhost only | Prevent network exposure |
| ✅ Use restrictive file allowlists | Prevent arbitrary file access |
| ✅ Enable audit logging | Track all browser actions |
| ✅ Set domain allowlists | Prevent navigation to untrusted sites |
| ✅ Use action budgets | Prevent runaway automation |
| ✅ Regularly clear sessions/logs | Minimize sensitive data retention |
| ✅ Encrypt sensitive data at rest | Protect stored sessions/HAR files |
| ✅ Run on trusted machines only | Local process can fully control browser |

---

## Summary

The `athena-browser-mcp` package provides a production-ready MCP server with 40+ tools that connects to browsers via Chrome DevTools Protocol. To use it with Athena Browser:

1. Add `--remote-debugging-port=9222` to CEF settings (already configured)
2. Rebuild and run Athena Browser
3. Use `npx athena-browser-mcp` or configure Claude Desktop
4. Configure `ATHENA_ALLOWED_DIRS` for file upload security
5. Review security considerations above for production deployments

This gives you 2.3x more tools with industry-standard CDP protocol, comprehensive security controls, and less code to maintain!

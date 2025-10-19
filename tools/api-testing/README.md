# Athena Browser API Testing Tools

Tools for testing the Athena Browser Control API via Unix sockets.

## Quick Start

```bash
# Interactive menu (recommended)
./test-api.sh

# OR use Postman with HTTP proxy
python3 proxy-server.py 3333
# Import Athena_Browser_API_HTTP.postman_collection.json in Postman

# OR use curl directly
curl -X GET --unix-socket /tmp/athena-$(id -u)-control.sock \
  http://localhost/internal/get_url
```

## Testing Options

### 1. Interactive Script ⭐ (Recommended)

```bash
./test-api.sh
```

**Features:**
- Interactive menu with all 20 endpoints
- Pre-configured examples
- Quick test suite
- Automatic socket detection

**Requirements:** curl, jq (optional)

---

### 2. Postman with HTTP Proxy

```bash
# Start proxy
python3 proxy-server.py 3333

# In Postman:
# 1. Import: Athena_Browser_API_HTTP.postman_collection.json
# 2. Base URL: http://localhost:3333 (already set)
# 3. Send requests
```

**Why proxy?** Postman's Unix socket support is unreliable. The proxy forwards HTTP → Unix socket.

**Requirements:** Python 3, Postman Desktop

---

### 3. Direct curl

```bash
SOCKET="/tmp/athena-$(id -u)-control.sock"

# Navigate
curl -X POST --unix-socket "$SOCKET" \
  http://localhost/internal/navigate \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com"}'

# Get page summary
curl -X GET --unix-socket "$SOCKET" \
  http://localhost/internal/get_page_summary | jq '.'

# Execute JavaScript
curl -X POST --unix-socket "$SOCKET" \
  http://localhost/internal/execute_js \
  -H "Content-Type: application/json" \
  -d '{"code": "document.title"}'
```

## API Endpoints

### Navigation
```bash
POST /internal/navigate          # Navigate to URL (waits for load)
POST /internal/open_url          # Open URL (creates tab if needed)
POST /internal/history           # Back/forward: {"action": "back|forward"}
POST /internal/reload            # Reload: {"ignoreCache": true|false}
GET  /internal/get_url           # Get current URL
```

### Content Extraction
```bash
GET  /internal/get_html                  # Full HTML (50-500KB)
GET  /internal/get_page_summary          # Structured summary (1-2KB) ⚡
GET  /internal/get_interactive_elements  # Clickable elements with positions
GET  /internal/get_accessibility_tree    # Semantic DOM structure
POST /internal/query_content             # Extract: {"queryType": "forms|navigation|article|tables|media"}
```

**💡 Tip:** Use `get_page_summary` and `get_interactive_elements` for efficient content extraction.

### JavaScript Execution
```bash
POST /internal/execute_js        # Execute arbitrary JavaScript
# Body: {"code": "document.title", "tabIndex": 0}
```

Returns typed results: string, number, boolean, object, array, null, undefined

### Screenshots
```bash
GET  /internal/screenshot                 # Viewport screenshot (base64 PNG)
POST /internal/screenshot                 # {"fullPage": true} for full page
GET  /internal/get_annotated_screenshot   # With element overlays
```

### Tab Management
```bash
GET  /internal/tab_count         # Total tab count
GET  /internal/tab_info          # Count + active tab index
POST /internal/tab_create        # Create tab: {"url": "..."}
POST /internal/tab_switch        # Switch: {"tabIndex": 0}
POST /internal/tab_close         # Close: {"tabIndex": 0}
```

### Health
```bash
GET  /health                     # Server health check
```

## Response Format

All endpoints return JSON with `success` field:

**Success:**
```json
{
  "success": true,
  "result": "...",
  "tabIndex": 0
}
```

**Error:**
```json
{
  "success": false,
  "error": "Error message"
}
```

## Example Workflows

### Navigate and Extract Content
```bash
SOCKET="/tmp/athena-$(id -u)-control.sock"

# 1. Navigate
curl -s -X POST --unix-socket "$SOCKET" \
  http://localhost/internal/navigate \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com"}'

# 2. Get summary (1-2KB instead of 50KB+ HTML)
curl -s -X GET --unix-socket "$SOCKET" \
  http://localhost/internal/get_page_summary | jq '.summary'

# 3. Find clickable elements
curl -s -X GET --unix-socket "$SOCKET" \
  http://localhost/internal/get_interactive_elements | \
  jq '.elements[] | {tag, text, href}'
```

### Multi-Tab Session
```bash
SOCKET="/tmp/athena-$(id -u)-control.sock"

# Create tabs
curl -s -X POST --unix-socket "$SOCKET" \
  http://localhost/internal/tab_create \
  -H "Content-Type: application/json" \
  -d '{"url": "https://github.com"}'

curl -s -X POST --unix-socket "$SOCKET" \
  http://localhost/internal/tab_create \
  -H "Content-Type: application/json" \
  -d '{"url": "https://anthropic.com"}'

# Check tabs
curl -s -X GET --unix-socket "$SOCKET" \
  http://localhost/internal/tab_info | jq '.'

# Switch to first tab
curl -s -X POST --unix-socket "$SOCKET" \
  http://localhost/internal/tab_switch \
  -H "Content-Type: application/json" \
  -d '{"tabIndex": 0}'
```

### Automated Testing Script
```bash
#!/bin/bash
SOCKET="/tmp/athena-$(id -u)-control.sock"

# Navigate
curl -s -X POST --unix-socket "$SOCKET" \
  http://localhost/internal/navigate \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com"}' | jq '.success'

# Execute JS
curl -s -X POST --unix-socket "$SOCKET" \
  http://localhost/internal/execute_js \
  -H "Content-Type: application/json" \
  -d '{"code": "document.title"}' | jq '.result'

# Screenshot
curl -s -X GET --unix-socket "$SOCKET" \
  http://localhost/internal/screenshot | jq '.success'
```

## Troubleshooting

**Socket not found:**
```bash
# Check browser is running
ps aux | grep athena-browser

# Find socket
ls -la /tmp/athena-*-control.sock
# Expected: srwx------ (700 permissions)
```

**Permission denied:**
- Ensure you own the socket
- Check permissions with `ls -la`

**Proxy connection refused:**
```bash
# Check if proxy is running
lsof -i :3333

# Restart if needed
python3 proxy-server.py 3333
```

**Page still loading:**
- Wait a few seconds and retry
- Navigation waits 15s, content extraction waits 5s

## Files

- **`test-api.sh`** - Interactive test menu ⭐
- **`proxy-server.py`** - HTTP-to-Unix-socket proxy (no dependencies)
- **`proxy-server.js`** - Node.js version (requires: `npm install http-proxy`)
- **`Athena_Browser_API_HTTP.postman_collection.json`** - Postman collection (50+ requests)
- **`README.md`** - This file

## Performance Tips

| Endpoint | Size | Use When |
|----------|------|----------|
| `get_page_summary` | 1-2KB | Quick overview, Claude conversations |
| `get_interactive_elements` | 5-20KB | Finding clickable items |
| `get_accessibility_tree` | 10-30KB | Semantic structure |
| `get_html` | 50-500KB | Need full DOM |

**For AI/Claude:** Use context-efficient endpoints to reduce token usage by 98%.

## Security

- **Local only** - Unix sockets, no network exposure
- **Owner access only** - 0600 file permissions
- **No authentication** - Trusts local processes
- **DoS protection** - 1MB request size limit

## Support

1. Verify browser is running: `ps aux | grep athena-browser`
2. Check socket exists: `ls /tmp/athena-*-control.sock`
3. Test with simple curl command first
4. Run quick test suite: `./test-api.sh` → option 20

#!/usr/bin/env bash
# Demo script showing how to view and test Athena logs
#
# Usage:
#   ./scripts/test-all-logs.sh          # Run demo
#   LOG_LEVEL=debug ./scripts/test-all-logs.sh   # With debug logs

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOCKET_PATH="/tmp/athena-$(id -u).sock"
LOG_FILE="/tmp/athena-log-demo.log"

echo "=== Athena Logging Demo ==="
echo ""
echo "This script demonstrates unified logging across C++ and Node.js"
echo ""

# Check if browser is running
if pgrep -f "athena-browser" > /dev/null; then
    echo "✓ Browser is already running"
    echo "  View live logs: journalctl --user -u athena-browser.service -f"
    ALREADY_RUNNING=true
else
    echo "Starting browser with LOG_LEVEL=${LOG_LEVEL:-info}..."
    "$ROOT_DIR/scripts/run.sh" > "$LOG_FILE" 2>&1 &
    BROWSER_PID=$!
    echo "  Browser PID: $BROWSER_PID"
    echo "  Logs: $LOG_FILE"
    echo "  Waiting 3 seconds for startup..."
    sleep 3
    ALREADY_RUNNING=false
fi

echo ""
echo "=== Testing Log Modules ==="
echo ""

# Test health endpoint
echo "1. Testing health endpoint (triggers BrowserApiClient logs)..."
if curl --unix-socket "$SOCKET_PATH" -s http://localhost/health > /dev/null 2>&1; then
    echo "   ✓ Health check successful"
else
    echo "   ✗ Health check failed (browser may not be ready)"
fi

echo ""
echo "2. Example API call (triggers multiple log modules)..."
echo "   curl --unix-socket $SOCKET_PATH -X GET http://localhost/health"

echo ""
echo "=== Viewing Logs ==="
echo ""

if [ "$ALREADY_RUNNING" = false ]; then
    echo "Logs from this run:"
    echo "  cat $LOG_FILE | $ROOT_DIR/scripts/view-logs.sh"
    echo ""
    echo "Sample logs:"
    tail -20 "$LOG_FILE" | "$ROOT_DIR/scripts/view-logs.sh" | head -10
else
    echo "View system logs:"
    echo "  journalctl --user -u athena-browser.service -n 50"
    echo ""
    echo "View live logs:"
    echo "  journalctl --user -u athena-browser.service -f | $ROOT_DIR/scripts/view-logs.sh"
fi

echo ""
echo "=== Log Modules ==="
echo ""
echo "C++ Modules:"
echo "  - Application: High-level app lifecycle"
echo "  - BrowserWindow: Window management"
echo "  - CEFEngine: Browser engine"
echo "  - GLRenderer: OpenGL rendering"
echo "  - NodeRuntime: Node.js integration"
echo ""
echo "Node.js Modules:"
echo "  - Server: HTTP server"
echo "  - NativeController: Browser control"
echo "  - BrowserApiClient: API client"
echo "  - SessionManager: Chat sessions"
echo "  - ClaudeClient: Claude API"
echo "  - MCPServer: MCP tools"
echo ""
echo "=== Log Levels ==="
echo ""
echo "Set LOG_LEVEL environment variable:"
echo "  LOG_LEVEL=debug ./scripts/run.sh    # All logs"
echo "  LOG_LEVEL=info ./scripts/run.sh     # Info and above (default)"
echo "  LOG_LEVEL=warn ./scripts/run.sh     # Warnings and errors only"
echo "  LOG_LEVEL=error ./scripts/run.sh    # Errors only"
echo ""

if [ "$ALREADY_RUNNING" = false ]; then
    echo "Stopping demo browser (PID: $BROWSER_PID)..."
    kill "$BROWSER_PID" 2>/dev/null || true
    sleep 1
fi

echo ""
echo "Demo complete!"
echo ""

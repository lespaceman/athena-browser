#!/bin/bash

# Test Athena Browser MCP Integration
# This script verifies that all MCP tools are working correctly

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
AGENT_DIR="$PROJECT_ROOT/agent"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  Athena Browser MCP Integration Test${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Check if browser is running
SOCKET_PATH="/tmp/athena-$(id -u).sock"
CONTROL_SOCKET="/tmp/athena-$(id -u)-control.sock"

if [ ! -S "$SOCKET_PATH" ]; then
  echo -e "${RED}❌ Error: Athena Browser is not running${NC}"
  echo -e "${YELLOW}   Please start the browser first: ./scripts/run.sh${NC}"
  exit 1
fi

echo -e "${GREEN}✅ Browser socket found: $SOCKET_PATH${NC}"

# Check if agent is built
if [ ! -d "$AGENT_DIR/dist" ]; then
  echo -e "${YELLOW}⚠️  Agent not built, building now...${NC}"
  cd "$AGENT_DIR"
  npm install
  npm run build
  cd "$PROJECT_ROOT"
  echo -e "${GREEN}✅ Agent built successfully${NC}"
fi

echo ""
echo -e "${BLUE}Testing MCP Tools:${NC}"
echo ""

# Test 1: Health Check
echo -e "${YELLOW}1️⃣  Testing health endpoint...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" http://localhost/health)
if echo "$RESPONSE" | grep -q "ok"; then
  echo -e "${GREEN}   ✅ Health check passed${NC}"
else
  echo -e "${RED}   ❌ Health check failed${NC}"
  exit 1
fi

# Test 2: Navigation
echo -e "${YELLOW}2️⃣  Testing navigation...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" \
  -X POST http://localhost/internal/navigate \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com"}')
if echo "$RESPONSE" | grep -q "finalUrl"; then
  echo -e "${GREEN}   ✅ Navigation works${NC}"
else
  echo -e "${RED}   ❌ Navigation failed: $RESPONSE${NC}"
  exit 1
fi

# Wait for page to load
sleep 2

# Test 3: Get URL
echo -e "${YELLOW}3️⃣  Testing URL retrieval...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" \
  -X GET "http://localhost/internal/get_url?tabIndex=0")
if echo "$RESPONSE" | grep -q "url"; then
  URL=$(echo "$RESPONSE" | jq -r '.url')
  echo -e "${GREEN}   ✅ Current URL: $URL${NC}"
else
  echo -e "${RED}   ❌ URL retrieval failed${NC}"
  exit 1
fi

# Test 4: Page Summary
echo -e "${YELLOW}4️⃣  Testing page summary...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" \
  -X GET "http://localhost/internal/get_page_summary?tabIndex=0")
if echo "$RESPONSE" | grep -q "summary"; then
  TITLE=$(echo "$RESPONSE" | jq -r '.summary.title')
  echo -e "${GREEN}   ✅ Page title: $TITLE${NC}"
else
  echo -e "${RED}   ❌ Page summary failed${NC}"
  exit 1
fi

# Test 5: JavaScript Execution
echo -e "${YELLOW}5️⃣  Testing JavaScript execution...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" \
  -X POST http://localhost/internal/execute_js \
  -H "Content-Type: application/json" \
  -d '{"code": "document.title", "tabIndex": 0}')
if echo "$RESPONSE" | grep -q "result"; then
  RESULT=$(echo "$RESPONSE" | jq -r '.result')
  echo -e "${GREEN}   ✅ JS result: $RESULT${NC}"
else
  echo -e "${RED}   ❌ JavaScript execution failed${NC}"
  exit 1
fi

# Test 6: Interactive Elements
echo -e "${YELLOW}6️⃣  Testing interactive elements...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" \
  -X GET "http://localhost/internal/get_interactive_elements?tabIndex=0")
if echo "$RESPONSE" | grep -q "elements"; then
  COUNT=$(echo "$RESPONSE" | jq '.elements | length')
  echo -e "${GREEN}   ✅ Found $COUNT interactive elements${NC}"
else
  echo -e "${RED}   ❌ Interactive elements failed${NC}"
  exit 1
fi

# Test 7: Screenshot
echo -e "${YELLOW}7️⃣  Testing screenshot capture...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" \
  -X GET "http://localhost/internal/screenshot?tabIndex=0")
if echo "$RESPONSE" | grep -q "screenshot"; then
  SCREENSHOT_SIZE=$(echo "$RESPONSE" | jq -r '.screenshot' | wc -c)
  SIZE_KB=$((SCREENSHOT_SIZE / 1024))
  echo -e "${GREEN}   ✅ Screenshot captured (${SIZE_KB} KB base64)${NC}"
else
  echo -e "${RED}   ❌ Screenshot failed${NC}"
  exit 1
fi

# Test 8: Tab Info
echo -e "${YELLOW}8️⃣  Testing tab management...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" \
  -X GET "http://localhost/internal/get_tab_info")
if echo "$RESPONSE" | grep -q "count"; then
  TAB_COUNT=$(echo "$RESPONSE" | jq -r '.count')
  ACTIVE_TAB=$(echo "$RESPONSE" | jq -r '.activeTabIndex')
  echo -e "${GREEN}   ✅ Tab count: $TAB_COUNT, Active: $ACTIVE_TAB${NC}"
else
  echo -e "${RED}   ❌ Tab info failed${NC}"
  exit 1
fi

# Test 9: Query Content
echo -e "${YELLOW}9️⃣  Testing content query (forms)...${NC}"
RESPONSE=$(curl -s --unix-socket "$SOCKET_PATH" \
  -X POST http://localhost/internal/query_content \
  -H "Content-Type: application/json" \
  -d '{"queryType": "forms", "tabIndex": 0}')
if echo "$RESPONSE" | grep -q "data"; then
  echo -e "${GREEN}   ✅ Content query works${NC}"
else
  echo -e "${RED}   ❌ Content query failed${NC}"
  exit 1
fi

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✨ All MCP tools tested successfully!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${BLUE}📊 Test Summary:${NC}"
echo -e "   ${GREEN}✅${NC} Health check"
echo -e "   ${GREEN}✅${NC} Navigation (browser_navigate)"
echo -e "   ${GREEN}✅${NC} URL retrieval (browser_get_url)"
echo -e "   ${GREEN}✅${NC} Page summary (browser_get_page_summary)"
echo -e "   ${GREEN}✅${NC} JavaScript execution (browser_execute_js)"
echo -e "   ${GREEN}✅${NC} Interactive elements (browser_get_interactive_elements)"
echo -e "   ${GREEN}✅${NC} Screenshot capture (browser_screenshot)"
echo -e "   ${GREEN}✅${NC} Tab management (window_get_tab_info)"
echo -e "   ${GREEN}✅${NC} Content query (browser_query_content)"
echo ""
echo -e "${BLUE}🎉 Athena Browser MCP integration is fully functional!${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo -e "   • Test interactively: ${GREEN}cd agent && npm run mcp:inspect${NC}"
echo -e "   • Run demo script: ${GREEN}cd agent && tsx examples/mcp-demo.ts${NC}"
echo -e "   • Use with Claude Desktop: See ${GREEN}agent/examples/claude-desktop-config.json${NC}"
echo -e "   • Read full guide: ${GREEN}cat MCP_GUIDE.md${NC}"
echo ""

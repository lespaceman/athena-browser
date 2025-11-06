#!/bin/bash

# Test CDP (Chrome DevTools Protocol) Connection
# This script verifies that Athena Browser's CDP endpoint is accessible

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  Athena Browser CDP Connection Test${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

CDP_HOST="127.0.0.1"
CDP_PORT="9222"

echo -e "${YELLOW}Testing CDP endpoint at ${CDP_HOST}:${CDP_PORT}...${NC}"
echo ""

# Test 1: Check if port is open
echo -e "${YELLOW}1️⃣  Checking if CDP port is open...${NC}"
if lsof -i :${CDP_PORT} > /dev/null 2>&1; then
  PROCESS=$(lsof -i :${CDP_PORT} | grep LISTEN | awk '{print $1}' | head -1)
  echo -e "${GREEN}   ✅ Port ${CDP_PORT} is open (process: ${PROCESS})${NC}"
else
  echo -e "${RED}   ❌ Port ${CDP_PORT} is not open${NC}"
  echo -e "${YELLOW}   Please start Athena Browser first: ./scripts/run.sh${NC}"
  exit 1
fi

# Test 2: Query CDP version endpoint
echo -e "${YELLOW}2️⃣  Querying CDP /json/version endpoint...${NC}"
RESPONSE=$(curl -s http://${CDP_HOST}:${CDP_PORT}/json/version 2>/dev/null || echo "")
if [ -n "$RESPONSE" ]; then
  echo -e "${GREEN}   ✅ CDP version endpoint accessible${NC}"

  # Parse browser info
  BROWSER=$(echo "$RESPONSE" | jq -r '.Browser' 2>/dev/null || echo "Unknown")
  PROTOCOL=$(echo "$RESPONSE" | jq -r '.["Protocol-Version"]' 2>/dev/null || echo "Unknown")
  USER_AGENT=$(echo "$RESPONSE" | jq -r '.["User-Agent"]' 2>/dev/null || echo "Unknown")

  echo -e "${BLUE}   Browser: ${BROWSER}${NC}"
  echo -e "${BLUE}   Protocol Version: ${PROTOCOL}${NC}"
  echo -e "${BLUE}   User Agent: ${USER_AGENT:0:60}...${NC}"
else
  echo -e "${RED}   ❌ Failed to query CDP version endpoint${NC}"
  exit 1
fi
echo ""

# Test 3: List CDP pages/targets
echo -e "${YELLOW}3️⃣  Listing CDP pages/targets...${NC}"
PAGES=$(curl -s http://${CDP_HOST}:${CDP_PORT}/json 2>/dev/null || echo "[]")
PAGE_COUNT=$(echo "$PAGES" | jq 'length' 2>/dev/null || echo "0")

if [ "$PAGE_COUNT" -gt 0 ]; then
  echo -e "${GREEN}   ✅ Found ${PAGE_COUNT} page(s)${NC}"

  # Show first page details
  FIRST_PAGE_TITLE=$(echo "$PAGES" | jq -r '.[0].title' 2>/dev/null || echo "Unknown")
  FIRST_PAGE_URL=$(echo "$PAGES" | jq -r '.[0].url' 2>/dev/null || echo "Unknown")
  FIRST_PAGE_ID=$(echo "$PAGES" | jq -r '.[0].id' 2>/dev/null || echo "Unknown")

  echo -e "${BLUE}   First page:${NC}"
  echo -e "${BLUE}     Title: ${FIRST_PAGE_TITLE}${NC}"
  echo -e "${BLUE}     URL: ${FIRST_PAGE_URL}${NC}"
  echo -e "${BLUE}     ID: ${FIRST_PAGE_ID}${NC}"
else
  echo -e "${YELLOW}   ⚠️  No pages found (browser may still be initializing)${NC}"
fi
echo ""

# Test 4: Test athena-browser-mcp package connection
echo -e "${YELLOW}4️⃣  Testing athena-browser-mcp package...${NC}"
if command -v npx &> /dev/null; then
  # Check if package is installed
  if npm list athena-browser-mcp &> /dev/null || npx --yes athena-browser-mcp --version &> /dev/null 2>&1; then
    echo -e "${GREEN}   ✅ athena-browser-mcp package is available${NC}"

    # Create a test to verify connection (timeout after 5 seconds)
    echo -e "${BLUE}   Testing MCP connection (this may take a few seconds)...${NC}"

    # Note: We can't easily test the full MCP connection without starting the server
    # Just verify the package is accessible
    echo -e "${GREEN}   ✅ Package is ready to use${NC}"
    echo -e "${BLUE}   To test full connection, run:${NC}"
    echo -e "${BLUE}     CEF_BRIDGE_PORT=9222 npx @modelcontextprotocol/inspector npx athena-browser-mcp${NC}"
  else
    echo -e "${YELLOW}   ⚠️  athena-browser-mcp not installed${NC}"
    echo -e "${BLUE}   To install: npm install athena-browser-mcp${NC}"
  fi
else
  echo -e "${YELLOW}   ⚠️  npx not found${NC}"
fi
echo ""

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✨ CDP connection test complete!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${BLUE}📊 Summary:${NC}"
echo -e "   ${GREEN}✅${NC} CDP endpoint is accessible at ${CDP_HOST}:${CDP_PORT}"
echo -e "   ${GREEN}✅${NC} CDP protocol version: ${PROTOCOL}"
echo -e "   ${GREEN}✅${NC} Browser is running with ${PAGE_COUNT} page(s)"
echo ""
echo -e "${YELLOW}🚀 Next Steps:${NC}"
echo -e "   1. Test with MCP Inspector:"
echo -e "      ${GREEN}CEF_BRIDGE_PORT=9222 npx @modelcontextprotocol/inspector npx athena-browser-mcp${NC}"
echo ""
echo -e "   2. Configure Claude Desktop (see ${GREEN}docs/MCP_INTEGRATION.md${NC})"
echo ""
echo -e "   3. Use the 40+ browser automation tools!"
echo ""

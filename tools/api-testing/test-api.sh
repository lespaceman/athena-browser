#!/bin/bash
#
# Athena Browser API Test Script
# Interactive menu to test all browser control endpoints
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Detect socket path
SOCKET_PATH="${ATHENA_CONTROL_SOCKET_PATH:-/tmp/athena-$(id -u)-control.sock}"

# Check if socket exists
if [ ! -S "$SOCKET_PATH" ]; then
    echo -e "${RED}Error: Socket not found at $SOCKET_PATH${NC}"
    echo "Please start Athena Browser first."
    exit 1
fi

echo -e "${GREEN}Using socket: $SOCKET_PATH${NC}\n"

# Helper function to make API calls
api_call() {
    local method=$1
    local endpoint=$2
    local data=$3

    echo -e "${BLUE}Request: $method $endpoint${NC}"
    if [ -n "$data" ]; then
        echo -e "${YELLOW}Data: $data${NC}"
    fi
    echo ""

    if [ "$method" = "GET" ]; then
        curl -s -X GET \
            --unix-socket "$SOCKET_PATH" \
            "http://localhost$endpoint" | jq '.'
    else
        curl -s -X POST \
            --unix-socket "$SOCKET_PATH" \
            -H "Content-Type: application/json" \
            -d "$data" \
            "http://localhost$endpoint" | jq '.'
    fi

    echo ""
}

# Main menu
show_menu() {
    echo -e "${GREEN}=== Athena Browser API Test Menu ===${NC}\n"
    echo "Navigation:"
    echo "  1) Navigate to URL"
    echo "  2) Go Back"
    echo "  3) Go Forward"
    echo "  4) Reload Page"
    echo "  5) Get Current URL"
    echo ""
    echo "Content Extraction:"
    echo "  6) Get Page HTML"
    echo "  7) Get Page Summary"
    echo "  8) Get Interactive Elements"
    echo "  9) Get Accessibility Tree"
    echo ""
    echo "JavaScript:"
    echo "  10) Get Page Title (JS)"
    echo "  11) Count Elements (JS)"
    echo "  12) Execute Custom JavaScript"
    echo ""
    echo "Screenshots:"
    echo "  13) Take Screenshot (Viewport)"
    echo "  14) Take Screenshot (Full Page)"
    echo ""
    echo "Tab Management:"
    echo "  15) Get Tab Info"
    echo "  16) Create New Tab"
    echo "  17) Switch Tab"
    echo "  18) Close Tab"
    echo ""
    echo "Other:"
    echo "  19) Health Check"
    echo "  20) Run Quick Test Suite"
    echo ""
    echo "  0) Exit"
    echo ""
}

# Test functions
test_navigate() {
    read -p "Enter URL to navigate to: " url
    if [ -z "$url" ]; then
        url="https://example.com"
    fi
    api_call "POST" "/internal/navigate" "{\"url\": \"$url\"}"
}

test_back() {
    api_call "POST" "/internal/history" '{"action": "back"}'
}

test_forward() {
    api_call "POST" "/internal/history" '{"action": "forward"}'
}

test_reload() {
    api_call "POST" "/internal/reload" '{"ignoreCache": false}'
}

test_get_url() {
    api_call "GET" "/internal/get_url"
}

test_get_html() {
    echo -e "${YELLOW}Note: Output may be very large${NC}"
    read -p "Continue? (y/n): " confirm
    if [ "$confirm" = "y" ]; then
        api_call "GET" "/internal/get_html"
    fi
}

test_page_summary() {
    api_call "GET" "/internal/get_page_summary"
}

test_interactive_elements() {
    api_call "GET" "/internal/get_interactive_elements"
}

test_accessibility_tree() {
    api_call "GET" "/internal/get_accessibility_tree"
}

test_get_title() {
    api_call "POST" "/internal/execute_js" '{"code": "document.title"}'
}

test_count_elements() {
    local code="(function() { return { links: document.querySelectorAll('a').length, images: document.querySelectorAll('img').length, paragraphs: document.querySelectorAll('p').length, headings: document.querySelectorAll('h1,h2,h3,h4,h5,h6').length }; })()"
    api_call "POST" "/internal/execute_js" "{\"code\": \"$code\"}"
}

test_custom_js() {
    echo "Enter JavaScript code to execute:"
    read -r jscode
    if [ -n "$jscode" ]; then
        # Escape quotes in the code
        escaped_code=$(echo "$jscode" | sed 's/"/\\"/g')
        api_call "POST" "/internal/execute_js" "{\"code\": \"$escaped_code\"}"
    fi
}

test_screenshot() {
    echo -e "${YELLOW}Screenshot will be returned as base64 PNG${NC}"
    api_call "GET" "/internal/screenshot"
}

test_screenshot_full() {
    echo -e "${YELLOW}Full page screenshot will be returned as base64 PNG${NC}"
    api_call "POST" "/internal/screenshot" '{"fullPage": true}'
}

test_tab_info() {
    api_call "GET" "/internal/tab_info"
}

test_create_tab() {
    read -p "Enter URL for new tab: " url
    if [ -z "$url" ]; then
        url="https://github.com"
    fi
    api_call "POST" "/internal/tab_create" "{\"url\": \"$url\"}"
}

test_switch_tab() {
    read -p "Enter tab index to switch to: " idx
    if [ -n "$idx" ]; then
        api_call "POST" "/internal/tab_switch" "{\"tabIndex\": $idx}"
    fi
}

test_close_tab() {
    read -p "Enter tab index to close: " idx
    if [ -n "$idx" ]; then
        api_call "POST" "/internal/tab_close" "{\"tabIndex\": $idx}"
    fi
}

test_health() {
    api_call "GET" "/health"
}

# Quick test suite
run_quick_test() {
    echo -e "${GREEN}=== Running Quick Test Suite ===${NC}\n"

    echo -e "${BLUE}Test 1: Health Check${NC}"
    api_call "GET" "/health"
    sleep 1

    echo -e "${BLUE}Test 2: Get Tab Info${NC}"
    api_call "GET" "/internal/tab_info"
    sleep 1

    echo -e "${BLUE}Test 3: Navigate to Example.com${NC}"
    api_call "POST" "/internal/navigate" '{"url": "https://example.com"}'
    sleep 2

    echo -e "${BLUE}Test 4: Get Current URL${NC}"
    api_call "GET" "/internal/get_url"
    sleep 1

    echo -e "${BLUE}Test 5: Get Page Summary${NC}"
    api_call "GET" "/internal/get_page_summary"
    sleep 1

    echo -e "${BLUE}Test 6: Get Page Title via JavaScript${NC}"
    api_call "POST" "/internal/execute_js" '{"code": "document.title"}'
    sleep 1

    echo -e "${BLUE}Test 7: Count Elements${NC}"
    local code="(function() { return { links: document.querySelectorAll('a').length, paragraphs: document.querySelectorAll('p').length }; })()"
    api_call "POST" "/internal/execute_js" "{\"code\": \"$code\"}"

    echo -e "${GREEN}Test suite completed!${NC}"
}

# Main loop
while true; do
    show_menu
    read -p "Select option: " choice
    echo ""

    case $choice in
        1) test_navigate ;;
        2) test_back ;;
        3) test_forward ;;
        4) test_reload ;;
        5) test_get_url ;;
        6) test_get_html ;;
        7) test_page_summary ;;
        8) test_interactive_elements ;;
        9) test_accessibility_tree ;;
        10) test_get_title ;;
        11) test_count_elements ;;
        12) test_custom_js ;;
        13) test_screenshot ;;
        14) test_screenshot_full ;;
        15) test_tab_info ;;
        16) test_create_tab ;;
        17) test_switch_tab ;;
        18) test_close_tab ;;
        19) test_health ;;
        20) run_quick_test ;;
        0)
            echo "Goodbye!"
            exit 0
            ;;
        *)
            echo -e "${RED}Invalid option${NC}"
            ;;
    esac

    echo ""
    read -p "Press Enter to continue..."
    clear
done

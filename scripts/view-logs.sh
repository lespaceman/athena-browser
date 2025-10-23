#!/usr/bin/env bash
# Helper script to view Athena MCP server logs in a readable format

# Check if jq is available for pretty-printing
if command -v jq &> /dev/null; then
    # Pretty-print JSON logs with jq
    jq -R 'try (fromjson | "\(.timestamp) [\(.level | ascii_upcase)] \(.module): \(.message)" + (if .url then " (url: \(.url))" else "" end) + (if .error then " ERROR: \(.error)" else "" end)) catch .'
else
    # Fallback: just grep for JSON lines and format minimally
    grep '^{' | sed 's/\\u001b\[[0-9;]*m//g'
fi

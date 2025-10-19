#!/bin/bash

echo "=== Testing all content extraction endpoints ==="
echo ""

echo "1. Get Page Summary:"
curl --unix-socket /tmp/athena-1000-control.sock -X GET http://localhost/internal/get_page_summary 2>&1 | jq -c '{success, title: .summary.title, headings: (.summary.headings | length)}'

echo ""
echo "2. Get Interactive Elements:"
curl --unix-socket /tmp/athena-1000-control.sock -X GET http://localhost/internal/get_interactive_elements 2>&1 | jq -c '{success, count}'

echo ""
echo "3. Get Accessibility Tree:"
curl --unix-socket /tmp/athena-1000-control.sock -X GET http://localhost/internal/get_accessibility_tree 2>&1 | jq -c '{success, hasTree: (.tree != null)}'

echo ""
echo "4. Query Content - Forms:"
curl --unix-socket /tmp/athena-1000-control.sock -X POST -H "Content-Type: application/json" -d '{"queryType": "forms"}' http://localhost/internal/query_content 2>&1 | jq -c '{success, queryType, dataType: (.data | type)}'

echo ""
echo "5. Query Content - Navigation:"
curl --unix-socket /tmp/athena-1000-control.sock -X POST -H "Content-Type: application/json" -d '{"queryType": "navigation"}' http://localhost/internal/query_content 2>&1 | jq -c '{success, queryType, dataType: (.data | type)}'

echo ""
echo "6. Query Content - Article:"
curl --unix-socket /tmp/athena-1000-control.sock -X POST -H "Content-Type: application/json" -d '{"queryType": "article"}' http://localhost/internal/query_content 2>&1 | jq -c '{success, queryType, dataType: (.data | type)}'

echo ""
echo "7. Query Content - Tables:"
curl --unix-socket /tmp/athena-1000-control.sock -X POST -H "Content-Type: application/json" -d '{"queryType": "tables"}' http://localhost/internal/query_content 2>&1 | jq -c '{success, queryType, dataType: (.data | type)}'

echo ""
echo "8. Query Content - Media:"
curl --unix-socket /tmp/athena-1000-control.sock -X POST -H "Content-Type: application/json" -d '{"queryType": "media"}' http://localhost/internal/query_content 2>&1 | jq -c '{success, queryType, dataType: (.data | type)}'

echo ""
echo "=== All tests complete ==="

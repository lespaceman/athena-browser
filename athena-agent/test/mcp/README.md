# MCP Server Testing Guide

This directory contains tests for the Athena Browser MCP (Model Context Protocol) server.

## Overview

The MCP server exposes browser control capabilities as tools that can be used by Claude and other MCP clients. These tests validate that:

1. **Tool Discovery**: All tools are properly registered and discoverable
2. **Tool Execution**: Tools execute correctly and return proper responses
3. **Error Handling**: Invalid inputs and edge cases are handled gracefully
4. **Schema Validation**: Tool schemas match the MCP specification

## Running Tests

### Run All MCP Tests

```bash
npm run test:mcp
```

### Run Tests in Watch Mode

```bash
npx vitest test/mcp
```

### Run Specific Test File

```bash
npx vitest test/mcp/mcp-server.test.ts
```

## Test Structure

### `mcp-server.test.ts`

Main test suite covering:

- **Tool Discovery**: Validates all 17 tools are exposed
- **Navigation Tools**: Tests browser navigation (navigate, back, forward, reload)
- **Information Tools**: Tests content retrieval (HTML, summaries, accessibility tree)
- **Interaction Tools**: Tests JavaScript execution and screenshots
- **Tab Management**: Tests tab creation, switching, and closing
- **Error Handling**: Tests invalid inputs and edge cases
- **Response Format**: Validates MCP response structure
- **Schema Validation**: Tests tool input schemas

### `helpers.ts`

Utility functions for testing:

- `createMcpClient()`: Create a connected MCP client
- `extractTextContent()`: Extract text from tool results
- `extractImageData()`: Extract image data from tool results
- `isErrorResult()`: Check if a result is an error
- `validateToolSchema()`: Validate tool schema structure
- `EXPECTED_TOOLS`: List of all expected tool names
- `TEST_DATA`: Common test data (URLs, JS code, etc.)

## Testing with MCP Inspector

For interactive testing and debugging, use the official MCP Inspector:

```bash
# Make sure server is built
npm run build

# Run inspector
npx @modelcontextprotocol/inspector node dist/server.js
```

The inspector provides:
- Interactive tool listing
- Tool call testing with custom inputs
- Real-time response viewing
- Schema validation

## Testing with Claude Desktop

To test the MCP server with Claude Desktop:

1. **Build the server**:
   ```bash
   npm run build
   ```

2. **Configure Claude Desktop** (`~/.config/Claude/claude_desktop_config.json`):
   ```json
   {
     "mcpServers": {
       "athena-browser": {
         "command": "node",
         "args": ["/path/to/athena-browser/athena-agent/dist/server.js"],
         "env": {
           "ATHENA_SOCKET_PATH": "/tmp/athena-claude.sock"
         }
       }
     }
   }
   ```

3. **Start Athena Browser**:
   ```bash
   GDK_BACKEND=x11 ./build/release/app/athena-browser
   ```

4. **Open Claude Desktop** and test commands like:
   - "Navigate to https://example.com"
   - "Get the current page summary"
   - "Take a screenshot"

## Architecture

```
Claude Desktop / MCP Client
         ↓
    MCP Protocol (JSON-RPC)
         ↓
  athena-agent (mcp-server.ts)
         ↓
   HTTP API (v1 routes)
         ↓
  Browser Controller
         ↓
   C++ GTK Browser
```

## Test Coverage

Current test coverage:

- ✅ Tool Discovery (6 tests)
- ✅ Navigation Tools (5 tests)
- ✅ Information Tools (6 tests)
- ✅ Interaction Tools (3 tests)
- ✅ Tab Management (3 tests)
- ✅ Error Handling (4 tests)
- ✅ Response Format (3 tests)
- ✅ Schema Validation (3 tests)

**Total: 33 tests**

## Expected Tools

The MCP server should expose these 17 tools:

### Navigation (4 tools)
- `browser_navigate` - Navigate to a URL
- `browser_back` - Go back in history
- `browser_forward` - Go forward in history
- `browser_reload` - Reload current page

### Information (6 tools)
- `browser_get_url` - Get current URL
- `browser_get_html` - Get full page HTML
- `browser_get_page_summary` - Get compact page summary (1-2KB)
- `browser_get_interactive_elements` - Get clickable elements
- `browser_get_accessibility_tree` - Get semantic page structure
- `browser_query_content` - Query specific content (forms, navigation, etc.)

### Interaction (3 tools)
- `browser_execute_js` - Execute JavaScript code
- `browser_screenshot` - Capture screenshot
- `browser_get_annotated_screenshot` - Screenshot with element annotations

### Tab Management (3 tools)
- `window_create_tab` - Create new tab
- `window_close_tab` - Close a tab
- `window_switch_tab` - Switch to a tab

### POC (1 tool)
- `open_url` - POC tool for opening URLs (combines navigation + waiting)

## Common Issues

### Server Fails to Start

```bash
# Check if socket already exists
ls -la /tmp/athena-test-mcp.sock

# Remove stale socket
rm /tmp/athena-test-mcp.sock
```

### Tests Timeout

- Increase timeout in `beforeAll()` (currently 15s)
- Check server logs for startup errors
- Ensure no other process is using the socket

### Connection Refused

- Ensure server is built: `npm run build`
- Check that `dist/server.js` exists
- Verify socket path is correct

### Tool Not Found

- Check tool is registered in `src/mcp-server.ts`
- Verify tool name matches exactly (case-sensitive)
- Run inspector to see all available tools

## Writing New Tests

When adding new tools:

1. **Add to helpers.ts**:
   ```typescript
   export const EXPECTED_TOOLS = {
     // ...
     newCategory: ['new_tool_name']
   };
   ```

2. **Add test case**:
   ```typescript
   it('should call new_tool_name', async () => {
     const result = await client.callTool({
       name: 'new_tool_name',
       arguments: { /* ... */ }
     });

     expect(result.content).toBeDefined();
     expect(result.isError).toBeUndefined();
   });
   ```

3. **Add schema validation**:
   ```typescript
   it('should have valid schema for new_tool_name', async () => {
     const response = await client.listTools();
     const tool = response.tools.find(t => t.name === 'new_tool_name');

     expect(tool).toBeDefined();
     expect(tool?.inputSchema.properties).toHaveProperty('required_param');
   });
   ```

## Debugging

### Enable Verbose Logging

Set `LOG_LEVEL=debug` when running tests:

```bash
LOG_LEVEL=debug npm run test:mcp
```

### View Server Logs

The test harness captures server stdout/stderr. To see logs:

```typescript
serverProcess.stdout?.on('data', (data: Buffer) => {
  console.log('Server:', data.toString());
});

serverProcess.stderr?.on('data', (data: Buffer) => {
  console.error('Server error:', data.toString());
});
```

### Test Individual Tools

```bash
npx vitest test/mcp/mcp-server.test.ts -t "should call browser_navigate"
```

## References

- [MCP Specification](https://spec.modelcontextprotocol.io/)
- [MCP TypeScript SDK](https://github.com/modelcontextprotocol/typescript-sdk)
- [MCP Inspector](https://github.com/modelcontextprotocol/inspector)
- [Athena Agent Architecture](../../AGENT_ARCHITECTURE.md)
- [Browser Control API](../../API_REFERENCE.md)

## Contributing

When adding new tests:

1. Follow the existing test structure
2. Use helper functions from `helpers.ts`
3. Add schema validation for new tools
4. Test both success and error cases
5. Update this README with new tool documentation

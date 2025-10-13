# Claude Agent SDK Integration

The Node runtime includes the Claude Agent SDK, allowing the browser to leverage Claude's AI capabilities for code analysis, generation, refactoring, and more.

## Overview

The Claude Agent SDK is integrated into the Node runtime via the `ClaudeClient` class, which provides a clean API for interacting with Claude Code programmatically. This enables powerful AI-assisted operations directly from the GTK application.

## Architecture

```
GTK Application (C++)
    ↓ (HTTP over Unix socket)
Node Runtime (Express Server)
    ↓ (JavaScript)
ClaudeClient Wrapper
    ↓ (SDK)
@anthropic-ai/claude-agent-sdk
    ↓ (Subprocess)
Claude Code CLI
```

## Available Endpoints

All Claude SDK endpoints are under `/v1/claude/*`:

### 1. General Query: `POST /v1/claude/query`

Execute arbitrary Claude Code queries with full control.

**Request:**
```json
{
  "prompt": "Analyze the main.cpp file and explain the initialization flow",
  "options": {
    "model": "claude-sonnet-4-5",
    "allowedTools": ["Read", "Grep", "Glob"],
    "permissionMode": "bypassPermissions"
  }
}
```

**Response:**
```json
{
  "success": true,
  "result": "The initialization flow in main.cpp...",
  "sessionId": "abc123...",
  "usage": {
    "input_tokens": 1500,
    "output_tokens": 800
  },
  "cost": 0.023,
  "numTurns": 3,
  "durationMs": 2500
}
```

### 2. Analyze Code: `POST /v1/claude/analyze-code`

Analyze code in a specific file.

**Request:**
```json
{
  "filePath": "/home/user/project/src/main.cpp",
  "question": "What design patterns are used in this file?"
}
```

**Response:**
```json
{
  "success": true,
  "analysis": "The file uses the following design patterns:\n1. RAII...",
  "sessionId": "abc123..."
}
```

### 3. Generate Code: `POST /v1/claude/generate-code`

Generate code based on a specification.

**Request:**
```json
{
  "spec": "Create a C++ class for managing user sessions with login/logout",
  "outputPath": "/home/user/project/src/session_manager.cpp"
}
```

**Response:**
```json
{
  "success": true,
  "result": "I've created session_manager.cpp with the SessionManager class...",
  "sessionId": "abc123..."
}
```

### 4. Refactor Code: `POST /v1/claude/refactor-code`

Refactor existing code.

**Request:**
```json
{
  "filePath": "/home/user/project/src/legacy.cpp",
  "instructions": "Refactor to use modern C++17 features and improve error handling"
}
```

**Response:**
```json
{
  "success": true,
  "result": "I've refactored legacy.cpp with the following changes...",
  "sessionId": "abc123..."
}
```

### 5. Search Code: `POST /v1/claude/search-code`

Search codebase with AI-powered analysis.

**Request:**
```json
{
  "pattern": "Result<.*>",
  "globPattern": "**/*.cpp"
}
```

**Response:**
```json
{
  "success": true,
  "result": "Found 45 usages of Result<T> pattern across 12 files...",
  "sessionId": "abc123..."
}
```

### 6. Run Command: `POST /v1/claude/run-command`

Execute shell commands via Claude.

**Request:**
```json
{
  "command": "grep -r 'TODO' src/"
}
```

**Response:**
```json
{
  "success": true,
  "result": "Found 12 TODO comments:\n- src/main.cpp:45: TODO: Add error handling...",
  "sessionId": "abc123..."
}
```

### 7. Analyze Web: `POST /v1/claude/analyze-web`

Fetch and analyze web content.

**Request:**
```json
{
  "url": "https://example.com/api-docs",
  "question": "What authentication methods are supported?"
}
```

**Response:**
```json
{
  "success": true,
  "analysis": "The API supports OAuth 2.0 and API key authentication...",
  "sessionId": "abc123..."
}
```

### 8. Search Web: `POST /v1/claude/search-web`

Search the web via Claude.

**Request:**
```json
{
  "query": "C++17 best practices for error handling"
}
```

**Response:**
```json
{
  "success": true,
  "result": "Here are the top resources on C++17 error handling...",
  "sessionId": "abc123..."
}
```

### 9. Continue Conversation: `POST /v1/claude/continue`

Continue the most recent Claude conversation.

**Request:**
```json
{
  "prompt": "Can you also check for memory leaks?"
}
```

**Response:**
```json
{
  "success": true,
  "result": "Analyzing for memory leaks...",
  "sessionId": "abc123..."
}
```

### 10. Resume Session: `POST /v1/claude/resume`

Resume a previous Claude session by ID.

**Request:**
```json
{
  "sessionId": "abc123...",
  "prompt": "What were the main findings?"
}
```

**Response:**
```json
{
  "success": true,
  "result": "Based on our previous analysis, the main findings were...",
  "sessionId": "abc123..."
}
```

## Using from C++

The GTK application can call these endpoints using the existing `NodeRuntime::Call()` method:

```cpp
// Example: Analyze code
auto* runtime = app->GetNodeRuntime();

std::string request = R"({
  "filePath": "/home/user/project/src/main.cpp",
  "question": "What is the purpose of this file?"
})";

auto response = runtime->Call("POST", "/v1/claude/analyze-code", request);

if (response.IsOk()) {
  std::cout << "Analysis: " << response.Value() << std::endl;
} else {
  std::cerr << "Error: " << response.GetError().Message() << std::endl;
}
```

## Configuration

The `ClaudeClient` is initialized with the following defaults:

```javascript
{
  cwd: process.cwd(),                  // Current working directory
  model: 'claude-sonnet-4-5',          // Claude model to use
  permissionMode: 'bypassPermissions', // Auto-approve operations
  settingSources: ['project'],         // Load project settings (CLAUDE.md)
  allowedTools: [                      // Default allowed tools
    'Read',
    'Write',
    'Edit',
    'Glob',
    'Grep',
    'Bash',
    'WebFetch',
    'WebSearch'
  ],
  systemPrompt: {                      // Use Claude Code system prompt
    type: 'preset',
    preset: 'claude_code'
  }
}
```

You can override these settings per-request via the `options` parameter in `/v1/claude/query`.

## Permission Modes

- `'bypassPermissions'` (default): Auto-approve all operations
- `'default'`: Use standard permission checks
- `'acceptEdits'`: Auto-accept file edits only
- `'plan'`: Planning mode - no execution

## Tool Access Control

Control which tools Claude can use:

```json
{
  "prompt": "Search for bugs",
  "options": {
    "allowedTools": ["Read", "Grep"],
    "disallowedTools": ["Write", "Edit", "Bash"]
  }
}
```

## Session Management

Claude maintains conversational context via sessions:

1. **New session**: Each query creates a new session
2. **Continue session**: Use `/v1/claude/continue` to add to the most recent session
3. **Resume session**: Use `/v1/claude/resume` with a `sessionId` to resume any session

Session IDs are returned in all responses and can be stored for later resumption.

## Error Handling

All endpoints return a consistent error format:

```json
{
  "success": false,
  "error": "Error message here",
  "sessionId": null
}
```

Common errors:
- **Missing required fields**: 400 Bad Request
- **Claude execution failed**: 500 Internal Server Error with error details
- **Permission denied**: Returned in the `error` field

## Performance Considerations

- **Latency**: Claude queries typically take 1-10 seconds depending on complexity
- **Cost**: Each query consumes API tokens (tracked in `usage` and `cost` fields)
- **Concurrency**: Multiple queries can run in parallel, but each blocks until completion
- **Caching**: Claude Code uses prompt caching to reduce costs for repeated context

## Security

- **API Key**: Claude Agent SDK requires `ANTHROPIC_API_KEY` environment variable
- **File Access**: Claude respects file permissions and can only access files readable by the Node process
- **Command Execution**: Bash commands run with the same permissions as Node process
- **Network Access**: WebFetch and WebSearch work only if network is accessible

## Examples

### Code Review from GTK

```cpp
// Trigger a code review
std::string review_request = R"({
  "filePath": "/home/user/project/src/new_feature.cpp",
  "question": "Review this code for potential bugs and suggest improvements"
})";

auto review = runtime->Call("POST", "/v1/claude/analyze-code", review_request);
```

### Generate Tests

```cpp
// Generate unit tests
std::string test_request = R"({
  "spec": "Generate unit tests for the NodeRuntime class in app/src/runtime/node_runtime.cpp",
  "outputPath": "/home/user/project/app/tests/runtime/node_runtime_test.cpp"
})";

auto result = runtime->Call("POST", "/v1/claude/generate-code", test_request);
```

### Search and Fix TODOs

```cpp
// Search for TODOs
std::string search_request = R"({
  "pattern": "TODO|FIXME",
  "globPattern": "**/*.cpp"
})";

auto todos = runtime->Call("POST", "/v1/claude/search-code", search_request);

// Parse results and ask Claude to fix them
if (todos.IsOk()) {
  std::string fix_request = R"({
    "prompt": "Fix the TODOs found in the previous search"
  })";

  auto fixes = runtime->Call("POST", "/v1/claude/continue", fix_request);
}
```

### Interactive Refactoring

```cpp
// Start refactoring
std::string refactor_request = R"({
  "filePath": "/home/user/project/src/legacy.cpp",
  "instructions": "Modernize to C++17"
})";

auto result = runtime->Call("POST", "/v1/claude/refactor-code", refactor_request);

// Continue with follow-up
std::string followup = R"({
  "prompt": "Also add comprehensive error handling"
})";

auto followup_result = runtime->Call("POST", "/v1/claude/continue", followup);
```

## Troubleshooting

### "ANTHROPIC_API_KEY not found"

Set the environment variable before starting the Node runtime:

```bash
export ANTHROPIC_API_KEY=your_key_here
node server.js
```

Or set it in the GTK application before spawning Node:

```cpp
setenv("ANTHROPIC_API_KEY", "your_key_here", 1);
```

### "Permission denied" errors

Check the `permissionMode` setting. For automated operations, use `'bypassPermissions'`.

### Slow responses

- Claude queries can take several seconds
- Consider running long queries asynchronously in the background
- Use session resumption to avoid re-analyzing context

### High API costs

- Monitor the `cost` field in responses
- Use prompt caching by continuing sessions instead of creating new ones
- Limit the `allowedTools` to reduce token usage

## Advanced Usage

### Custom System Prompts

Override the system prompt for specialized behavior:

```json
{
  "prompt": "Review this code",
  "options": {
    "systemPrompt": "You are a C++ security expert. Focus on security vulnerabilities."
  }
}
```

### Streaming Responses

Currently, the HTTP API buffers the complete response. For streaming support, extend `claude-client.js` with SSE or WebSocket endpoints.

### MCP Server Integration

The Claude Agent SDK supports MCP servers. Configure them in `claude-client.js`:

```javascript
const claudeClient = new ClaudeClient({
  mcpServers: {
    'my-server': {
      type: 'stdio',
      command: 'node',
      args: ['path/to/mcp-server.js']
    }
  }
});
```

## Further Reading

- [Claude Agent SDK TypeScript Reference](https://docs.anthropic.com/en/api/agent-sdk/typescript)
- [Claude Code Documentation](https://docs.anthropic.com/en/docs/claude-code)
- [Anthropic API Documentation](https://docs.anthropic.com/)

# Node.js Runtime Integration

Athena Browser includes a separate Node.js runtime process that provides Node-grade capabilities to the GTK application via HTTP over Unix domain sockets.

## Architecture

```
GTK Application (C++)
    ↓
NodeRuntime (Process Manager)
    ↓ (Unix socket + HTTP)
Node.js Helper Process
    ↓
Your Node.js Application Code
```

**Key Design Decisions:**
- **IPC**: HTTP over Unix domain socket (`/tmp/athena-<uid>.sock`)
- **Lifecycle**: Node process starts with GTK app, dies with GTK shutdown
- **Security**: Socket permissions locked to user (0600), no network exposure
- **Health**: Automatic health checks with `/health` endpoint
- **Restart**: Exponential backoff restart on crash (configurable max attempts)
- **Observability**: Structured JSON logs, request tracing with X-Request-Id

## Setup

### 1. Install Node Dependencies

```bash
cd node-runtime
npm install
```

### 2. Configure Application

In your C++ code:

```cpp
#include "core/application.h"
#include "runtime/node_runtime.h"
#include "browser/cef_engine.h"
#include "platform/gtk_window_system.h"

// Configure application
ApplicationConfig app_config;
app_config.cache_path = "/tmp/athena_cache";
app_config.enable_node_runtime = true;
app_config.node_runtime_script_path = "/path/to/athena-browser/node-runtime/server.js";

// Configure Node runtime
runtime::NodeRuntimeConfig node_config;
node_config.runtime_script_path = app_config.node_runtime_script_path;
node_config.startup_timeout_ms = 5000;
node_config.restart_max_attempts = 3;

// Create components
auto browser_engine = std::make_unique<browser::CefEngine>();
auto window_system = std::make_unique<platform::GtkWindowSystem>();
auto node_runtime = std::make_unique<runtime::NodeRuntime>(node_config);

// Create application
auto app = std::make_unique<Application>(
    app_config,
    std::move(browser_engine),
    std::move(window_system),
    std::move(node_runtime)
);

// Initialize (starts Node runtime automatically)
if (auto result = app->Initialize(argc, argv); !result) {
    std::cerr << "Failed to initialize: " << result.GetError().Message() << std::endl;
    return 1;
}

// Node runtime is now ready!
auto* runtime = app->GetNodeRuntime();
if (runtime && runtime->IsReady()) {
    std::cout << "Node runtime ready at: " << runtime->GetSocketPath() << std::endl;
}

app->Run();
```

## Making API Calls

### From C++ to Node

```cpp
auto* runtime = app->GetNodeRuntime();
if (!runtime || !runtime->IsReady()) {
    std::cerr << "Node runtime not available" << std::endl;
    return;
}

// Example: Echo endpoint
std::string request_body = R"({"message": "Hello from GTK!"})";
auto response = runtime->Call("POST", "/v1/echo", request_body);

if (response) {
    std::cout << "Response: " << response.Value() << std::endl;
} else {
    std::cerr << "Error: " << response.GetError().Message() << std::endl;
}

// Example: Get system info
auto sys_info = runtime->Call("GET", "/v1/system/info");
if (sys_info) {
    std::cout << "System info: " << sys_info.Value() << std::endl;
}

// Example: Read file
std::string read_body = R"({"path": "/tmp/test.txt"})";
auto file_content = runtime->Call("POST", "/v1/fs/read", read_body);

// Example: Write file
std::string write_body = R"({"path": "/tmp/test.txt", "content": "Hello World"})";
auto write_result = runtime->Call("POST", "/v1/fs/write", write_body);
```

### Adding Custom Endpoints in Node

Edit `node-runtime/server.js`:

```javascript
// Add your custom endpoint
app.post('/v1/custom/action', (req, res) => {
  const { param1, param2 } = req.body;

  // Your business logic here
  const result = doSomething(param1, param2);

  res.json({
    success: true,
    result: result,
    timestamp: Date.now()
  });
});
```

Then call from C++:

```cpp
std::string body = R"({"param1": "value1", "param2": "value2"})";
auto result = runtime->Call("POST", "/v1/custom/action", body);
```

## Available Node Endpoints

### Core Endpoints

#### Health Check
```
GET /health
```
Returns runtime status and uptime.

### Capabilities
```
GET /v1/capabilities
```
Lists available features and Node version.

### System Info
```
GET /v1/system/info
```
Returns OS, CPU, memory information.

### Echo (Testing)
```
POST /v1/echo
Body: {"message": "text"}
```

### Filesystem Read
```
POST /v1/fs/read
Body: {"path": "/path/to/file"}
```

### Filesystem Write
```
POST /v1/fs/write
Body: {"path": "/path/to/file", "content": "text"}
```

### Claude Agent SDK Endpoints

The Node runtime includes the Claude Agent SDK for AI-assisted operations. See `node-runtime/CLAUDE_SDK.md` for complete documentation.

**Available operations:**
- `POST /v1/claude/query` - General Claude query with full control
- `POST /v1/claude/analyze-code` - Analyze code in a specific file
- `POST /v1/claude/generate-code` - Generate code from specification
- `POST /v1/claude/refactor-code` - Refactor existing code
- `POST /v1/claude/search-code` - Search codebase with AI assistance
- `POST /v1/claude/run-command` - Execute shell commands via Claude
- `POST /v1/claude/analyze-web` - Fetch and analyze web content
- `POST /v1/claude/search-web` - Search the web
- `POST /v1/claude/continue` - Continue most recent conversation
- `POST /v1/claude/resume` - Resume a previous session

**Quick example from C++:**
```cpp
// Analyze code with Claude
std::string request = R"({
  "filePath": "/path/to/file.cpp",
  "question": "What design patterns are used here?"
})";

auto response = runtime->Call("POST", "/v1/claude/analyze-code", request);
if (response) {
  std::cout << "Analysis: " << response.Value() << std::endl;
}
```

## Health Monitoring

The Node runtime automatically monitors health:

```cpp
// Check health manually
auto health = runtime->CheckHealth();
if (health) {
    std::cout << "Healthy: " << health.Value().healthy << std::endl;
    std::cout << "Ready: " << health.Value().ready << std::endl;
    std::cout << "Uptime: " << health.Value().uptime_ms << "ms" << std::endl;
}

// Start automatic health monitoring (optional)
runtime->StartHealthMonitoring();

// Get current state
auto state = runtime->GetState();
// RuntimeState: STOPPED, STARTING, READY, UNHEALTHY, CRASHED
```

## Lifecycle

The Node runtime follows the application lifecycle:

```
Application::Initialize()
    ↓
NodeRuntime::Initialize()
    ↓
Spawn Node process → Wait for READY → Check socket connection
    ↓
RuntimeState::READY
    ↓
[Application runs...]
    ↓
Application::Shutdown()
    ↓
NodeRuntime::Shutdown()
    ↓
Send SIGTERM → Wait 2s → SIGKILL if needed → Clean exit
```

## Error Handling

All API calls return `Result<T>`:

```cpp
auto response = runtime->Call("GET", "/v1/endpoint");

if (response.IsOk()) {
    std::string data = response.Value();
    // Use data
} else {
    std::cerr << "Error: " << response.GetError().Message() << std::endl;
    // Handle error
}
```

## Request Tracing

Add request IDs for correlation:

```cpp
std::string request_id = "req-12345";
auto response = runtime->Call("POST", "/v1/echo", body, request_id);

// Node logs will include: "requestId": "req-12345"
// Response headers will include: X-Request-Id: req-12345
```

## Crash Recovery

The runtime automatically restarts on crash:

```cpp
// Configure restart behavior
runtime::NodeRuntimeConfig config;
config.restart_max_attempts = 3;  // Try 3 times
config.restart_backoff_ms = 100;  // Start with 100ms, doubles each retry

// If Node crashes, runtime will:
// 1. Detect crash → State becomes CRASHED
// 2. Wait backoff (100ms → 200ms → 400ms)
// 3. Attempt restart
// 4. If max attempts reached, give up
```

## Development Workflow

### 1. Run Node server standalone for testing:
```bash
cd node-runtime
npm run dev  # Auto-reload with nodemon

# Test with curl
curl --unix-socket /tmp/athena-<uid>.sock http://localhost/health
```

### 2. Run GTK app with Node runtime:
```bash
./scripts/build.sh
./scripts/run.sh
```

### 3. Check logs:
Node outputs structured JSON logs to stdout:
```json
{"timestamp":"...","level":"INFO","module":"NodeRuntime","message":"HTTP Request","requestId":"...","method":"POST","path":"/v1/echo","status":200,"duration":5}
```

## Configuration Options

### NodeRuntimeConfig

```cpp
struct NodeRuntimeConfig {
  std::string node_executable = "node";
  std::string runtime_script_path;         // Required
  std::string socket_path;                 // Auto-generated if empty
  int startup_timeout_ms = 5000;           // Max wait for READY
  int health_check_interval_ms = 10000;    // Health check frequency
  int restart_max_attempts = 3;            // Max restart tries
  int restart_backoff_ms = 100;            // Initial backoff, doubles each retry
};
```

### ApplicationConfig

```cpp
struct ApplicationConfig {
  // ... existing fields ...
  bool enable_node_runtime = true;
  std::string node_runtime_script_path;
};
```

## Security Considerations

1. **Socket Permissions**: Socket file is created with 0600 (user-only)
2. **No Network Exposure**: Unix socket is local-only, no TCP listening
3. **Input Validation**: All parameters validated server-side in Node
4. **Path Restrictions**: Implement allowlists for filesystem operations
5. **No Code Injection**: UI never passes arbitrary code to Node

## Performance

- **IPC Latency**: ~1-5ms for typical requests over Unix socket
- **Concurrency**: Node event loop handles concurrent requests
- **Memory**: ~40MB overhead for Node process
- **CPU**: Minimal when idle, scales with request load

## Troubleshooting

### "Runtime not ready"
- Check Node is installed: `node --version`
- Check script path is correct
- Look for READY line in logs
- Verify socket permissions

### "Failed to connect to socket"
- Socket file may be stale: `rm /tmp/athena-<uid>.sock`
- Check Node process is running: `ps aux | grep node`
- Verify socket path matches

### Crashes on startup
- Check Node script syntax: `node node-runtime/server.js`
- Verify all dependencies installed: `npm install`
- Check logs for errors

## Next Steps

1. Add authentication if exposing sensitive operations
2. Implement request rate limiting
3. Add WebSocket support for push notifications
4. Create TypeScript types for API contracts
5. Add performance metrics/tracing
6. Package Node runtime with the application

## Example: Full Integration

See `app/src/main.cpp` for a complete example of integrating the Node runtime into the Athena Browser application.

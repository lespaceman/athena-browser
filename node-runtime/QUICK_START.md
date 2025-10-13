# Node Runtime Quick Start

## 5-Minute Integration Guide

### 1. Install Dependencies
```bash
cd node-runtime
npm install
```

### 2. Enable in Your Application

Edit your `main.cpp` or wherever you create the Application:

```cpp
#include "core/application.h"
#include "runtime/node_runtime.h"

int main(int argc, char* argv[]) {
    // Configure application
    ApplicationConfig app_config;
    app_config.enable_node_runtime = true;
    app_config.node_runtime_script_path =
        "/path/to/athena-browser/node-runtime/server.js";

    // Configure Node runtime
    runtime::NodeRuntimeConfig node_config;
    node_config.runtime_script_path = app_config.node_runtime_script_path;

    // Create application with Node runtime
    auto app = std::make_unique<Application>(
        app_config,
        std::make_unique<browser::CefEngine>(),
        std::make_unique<platform::GtkWindowSystem>(),
        std::make_unique<runtime::NodeRuntime>(node_config)
    );

    // Initialize (auto-starts Node)
    if (auto result = app->Initialize(argc, argv); !result) {
        std::cerr << result.GetError().Message() << std::endl;
        return 1;
    }

    // Use Node runtime
    auto* runtime = app->GetNodeRuntime();
    if (runtime && runtime->IsReady()) {
        auto response = runtime->Call("GET", "/v1/system/info");
        if (response) {
            std::cout << "Node runtime ready!\n";
            std::cout << response.Value() << std::endl;
        }
    }

    app->Run();
    return 0;
}
```

### 3. Build & Run
```bash
./scripts/build.sh
./scripts/run.sh
```

That's it! Node runtime is now running alongside your GTK app.

## Making API Calls

### Simple GET Request
```cpp
auto* runtime = app->GetNodeRuntime();
auto response = runtime->Call("GET", "/health");
```

### POST with JSON Body
```cpp
std::string json = R"({"message": "Hello from GTK!"})";
auto response = runtime->Call("POST", "/v1/echo", json);
```

### With Request Tracing
```cpp
std::string request_id = "req-" + std::to_string(time(nullptr));
auto response = runtime->Call("POST", "/v1/echo", json, request_id);
// Logs will show: "requestId": "req-..."
```

### Error Handling
```cpp
auto response = runtime->Call("POST", "/v1/endpoint", body);
if (response.IsOk()) {
    std::cout << "Success: " << response.Value() << std::endl;
} else {
    std::cerr << "Error: " << response.GetError().Message() << std::endl;
}
```

## Adding Custom Endpoints

Edit `node-runtime/server.js`:

```javascript
// Add your endpoint
app.post('/v1/myfeature/action', (req, res) => {
  const { input } = req.body;

  // Your logic here
  const result = processInput(input);

  res.json({
    success: true,
    result: result
  });
});
```

Call from C++:
```cpp
std::string body = R"({"input": "test"})";
auto result = runtime->Call("POST", "/v1/myfeature/action", body);
```

## Available Endpoints

- `GET /health` - Health check
- `GET /v1/capabilities` - List features
- `GET /v1/system/info` - System information
- `POST /v1/echo` - Echo test
- `POST /v1/fs/read` - Read file
- `POST /v1/fs/write` - Write file

## Testing Manually

```bash
# Start Node server
cd node-runtime
node server.js &

# Test with curl
curl --unix-socket /tmp/athena-$(id -u).sock \
  http://localhost/health

curl --unix-socket /tmp/athena-$(id -u).sock \
  -X POST http://localhost/v1/echo \
  -H "Content-Type: application/json" \
  -d '{"message":"hello"}'

# Stop
pkill -f "node server.js"
```

## Troubleshooting

**"Runtime not ready"**
- Ensure Node is installed: `node --version`
- Check script path in config
- Look for "READY" in logs

**"Failed to connect"**
- Remove stale socket: `rm /tmp/athena-$(id -u).sock`
- Restart application

**Node crashes**
- Test script: `node node-runtime/server.js`
- Check dependencies: `npm install`
- Review logs

## Configuration Options

```cpp
runtime::NodeRuntimeConfig config;
config.runtime_script_path = "/path/to/server.js";  // Required
config.startup_timeout_ms = 5000;                    // Max wait for READY
config.restart_max_attempts = 3;                     // Auto-restart attempts
config.restart_backoff_ms = 100;                     // Initial backoff (doubles)
```

## Full Documentation

See `NODE_RUNTIME.md` for complete documentation including:
- Architecture details
- Security considerations
- Performance characteristics
- Advanced usage patterns
- API reference

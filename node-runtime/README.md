# Athena Node Runtime

Node.js helper process for Athena Browser, providing Node-grade capabilities via HTTP over Unix domain socket.

## Architecture

- **IPC**: HTTP over Unix domain socket (`/tmp/athena-<uid>.sock`)
- **Lifecycle**: Spawned by GTK app, supervised by `Application` class
- **Security**: Socket permissions locked to user (0600)
- **Health**: `/health` endpoint for liveness/readiness checks
- **Graceful shutdown**: Handles SIGTERM with cleanup

## Setup

```bash
cd node-runtime
npm install
```

## Development

```bash
# Start with nodemon (auto-reload)
npm run dev

# Start normally
npm start

# Test manually
curl --unix-socket /tmp/athena-<uid>.sock http://localhost/health
```

## API Endpoints

### Health Check
```bash
GET /health
```

Returns runtime status, uptime, and request count.

### Capabilities
```bash
GET /v1/capabilities
```

Lists available features and Node version.

### System Info
```bash
GET /v1/system/info
```

Returns OS information (platform, arch, memory, CPU).

### Echo (Testing)
```bash
POST /v1/echo
Content-Type: application/json

{"message": "hello"}
```

Echoes back the message with timestamp and request ID.

### Filesystem Read
```bash
POST /v1/fs/read
Content-Type: application/json

{"path": "/path/to/file.txt"}
```

Reads file contents.

### Filesystem Write
```bash
POST /v1/fs/write
Content-Type: application/json

{"path": "/path/to/file.txt", "content": "file contents"}
```

Writes content to file.

## Request/Response Format

All responses include:
- `X-Request-Id` header for tracing
- JSON body with structured data
- Proper HTTP status codes (200, 400, 404, 500)

All requests can include:
- `X-Request-Id` header for correlation
- `X-Api-Version` header (optional, defaults to v1)

## Logging

Structured JSON logs to stdout:
```json
{
  "timestamp": "2025-10-13T...",
  "level": "INFO",
  "module": "NodeRuntime",
  "message": "HTTP Request",
  "pid": 12345,
  "requestId": "req-...",
  "method": "POST",
  "path": "/v1/echo",
  "status": 200,
  "duration": 5
}
```

## Readiness Handshake

On successful startup, prints:
```
READY /tmp/athena-<uid>.sock
```

GTK reads this line to know the endpoint is available.

## Shutdown

1. GTK sends SIGTERM
2. Node stops accepting new connections
3. Finishes in-flight requests
4. Unlinks socket file
5. Exits gracefully (or SIGKILL after 2s timeout)

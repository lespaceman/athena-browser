#!/usr/bin/env node
/**
 * HTTP-to-Unix-Socket Proxy for Postman
 *
 * This proxy allows Postman to communicate with Athena Browser's Unix socket
 * by providing a standard HTTP endpoint that forwards requests.
 *
 * Usage:
 *   node proxy-server.js [port] [socket-path]
 *
 * Example:
 *   node proxy-server.js 3333 /tmp/athena-1000-control.sock
 *
 * Then in Postman, use: http://localhost:3333/internal/navigate
 */

const http = require('http');
const httpProxy = require('http-proxy');
const { URL } = require('url');

// Parse command line arguments
const PORT = process.argv[2] || 3333;
const SOCKET_PATH = process.argv[3] || `/tmp/athena-${process.getuid()}-control.sock`;

// Check if socket exists
const fs = require('fs');
if (!fs.existsSync(SOCKET_PATH)) {
  console.error(`❌ Error: Socket not found at ${SOCKET_PATH}`);
  console.error('Please start Athena Browser first.');
  process.exit(1);
}

console.log('🚀 Athena Browser HTTP-to-Unix-Socket Proxy');
console.log(`📁 Socket: ${SOCKET_PATH}`);
console.log(`🌐 HTTP Port: ${PORT}`);
console.log('');

// Create proxy server
const proxy = httpProxy.createProxyServer({
  target: {
    socketPath: SOCKET_PATH
  }
});

// Handle proxy errors
proxy.on('error', (err, req, res) => {
  console.error('❌ Proxy error:', err.message);
  if (!res.headersSent) {
    res.writeHead(502, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      success: false,
      error: `Proxy error: ${err.message}`,
      hint: 'Is Athena Browser running?'
    }));
  }
});

// Create HTTP server
const server = http.createServer((req, res) => {
  // Log request
  console.log(`${new Date().toISOString()} ${req.method} ${req.url}`);

  // Add CORS headers for Postman
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  // Handle OPTIONS preflight
  if (req.method === 'OPTIONS') {
    res.writeHead(200);
    res.end();
    return;
  }

  // Proxy to Unix socket
  proxy.web(req, res, {
    target: {
      socketPath: SOCKET_PATH,
      host: 'localhost',
      path: req.url
    }
  });
});

// Start server
server.listen(PORT, () => {
  console.log(`✅ Proxy server running on http://localhost:${PORT}`);
  console.log('');
  console.log('📝 Usage in Postman:');
  console.log(`   GET  http://localhost:${PORT}/internal/get_url`);
  console.log(`   POST http://localhost:${PORT}/internal/navigate`);
  console.log('');
  console.log('Press Ctrl+C to stop');
});

// Handle shutdown
process.on('SIGINT', () => {
  console.log('\n👋 Shutting down proxy server...');
  server.close(() => {
    console.log('✅ Server stopped');
    process.exit(0);
  });
});

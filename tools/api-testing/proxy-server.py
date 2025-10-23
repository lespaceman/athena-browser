#!/usr/bin/env python3
"""
HTTP-to-Unix-Socket Proxy for Postman

This proxy allows Postman to communicate with Athena Browser's Unix socket
by providing a standard HTTP endpoint that forwards requests.

Usage:
    python3 proxy-server.py [port] [socket-path]

Example:
    python3 proxy-server.py 3333 /tmp/athena-1000-control.sock

Then in Postman, use: http://localhost:3333/internal/navigate
"""

import sys
import os
import socket
import http.server
import socketserver
from urllib.parse import urlparse
from datetime import datetime

# Parse command line arguments
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 3333
SOCKET_PATH = sys.argv[2] if len(sys.argv) > 2 else f"/tmp/athena-{os.getuid()}-control.sock"

# Check if socket exists
if not os.path.exists(SOCKET_PATH):
    print(f"❌ Error: Socket not found at {SOCKET_PATH}")
    print("Please start Athena Browser first.")
    sys.exit(1)

print("🚀 Athena Browser HTTP-to-Unix-Socket Proxy")
print(f"📁 Socket: {SOCKET_PATH}")
print(f"🌐 HTTP Port: {PORT}")
print()


class UnixSocketHTTPHandler(http.server.BaseHTTPRequestHandler):
    """HTTP handler that forwards requests to Unix socket"""

    def log_message(self, format, *args):
        """Custom log format"""
        timestamp = datetime.now().isoformat()
        sys.stdout.write(f"{timestamp} {format % args}\n")

    def do_GET(self):
        """Handle GET requests"""
        self.proxy_request("GET")

    def do_POST(self):
        """Handle POST requests"""
        self.proxy_request("POST")

    def do_OPTIONS(self):
        """Handle OPTIONS preflight requests"""
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

    def proxy_request(self, method):
        """Forward request to Unix socket"""
        try:
            # Read request body if present
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length) if content_length > 0 else b''

            # Build HTTP request
            request_line = f"{method} {self.path} HTTP/1.1\r\n"
            headers = "Host: localhost\r\n"
            headers += "Connection: close\r\n"

            # Forward relevant headers
            if content_length > 0:
                headers += f"Content-Length: {content_length}\r\n"
            if self.headers.get('Content-Type'):
                headers += f"Content-Type: {self.headers.get('Content-Type')}\r\n"

            headers += "\r\n"

            # Complete HTTP request
            http_request = request_line.encode() + headers.encode() + body

            # Connect to Unix socket
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(SOCKET_PATH)
            sock.sendall(http_request)

            # Read response
            response = b''
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
            sock.close()

            # Parse response
            headers_end = response.find(b'\r\n\r\n')
            if headers_end == -1:
                raise Exception("Invalid HTTP response")

            response_headers = response[:headers_end].decode()
            response_body = response[headers_end + 4:]

            # Parse status line
            status_line = response_headers.split('\r\n')[0]
            status_parts = status_line.split(' ', 2)
            status_code = int(status_parts[1])

            # Send response to client
            self.send_response(status_code)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', len(response_body))
            self.end_headers()
            self.wfile.write(response_body)

        except Exception as e:
            # Send error response
            error_msg = f'{{"success": false, "error": "Proxy error: {str(e)}"}}'
            self.send_response(502)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(error_msg.encode())


# Create server
class ThreadedHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    """Handle requests in separate threads"""
    pass


# Start server
try:
    with ThreadedHTTPServer(("", PORT), UnixSocketHTTPHandler) as httpd:
        print(f"✅ Proxy server running on http://localhost:{PORT}")
        print()
        print("📝 Usage in Postman:")
        print(f"   GET  http://localhost:{PORT}/internal/get_url")
        print(f"   POST http://localhost:{PORT}/internal/navigate")
        print()
        print("Press Ctrl+C to stop")
        httpd.serve_forever()
except KeyboardInterrupt:
    print("\n👋 Shutting down proxy server...")
    print("✅ Server stopped")
except Exception as e:
    print(f"❌ Error: {e}")
    sys.exit(1)

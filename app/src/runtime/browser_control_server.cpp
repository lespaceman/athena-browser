/**
 * Browser Control Server Implementation
 *
 * Internal HTTP server for browser control via Unix socket.
 * Runs entirely on Qt main thread using non-blocking I/O.
 *
 * This file contains only core server lifecycle and connection management.
 * Request routing and handlers are in separate files for modularity.
 */

#include "runtime/browser_control_server.h"

#include "platform/qt_mainwindow.h"
#include "runtime/browser_control_server_internal.h"
#include "utils/logging.h"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <QObject>
#include <QSocketNotifier>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace athena {
namespace runtime {

static utils::Logger logger("BrowserControlServer");

// ============================================================================
// Client Connection Context
// ============================================================================

struct ClientConnection {
  int fd;
  QSocketNotifier* notifier;
  std::string buffer;
  bool headers_complete;
  size_t content_length;
  size_t header_end_pos;  // Cache position where headers end

  explicit ClientConnection(int client_fd)
      : fd(client_fd),
        notifier(nullptr),
        headers_complete(false),
        content_length(0),
        header_end_pos(0) {
    // Set non-blocking
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }

  ~ClientConnection() {
    if (notifier) {
      notifier->setEnabled(false);
      delete notifier;
      notifier = nullptr;
    }
    if (fd >= 0) {
      close(fd);
      fd = -1;
    }
  }
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

BrowserControlServer::BrowserControlServer(const BrowserControlServerConfig& config)
    : config_(config), server_fd_(-1), server_watch_id_(nullptr), running_(false) {
  logger.Debug("BrowserControlServer created");
}

BrowserControlServer::~BrowserControlServer() {
  Shutdown();
}

// ============================================================================
// Public Methods
// ============================================================================

void BrowserControlServer::SetBrowserWindow(const std::shared_ptr<platform::QtMainWindow>& window) {
  if (window) {
    logger.Debug("Browser window registered with control server");
  } else {
    logger.Debug("Browser window cleared from control server");
  }
  window_ = window;
}

utils::Result<void> BrowserControlServer::Initialize() {
  if (running_) {
    return utils::Error("Server already running");
  }

  if (window_.expired()) {
    return utils::Error("Browser window not set");
  }

  logger.Info("Initializing browser control server");

  // Clean up stale socket file
  if (std::filesystem::exists(config_.socket_path)) {
    logger.Warn("Removing stale socket file");
    try {
      std::filesystem::remove(config_.socket_path);
    } catch (const std::filesystem::filesystem_error& e) {
      logger.Warn("Failed to remove stale socket");
    }
  }

  // Create Unix socket
  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    return utils::Error("Failed to create socket: " + std::string(strerror(errno)));
  }

  // Set non-blocking
  int flags = fcntl(server_fd_, F_GETFL, 0);
  fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

  // Bind to Unix socket
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, config_.socket_path.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    return utils::Error("Failed to bind socket: " + std::string(strerror(errno)));
  }

  // Listen for connections
  if (listen(server_fd_, 5) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    std::filesystem::remove(config_.socket_path);
    return utils::Error("Failed to listen on socket: " + std::string(strerror(errno)));
  }

  // Create Qt socket notifier
  server_watch_id_ = new QSocketNotifier(server_fd_, QSocketNotifier::Read);
  QObject::connect(server_watch_id_, &QSocketNotifier::activated, [this](QSocketDescriptor) {
    AcceptConnection();
  });
  server_watch_id_->setEnabled(true);

  running_ = true;

  logger.Info("Browser control server listening on main thread");

  return utils::Ok();
}

void BrowserControlServer::Shutdown() {
  if (!running_) {
    return;
  }

  logger.Info("Shutting down browser control server");

  // Remove Qt socket notifier
  if (server_watch_id_) {
    delete server_watch_id_;
    server_watch_id_ = nullptr;
  }

  // Close all client connections
  active_clients_.clear();

  // Close socket
  if (server_fd_ >= 0) {
    close(server_fd_);
    server_fd_ = -1;
  }

  // Remove socket file
  if (!config_.socket_path.empty() && std::filesystem::exists(config_.socket_path)) {
    try {
      std::filesystem::remove(config_.socket_path);
      logger.Debug("Socket file removed");
    } catch (const std::filesystem::filesystem_error& e) {
      logger.Warn("Failed to remove socket file");
    }
  }

  window_.reset();

  running_ = false;
  logger.Info("Browser control server shut down");
}

bool BrowserControlServer::IsRunning() const {
  return running_;
}

std::string BrowserControlServer::GetSocketPath() const {
  return config_.socket_path;
}

// ============================================================================
// Connection Handling
// ============================================================================

void BrowserControlServer::AcceptConnection() {
  int client_fd = accept(server_fd_, nullptr, nullptr);
  if (client_fd < 0) {
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      logger.Error("Accept failed: " + std::string(strerror(errno)));
    }
    return;
  }

  logger.Debug("Client connected");

  // Create client connection context
  auto client = std::make_unique<ClientConnection>(client_fd);
  auto* client_ptr = client.get();

  // Set up Qt socket notifier for client data
  client_ptr->notifier = new QSocketNotifier(client_fd, QSocketNotifier::Read);
  QObject::connect(
      client_ptr->notifier, &QSocketNotifier::activated, [this, client_ptr](QSocketDescriptor) {
        if (!HandleClientData(client_ptr)) {
          // Error or request complete - close connection
          CloseClient(client_ptr);
        }
      });
  client_ptr->notifier->setEnabled(true);

  active_clients_.push_back(std::move(client));
}

bool BrowserControlServer::HandleClientData(ClientConnection* client) {
  char buffer[4096];
  ssize_t bytes_read = recv(client->fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes_read <= 0) {
    if (bytes_read < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      return true;  // No data available, keep connection
    }
    return false;  // Error or EOF
  }

  buffer[bytes_read] = '\0';
  client->buffer.append(buffer, bytes_read);

  // Enforce size limit while reading
  if (client->buffer.size() > MAX_REQUEST_SIZE) {
    logger.Error("Request size exceeds maximum allowed");
    std::string error_response = BuildHttpResponse(
        413, "Payload Too Large", R"({"success":false,"error":"Request too large"})");
    send(client->fd, error_response.c_str(), error_response.size(), 0);
    return false;
  }

  // Check if we have complete headers (only parse once)
  if (!client->headers_complete) {
    size_t header_end = client->buffer.find("\r\n\r\n");
    if (header_end != std::string::npos) {
      client->headers_complete = true;
      client->header_end_pos = header_end;  // Cache position

      // Parse Content-Length
      size_t cl_pos = client->buffer.find("Content-Length:");
      if (cl_pos != std::string::npos && cl_pos < header_end) {
        size_t cl_start = cl_pos + 15;
        size_t cl_end = client->buffer.find("\r\n", cl_start);
        if (cl_end != std::string::npos) {
          std::string cl_str = client->buffer.substr(cl_start, cl_end - cl_start);
          // Trim whitespace
          size_t first = cl_str.find_first_not_of(" \t");
          size_t last = cl_str.find_last_not_of(" \t");
          if (first != std::string::npos && last != std::string::npos) {
            cl_str = cl_str.substr(first, last - first + 1);
            try {
              client->content_length = std::stoull(cl_str);
            } catch (...) {
              client->content_length = 0;
            }
          }
        }
      }
    } else {
      // Headers not complete yet, keep reading
      return true;
    }
  }

  // Check if we have complete body (use cached header position)
  size_t body_start = client->header_end_pos + 4;
  size_t body_received = client->buffer.size() - body_start;

  if (client->content_length > 0 && body_received < client->content_length) {
    // Body not complete yet, keep reading
    return true;
  }

  // We have a complete request - process it on main thread (we're already on it!)
  std::string response = ProcessRequest(client->buffer);

  // Send response
  ssize_t bytes_sent = send(client->fd, response.c_str(), response.size(), 0);
  if (bytes_sent < 0) {
    logger.Error("Failed to send response");
  } else {
    logger.Debug("Response sent (" + std::to_string(bytes_sent) + " bytes)");
  }

  return false;  // Close connection after response
}

void BrowserControlServer::CloseClient(ClientConnection* client) {
  auto it = std::find_if(active_clients_.begin(),
                         active_clients_.end(),
                         [client](const std::unique_ptr<ClientConnection>& candidate) {
                           return candidate.get() == client;
                         });
  if (it != active_clients_.end()) {
    active_clients_.erase(it);
    logger.Debug("Client connection closed");
    return;
  }

  logger.Warn("Client connection already closed");
}

}  // namespace runtime
}  // namespace athena

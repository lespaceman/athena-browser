/**
 * Browser Control Server Implementation
 *
 * Internal HTTP server for browser control via Unix socket.
 */

#include "runtime/browser_control_server.h"
#include "utils/logging.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <filesystem>
#include <glib.h>

namespace athena {
namespace runtime {

static utils::Logger logger("BrowserControlServer");

// ============================================================================
// Constructor / Destructor
// ============================================================================

BrowserControlServer::BrowserControlServer(
    const BrowserControlServerConfig& config)
    : config_(config),
      window_(nullptr),
      server_fd_(-1),
      server_watch_id_(0),
      running_(false) {
  logger.Debug("BrowserControlServer created");
}

BrowserControlServer::~BrowserControlServer() {
  Shutdown();
}

// ============================================================================
// Public Methods
// ============================================================================

void BrowserControlServer::SetBrowserWindow(platform::GtkWindow* window) {
  window_ = window;
  logger.Debug("Browser window registered with control server");
}

utils::Result<void> BrowserControlServer::Initialize() {
  if (running_) {
    return utils::Error("Server already running");
  }

  if (!window_) {
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
    return utils::Error("Failed to create socket: " +
                       std::string(strerror(errno)));
  }

  // Set non-blocking
  int flags = fcntl(server_fd_, F_GETFL, 0);
  fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

  // Bind to Unix socket
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, config_.socket_path.c_str(),
          sizeof(addr.sun_path) - 1);

  if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    return utils::Error("Failed to bind socket: " +
                       std::string(strerror(errno)));
  }

  // Listen for connections
  if (listen(server_fd_, 5) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    std::filesystem::remove(config_.socket_path);
    return utils::Error("Failed to listen on socket: " +
                       std::string(strerror(errno)));
  }

  // Create GLib I/O watch
  GIOChannel* channel = g_io_channel_unix_new(server_fd_);
  server_watch_id_ = g_io_add_watch(channel,
                                    (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP),
                                    OnServerReadable,
                                    this);
  g_io_channel_unref(channel);

  running_ = true;

  logger.Info("Browser control server listening");

  return utils::Ok();
}

void BrowserControlServer::Shutdown() {
  if (!running_) {
    return;
  }

  logger.Info("Shutting down browser control server");

  // Remove GLib watch
  if (server_watch_id_ > 0) {
    g_source_remove(server_watch_id_);
    server_watch_id_ = 0;
  }

  // Close socket
  if (server_fd_ >= 0) {
    close(server_fd_);
    server_fd_ = -1;
  }

  // Remove socket file
  if (!config_.socket_path.empty() &&
      std::filesystem::exists(config_.socket_path)) {
    try {
      std::filesystem::remove(config_.socket_path);
      logger.Debug("Socket file removed");
    } catch (const std::filesystem::filesystem_error& e) {
      logger.Warn("Failed to remove socket file");
    }
  }

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
// GLib Callbacks
// ============================================================================

gboolean BrowserControlServer::OnServerReadable(GIOChannel* source,
                                                GIOCondition condition,
                                                gpointer user_data) {
  auto* server = static_cast<BrowserControlServer*>(user_data);

  if (condition & (G_IO_ERR | G_IO_HUP)) {
    logger.Error("Server socket error");
    return G_SOURCE_REMOVE;
  }

  if (condition & G_IO_IN) {
    if (!server->AcceptConnection()) {
      logger.Warn("Failed to accept connection");
    }
  }

  return G_SOURCE_CONTINUE;
}

// ============================================================================
// Connection Handling
// ============================================================================

bool BrowserControlServer::AcceptConnection() {
  // Accept connection
  int client_fd = accept(server_fd_, nullptr, nullptr);
  if (client_fd < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return true;  // No connections pending
    }
    logger.Error("Accept failed");
    return false;
  }

  logger.Debug("Client connected");

  // Handle request synchronously (keep it simple)
  bool success = HandleRequest(client_fd);

  // Close connection
  close(client_fd);

  return success;
}

bool BrowserControlServer::HandleRequest(int client_fd) {
  // Read HTTP request
  std::string request;
  char buffer[4096];
  ssize_t bytes_read;

  // Read until we have headers
  while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
    buffer[bytes_read] = '\0';
    request += buffer;

    // Check for end of headers
    if (request.find("\r\n\r\n") != std::string::npos) {
      break;
    }
  }

  if (bytes_read < 0) {
    logger.Error("Failed to read request");
    return false;
  }

  if (request.empty()) {
    logger.Warn("Empty request received");
    return false;
  }

  logger.Debug("Received request");

  // Process request and generate response
  std::string response = ProcessRequest(request);

  // Send response
  ssize_t bytes_sent = send(client_fd, response.c_str(), response.size(), 0);
  if (bytes_sent < 0) {
    logger.Error("Failed to send response");
    return false;
  }

  logger.Debug("Response sent");

  return true;
}

// ============================================================================
// Request Processing
// ============================================================================

std::string BrowserControlServer::ProcessRequest(const std::string& request) {
  std::string method = ParseHttpMethod(request);
  std::string path = ParseHttpPath(request);
  std::string body = ParseHttpBody(request);

  logger.Debug("Processing request");

  // Route to handlers
  if (method == "POST" && path == "/internal/open_url") {
    // Extract URL from JSON body: {"url":"..."}
    size_t url_start = body.find("\"url\"");
    if (url_start == std::string::npos) {
      return BuildHttpResponse(400, "Bad Request",
                              R"({"success":false,"error":"Missing url parameter"})");
    }

    url_start = body.find("\"", url_start + 5);  // Find opening quote after "url"
    if (url_start == std::string::npos) {
      return BuildHttpResponse(400, "Bad Request",
                              R"({"success":false,"error":"Invalid JSON"})");
    }
    url_start++;  // Skip opening quote

    size_t url_end = body.find("\"", url_start);
    if (url_end == std::string::npos) {
      return BuildHttpResponse(400, "Bad Request",
                              R"({"success":false,"error":"Invalid JSON"})");
    }

    std::string url = body.substr(url_start, url_end - url_start);
    return BuildHttpResponse(200, "OK", HandleOpenUrl(url));

  } else if (method == "GET" && path == "/internal/get_url") {
    return BuildHttpResponse(200, "OK", HandleGetUrl());

  } else if (method == "GET" && path == "/internal/tab_count") {
    return BuildHttpResponse(200, "OK", HandleGetTabCount());

  } else {
    logger.Warn("Unknown endpoint");
    return BuildHttpResponse(404, "Not Found",
                            R"({"success":false,"error":"Endpoint not found"})");
  }
}

// ============================================================================
// Request Handlers
// ============================================================================

std::string BrowserControlServer::HandleOpenUrl(const std::string& url) {
  logger.Info("Opening URL");

  if (!window_) {
    return R"({"success":false,"error":"Browser window not available"})";
  }

  try {
    // Check if we have tabs
    size_t tab_count = window_->GetTabCount();

    if (tab_count == 0) {
      // Create new tab
      int tab_index = window_->CreateTab(url);
      if (tab_index < 0) {
        return R"({"success":false,"error":"Failed to create tab"})";
      }

      std::ostringstream response;
      response << R"({"success":true,"message":"Created tab and navigated to )"
               << url << R"(","tabIndex":)" << tab_index << "}";
      return response.str();

    } else {
      // Navigate active tab
      window_->LoadURL(url);

      std::ostringstream response;
      response << R"({"success":true,"message":"Navigated to )" << url << R"(")"
               << R"(,"tabIndex":)" << window_->GetActiveTabIndex() << "}";
      return response.str();
    }

  } catch (const std::exception& e) {
    logger.Error("Failed to open URL");
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    return response.str();
  }
}

std::string BrowserControlServer::HandleGetUrl() {
  if (!window_) {
    return R"({"success":false,"error":"Browser window not available"})";
  }

  try {
    auto* tab = window_->GetActiveTab();
    if (!tab) {
      return R"({"success":false,"error":"No active tab"})";
    }

    std::ostringstream response;
    response << R"({"success":true,"url":")" << tab->url << R"("})";
    return response.str();

  } catch (const std::exception& e) {
    logger.Error("Failed to get URL");
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    return response.str();
  }
}

std::string BrowserControlServer::HandleGetTabCount() {
  if (!window_) {
    return R"({"success":false,"error":"Browser window not available"})";
  }

  try {
    size_t count = window_->GetTabCount();

    std::ostringstream response;
    response << R"({"success":true,"count":)" << count << "}";
    return response.str();

  } catch (const std::exception& e) {
    logger.Error("Failed to get tab count");
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    return response.str();
  }
}

// ============================================================================
// HTTP Helpers
// ============================================================================

std::string BrowserControlServer::ParseHttpMethod(const std::string& request) {
  size_t space_pos = request.find(' ');
  if (space_pos == std::string::npos) {
    return "";
  }
  return request.substr(0, space_pos);
}

std::string BrowserControlServer::ParseHttpPath(const std::string& request) {
  size_t first_space = request.find(' ');
  if (first_space == std::string::npos) {
    return "";
  }

  size_t second_space = request.find(' ', first_space + 1);
  if (second_space == std::string::npos) {
    return "";
  }

  return request.substr(first_space + 1, second_space - first_space - 1);
}

std::string BrowserControlServer::ParseHttpBody(const std::string& request) {
  size_t body_start = request.find("\r\n\r\n");
  if (body_start == std::string::npos) {
    return "";
  }

  return request.substr(body_start + 4);
}

std::string BrowserControlServer::BuildHttpResponse(int status_code,
                                                    const std::string& status_text,
                                                    const std::string& body) {
  std::ostringstream response;
  response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
  response << "Content-Type: application/json\r\n";
  response << "Content-Length: " << body.size() << "\r\n";
  response << "Connection: close\r\n";
  response << "\r\n";
  response << body;

  return response.str();
}

}  // namespace runtime
}  // namespace athena

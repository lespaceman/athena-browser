/**
 * Browser Control Server Implementation
 *
 * Internal HTTP server for browser control via Unix socket.
 * Runs entirely on GTK main thread using non-blocking I/O.
 */

#include "runtime/browser_control_server.h"
#include "utils/logging.h"
#include <nlohmann/json.hpp>
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

// Maximum size for HTTP requests (1MB to prevent DoS attacks)
static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;

// ============================================================================
// Client Connection Context
// ============================================================================

struct ClientConnection {
  BrowserControlServer* server;
  int fd;
  GIOChannel* channel;
  guint watch_id;
  std::string buffer;
  bool headers_complete;
  size_t content_length;

  ClientConnection(BrowserControlServer* s, int client_fd)
      : server(s),
        fd(client_fd),
        channel(nullptr),
        watch_id(0),
        headers_complete(false),
        content_length(0) {
    // Set non-blocking
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    // Create GLib I/O watch for this client
    channel = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding(channel, nullptr, nullptr);  // Binary mode
    g_io_channel_set_buffered(channel, FALSE);
  }

  ~ClientConnection() {
    if (watch_id > 0) {
      g_source_remove(watch_id);
    }
    if (channel) {
      g_io_channel_unref(channel);
    }
    if (fd >= 0) {
      close(fd);
    }
  }
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

BrowserControlServer::BrowserControlServer(
    const BrowserControlServerConfig& config)
    : config_(config),
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

void BrowserControlServer::SetBrowserWindow(const std::shared_ptr<platform::GtkWindow>& window) {
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

  logger.Info("Browser control server listening on main thread");

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

  // Close all client connections
  for (auto* client_ptr : active_clients_) {
    auto* client = static_cast<ClientConnection*>(client_ptr);
    delete client;
  }
  active_clients_.clear();

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
    server->AcceptConnection();
  }

  return G_SOURCE_CONTINUE;
}

gboolean BrowserControlServer::OnClientReadable(GIOChannel* source,
                                                GIOCondition condition,
                                                gpointer user_data) {
  auto* client = static_cast<ClientConnection*>(user_data);
  auto* server = client->server;

  if (condition & (G_IO_ERR | G_IO_HUP)) {
    logger.Debug("Client disconnected");
    server->CloseClient(client);
    return G_SOURCE_REMOVE;
  }

  if (condition & G_IO_IN) {
    if (!server->HandleClientData(client)) {
      // Error or request complete - close connection
      server->CloseClient(client);
      return G_SOURCE_REMOVE;
    }
  }

  return G_SOURCE_CONTINUE;
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
  auto* client = new ClientConnection(this, client_fd);

  // Set up watch for client data
  client->watch_id = g_io_add_watch(client->channel,
                                     (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP),
                                     OnClientReadable,
                                     client);

  active_clients_.push_back(client);
}

bool BrowserControlServer::HandleClientData(void* client_ptr) {
  auto* client = static_cast<ClientConnection*>(client_ptr);
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
    std::string error_response = BuildHttpResponse(413, "Payload Too Large",
                                                   R"({"success":false,"error":"Request too large"})");
    send(client->fd, error_response.c_str(), error_response.size(), 0);
    return false;
  }

  // Check if we have complete headers
  if (!client->headers_complete) {
    size_t header_end = client->buffer.find("\r\n\r\n");
    if (header_end != std::string::npos) {
      client->headers_complete = true;

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

  // Check if we have complete body
  size_t header_end = client->buffer.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return true;  // Should not happen, but be safe
  }

  size_t body_start = header_end + 4;
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

void BrowserControlServer::CloseClient(void* client_ptr) {
  auto* client = static_cast<ClientConnection*>(client_ptr);
  auto it = std::find(active_clients_.begin(), active_clients_.end(), client_ptr);
  if (it != active_clients_.end()) {
    active_clients_.erase(it);
  }
  delete client;
  logger.Debug("Client connection closed");
}

// ============================================================================
// Request Processing (runs on GTK main thread)
// ============================================================================

std::string BrowserControlServer::ProcessRequest(const std::string& request) {
  std::string method = ParseHttpMethod(request);
  std::string path = ParseHttpPath(request);
  std::string body = ParseHttpBody(request);

  logger.Debug("Processing " + method + " " + path);

  // Route to handlers - all run synchronously on main thread
  if (method == "POST" && path == "/internal/open_url") {
    try {
      auto json = nlohmann::json::parse(body);

      if (!json.contains("url")) {
        return BuildHttpResponse(400, "Bad Request",
                                R"({"success":false,"error":"Missing url parameter"})");
      }

      std::string url = json["url"].get<std::string>();
      return BuildHttpResponse(200, "OK", HandleOpenUrl(url));

    } catch (const nlohmann::json::exception& e) {
      logger.Error("JSON parsing error: " + std::string(e.what()));
      return BuildHttpResponse(400, "Bad Request",
                              R"({"success":false,"error":"Invalid JSON"})");
    }

  } else if (method == "GET" && path == "/internal/get_url") {
    return BuildHttpResponse(200, "OK", HandleGetUrl());

  } else if (method == "GET" && path == "/internal/tab_count") {
    return BuildHttpResponse(200, "OK", HandleGetTabCount());

  } else if (method == "GET" && path == "/internal/get_html") {
    return BuildHttpResponse(200, "OK", HandleGetPageHtml());

  } else if (method == "POST" && path == "/internal/execute_js") {
    try {
      auto json = nlohmann::json::parse(body);

      if (!json.contains("code")) {
        return BuildHttpResponse(400, "Bad Request",
                                R"({"success":false,"error":"Missing code parameter"})");
      }

      std::string code = json["code"].get<std::string>();
      return BuildHttpResponse(200, "OK", HandleExecuteJavaScript(code));

    } catch (const nlohmann::json::exception& e) {
      logger.Error("JSON parsing error: " + std::string(e.what()));
      return BuildHttpResponse(400, "Bad Request",
                              R"({"success":false,"error":"Invalid JSON"})");
    }

  } else if (method == "GET" && path == "/internal/screenshot") {
    return BuildHttpResponse(200, "OK", HandleTakeScreenshot());

  } else {
    logger.Warn("Unknown endpoint: " + path);
    return BuildHttpResponse(404, "Not Found",
                            R"({"success":false,"error":"Endpoint not found"})");
  }
}

// ============================================================================
// Request Handlers (run synchronously on GTK main thread)
// ============================================================================

std::string BrowserControlServer::HandleOpenUrl(const std::string& url) {
  logger.Info("Opening URL: " + url);

  auto window = window_.lock();
  if (!running_ || !window) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  try {
    // Check tab count to decide whether to create tab or navigate
    size_t tab_count = window->GetTabCount();

    if (tab_count == 0) {
      // Create new tab
      int tab_index = window->CreateTab(url);
      if (tab_index < 0) {
        return R"({"success":false,"error":"Failed to create tab"})";
      }

      std::ostringstream response;
      response << R"({"success":true,"message":"Created tab and navigated to )"
               << url << R"(","tabIndex":)" << tab_index << "}";
      return response.str();

    } else {
      // Navigate active tab
      window->LoadURL(url);

      std::ostringstream response;
      response << R"({"success":true,"message":"Navigated to )" << url << R"(")"
               << R"(,"tabIndex":)" << window->GetActiveTabIndex() << "}";
      return response.str();
    }

  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    return response.str();
  }
}

std::string BrowserControlServer::HandleGetUrl() {
  auto window = window_.lock();
  if (!running_ || !window) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  try {
    auto* tab = window->GetActiveTab();
    if (!tab) {
      return R"({"success":false,"error":"No active tab"})";
    }

    std::ostringstream response;
    response << R"({"success":true,"url":")" << tab->url << R"("})";
    return response.str();

  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    return response.str();
  }
}

std::string BrowserControlServer::HandleGetTabCount() {
  auto window = window_.lock();
  if (!running_ || !window) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  try {
    size_t count = window->GetTabCount();
    std::ostringstream response;
    response << R"({"success":true,"count":)" << count << "}";
    return response.str();

  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    return response.str();
  }
}

std::string BrowserControlServer::HandleGetPageHtml() {
  auto window = window_.lock();
  if (!running_ || !window) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  try {
    std::string html = window->GetPageHTML();
    if (html.empty()) {
      return R"({"success":false,"error":"Failed to retrieve HTML"})";
    }

    // Escape HTML for JSON (replace quotes and newlines)
    std::string escaped;
    escaped.reserve(html.size());
    for (char c : html) {
      switch (c) {
        case '"':  escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:   escaped += c; break;
      }
    }

    std::ostringstream response;
    response << R"({"success":true,"html":")" << escaped << R"("})";
    return response.str();

  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    return response.str();
  }
}

std::string BrowserControlServer::HandleExecuteJavaScript(const std::string& code) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  try {
    std::string result = window->ExecuteJavaScript(code);
    std::ostringstream response;
    response << R"({"success":true,"result":)" << result << "}";
    return response.str();

  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    return response.str();
  }
}

std::string BrowserControlServer::HandleTakeScreenshot() {
  auto window = window_.lock();
  if (!running_ || !window) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  try {
    std::string base64_png = window->TakeScreenshot();
    if (base64_png.empty()) {
      return R"({"success":false,"error":"Failed to capture screenshot"})";
    }

    std::ostringstream response;
    response << R"({"success":true,"screenshot":")" << base64_png << R"("})";
    return response.str();

  } catch (const std::exception& e) {
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

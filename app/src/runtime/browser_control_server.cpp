/**
 * Browser Control Server Implementation
 *
 * Internal HTTP server for browser control via Unix socket.
 * Runs entirely on Qt main thread using non-blocking I/O.
 */

#include "runtime/browser_control_server.h"
#include "platform/qt_mainwindow.h"
#include "utils/logging.h"
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <QSocketNotifier>
#include <QObject>

namespace athena {
namespace runtime {

static utils::Logger logger("BrowserControlServer");

// Maximum size for HTTP requests (1MB to prevent DoS attacks)
static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;

namespace {

constexpr int kDefaultNavigationTimeoutMs = 15000;
constexpr int kDefaultContentTimeoutMs = 5000;

bool SwitchToRequestedTab(const std::shared_ptr<platform::QtMainWindow>& window,
                          std::optional<size_t> tab_index,
                          std::string& error_message) {
  if (!tab_index.has_value()) {
    return true;
  }

  size_t count = window->GetTabCount();
  if (*tab_index >= count) {
    error_message = "Invalid tab index";
    return false;
  }

  if (window->GetActiveTabIndex() != *tab_index) {
    window->SwitchToTab(*tab_index);
  }
  return true;
}

}  // namespace

// ============================================================================
// Client Connection Context
// ============================================================================

struct ClientConnection {
  BrowserControlServer* server;
  int fd;
  QSocketNotifier* notifier;
  std::string buffer;
  bool headers_complete;
  size_t content_length;

  ClientConnection(BrowserControlServer* s, int client_fd)
      : server(s),
        fd(client_fd),
        notifier(nullptr),
        headers_complete(false),
        content_length(0) {
    // Set non-blocking
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }

  ~ClientConnection() {
    if (notifier) {
      delete notifier;
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
      server_watch_id_(nullptr),
      running_(false) {
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

  // Create Qt socket notifier
  server_watch_id_ = new QSocketNotifier(server_fd_, QSocketNotifier::Read);
  QObject::connect(server_watch_id_, &QSocketNotifier::activated,
                   [this](QSocketDescriptor) {
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

  // Set up Qt socket notifier for client data
  client->notifier = new QSocketNotifier(client_fd, QSocketNotifier::Read);
  QObject::connect(client->notifier, &QSocketNotifier::activated,
                   [this, client](QSocketDescriptor) {
    if (!HandleClientData(client)) {
      // Error or request complete - close connection
      CloseClient(client);
    }
  });
  client->notifier->setEnabled(true);

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
// Request Processing (runs on Qt main thread)
// ============================================================================

std::string BrowserControlServer::ProcessRequest(const std::string& request) {
  std::string method = ParseHttpMethod(request);
  std::string path = ParseHttpPath(request);
  std::string body = ParseHttpBody(request);

  logger.Debug("Processing " + method + " " + path);

  auto parse_json = [&](nlohmann::json& json_out) -> bool {
    try {
      if (body.empty()) {
        json_out = nlohmann::json::object();
      } else {
        json_out = nlohmann::json::parse(body);
      }
      return true;
    } catch (const nlohmann::json::exception& e) {
      logger.Error("JSON parsing error: " + std::string(e.what()));
      return false;
    }
  };

  // Route to handlers - all run synchronously on main thread
  if (method == "POST" && path == "/internal/open_url") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("url") || !json["url"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing url parameter"})");
    }
    std::string url = json["url"].get<std::string>();
    return BuildHttpResponse(200, "OK", HandleOpenUrl(url));

  } else if ((method == "GET" || method == "POST") && path == "/internal/get_url") {
    std::optional<size_t> tab_index;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleGetUrl(tab_index));

  } else if (method == "GET" && path == "/internal/tab_count") {
    return BuildHttpResponse(200, "OK", HandleGetTabCount());

  } else if ((method == "GET" || method == "POST") && path == "/internal/get_html") {
    std::optional<size_t> tab_index;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleGetPageHtml(tab_index));

  } else if (method == "POST" && path == "/internal/execute_js") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("code") || !json["code"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing code parameter"})");
    }
    std::optional<size_t> tab_index;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    std::string code = json["code"].get<std::string>();
    return BuildHttpResponse(200, "OK", HandleExecuteJavaScript(code, tab_index));

  } else if ((method == "GET" || method == "POST") && path == "/internal/screenshot") {
    std::optional<size_t> tab_index;
    std::optional<bool> full_page;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
      if (json.contains("fullPage") && json["fullPage"].is_boolean()) {
        full_page = json["fullPage"].get<bool>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleTakeScreenshot(tab_index, full_page));

  } else if (method == "POST" && path == "/internal/navigate") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("url") || !json["url"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing url parameter"})");
    }
    std::optional<size_t> tab_index;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    return BuildHttpResponse(200, "OK", HandleNavigate(json["url"].get<std::string>(), tab_index));

  } else if (method == "POST" && path == "/internal/history") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("action") || !json["action"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing action parameter"})");
    }
    std::optional<size_t> tab_index;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    return BuildHttpResponse(200, "OK",
                             HandleHistory(json["action"].get<std::string>(), tab_index));

  } else if (method == "POST" && path == "/internal/reload") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    std::optional<size_t> tab_index;
    std::optional<bool> ignore_cache;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    if (json.contains("ignoreCache") && json["ignoreCache"].is_boolean()) {
      ignore_cache = json["ignoreCache"].get<bool>();
    }
    return BuildHttpResponse(200, "OK", HandleReload(tab_index, ignore_cache));

  } else if (method == "POST" && path == "/internal/tab/create") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("url") || !json["url"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing url parameter"})");
    }
    return BuildHttpResponse(200, "OK", HandleCreateTab(json["url"].get<std::string>()));

  } else if (method == "POST" && path == "/internal/tab/close") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("tabIndex") || !json["tabIndex"].is_number_unsigned()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing tabIndex parameter"})");
    }
    return BuildHttpResponse(200, "OK", HandleCloseTab(json["tabIndex"].get<size_t>()));

  } else if (method == "POST" && path == "/internal/tab/switch") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("tabIndex") || !json["tabIndex"].is_number_unsigned()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing tabIndex parameter"})");
    }
    return BuildHttpResponse(200, "OK", HandleSwitchTab(json["tabIndex"].get<size_t>()));

  } else if (method == "GET" && path == "/internal/tab_info") {
    return BuildHttpResponse(200, "OK", HandleTabInfo());

  } else {
    logger.Warn("Unknown endpoint: " + path);
    return BuildHttpResponse(404, "Not Found",
                             R"({"success":false,"error":"Endpoint not found"})");
  }
}

// ============================================================================
// Request Handlers (run synchronously on Qt main thread)
// ============================================================================

std::string BrowserControlServer::HandleOpenUrl(const std::string& url) {
  logger.Info("Opening URL: " + url);

  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    const auto start = std::chrono::steady_clock::now();
    size_t target_tab = 0;
    bool created_tab = false;

    size_t tab_count = window->GetTabCount();
    if (tab_count == 0) {
      int tab_index = window->CreateTab(QString::fromStdString(url));
      if (tab_index < 0) {
        return nlohmann::json{
            {"success", false},
            {"error", "Failed to create tab"}}.dump();
      }

      target_tab = static_cast<size_t>(tab_index);
      created_tab = true;
    } else {
      target_tab = window->GetActiveTabIndex();
      window->LoadURL(QString::fromStdString(url));
    }

    bool loaded = window->WaitForLoadToComplete(target_tab, kDefaultNavigationTimeoutMs);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    if (!loaded) {
      return nlohmann::json{
          {"success", false},
          {"error", "Navigation timed out"},
          {"tabIndex", static_cast<int>(target_tab)},
          {"loadTimeMs", elapsed}}.dump();
    }

    const std::string final_url = window->GetCurrentUrl().toStdString();
    return nlohmann::json{
        {"success", true},
        {"tabIndex", static_cast<int>(target_tab)},
        {"finalUrl", final_url.empty() ? url : final_url},
        {"createdTab", created_tab},
        {"loadTimeMs", elapsed}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleGetUrl(std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    QString url = window->GetCurrentUrl();
    return nlohmann::json{
        {"success", true},
        {"url", url.toStdString()},
        {"tabIndex", static_cast<int>(window->GetActiveTabIndex())}}.dump();
  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
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

std::string BrowserControlServer::HandleGetPageHtml(std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"},
          {"tabIndex", static_cast<int>(target_tab)}}.dump();
    }

    QString html = window->GetPageHTML();
    if (html.isEmpty()) {
      return nlohmann::json{
          {"success", false},
          {"error", "Failed to retrieve HTML"}}.dump();
    }

    // Escape HTML for JSON (replace quotes and newlines)
    std::string html_str = html.toStdString();
    std::string escaped;
    escaped.reserve(html_str.size());
    for (char c : html_str) {
      switch (c) {
        case '"':  escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:   escaped += c; break;
      }
    }

    nlohmann::json response = {
        {"success", true},
        {"html", escaped},
        {"tabIndex", static_cast<int>(window->GetActiveTabIndex())}};
    return response.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleExecuteJavaScript(const std::string& code,
                                                          std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"},
          {"tabIndex", static_cast<int>(target_tab)}}.dump();
    }

    QString result = window->ExecuteJavaScript(QString::fromStdString(code));
    return nlohmann::json{
        {"success", true},
        {"result", result.toStdString()},
        {"tabIndex", static_cast<int>(target_tab)}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleTakeScreenshot(std::optional<size_t> tab_index,
                                                       std::optional<bool> full_page) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"},
          {"tabIndex", static_cast<int>(target_tab)}}.dump();
    }

    if (full_page.value_or(false)) {
      logger.Warn("Full page screenshot requested but not supported; capturing viewport only");
    }

    QString base64_png = window->TakeScreenshot();
    if (base64_png.isEmpty()) {
      return nlohmann::json{
          {"success", false},
          {"error", "Failed to capture screenshot"}}.dump();
    }

    return nlohmann::json{
        {"success", true},
        {"screenshot", base64_png.toStdString()},
        {"tabIndex", static_cast<int>(target_tab)}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleNavigate(const std::string& url,
                                                 std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  if (window->GetTabCount() == 0) {
    return HandleOpenUrl(url);
  }

  std::string error;
  if (!SwitchToRequestedTab(window, tab_index, error)) {
    return nlohmann::json{
        {"success", false},
        {"error", error}}.dump();
  }

  size_t target_tab = tab_index.has_value()
      ? *tab_index
      : window->GetActiveTabIndex();

  const auto start = std::chrono::steady_clock::now();
  window->LoadURL(QString::fromStdString(url));

  bool loaded = window->WaitForLoadToComplete(target_tab, kDefaultNavigationTimeoutMs);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();

  if (!loaded) {
    return nlohmann::json{
        {"success", false},
        {"error", "Navigation timed out"},
        {"tabIndex", static_cast<int>(target_tab)},
        {"loadTimeMs", elapsed}}.dump();
  }

  const std::string final_url = window->GetCurrentUrl().toStdString();
  return nlohmann::json{
      {"success", true},
      {"tabIndex", static_cast<int>(target_tab)},
      {"finalUrl", final_url.empty() ? url : final_url},
      {"loadTimeMs", elapsed}}.dump();
}

std::string BrowserControlServer::HandleHistory(const std::string& action,
                                                std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  std::string error;
  if (!SwitchToRequestedTab(window, tab_index, error)) {
    return nlohmann::json{
        {"success", false},
        {"error", error}}.dump();
  }

  std::string action_lower = action;
  std::transform(
      action_lower.begin(),
      action_lower.end(),
      action_lower.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

  size_t target_tab = window->GetActiveTabIndex();
  const auto start = std::chrono::steady_clock::now();

  if (action_lower == "back") {
    window->GoBack();
  } else if (action_lower == "forward") {
    window->GoForward();
  } else {
    return nlohmann::json{
        {"success", false},
        {"error", "Invalid history action"}}.dump();
  }
  bool loaded = window->WaitForLoadToComplete(target_tab, kDefaultNavigationTimeoutMs);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();

  if (!loaded) {
    return nlohmann::json{
        {"success", false},
        {"error", "Navigation timed out"},
        {"action", action_lower},
        {"tabIndex", static_cast<int>(target_tab)},
        {"loadTimeMs", elapsed}}.dump();
  }

  const std::string final_url = window->GetCurrentUrl().toStdString();
  return nlohmann::json{
      {"success", true},
      {"action", action_lower},
      {"tabIndex", static_cast<int>(target_tab)},
      {"finalUrl", final_url},
      {"loadTimeMs", elapsed}}.dump();
}

std::string BrowserControlServer::HandleReload(std::optional<size_t> tab_index,
                                               std::optional<bool> ignore_cache) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  std::string error;
  if (!SwitchToRequestedTab(window, tab_index, error)) {
    return nlohmann::json{
        {"success", false},
        {"error", error}}.dump();
  }

  size_t target_tab = window->GetActiveTabIndex();
  const auto start = std::chrono::steady_clock::now();

  window->Reload(ignore_cache.value_or(false));
  bool loaded = window->WaitForLoadToComplete(target_tab, kDefaultNavigationTimeoutMs);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();

  if (!loaded) {
    return nlohmann::json{
        {"success", false},
        {"error", "Reload timed out"},
        {"tabIndex", static_cast<int>(target_tab)},
        {"ignoreCache", ignore_cache.value_or(false)},
        {"loadTimeMs", elapsed}}.dump();
  }

  return nlohmann::json{
      {"success", true},
      {"tabIndex", static_cast<int>(target_tab)},
      {"ignoreCache", ignore_cache.value_or(false)},
      {"loadTimeMs", elapsed}}.dump();
}

std::string BrowserControlServer::HandleCreateTab(const std::string& url) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  const auto start = std::chrono::steady_clock::now();
  int tab_index = window->CreateTab(QString::fromStdString(url));
  if (tab_index < 0) {
    return nlohmann::json{
        {"success", false},
        {"error", "Failed to create tab"}}.dump();
  }

  bool loaded = window->WaitForLoadToComplete(static_cast<size_t>(tab_index),
                                              kDefaultNavigationTimeoutMs);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();

  if (!loaded) {
    return nlohmann::json{
        {"success", false},
        {"error", "Tab creation timed out"},
        {"tabIndex", tab_index},
        {"loadTimeMs", elapsed}}.dump();
  }

  const std::string final_url = window->GetCurrentUrl().toStdString();
  return nlohmann::json{
      {"success", true},
      {"tabIndex", tab_index},
      {"url", url},
      {"finalUrl", final_url.empty() ? url : final_url},
      {"loadTimeMs", elapsed}}.dump();
}

std::string BrowserControlServer::HandleCloseTab(size_t tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  size_t count = window->GetTabCount();
  if (tab_index >= count) {
    return nlohmann::json{
        {"success", false},
        {"error", "Invalid tab index"}}.dump();
  }

  window->CloseTab(tab_index);
  return nlohmann::json{
      {"success", true},
      {"tabIndex", static_cast<int>(tab_index)}}.dump();
}

std::string BrowserControlServer::HandleSwitchTab(size_t tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  size_t count = window->GetTabCount();
  if (tab_index >= count) {
    return nlohmann::json{
        {"success", false},
        {"error", "Invalid tab index"}}.dump();
  }

  window->SwitchToTab(tab_index);
  return nlohmann::json{
      {"success", true},
      {"tabIndex", static_cast<int>(window->GetActiveTabIndex())}}.dump();
}

std::string BrowserControlServer::HandleTabInfo() {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  size_t count = window->GetTabCount();
  size_t active = window->GetActiveTabIndex();
  return nlohmann::json{
      {"success", true},
      {"count", count},
      {"activeTabIndex", active}}.dump();
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

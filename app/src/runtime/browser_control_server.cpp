/**
 * Browser Control Server Implementation
 *
 * Internal HTTP server for browser control via Unix socket.
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
// Request Context for Background Processing
// ============================================================================

struct RequestContext {
  BrowserControlServer* server;
  int client_fd;
};

// Worker function that runs in background thread
// Note: Not static because it's declared as friend in header
void handle_request_worker(gpointer data, gpointer user_data) {
  auto* context = static_cast<RequestContext*>(data);

  // Process request in background thread (THIS NOW RUNS OFF GTK MAIN THREAD)
  bool success = context->server->HandleRequest(context->client_fd);

  if (!success) {
    logger.Warn("Request handling failed");
  }

  // Close connection
  close(context->client_fd);

  // Clean up context
  delete context;
}

// ============================================================================
// Main Thread Marshaling Contexts
// ============================================================================

// Context for marshaling LoadURL to main thread
struct LoadURLContext {
  platform::GtkWindow* window;
  std::string url;
  std::string* result_json;  // Output parameter
  GMutex mutex;
  GCond cond;
  bool done;
};

// Context for marshaling CreateTab to main thread
struct CreateTabContext {
  platform::GtkWindow* window;
  std::string url;
  int* tab_index;  // Output parameter
  std::string* result_json;  // Output parameter
  GMutex mutex;
  GCond cond;
  bool done;
};

// Context for marshaling GetActiveTab to main thread
struct GetActiveTabContext {
  platform::GtkWindow* window;
  std::string* result_json;  // Output parameter
  GMutex mutex;
  GCond cond;
  bool done;
};

// Context for marshaling GetTabCount to main thread
struct GetTabCountContext {
  platform::GtkWindow* window;
  std::string* result_json;  // Output parameter
  GMutex mutex;
  GCond cond;
  bool done;
};

// Idle callbacks for main thread operations
static gboolean load_url_idle(gpointer data) {
  auto* ctx = static_cast<LoadURLContext*>(data);

  try {
    // NOW on main thread - safe to call GTK methods
    ctx->window->LoadURL(ctx->url);
    std::ostringstream response;
    response << R"({"success":true,"message":"Navigated to )" << ctx->url << R"(")"
             << R"(,"tabIndex":)" << ctx->window->GetActiveTabIndex() << "}";
    *ctx->result_json = response.str();
  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    *ctx->result_json = response.str();
  }

  // Signal completion
  g_mutex_lock(&ctx->mutex);
  ctx->done = true;
  g_cond_signal(&ctx->cond);
  g_mutex_unlock(&ctx->mutex);

  return G_SOURCE_REMOVE;
}

static gboolean create_tab_idle(gpointer data) {
  auto* ctx = static_cast<CreateTabContext*>(data);

  try {
    // NOW on main thread - safe to call GTK methods
    int tab_index = ctx->window->CreateTab(ctx->url);
    *ctx->tab_index = tab_index;

    if (tab_index < 0) {
      *ctx->result_json = R"({"success":false,"error":"Failed to create tab"})";
    } else {
      std::ostringstream response;
      response << R"({"success":true,"message":"Created tab and navigated to )"
               << ctx->url << R"(","tabIndex":)" << tab_index << "}";
      *ctx->result_json = response.str();
    }
  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    *ctx->result_json = response.str();
  }

  // Signal completion
  g_mutex_lock(&ctx->mutex);
  ctx->done = true;
  g_cond_signal(&ctx->cond);
  g_mutex_unlock(&ctx->mutex);

  return G_SOURCE_REMOVE;
}

static gboolean get_active_tab_idle(gpointer data) {
  auto* ctx = static_cast<GetActiveTabContext*>(data);

  try {
    // NOW on main thread - safe to call GTK methods
    auto* tab = ctx->window->GetActiveTab();
    if (!tab) {
      *ctx->result_json = R"({"success":false,"error":"No active tab"})";
    } else {
      std::ostringstream response;
      response << R"({"success":true,"url":")" << tab->url << R"("})";
      *ctx->result_json = response.str();
    }
  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    *ctx->result_json = response.str();
  }

  // Signal completion
  g_mutex_lock(&ctx->mutex);
  ctx->done = true;
  g_cond_signal(&ctx->cond);
  g_mutex_unlock(&ctx->mutex);

  return G_SOURCE_REMOVE;
}

static gboolean get_tab_count_idle(gpointer data) {
  auto* ctx = static_cast<GetTabCountContext*>(data);

  try {
    // NOW on main thread - safe to call GTK methods
    size_t count = ctx->window->GetTabCount();
    std::ostringstream response;
    response << R"({"success":true,"count":)" << count << "}";
    *ctx->result_json = response.str();
  } catch (const std::exception& e) {
    std::ostringstream response;
    response << R"({"success":false,"error":")" << e.what() << R"("})";
    *ctx->result_json = response.str();
  }

  // Signal completion
  g_mutex_lock(&ctx->mutex);
  ctx->done = true;
  g_cond_signal(&ctx->cond);
  g_mutex_unlock(&ctx->mutex);

  return G_SOURCE_REMOVE;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

BrowserControlServer::BrowserControlServer(
    const BrowserControlServerConfig& config)
    : config_(config),
      window_(nullptr),
      server_fd_(-1),
      server_watch_id_(0),
      thread_pool_(nullptr),
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

  // Create thread pool for async request handling
  // Use 4 worker threads, non-exclusive mode
  GError* error = nullptr;
  thread_pool_ = g_thread_pool_new(
      handle_request_worker,  // Worker function
      this,                   // User data passed to worker
      4,                      // Max threads
      FALSE,                  // Non-exclusive
      &error);

  if (error) {
    logger.Error("Failed to create thread pool: " + std::string(error->message));
    g_error_free(error);
    close(server_fd_);
    server_fd_ = -1;
    std::filesystem::remove(config_.socket_path);
    return utils::Error("Failed to create thread pool");
  }

  running_ = true;

  logger.Info("Browser control server listening with thread pool (4 workers)");

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

  // Shutdown thread pool (wait for pending tasks to complete)
  if (thread_pool_) {
    logger.Debug("Waiting for thread pool to finish pending requests...");
    g_thread_pool_free(thread_pool_, FALSE, TRUE);  // FALSE = wait for completion
    thread_pool_ = nullptr;
    logger.Debug("Thread pool shut down");
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
  // Accept connection (fast - runs on GTK main thread)
  int client_fd = accept(server_fd_, nullptr, nullptr);
  if (client_fd < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return true;  // No connections pending
    }
    logger.Error("Accept failed");
    return false;
  }

  logger.Debug("Client connected, offloading to thread pool");

  // Offload request handling to background thread pool
  // This prevents blocking the GTK main thread during HTTP processing
  auto* context = new RequestContext{this, client_fd};

  GError* error = nullptr;
  g_thread_pool_push(thread_pool_, context, &error);

  if (error) {
    logger.Error("Failed to push request to thread pool: " +
                 std::string(error->message));
    g_error_free(error);
    close(client_fd);
    delete context;
    return false;
  }

  return true;
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

    // Check request size limit to prevent DoS attacks
    if (request.size() > MAX_REQUEST_SIZE) {
      logger.Error("Request size exceeds maximum allowed (" +
                   std::to_string(MAX_REQUEST_SIZE) + " bytes)");
      std::string error_response = BuildHttpResponse(413, "Payload Too Large",
                                                     R"({"success":false,"error":"Request too large"})");
      send(client_fd, error_response.c_str(), error_response.size(), 0);
      return false;
    }

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
    // Parse JSON body using nlohmann/json
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
  logger.Info("Opening URL (from background thread)");

  // Check if server is still running and window is available
  if (!running_ || !window_) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  // THREAD SAFETY: We're in a background thread, so marshal to main thread

  // First, check tab count to decide whether to create tab or navigate
  GetTabCountContext count_ctx = {
    window_,
    new std::string(),
    {},  // mutex - will be initialized below
    {},  // cond - will be initialized below
    false
  };
  g_mutex_init(&count_ctx.mutex);
  g_cond_init(&count_ctx.cond);

  guint source_id = g_idle_add(get_tab_count_idle, &count_ctx);
  if (source_id == 0) {
    // Main loop is shutting down or failed
    logger.Error("Failed to schedule get_tab_count on main thread");
    delete count_ctx.result_json;
    g_mutex_clear(&count_ctx.mutex);
    g_cond_clear(&count_ctx.cond);
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  // Wait for completion
  g_mutex_lock(&count_ctx.mutex);
  while (!count_ctx.done) {
    g_cond_wait(&count_ctx.cond, &count_ctx.mutex);
  }
  g_mutex_unlock(&count_ctx.mutex);

  std::string count_result = *count_ctx.result_json;
  delete count_ctx.result_json;
  g_mutex_clear(&count_ctx.mutex);
  g_cond_clear(&count_ctx.cond);

  // Parse count from JSON using nlohmann/json
  size_t tab_count = 0;
  try {
    auto json = nlohmann::json::parse(count_result);
    if (json.contains("count")) {
      tab_count = json["count"].get<size_t>();
    }
  } catch (const nlohmann::json::exception& e) {
    logger.Error("Failed to parse tab count: " + std::string(e.what()));
    return R"({"success":false,"error":"Failed to get tab count"})";
  }

  // Now either create tab or navigate based on count
  std::string result;

  if (tab_count == 0) {
    // Create new tab (marshal to main thread)
    int tab_index = -1;
    CreateTabContext create_ctx = {
      window_,
      url,
      &tab_index,
      new std::string(),
      {},  // mutex - will be initialized below
      {},  // cond - will be initialized below
      false
    };
    g_mutex_init(&create_ctx.mutex);
    g_cond_init(&create_ctx.cond);

    source_id = g_idle_add(create_tab_idle, &create_ctx);
    if (source_id == 0) {
      logger.Error("Failed to schedule create_tab on main thread");
      delete create_ctx.result_json;
      g_mutex_clear(&create_ctx.mutex);
      g_cond_clear(&create_ctx.cond);
      return R"({"success":false,"error":"Server is shutting down"})";
    }

    // Wait for completion
    g_mutex_lock(&create_ctx.mutex);
    while (!create_ctx.done) {
      g_cond_wait(&create_ctx.cond, &create_ctx.mutex);
    }
    g_mutex_unlock(&create_ctx.mutex);

    result = *create_ctx.result_json;
    delete create_ctx.result_json;
    g_mutex_clear(&create_ctx.mutex);
    g_cond_clear(&create_ctx.cond);

  } else {
    // Navigate active tab (marshal to main thread)
    LoadURLContext load_ctx = {
      window_,
      url,
      new std::string(),
      {},  // mutex - will be initialized below
      {},  // cond - will be initialized below
      false
    };
    g_mutex_init(&load_ctx.mutex);
    g_cond_init(&load_ctx.cond);

    source_id = g_idle_add(load_url_idle, &load_ctx);
    if (source_id == 0) {
      logger.Error("Failed to schedule load_url on main thread");
      delete load_ctx.result_json;
      g_mutex_clear(&load_ctx.mutex);
      g_cond_clear(&load_ctx.cond);
      return R"({"success":false,"error":"Server is shutting down"})";
    }

    // Wait for completion
    g_mutex_lock(&load_ctx.mutex);
    while (!load_ctx.done) {
      g_cond_wait(&load_ctx.cond, &load_ctx.mutex);
    }
    g_mutex_unlock(&load_ctx.mutex);

    result = *load_ctx.result_json;
    delete load_ctx.result_json;
    g_mutex_clear(&load_ctx.mutex);
    g_cond_clear(&load_ctx.cond);
  }

  return result;
}

std::string BrowserControlServer::HandleGetUrl() {
  // Check if server is still running and window is available
  if (!running_ || !window_) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  // THREAD SAFETY: We're in a background thread, so marshal to main thread
  GetActiveTabContext ctx = {
    window_,
    new std::string(),
    {},  // mutex - will be initialized below
    {},  // cond - will be initialized below
    false
  };
  g_mutex_init(&ctx.mutex);
  g_cond_init(&ctx.cond);

  guint source_id = g_idle_add(get_active_tab_idle, &ctx);
  if (source_id == 0) {
    logger.Error("Failed to schedule get_active_tab on main thread");
    delete ctx.result_json;
    g_mutex_clear(&ctx.mutex);
    g_cond_clear(&ctx.cond);
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  // Wait for completion
  g_mutex_lock(&ctx.mutex);
  while (!ctx.done) {
    g_cond_wait(&ctx.cond, &ctx.mutex);
  }
  g_mutex_unlock(&ctx.mutex);

  std::string result = *ctx.result_json;
  delete ctx.result_json;
  g_mutex_clear(&ctx.mutex);
  g_cond_clear(&ctx.cond);

  return result;
}

std::string BrowserControlServer::HandleGetTabCount() {
  // Check if server is still running and window is available
  if (!running_ || !window_) {
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  // THREAD SAFETY: We're in a background thread, so marshal to main thread
  GetTabCountContext ctx = {
    window_,
    new std::string(),
    {},  // mutex - will be initialized below
    {},  // cond - will be initialized below
    false
  };
  g_mutex_init(&ctx.mutex);
  g_cond_init(&ctx.cond);

  guint source_id = g_idle_add(get_tab_count_idle, &ctx);
  if (source_id == 0) {
    logger.Error("Failed to schedule get_tab_count on main thread");
    delete ctx.result_json;
    g_mutex_clear(&ctx.mutex);
    g_cond_clear(&ctx.cond);
    return R"({"success":false,"error":"Server is shutting down"})";
  }

  // Wait for completion
  g_mutex_lock(&ctx.mutex);
  while (!ctx.done) {
    g_cond_wait(&ctx.cond, &ctx.mutex);
  }
  g_mutex_unlock(&ctx.mutex);

  std::string result = *ctx.result_json;
  delete ctx.result_json;
  g_mutex_clear(&ctx.mutex);
  g_cond_clear(&ctx.cond);

  return result;
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

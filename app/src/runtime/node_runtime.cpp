#include "runtime/node_runtime.h"
#include "utils/logging.h"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <thread>
#include <filesystem>

namespace athena {
namespace runtime {

// Static logger for this module
static utils::Logger logger("NodeRuntime");

// ============================================================================
// Constructor / Destructor
// ============================================================================

NodeRuntime::NodeRuntime(const NodeRuntimeConfig& config)
    : config_(config),
      pid_(-1),
      state_(RuntimeState::STOPPED),
      health_monitoring_enabled_(false),
      restart_attempts_(0) {
  logger.Debug("NodeRuntime::NodeRuntime - Creating runtime");

  // Auto-generate socket path if not provided
  if (config_.socket_path.empty()) {
    std::ostringstream oss;
    oss << "/tmp/athena-" << getuid() << ".sock";
    config_.socket_path = oss.str();
  }
}

NodeRuntime::~NodeRuntime() {
  logger.Debug("NodeRuntime::~NodeRuntime - Destroying runtime");
  Shutdown();
}

// ============================================================================
// Lifecycle Management
// ============================================================================

utils::Result<void> NodeRuntime::Initialize() {
  logger.Debug("NodeRuntime::Initialize - Initializing runtime");

  if (state_ != RuntimeState::STOPPED) {
    return utils::Error("Runtime already initialized");
  }

  // Validate configuration
  if (config_.runtime_script_path.empty()) {
    return utils::Error("Runtime script path not specified");
  }

  if (access(config_.runtime_script_path.c_str(), R_OK) != 0) {
    return utils::Error("Runtime script not found or not readable: " +
                       config_.runtime_script_path);
  }

  // Clean up stale socket file BEFORE spawning
  if (std::filesystem::exists(config_.socket_path)) {
    logger.Warn("NodeRuntime::Initialize - Removing stale socket file: " + config_.socket_path);
    try {
      std::filesystem::remove(config_.socket_path);
      logger.Debug("NodeRuntime::Initialize - Stale socket removed successfully");
    } catch (const std::filesystem::filesystem_error& e) {
      logger.Warn("NodeRuntime::Initialize - Failed to remove stale socket: " + std::string(e.what()));
      // Continue anyway - socket might be in use by another instance
    }
  }

  // Spawn the Node process
  auto spawn_result = SpawnProcess();
  if (!spawn_result) {
    return spawn_result;
  }

  // Wait for READY signal
  auto ready_result = WaitForReady();
  if (!ready_result) {
    TerminateProcess(true);
    return ready_result;
  }

  // Verify socket was created
  if (!std::filesystem::exists(socket_path_)) {
    TerminateProcess(true);
    return utils::Error("Socket file was not created: " + socket_path_);
  }

  state_ = RuntimeState::READY;
  logger.Info("NodeRuntime initialized successfully");

  return utils::Ok();
}

void NodeRuntime::Shutdown() {
  if (state_ == RuntimeState::STOPPED) {
    return;
  }

  logger.Info("NodeRuntime::Shutdown - Shutting down runtime");

  // Stop health monitoring
  StopHealthMonitoring();

  // Terminate process (graceful first, then force if needed)
  TerminateProcess(false);

  // Clean up socket file (even if Node didn't clean it up)
  if (!socket_path_.empty() && std::filesystem::exists(socket_path_)) {
    logger.Debug("NodeRuntime::Shutdown - Removing socket file: " + socket_path_);
    try {
      std::filesystem::remove(socket_path_);
      logger.Debug("NodeRuntime::Shutdown - Socket file removed successfully");
    } catch (const std::filesystem::filesystem_error& e) {
      logger.Warn("NodeRuntime::Shutdown - Failed to remove socket: " + std::string(e.what()));
    }
  }

  // Reset state
  state_ = RuntimeState::STOPPED;
  pid_ = -1;
  socket_path_.clear();

  logger.Info("NodeRuntime shutdown complete");
}

bool NodeRuntime::IsReady() const {
  return state_ == RuntimeState::READY;
}

RuntimeState NodeRuntime::GetState() const {
  return state_;
}

// ============================================================================
// Health Monitoring
// ============================================================================

utils::Result<HealthStatus> NodeRuntime::CheckHealth() {
  if (!IsProcessAlive()) {
    return utils::Error("Process not running");
  }

  // Call /health endpoint
  auto response = Call("GET", "/health");
  if (!response) {
    return utils::Error("Health check failed: " + response.GetError().Message());
  }

  // Parse response (simplified - in production use a JSON parser)
  HealthStatus status;
  status.healthy = response.Value().find("\"status\":\"healthy\"") != std::string::npos;
  status.ready = response.Value().find("\"ready\":true") != std::string::npos;

  // Extract uptime (basic string search - use JSON parser in production)
  size_t uptime_pos = response.Value().find("\"uptime\":");
  if (uptime_pos != std::string::npos) {
    std::istringstream iss(response.Value().substr(uptime_pos + 9));
    iss >> status.uptime_ms;
  }

  return utils::Ok(std::move(status));
}

void NodeRuntime::StartHealthMonitoring() {
  health_monitoring_enabled_ = true;
  logger.Debug("NodeRuntime - Health monitoring started");

  // Note: In production, implement proper async health checks
  // For now, health checks are synchronous on-demand
}

void NodeRuntime::StopHealthMonitoring() {
  health_monitoring_enabled_ = false;
  logger.Debug("NodeRuntime - Health monitoring stopped");
}

// ============================================================================
// IPC Interface
// ============================================================================

utils::Result<std::string> NodeRuntime::Call(const std::string& method,
                                              const std::string& path,
                                              const std::string& body,
                                              const std::string& request_id) {
  if (state_ != RuntimeState::READY) {
    return utils::Error("Runtime not ready");
  }

  if (socket_path_.empty()) {
    return utils::Error("Socket path not set");
  }

  // Create Unix socket
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    return utils::Error("Failed to create socket: " + std::string(strerror(errno)));
  }

  // Connect to Unix socket
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

  if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(sock);
    return utils::Error("Failed to connect to socket: " + std::string(strerror(errno)));
  }

  // Build HTTP request
  std::ostringstream request;
  request << method << " " << path << " HTTP/1.1\r\n";
  request << "Host: localhost\r\n";
  request << "User-Agent: Athena-Browser/1.0\r\n";

  if (!request_id.empty()) {
    request << "X-Request-Id: " << request_id << "\r\n";
  }

  if (!body.empty()) {
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << body.size() << "\r\n";
  }

  request << "\r\n";

  if (!body.empty()) {
    request << body;
  }

  std::string request_str = request.str();

  logger.Debug("NodeRuntime::Call - Sending HTTP request:\n" + request_str);

  // Send request
  ssize_t sent = send(sock, request_str.c_str(), request_str.size(), 0);
  if (sent < 0) {
    close(sock);
    return utils::Error("Failed to send request: " + std::string(strerror(errno)));
  }

  logger.Debug("NodeRuntime::Call - Sent " + std::to_string(sent) + " bytes to " + path);

  // Receive response
  std::string response;
  char buffer[4096];
  ssize_t received;
  int read_attempts = 0;

  // First, read until we have the headers
  while ((received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
    buffer[received] = '\0';
    response += buffer;
    read_attempts++;

    logger.Debug("NodeRuntime::Call - Received " + std::to_string(received) +
                 " bytes (attempt " + std::to_string(read_attempts) + ")");

    // Check if we got the headers (end with \r\n\r\n)
    if (response.find("\r\n\r\n") != std::string::npos) {
      break;
    }
  }

  if (received <= 0) {
    close(sock);
    std::string error_msg = "Failed to receive response headers after " +
                           std::to_string(read_attempts) + " attempts";
    if (received < 0) {
      error_msg += ": " + std::string(strerror(errno));
    } else {
      error_msg += ": connection closed by server";
    }
    logger.Error("NodeRuntime::Call - " + error_msg);
    return utils::Error(error_msg);
  }

  logger.Debug("NodeRuntime::Call - Received full headers (" + std::to_string(response.length()) + " bytes)");

  // Parse Content-Length from headers
  size_t content_length = 0;
  size_t cl_pos = response.find("Content-Length:");
  if (cl_pos != std::string::npos) {
    size_t cl_start = cl_pos + 15;  // Length of "Content-Length:"
    size_t cl_end = response.find("\r\n", cl_start);
    if (cl_end != std::string::npos) {
      std::string cl_str = response.substr(cl_start, cl_end - cl_start);
      // Trim whitespace
      size_t first = cl_str.find_first_not_of(" \t");
      size_t last = cl_str.find_last_not_of(" \t");
      if (first != std::string::npos && last != std::string::npos) {
        cl_str = cl_str.substr(first, last - first + 1);
        content_length = std::stoull(cl_str);
      }
    }
  }

  // Find where the body starts
  size_t body_start = response.find("\r\n\r\n");
  if (body_start == std::string::npos) {
    close(sock);
    return utils::Error("Invalid HTTP response: no header/body separator");
  }
  body_start += 4;  // Skip past "\r\n\r\n"

  // Calculate how much body we already have
  size_t body_received = response.length() - body_start;

  // Continue reading until we have the full body
  if (content_length > 0) {
    while (body_received < content_length) {
      received = recv(sock, buffer, sizeof(buffer) - 1, 0);
      if (received <= 0) {
        break;
      }
      buffer[received] = '\0';
      response += buffer;
      body_received += received;
    }
  }

  close(sock);

  // Extract body from response
  std::string response_body = response.substr(body_start);

  return utils::Ok(std::move(response_body));
}

// ============================================================================
// Accessors
// ============================================================================

std::string NodeRuntime::GetSocketPath() const {
  return socket_path_;
}

int NodeRuntime::GetPid() const {
  return pid_;
}

const NodeRuntimeConfig& NodeRuntime::GetConfig() const {
  return config_;
}

// ============================================================================
// Private Methods
// ============================================================================

utils::Result<void> NodeRuntime::SpawnProcess() {
  logger.Debug("NodeRuntime::SpawnProcess - Spawning Node process");

  // Create pipe for reading stdout (READY line)
  int stdout_pipe[2];
  if (pipe(stdout_pipe) < 0) {
    return utils::Error("Failed to create pipe: " + std::string(strerror(errno)));
  }

  pid_ = fork();

  if (pid_ < 0) {
    // Fork failed
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return utils::Error("Failed to fork: " + std::string(strerror(errno)));
  }

  if (pid_ == 0) {
    // Child process

    // Close read end
    close(stdout_pipe[0]);

    // Redirect stdout to pipe (only for READY signal)
    dup2(stdout_pipe[1], STDOUT_FILENO);
    close(stdout_pipe[1]);

    // Important: Do NOT redirect stderr to stdout!
    // Keep stderr going to the terminal so Winston logs don't need the pipe.
    // dup2(STDOUT_FILENO, STDERR_FILENO);  // REMOVED

    // Ignore SIGPIPE - when parent closes pipe, writes to stdout should not crash
    // Node.js will just get EPIPE error which it can handle gracefully
    signal(SIGPIPE, SIG_IGN);

    // Set environment variable for socket path
    setenv("ATHENA_SOCKET_PATH", config_.socket_path.c_str(), 1);

    // Execute Node
    execlp(config_.node_executable.c_str(),
           config_.node_executable.c_str(),
           config_.runtime_script_path.c_str(),
           nullptr);

    // If we get here, exec failed
    std::cerr << "Failed to exec Node: " << strerror(errno) << std::endl;
    _exit(1);
  }

  // Parent process

  // Close write end
  close(stdout_pipe[1]);

  // Set pipe to non-blocking mode for reading READY line
  int flags = fcntl(stdout_pipe[0], F_GETFL, 0);
  fcntl(stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

  // Read READY line (may need multiple reads due to JSON logging)
  // The athena-agent outputs JSON logs before the READY line, so we need to
  // keep reading until we find it. Node.js startup takes ~2 seconds.
  std::string output;
  char buffer[4096];
  int attempts = 0;
  const int max_attempts = 60;  // 60 * 100ms = 6 seconds max wait

  while (attempts < max_attempts && output.find("READY ") == std::string::npos) {
    ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer) - 1);
    if (n > 0) {
      buffer[n] = '\0';
      output += buffer;
      // Reset attempts when we get data
      attempts = 0;
    } else if (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      // No data available yet, wait a bit
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      attempts++;
    } else if (n == 0) {
      // EOF - process closed stdout
      break;
    } else {
      // Real error
      close(stdout_pipe[0]);
      TerminateProcess(true);
      return utils::Error("Failed to read from child process: " + std::string(strerror(errno)));
    }
  }

  logger.Debug("NodeRuntime - Total output length: " + std::to_string(output.length()) +
               ", attempts: " + std::to_string(attempts));

  // Don't close the pipe! The Node process will continue writing logs.
  // If we close it, the process gets SIGPIPE and dies.
  // Instead, we'll just stop reading from it.
  // TODO: In production, redirect to a log file or implement async log reading

  // Close our end of the pipe to avoid leaking file descriptors
  // The child process will keep its stdout open
  close(stdout_pipe[0]);

  // Parse READY line
  size_t ready_pos = output.find("READY ");
  if (ready_pos == std::string::npos) {
    TerminateProcess(true);
    return utils::Error("Failed to receive READY signal from Node process");
  }

  // Extract socket path
  size_t path_start = ready_pos + 6;  // Length of "READY "
  size_t path_end = output.find('\n', path_start);
  if (path_end == std::string::npos) {
    path_end = output.size();
  }

  socket_path_ = output.substr(path_start, path_end - path_start);

  // Trim whitespace
  socket_path_.erase(0, socket_path_.find_first_not_of(" \t\r\n"));
  socket_path_.erase(socket_path_.find_last_not_of(" \t\r\n") + 1);

  logger.Debug("NodeRuntime - Process spawned: pid=" + std::to_string(pid_) +
               ", socket=" + socket_path_);

  state_ = RuntimeState::STARTING;

  return utils::Ok();
}

utils::Result<std::string> NodeRuntime::ReadReadyLine() {
  // Already handled in SpawnProcess() for simplicity
  return utils::Ok(std::string(socket_path_));
}

utils::Result<void> NodeRuntime::WaitForReady() {
  logger.Debug("NodeRuntime::WaitForReady - Waiting for runtime to be ready");

  // Give Node a moment to fully initialize
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Check if process is alive
  if (!IsProcessAlive()) {
    return utils::Error("Process died during startup");
  }

  // Try to connect to socket (basic readiness check)
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    return utils::Error("Failed to create socket for readiness check");
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

  // Retry connection with timeout
  int max_retries = 10;
  bool connected = false;

  for (int i = 0; i < max_retries; i++) {
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
      connected = true;
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  close(sock);

  if (!connected) {
    return utils::Error("Failed to connect to Node runtime socket");
  }

  logger.Debug("NodeRuntime - Runtime is ready");

  return utils::Ok();
}

void NodeRuntime::TerminateProcess(bool force) {
  if (pid_ <= 0) {
    return;
  }

  logger.Debug("NodeRuntime::TerminateProcess - pid=" + std::to_string(pid_) +
               ", force=" + (force ? "true" : "false"));

  if (force) {
    // SIGKILL immediately
    kill(pid_, SIGKILL);
    waitpid(pid_, nullptr, 0);
  } else {
    // SIGTERM with grace period
    kill(pid_, SIGTERM);

    // Wait up to 2 seconds
    int max_wait_ms = 2000;
    int interval_ms = 100;
    int waited_ms = 0;

    while (waited_ms < max_wait_ms) {
      int status;
      pid_t result = waitpid(pid_, &status, WNOHANG);

      if (result > 0) {
        // Process exited
        logger.Debug("NodeRuntime - Process exited gracefully");
        return;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
      waited_ms += interval_ms;
    }

    // Force kill after timeout
    logger.Warn("NodeRuntime - Process didn't exit gracefully, forcing SIGKILL");
    kill(pid_, SIGKILL);
    waitpid(pid_, nullptr, 0);
  }
}

bool NodeRuntime::IsProcessAlive() const {
  if (pid_ <= 0) {
    return false;
  }

  // Send signal 0 to check if process exists
  return kill(pid_, 0) == 0;
}

void NodeRuntime::HandleCrash() {
  logger.Error("NodeRuntime::HandleCrash - Process crashed");

  state_ = RuntimeState::CRASHED;

  // Attempt restart if within limits
  if (restart_attempts_ < config_.restart_max_attempts) {
    logger.Info("NodeRuntime - Attempting restart: attempt=" +
                std::to_string(restart_attempts_ + 1) + "/" +
                std::to_string(config_.restart_max_attempts));

    auto result = Restart();
    if (!result) {
      logger.Error("NodeRuntime - Restart failed: " + result.GetError().Message());
    }
  } else {
    logger.Error("NodeRuntime - Max restart attempts reached, giving up");
  }
}

utils::Result<void> NodeRuntime::Restart() {
  restart_attempts_++;

  // Calculate backoff
  int backoff_ms = CalculateBackoff();
  logger.Debug("NodeRuntime - Waiting backoff before restart: " +
               std::to_string(backoff_ms) + "ms");

  std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));

  // Clean up old process
  pid_ = -1;
  socket_path_.clear();
  state_ = RuntimeState::STOPPED;

  // Reinitialize
  return Initialize();
}

int NodeRuntime::CalculateBackoff() const {
  // Exponential backoff: initial * 2^attempts
  int backoff = config_.restart_backoff_ms;
  for (int i = 0; i < restart_attempts_; i++) {
    backoff *= 2;
  }

  // Cap at 10 seconds
  return std::min(backoff, 10000);
}

}  // namespace runtime
}  // namespace athena

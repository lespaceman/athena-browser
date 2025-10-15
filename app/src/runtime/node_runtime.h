#ifndef ATHENA_RUNTIME_NODE_RUNTIME_H_
#define ATHENA_RUNTIME_NODE_RUNTIME_H_

#include <string>
#include <memory>
#include <chrono>
#include "utils/error.h"

namespace athena {
namespace runtime {

/**
 * Configuration for Node.js runtime.
 */
struct NodeRuntimeConfig {
  std::string node_executable = "node";
  std::string runtime_script_path;  // Path to server.js
  std::string socket_path;  // Unix socket path (auto-generated if empty)
  int startup_timeout_ms = 5000;
  int health_check_interval_ms = 10000;
  int restart_max_attempts = 3;
  int restart_backoff_ms = 100;  // Initial backoff, doubles each retry
};

/**
 * Process state for tracking Node runtime lifecycle.
 */
enum class RuntimeState {
  STOPPED,      // Not started
  STARTING,     // Spawning process, waiting for READY
  READY,        // Running and healthy
  UNHEALTHY,    // Process alive but health check failing
  CRASHED       // Process died unexpectedly
};

/**
 * Health status from /health endpoint.
 */
struct HealthStatus {
  bool healthy = false;
  bool ready = false;
  int uptime_ms = 0;
  int request_count = 0;
  std::string version;
};

/**
 * NodeRuntime manages a separate Node.js process for providing Node-grade
 * capabilities to the GTK application.
 *
 * Architecture:
 *   - Process management: spawn, supervise, restart on crash
 *   - IPC: HTTP over Unix domain socket
 *   - Lifecycle: starts on Initialize(), stops on Shutdown()
 *   - Health monitoring: periodic checks with /health endpoint
 *   - Graceful shutdown: SIGTERM → wait → SIGKILL fallback
 *
 * Responsibilities:
 *   - Spawn Node.js helper process
 *   - Parse READY line to get socket path
 *   - Monitor process health
 *   - Restart on crash with exponential backoff
 *   - Provide IPC interface to application
 *   - Clean shutdown on application exit
 *
 * Thread safety:
 *   - Not thread-safe; must be called from main thread
 *   - Health checks run in background but callback on main thread
 *
 * Example:
 * ```cpp
 * NodeRuntimeConfig config;
 * config.runtime_script_path = "/path/to/node-runtime/server.js";
 *
 * auto runtime = std::make_unique<NodeRuntime>(config);
 * if (auto result = runtime->Initialize(); !result) {
 *   std::cerr << "Failed: " << result.GetError().Message() << std::endl;
 * }
 *
 * // Make API calls
 * auto response = runtime->Call("POST", "/v1/echo", R"({"message":"hello"})");
 *
 * runtime->Shutdown();
 * ```
 */
class NodeRuntime {
 public:
  /**
   * Create a Node runtime manager.
   *
   * @param config Runtime configuration
   */
  explicit NodeRuntime(const NodeRuntimeConfig& config);

  /**
   * Destructor - performs clean shutdown.
   */
  ~NodeRuntime();

  // Non-copyable, movable
  NodeRuntime(const NodeRuntime&) = delete;
  NodeRuntime& operator=(const NodeRuntime&) = delete;
  NodeRuntime(NodeRuntime&&) = default;
  NodeRuntime& operator=(NodeRuntime&&) = default;

  // ============================================================================
  // Lifecycle Management
  // ============================================================================

  /**
   * Initialize and start the Node.js runtime.
   * Spawns the process and waits for READY signal.
   *
   * @return Ok on success, error on failure
   */
  utils::Result<void> Initialize();

  /**
   * Shutdown the runtime.
   * Sends SIGTERM, waits for graceful exit, falls back to SIGKILL.
   */
  void Shutdown();

  /**
   * Check if runtime is initialized and ready.
   */
  bool IsReady() const;

  /**
   * Get current runtime state.
   */
  RuntimeState GetState() const;

  // ============================================================================
  // Health Monitoring
  // ============================================================================

  /**
   * Check health of Node runtime.
   * Calls /health endpoint and returns status.
   *
   * @return Health status on success, error on failure
   */
  utils::Result<HealthStatus> CheckHealth();

  /**
   * Start periodic health checks in background.
   * Automatically restarts on failure if configured.
   */
  void StartHealthMonitoring();

  /**
   * Stop periodic health checks.
   */
  void StopHealthMonitoring();

  // ============================================================================
  // IPC Interface
  // ============================================================================

  /**
   * Make an HTTP call to the Node runtime.
   *
   * @param method HTTP method (GET, POST, etc.)
   * @param path Endpoint path (e.g., "/v1/echo")
   * @param body Request body (empty for GET)
   * @param request_id Optional request ID for tracing
   * @return Response body on success, error on failure
   */
  utils::Result<std::string> Call(const std::string& method,
                                   const std::string& path,
                                   const std::string& body = "",
                                   const std::string& request_id = "");

  // ============================================================================
  // Accessors
  // ============================================================================

  /**
   * Get the socket path for IPC.
   */
  std::string GetSocketPath() const;

  /**
   * Get the Node process PID.
   * Returns -1 if not running.
   */
  int GetPid() const;

  /**
   * Get configuration.
   */
  const NodeRuntimeConfig& GetConfig() const;

  /**
   * Check if the Node process is alive.
   * Returns true if process exists, false otherwise.
   */
  bool IsProcessAlive() const;

  /**
   * Handle runtime crash (called by health monitoring).
   * Attempts automatic restart with exponential backoff.
   */
  void HandleCrash();

 private:
  // Configuration
  NodeRuntimeConfig config_;

  // Process management
  int pid_;
  RuntimeState state_;
  std::string socket_path_;

  // Health monitoring
  bool health_monitoring_enabled_;
  void* health_check_timer_handle_;  // Platform-specific timer handle (GLib source ID or QTimer*)
  std::chrono::steady_clock::time_point last_health_check_;

  // Restart tracking
  int restart_attempts_;
  std::chrono::steady_clock::time_point last_restart_time_;

  // Internal methods
  utils::Result<void> SpawnProcess();
  utils::Result<std::string> ReadReadyLine();
  utils::Result<void> WaitForReady();
  void TerminateProcess(bool force);
  utils::Result<void> Restart();
  int CalculateBackoff() const;
};

}  // namespace runtime
}  // namespace athena

#endif  // ATHENA_RUNTIME_NODE_RUNTIME_H_

#ifndef ATHENA_CORE_APPLICATION_H_
#define ATHENA_CORE_APPLICATION_H_

#include "browser/browser_engine.h"
#include "core/browser_window.h"
#include "platform/window_system.h"
#include "runtime/browser_control_server.h"
#include "runtime/node_runtime.h"
#include "utils/error.h"

#include <memory>
#include <string>
#include <vector>

namespace athena {
namespace core {

/**
 * Configuration for the Athena application.
 */
struct ApplicationConfig {
  std::string cache_path = "/tmp/athena_browser_cache";
  std::string subprocess_path;  // Auto-detected if empty
  bool enable_sandbox = false;
  bool enable_windowless_rendering = true;
  int windowless_frame_rate = 60;
  bool enable_node_runtime = true;
  std::string node_runtime_script_path;  // Path to agent/dist/server/server.js
};

/**
 * High-level application controller for Athena Browser.
 *
 * Application manages the entire lifecycle of the browser application,
 * coordinating between the platform layer (window management) and the
 * browser engine (web content rendering).
 *
 * Responsibilities:
 *   - Initialize and manage the browser engine
 *   - Initialize and manage the window system
 *   - Create and track browser windows
 *   - Manage application lifecycle (init, run, shutdown)
 *   - Coordinate between platform and engine layers
 *
 * Lifecycle:
 *   1. Create Application with config
 *   2. Initialize() - set up engine and window system
 *   3. CreateWindow() - create browser windows
 *   4. Run() - enter main event loop
 *   5. Shutdown() or destructor - clean shutdown
 *
 * Example:
 * ```cpp
 * ApplicationConfig config;
 * config.cache_path = "/tmp/my_browser";
 *
 * auto app = std::make_unique<Application>(
 *     config,
 *     std::make_unique<CefEngine>(),
 *     std::make_unique<QtWindowSystem>());
 *
 * if (auto result = app->Initialize(); !result) {
 *   std::cerr << "Init failed: " << result.GetError().Message() << std::endl;
 *   return -1;
 * }
 *
 * BrowserWindowConfig window_config;
 * window_config.url = "https://example.com";
 * auto window = app->CreateWindow(window_config);
 * window->Show();
 *
 * app->Run();
 * ```
 */
class Application {
 public:
  /**
   * Create an application.
   *
   * @param config Application configuration
   * @param browser_engine Browser engine implementation (ownership transferred)
   * @param window_system Window system implementation (ownership transferred)
   * @param node_runtime Node.js runtime (optional, ownership transferred)
   */
  Application(const ApplicationConfig& config,
              std::unique_ptr<browser::BrowserEngine> browser_engine,
              std::unique_ptr<platform::WindowSystem> window_system,
              std::unique_ptr<runtime::NodeRuntime> node_runtime = nullptr);

  /**
   * Destructor - performs clean shutdown.
   */
  ~Application();

  // Non-copyable, movable
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = default;
  Application& operator=(Application&&) = default;

  // ============================================================================
  // Lifecycle Management
  // ============================================================================

  /**
   * Initialize the application.
   * Must be called before any other operations.
   *
   * @param argc Command line argument count (passed by reference for Qt compatibility)
   * @param argv Command line arguments
   * @return Ok on success, error on failure
   */
  utils::Result<void> Initialize(int& argc, char* argv[]);

  /**
   * Initialize the application (no command line args).
   */
  utils::Result<void> Initialize();

  /**
   * Run the main event loop.
   * This is a blocking call that returns when Quit() is called or all windows close.
   */
  void Run();

  /**
   * Quit the event loop.
   * Safe to call from event handlers.
   */
  void Quit();

  /**
   * Shutdown the application.
   * Closes all windows and cleans up resources.
   */
  void Shutdown();

  /**
   * Check if the application is initialized.
   */
  bool IsInitialized() const;

  /**
   * Check if the event loop is running.
   */
  bool IsRunning() const;

  // ============================================================================
  // Window Management
  // ============================================================================

  /**
   * Create a browser window.
   *
   * @param config Window configuration
   * @param callbacks Event callbacks
   * @return Window instance on success, error on failure
   */
  utils::Result<std::unique_ptr<BrowserWindow>> CreateWindow(
      const BrowserWindowConfig& config, const BrowserWindowCallbacks& callbacks = {});

  /**
   * Get the number of open windows.
   */
  size_t GetWindowCount() const;

  /**
   * Close all windows.
   * @param force If true, close immediately without callbacks
   */
  void CloseAllWindows(bool force = false);

  // ============================================================================
  // Accessors
  // ============================================================================

  /**
   * Get the browser engine.
   */
  browser::BrowserEngine* GetBrowserEngine() const;

  /**
   * Get the window system.
   */
  platform::WindowSystem* GetWindowSystem() const;

  /**
   * Get the application configuration.
   */
  const ApplicationConfig& GetConfig() const;

  /**
   * Get the Node runtime (may be null if not enabled).
   */
  runtime::NodeRuntime* GetNodeRuntime() const;

 private:
  // Configuration
  ApplicationConfig config_;

  // Owned resources
  std::unique_ptr<browser::BrowserEngine> browser_engine_;
  std::unique_ptr<platform::WindowSystem> window_system_;
  std::unique_ptr<runtime::NodeRuntime> node_runtime_;
  std::unique_ptr<runtime::BrowserControlServer> browser_control_server_;

  // Window tracking (weak pointers - windows are owned by callers)
  std::vector<BrowserWindow*> windows_;

  // State
  bool initialized_;
  bool shutdown_requested_;

  // Internal helpers
  void SetupDefaultCallbacks(BrowserWindowCallbacks& callbacks);
  void OnWindowDestroyed(BrowserWindow* window);

  // Runtime lifecycle helpers
  utils::Result<void> InitializeRuntime();
  void ShutdownRuntime();

  // Browser control server lifecycle helpers
  utils::Result<void> InitializeBrowserControlServer();
  void ShutdownBrowserControlServer();
};

}  // namespace core
}  // namespace athena

#endif  // ATHENA_CORE_APPLICATION_H_

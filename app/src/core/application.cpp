#include "core/application.h"

#include "platform/qt_mainwindow.h"
#include "utils/logging.h"

#include <algorithm>
#include <iostream>
#include <limits.h>
#include <unistd.h>

namespace athena {
namespace core {

// Static logger for this module
static utils::Logger logger("Application");

// Get executable path for CEF subprocess
static std::string GetExecutablePath() {
  char path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len != -1) {
    path[len] = '\0';
    return std::string(path);
  }
  return "";
}

Application::Application(const ApplicationConfig& config,
                         std::unique_ptr<browser::BrowserEngine> browser_engine,
                         std::unique_ptr<platform::WindowSystem> window_system,
                         std::unique_ptr<runtime::NodeRuntime> node_runtime)
    : config_(config),
      browser_engine_(std::move(browser_engine)),
      window_system_(std::move(window_system)),
      node_runtime_(std::move(node_runtime)),
      initialized_(false),
      shutdown_requested_(false) {
  logger.Debug("Application::Application - Creating application");

  // Auto-detect subprocess path if not provided
  if (config_.subprocess_path.empty()) {
    config_.subprocess_path = GetExecutablePath();
  }
}

Application::~Application() {
  logger.Debug("Application::~Application - Destroying application");
  Shutdown();
}

// ============================================================================
// Lifecycle Management
// ============================================================================

utils::Result<void> Application::Initialize(int& argc, char* argv[]) {
  logger.Debug("Application::Initialize - Initializing application");

  if (initialized_) {
    return utils::Error("Application already initialized");
  }

  // Validate dependencies
  if (!browser_engine_) {
    return utils::Error("Browser engine is null");
  }

  if (!window_system_) {
    return utils::Error("Window system is null");
  }

  // Initialize window system first (initializes Qt platform)
  auto window_result = window_system_->Initialize(argc, argv, browser_engine_.get());
  if (!window_result) {
    return utils::Error("Failed to initialize window system: " +
                        window_result.GetError().Message());
  }

  logger.Debug("Application::Initialize - Window system initialized");

  // Initialize browser engine (initializes CEF)
  browser::EngineConfig engine_config;
  engine_config.cache_path = config_.cache_path;
  engine_config.subprocess_path = config_.subprocess_path;
  engine_config.enable_sandbox = config_.enable_sandbox;
  engine_config.enable_windowless_rendering = config_.enable_windowless_rendering;
  engine_config.windowless_frame_rate = config_.windowless_frame_rate;

  auto engine_result = browser_engine_->Initialize(engine_config);
  if (!engine_result) {
    window_system_->Shutdown();
    return utils::Error("Failed to initialize browser engine: " +
                        engine_result.GetError().Message());
  }

  logger.Debug("Application::Initialize - Browser engine initialized");

  // NOTE: Node runtime initialization is deferred until Run() is called.
  // This ensures the runtime starts right before the event loop begins,
  // providing better timing and guaranteed cleanup on exit.

  initialized_ = true;
  logger.Info("Application initialized successfully");

  return utils::Ok();
}

utils::Result<void> Application::Initialize() {
  int argc = 0;
  char** argv = nullptr;
  return Initialize(argc, argv);
}

void Application::Run() {
  if (!initialized_) {
    logger.Error("Application::Run - Application not initialized");
    return;
  }

  // Initialize browser control server FIRST (before Node runtime)
  // This ensures the server is listening before Node tries to connect
  auto server_result = InitializeBrowserControlServer();
  if (!server_result) {
    logger.Warn("Application::Run - Browser control server initialization failed: " +
                server_result.GetError().Message());
    // Continue without server (non-fatal)
  }

  // Initialize Node runtime right before event loop starts
  // Node will connect to browser control server during MCP initialization
  auto runtime_result = InitializeRuntime();
  if (!runtime_result) {
    logger.Warn("Application::Run - Node runtime initialization failed: " +
                runtime_result.GetError().Message());
    // Continue without runtime (non-fatal)
  }

  logger.Info("Application::Run - Entering main event loop");
  window_system_->Run();
  logger.Info("Application::Run - Exited main event loop");

  // Shutdown servers immediately after event loop exits
  ShutdownBrowserControlServer();
  ShutdownRuntime();
}

void Application::Quit() {
  logger.Info("Application::Quit - Quitting application");

  if (window_system_) {
    window_system_->Quit();
  }

  shutdown_requested_ = true;
}

void Application::Shutdown() {
  // Idempotency check: safe to call multiple times (e.g., from signal handler + main cleanup)
  if (!initialized_) {
    return;
  }

  logger.Info("Application::Shutdown - Shutting down application");

  shutdown_requested_ = true;

  // Close all windows
  CloseAllWindows(true);

  // Shutdown servers (if not already done in Run())
  ShutdownBrowserControlServer();
  ShutdownRuntime();

  // Shutdown in reverse order
  if (browser_engine_) {
    browser_engine_->Shutdown();
    logger.Debug("Application::Shutdown - Browser engine shutdown");
  }

  if (window_system_) {
    window_system_->Shutdown();
    logger.Debug("Application::Shutdown - Window system shutdown");
  }

  initialized_ = false;
  logger.Info("Application shutdown complete");
}

bool Application::IsInitialized() const {
  return initialized_;
}

bool Application::IsRunning() const {
  if (!window_system_) {
    return false;
  }
  return window_system_->IsRunning();
}

// ============================================================================
// Window Management
// ============================================================================

utils::Result<std::unique_ptr<BrowserWindow>> Application::CreateWindow(
    const BrowserWindowConfig& config, const BrowserWindowCallbacks& callbacks) {
  if (!initialized_) {
    return utils::Error("Application not initialized");
  }

  logger.Debug("Application::CreateWindow - Creating browser window");

  // Make a copy of config to inject node_runtime
  BrowserWindowConfig window_config = config;
  if (node_runtime_) {
    window_config.node_runtime = node_runtime_.get();
  }

  // Make a copy of callbacks to add our own handlers
  BrowserWindowCallbacks window_callbacks = callbacks;
  SetupDefaultCallbacks(window_callbacks);

  // Create the browser window
  auto window = std::make_unique<BrowserWindow>(
      window_config, window_callbacks, window_system_.get(), browser_engine_.get());

  // Track the window (weak pointer)
  windows_.push_back(window.get());

  logger.Debug("Application::CreateWindow - Window created successfully");

  return std::unique_ptr<BrowserWindow>(std::move(window));
}

size_t Application::GetWindowCount() const {
  // Remove closed windows from tracking
  auto& mutable_windows = const_cast<std::vector<BrowserWindow*>&>(windows_);
  mutable_windows.erase(std::remove_if(mutable_windows.begin(),
                                       mutable_windows.end(),
                                       [](BrowserWindow* w) { return w->IsClosed(); }),
                        mutable_windows.end());

  return windows_.size();
}

void Application::CloseAllWindows(bool force) {
  logger.Debug("Application::CloseAllWindows - Closing all windows");

  for (auto* window : windows_) {
    if (window && !window->IsClosed()) {
      window->Close(force);
    }
  }

  windows_.clear();
}

// ============================================================================
// Accessors
// ============================================================================

browser::BrowserEngine* Application::GetBrowserEngine() const {
  return browser_engine_.get();
}

platform::WindowSystem* Application::GetWindowSystem() const {
  return window_system_.get();
}

const ApplicationConfig& Application::GetConfig() const {
  return config_;
}

runtime::NodeRuntime* Application::GetNodeRuntime() const {
  return node_runtime_.get();
}

// ============================================================================
// Private Methods
// ============================================================================

void Application::SetupDefaultCallbacks(BrowserWindowCallbacks& callbacks) {
  // Store original destroy callback
  auto original_destroy = callbacks.on_destroy;

  // Wrap destroy callback to track window cleanup
  callbacks.on_destroy = [this, original_destroy]() {
    // Call original callback first
    if (original_destroy) {
      original_destroy();
    }

    // Check if all windows are closed, and quit if so
    if (GetWindowCount() == 0) {
      logger.Debug("Application - All windows closed, quitting");
      Quit();
    }
  };
}

void Application::OnWindowDestroyed(BrowserWindow* window) {
  // Remove from tracking
  windows_.erase(std::remove(windows_.begin(), windows_.end(), window), windows_.end());

  logger.Debug("Application::OnWindowDestroyed - Window removed from tracking");

  // Quit if all windows are closed
  if (windows_.empty()) {
    logger.Debug("Application - All windows destroyed, quitting");
    Quit();
  }
}

// ============================================================================
// Runtime Lifecycle Helpers
// ============================================================================

utils::Result<void> Application::InitializeRuntime() {
  if (!config_.enable_node_runtime || !node_runtime_) {
    logger.Debug("Application::InitializeRuntime - Node runtime disabled or not provided");
    return utils::Ok();
  }

  logger.Info("Application::InitializeRuntime - Starting Node runtime");

  auto result = node_runtime_->Initialize();
  if (!result) {
    return utils::Error("Failed to initialize Node runtime: " + result.GetError().Message());
  }

  // Start health monitoring with automatic restart on failure
  node_runtime_->StartHealthMonitoring();

  logger.Info(
      "Application::InitializeRuntime - Node runtime started successfully with health monitoring");
  return utils::Ok();
}

void Application::ShutdownRuntime() {
  if (!node_runtime_) {
    return;
  }

  // Check if already shut down
  if (node_runtime_->GetState() == runtime::RuntimeState::STOPPED) {
    return;
  }

  logger.Info("Application::ShutdownRuntime - Stopping Node runtime");
  node_runtime_->Shutdown();
  logger.Info("Application::ShutdownRuntime - Node runtime stopped");
}

// ============================================================================
// Browser Control Server Lifecycle Helpers
// ============================================================================

utils::Result<void> Application::InitializeBrowserControlServer() {
  if (!config_.enable_node_runtime || !node_runtime_) {
    logger.Debug(
        "Application::InitializeBrowserControlServer - Node runtime disabled, skipping server");
    return utils::Ok();
  }

  if (windows_.empty()) {
    logger.Debug("Application::InitializeBrowserControlServer - No windows yet, skipping server");
    return utils::Ok();
  }

  logger.Info("Application::InitializeBrowserControlServer - Starting browser control server");

  // Get the first window's native window
  auto* first_window = windows_[0];
  if (!first_window) {
    return utils::Error("First window is null");
  }

  auto window_shared = first_window->GetWindowShared();
  if (!window_shared) {
    return utils::Error("First window's native window is null");
  }

  // Cast to QtMainWindow (Qt-only build)
  auto qt_window = std::dynamic_pointer_cast<platform::QtMainWindow>(window_shared);
  if (!qt_window) {
    return utils::Error("Failed to cast window to QtMainWindow");
  }

  // Create server config
  runtime::BrowserControlServerConfig server_config;
  server_config.socket_path = "/tmp/athena-" + std::to_string(getuid()) + "-control.sock";

  // Create and initialize server
  browser_control_server_ = std::make_unique<runtime::BrowserControlServer>(server_config);
  browser_control_server_->SetBrowserWindow(qt_window);

  auto result = browser_control_server_->Initialize();
  if (!result) {
    browser_control_server_.reset();
    return utils::Error("Failed to initialize browser control server: " +
                        result.GetError().Message());
  }

  logger.Info("Application::InitializeBrowserControlServer - Server started successfully");
  return utils::Ok();
}

void Application::ShutdownBrowserControlServer() {
  if (!browser_control_server_) {
    return;
  }

  if (!browser_control_server_->IsRunning()) {
    return;
  }

  logger.Info("Application::ShutdownBrowserControlServer - Stopping browser control server");
  browser_control_server_->Shutdown();
  browser_control_server_.reset();
  logger.Info("Application::ShutdownBrowserControlServer - Server stopped");
}

}  // namespace core
}  // namespace athena

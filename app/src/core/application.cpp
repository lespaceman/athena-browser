#include "core/application.h"
#include "utils/logging.h"
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <limits.h>

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
                         std::unique_ptr<platform::WindowSystem> window_system)
    : config_(config),
      browser_engine_(std::move(browser_engine)),
      window_system_(std::move(window_system)),
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

utils::Result<void> Application::Initialize(int argc, char* argv[]) {
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

  // Initialize window system first (initializes platform, e.g., GTK)
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

  logger.Info("Application::Run - Entering main event loop");
  window_system_->Run();
  logger.Info("Application::Run - Exited main event loop");
}

void Application::Quit() {
  logger.Info("Application::Quit - Quitting application");

  if (window_system_) {
    window_system_->Quit();
  }

  shutdown_requested_ = true;
}

void Application::Shutdown() {
  if (!initialized_) {
    return;
  }

  logger.Info("Application::Shutdown - Shutting down application");

  shutdown_requested_ = true;

  // Close all windows
  CloseAllWindows(true);

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
    const BrowserWindowConfig& config,
    const BrowserWindowCallbacks& callbacks) {

  if (!initialized_) {
    return utils::Error("Application not initialized");
  }

  logger.Debug("Application::CreateWindow - Creating browser window");

  // Make a copy of callbacks to add our own handlers
  BrowserWindowCallbacks window_callbacks = callbacks;
  SetupDefaultCallbacks(window_callbacks);

  // Create the browser window
  auto window = std::make_unique<BrowserWindow>(
      config,
      window_callbacks,
      window_system_.get(),
      browser_engine_.get());

  // Track the window (weak pointer)
  windows_.push_back(window.get());

  logger.Debug("Application::CreateWindow - Window created successfully");

  return std::unique_ptr<BrowserWindow>(std::move(window));
}

size_t Application::GetWindowCount() const {
  // Remove closed windows from tracking
  auto& mutable_windows = const_cast<std::vector<BrowserWindow*>&>(windows_);
  mutable_windows.erase(
      std::remove_if(mutable_windows.begin(), mutable_windows.end(),
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
  windows_.erase(
      std::remove(windows_.begin(), windows_.end(), window),
      windows_.end());

  logger.Debug("Application::OnWindowDestroyed - Window removed from tracking");

  // Quit if all windows are closed
  if (windows_.empty()) {
    logger.Debug("Application - All windows destroyed, quitting");
    Quit();
  }
}

}  // namespace core
}  // namespace athena

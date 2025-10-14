/**
 * Athena Browser - Main Entry Point
 *
 * This is the primary entry point for the Athena Browser application.
 * It uses the Application class to manage the browser lifecycle.
 */

#include "core/application.h"
#include "browser/cef_engine.h"
#include "browser/app_handler.h"
#include "platform/gtk_window.h"
#include "runtime/node_runtime.h"
#include "include/cef_app.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <filesystem>
#include <atomic>

// ============================================================================
// Signal Handling for Clean Shutdown
// ============================================================================

// Async-signal-safe shutdown flag (atomic operations are safe in signal handlers)
static std::atomic<bool> shutdown_requested{false};

/**
 * Signal handler for graceful shutdown.
 * Handles SIGINT (Ctrl+C), SIGTERM, and SIGABRT.
 *
 * This handler uses only async-signal-safe operations (atomic store).
 * The actual shutdown is handled in the main thread via periodic polling.
 */
void signal_handler(int signum) {
  // Only async-signal-safe operation: set the shutdown flag
  shutdown_requested.store(true);

  // Note: We cannot safely call exit() or shutdown() here, as those
  // functions are not async-signal-safe. The main thread will detect
  // the flag and initiate shutdown.
  (void)signum;  // Suppress unused parameter warning
}

int main(int argc, char* argv[]) {
  using namespace athena;

  // ============================================================================
  // Signal Handler Registration
  // ============================================================================
  // Register signal handlers for graceful shutdown on crashes/interrupts

  std::signal(SIGINT, signal_handler);   // Ctrl+C
  std::signal(SIGTERM, signal_handler);  // kill command
  std::signal(SIGABRT, signal_handler);  // abort()

  std::cout << "Signal handlers registered (SIGINT, SIGTERM, SIGABRT)" << std::endl;

  // ============================================================================
  // CEF Subprocess Handling
  // ============================================================================
  // CEF runs helper processes (renderer, GPU, etc.). If this is a subprocess,
  // execute it and exit immediately.

  CefMainArgs main_args(argc, argv);
  CefRefPtr<AppHandler> app = new AppHandler();

  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    return exit_code;  // Subprocess completed
  }

  // ============================================================================
  // Application Configuration
  // ============================================================================

  core::ApplicationConfig config;
  config.cache_path = "/tmp/athena_browser_cache";
  // subprocess_path is auto-detected
  config.enable_windowless_rendering = true;
  config.windowless_frame_rate = 60;
  config.enable_sandbox = false;

  // Get initial URL from environment or use default
  std::string initial_url = "https://www.google.com";
  if (const char* env_url = std::getenv("DEV_URL")) {
    initial_url = env_url;
  }

  // ============================================================================
  // Create Node Runtime (if enabled)
  // ============================================================================

  std::unique_ptr<runtime::NodeRuntime> node_runtime = nullptr;

  if (config.enable_node_runtime) {
    // Determine the path to the Athena Agent server script
    // The script is at the project root: /path/to/project/athena-agent/dist/server.js
    // The binary is at: /path/to/project/build/release/app/athena-browser
    // So we need to go up 3 levels: app -> release -> build -> project
    std::filesystem::path exe_path(argv[0]);
    std::filesystem::path exe_dir = exe_path.parent_path();  // build/release/app
    std::filesystem::path project_root = exe_dir.parent_path().parent_path().parent_path();
    std::filesystem::path runtime_script = project_root / "athena-agent" / "dist" / "server.js";

    // Check if the script exists
    if (!std::filesystem::exists(runtime_script)) {
      std::cerr << "WARNING: Athena Agent script not found at: " << runtime_script << std::endl;
      std::cerr << "         Claude chat integration will not be available." << std::endl;
      std::cerr << "         Run 'cd athena-agent && npm run build' to build the agent." << std::endl;
    } else {
      runtime::NodeRuntimeConfig runtime_config;
      runtime_config.runtime_script_path = runtime_script.string();
      runtime_config.node_executable = "node";
      runtime_config.socket_path = "/tmp/athena-" + std::to_string(getuid()) + ".sock";

      node_runtime = std::make_unique<runtime::NodeRuntime>(runtime_config);

      std::cout << "Athena Agent will be initialized with script: " << runtime_script << std::endl;
    }
  }

  // ============================================================================
  // Create Application
  // ============================================================================

  auto browser_engine = std::make_unique<browser::CefEngine>(app, &main_args);
  auto window_system = std::make_unique<platform::GtkWindowSystem>();

  auto application = std::make_unique<core::Application>(
      config,
      std::move(browser_engine),
      std::move(window_system),
      std::move(node_runtime));

  // ============================================================================
  // Initialize Application
  // ============================================================================

  auto init_result = application->Initialize(argc, argv);
  if (!init_result) {
    std::cerr << "ERROR: Failed to initialize application: "
              << init_result.GetError().Message() << std::endl;
    return 1;
  }

  std::cout << "Athena Browser initialized successfully" << std::endl;

  // ============================================================================
  // Create Browser Window
  // ============================================================================

  core::BrowserWindowConfig window_config;
  window_config.title = "Athena Browser";
  window_config.size = {1200, 800};
  window_config.url = initial_url;

  core::BrowserWindowCallbacks window_callbacks;
  window_callbacks.on_url_changed = [](const std::string& url) {
    std::cout << "URL changed: " << url << std::endl;
  };
  window_callbacks.on_title_changed = [](const std::string& title) {
    std::cout << "Title changed: " << title << std::endl;
  };
  window_callbacks.on_loading_state_changed = [](bool is_loading) {
    std::cout << "Loading: " << (is_loading ? "true" : "false") << std::endl;
  };

  auto window_result = application->CreateWindow(window_config, window_callbacks);
  if (!window_result) {
    std::cerr << "ERROR: Failed to create window: "
              << window_result.GetError().Message() << std::endl;
    return 1;
  }

  auto window = std::move(window_result.Value());

  // Show the window
  auto show_result = window->Show();
  if (!show_result) {
    std::cerr << "ERROR: Failed to show window: "
              << show_result.GetError().Message() << std::endl;
    return 1;
  }

  std::cout << "Browser window created and shown" << std::endl;

  // ============================================================================
  // Run Main Event Loop
  // ============================================================================

  std::cout << "Entering main event loop..." << std::endl;

  // Set up periodic check for shutdown signal (every 100ms)
  // This allows the signal handler to request shutdown asynchronously
  auto check_shutdown = [&]() -> gboolean {
    if (shutdown_requested.load()) {
      std::cout << "\n[Main] Shutdown requested by signal, exiting event loop..." << std::endl;
      // Initiate shutdown from event loop context (async-signal-safe)
      // Note: Shutdown() is idempotent, safe to call multiple times
      application->Shutdown();
      gtk_main_quit();
      return G_SOURCE_REMOVE;  // Stop the timeout
    }
    return G_SOURCE_CONTINUE;  // Continue checking
  };

  // Convert lambda to function pointer for g_timeout_add
  static auto check_shutdown_func = check_shutdown;
  g_timeout_add(100, +[](gpointer data) -> gboolean {
    auto* func = static_cast<decltype(check_shutdown)*>(data);
    return (*func)();
  }, &check_shutdown_func);

  application->Run();  // Blocking call

  // ============================================================================
  // Cleanup
  // ============================================================================

  std::cout << "Shutting down..." << std::endl;

  // Check if shutdown was requested by signal handler
  if (shutdown_requested.load()) {
    std::cout << "Shutdown initiated by signal" << std::endl;
  }

  window.reset();  // Close window

  // Perform final cleanup shutdown
  // Note: Safe to call even if already called by signal handler (idempotent)
  // The Shutdown() method checks initialized_ flag and returns early if already shut down
  application->Shutdown();

  std::cout << "Shutdown complete" << std::endl;
  return 0;
}

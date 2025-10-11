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
#include "include/cef_app.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
  using namespace athena;

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
  // Create Application
  // ============================================================================

  auto browser_engine = std::make_unique<browser::CefEngine>(app, &main_args);
  auto window_system = std::make_unique<platform::GtkWindowSystem>();

  auto application = std::make_unique<core::Application>(
      config,
      std::move(browser_engine),
      std::move(window_system));

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
  application->Run();  // Blocking call

  // ============================================================================
  // Cleanup
  // ============================================================================

  std::cout << "Shutting down..." << std::endl;
  window.reset();  // Close window
  application->Shutdown();

  std::cout << "Shutdown complete" << std::endl;
  return 0;
}

/**
 * Athena Browser - Main entry point
 *
 * This is a minimal main.cpp that uses the platform abstraction layer
 * to create and run a browser window with CEF.
 *
 * Architecture:
 *   main.cpp -> WindowSystem (platform) -> CEF Engine (browser)
 *
 * All platform-specific code (GTK, input handling, etc.) is encapsulated
 * in the platform layer (platform/gtk_window.h/cpp).
 */

#include "browser/cef_engine.h"
#include "browser/app_handler.h"
#include "platform/gtk_window.h"
#include "include/cef_app.h"
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>

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
  // Engine Configuration
  // ============================================================================

  browser::EngineConfig engine_config;
  engine_config.cache_path = "/tmp/athena_browser_cache";
  engine_config.subprocess_path = GetExecutablePath();
  engine_config.enable_windowless_rendering = true;
  engine_config.windowless_frame_rate = 60;
  engine_config.enable_sandbox = false;

  // ============================================================================
  // Window Configuration
  // ============================================================================

  platform::WindowConfig window_config;
  window_config.title = "Athena Browser";
  window_config.size = {1200, 800};
  window_config.resizable = true;
  window_config.enable_input = true;

  platform::WindowCallbacks window_callbacks;
  window_callbacks.on_resize = [](int width, int height) {
    std::cout << "[main] Window resized: " << width << "x" << height << std::endl;
  };
  window_callbacks.on_close = []() {
    std::cout << "[main] Window close requested" << std::endl;
  };
  window_callbacks.on_destroy = []() {
    std::cout << "[main] Window destroyed" << std::endl;
  };
  window_callbacks.on_focus_changed = [](bool focused) {
    std::cout << "[main] Window focus: " << (focused ? "gained" : "lost") << std::endl;
  };

  // ============================================================================
  // Initialize Platform and Browser Engine
  // ============================================================================

  platform::GtkWindowSystem window_system;
  browser::CefEngine browser_engine(app, &main_args);

  // Initialize window system (also initializes GTK)
  auto init_result = window_system.Initialize(argc, argv, &browser_engine);
  if (!init_result) {
    std::cerr << "ERROR: Failed to initialize window system: "
              << init_result.GetError().Message() << std::endl;
    return -1;
  }

  // Initialize browser engine (initializes CEF with the same main_args from CefExecuteProcess)
  auto engine_result = browser_engine.Initialize(engine_config);
  if (!engine_result) {
    std::cerr << "ERROR: Failed to initialize browser engine: "
              << engine_result.GetError().Message() << std::endl;
    return -1;
  }

  std::cout << "[main] Platform and engine initialized successfully" << std::endl;

  // ============================================================================
  // Create and Show Window
  // ============================================================================

  auto window_result = window_system.CreateWindow(window_config, window_callbacks);
  if (!window_result) {
    std::cerr << "ERROR: Failed to create window: "
              << window_result.GetError().Message() << std::endl;
    browser_engine.Shutdown();
    window_system.Shutdown();
    return -1;
  }

  auto window = std::move(window_result.Value());
  window->Show();

  std::cout << "[main] Window created and shown" << std::endl;

  // ============================================================================
  // Run Main Event Loop
  // ============================================================================

  std::cout << "[main] Entering main event loop..." << std::endl;
  window_system.Run();  // Blocking call

  // ============================================================================
  // Cleanup
  // ============================================================================

  std::cout << "[main] Shutting down..." << std::endl;

  // Window is automatically destroyed by the platform layer
  window.reset();

  // Shutdown in reverse order
  browser_engine.Shutdown();
  window_system.Shutdown();

  std::cout << "[main] Cleanup complete" << std::endl;

  return 0;
}

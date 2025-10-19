/**
 * GtkWindowSystem Implementation
 *
 * Manages GTK initialization and the main event loop.
 * Integrates CEF message loop with GTK's event loop.
 */
#include "browser/browser_engine.h"
#include "include/cef_app.h"
#include "platform/gtk_window.h"

#include <iostream>

namespace athena {
namespace platform {

// ============================================================================
// GtkWindowSystem Implementation
// ============================================================================

GtkWindowSystem::GtkWindowSystem()
    : initialized_(false), running_(false), engine_(nullptr), message_loop_source_id_(0) {}

GtkWindowSystem::~GtkWindowSystem() {
  Shutdown();
}

utils::Result<void> GtkWindowSystem::Initialize(int& argc,
                                                char* argv[],
                                                browser::BrowserEngine* engine) {
  if (initialized_) {
    return utils::Error("WindowSystem already initialized");
  }

  if (!engine) {
    return utils::Error("BrowserEngine cannot be null");
  }

  // Disable GTK setlocale (CEF requirement)
  gtk_disable_setlocale();

  // Initialize GTK
  gtk_init(&argc, &argv);

  engine_ = engine;
  initialized_ = true;

  // Setup CEF message loop integration
  message_loop_source_id_ = g_idle_add(OnCefMessageLoopWork, this);

  return utils::Ok();
}

void GtkWindowSystem::Shutdown() {
  if (!initialized_)
    return;

  // Remove CEF message loop callback
  if (message_loop_source_id_ != 0) {
    g_source_remove(message_loop_source_id_);
    message_loop_source_id_ = 0;
  }

  initialized_ = false;
  running_ = false;
  engine_ = nullptr;
}

bool GtkWindowSystem::IsInitialized() const {
  return initialized_;
}

utils::Result<std::shared_ptr<Window>> GtkWindowSystem::CreateWindow(
    const WindowConfig& config, const WindowCallbacks& callbacks) {
  if (!initialized_) {
    return utils::Error("WindowSystem not initialized");
  }

  auto window = std::make_shared<GtkWindow>(config, callbacks, engine_);
  return std::static_pointer_cast<Window>(window);
}

void GtkWindowSystem::Run() {
  if (!initialized_) {
    std::cerr << "[GtkWindowSystem] Cannot run: WindowSystem not initialized" << std::endl;
    return;
  }

  running_ = true;
  gtk_main();
  running_ = false;
}

void GtkWindowSystem::Quit() {
  if (running_) {
    gtk_main_quit();
    running_ = false;
  }
}

bool GtkWindowSystem::IsRunning() const {
  return running_;
}

gboolean GtkWindowSystem::OnCefMessageLoopWork(gpointer data) {
  (void)data;
  CefDoMessageLoopWork();
  return G_SOURCE_CONTINUE;
}

}  // namespace platform
}  // namespace athena

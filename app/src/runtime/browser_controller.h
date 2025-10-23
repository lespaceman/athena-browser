#ifndef ATHENA_RUNTIME_BROWSER_CONTROLLER_H_
#define ATHENA_RUNTIME_BROWSER_CONTROLLER_H_

#include "utils/error.h"

#include <memory>
#include <string>

namespace athena {
namespace platform {
class GtkWindow;
}

namespace runtime {
class NodeRuntime;

/**
 * BrowserController provides an HTTP API bridge between the Node.js runtime
 * and the C++ GtkWindow browser controls.
 *
 * This class:
 * - Receives HTTP requests from the Node.js server (via MCP tools)
 * - Translates them into GtkWindow method calls
 * - Returns results as HTTP responses
 *
 * Architecture:
 *   Claude (MCP) → Node.js (Express) → HTTP over Unix Socket → BrowserController → GtkWindow
 *
 * The controller is registered with the Node runtime and handles all
 * browser-related endpoints defined in athena-agent/src/routes/browser.ts
 */
class BrowserController {
 public:
  /**
   * Create a browser controller.
   *
   * @param window The GtkWindow to control (non-owning pointer)
   * @param runtime The Node runtime for IPC (non-owning pointer)
   */
  BrowserController(platform::GtkWindow* window, NodeRuntime* runtime);

  ~BrowserController();

  // Disable copy and move
  BrowserController(const BrowserController&) = delete;
  BrowserController& operator=(const BrowserController&) = delete;
  BrowserController(BrowserController&&) = delete;
  BrowserController& operator=(BrowserController&&) = delete;

  /**
   * Register the controller with the Node runtime.
   * This sets up the JavaScript bridge so the Express routes can call C++ methods.
   *
   * @return Ok on success, error on failure
   */
  utils::Result<void> Register();

  /**
   * Unregister the controller.
   */
  void Unregister();

  /**
   * Check if the controller is registered.
   */
  bool IsRegistered() const;

 private:
  platform::GtkWindow* window_;  // Non-owning
  NodeRuntime* runtime_;         // Non-owning
  bool registered_;

  // Internal helper to execute JavaScript that calls C++ functions
  utils::Result<std::string> ExecuteBridgeScript();
};

}  // namespace runtime
}  // namespace athena

#endif  // ATHENA_RUNTIME_BROWSER_CONTROLLER_H_

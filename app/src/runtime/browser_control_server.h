/**
 * Browser Control Server
 *
 * Internal HTTP server that exposes browser control endpoints over Unix socket.
 * Allows the Node.js agent to control the browser via HTTP requests.
 *
 * This server runs in the GTK main thread using GLib's I/O watch mechanism,
 * avoiding the need for additional threading.
 */

#ifndef ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_H_
#define ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_H_

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "utils/error.h"
#include "platform/gtk_window.h"

namespace athena {
namespace runtime {

/**
 * Configuration for the browser control server.
 */
struct BrowserControlServerConfig {
  std::string socket_path;  // Unix socket path (e.g., /tmp/athena-<uid>-control.sock)
};

/**
 * Browser Control Server
 *
 * Lightweight HTTP server that:
 * - Listens on Unix socket
 * - Accepts HTTP requests from Node.js agent
 * - Calls browser control methods directly (on GTK main thread)
 * - Returns HTTP responses
 *
 * Runs entirely on GTK main thread using non-blocking I/O.
 *
 * Lifecycle:
 * 1. Create server with config
 * 2. SetBrowserWindow() to register browser instance
 * 3. Initialize() to start listening
 * 4. GTK main loop handles requests via GLib I/O watches
 * 5. Shutdown() to stop server
 *
 * Thread Safety:
 * - All methods must be called from GTK main thread
 * - No threading - all operations run synchronously on main thread
 * - Non-blocking I/O prevents stalling the event loop
 */
class BrowserControlServer {
 public:
  /**
   * Create a browser control server.
   *
   * @param config Server configuration
   */
  explicit BrowserControlServer(const BrowserControlServerConfig& config);

  /**
   * Destructor - performs clean shutdown.
   */
  ~BrowserControlServer();

  // Non-copyable, non-movable (due to GLib callbacks)
  BrowserControlServer(const BrowserControlServer&) = delete;
  BrowserControlServer& operator=(const BrowserControlServer&) = delete;
  BrowserControlServer(BrowserControlServer&&) = delete;
  BrowserControlServer& operator=(BrowserControlServer&&) = delete;

  /**
   * Set the browser window to control.
   * Must be called before Initialize().
   *
   * @param window Shared pointer to GtkWindow (server stores a weak reference)
   */
  void SetBrowserWindow(const std::shared_ptr<platform::GtkWindow>& window);

  /**
   * Initialize the server and start listening.
   *
   * @return Ok on success, error on failure
   */
  utils::Result<void> Initialize();

  /**
   * Shutdown the server.
   * Stops listening and cleans up resources.
   */
  void Shutdown();

  /**
   * Check if server is running.
   */
  bool IsRunning() const;

  /**
   * Get the socket path.
   */
  std::string GetSocketPath() const;

 private:
  // Configuration
  BrowserControlServerConfig config_;

  // Browser window (weak reference, does not own)
  std::weak_ptr<platform::GtkWindow> window_;

  // Socket file descriptor
  int server_fd_;

  // GLib I/O watch source IDs
  guint server_watch_id_;

  // Active client connections (opaque pointer - implementation detail)
  std::vector<void*> active_clients_;

  // State
  bool running_;

  // Internal methods
  void AcceptConnection();
  bool HandleClientData(void* client);
  void CloseClient(void* client);
  std::string ProcessRequest(const std::string& request);

  // Request handlers (run synchronously on GTK main thread)
  std::string HandleOpenUrl(const std::string& url);
  std::string HandleGetUrl();
  std::string HandleGetTabCount();
  std::string HandleGetPageHtml();
  std::string HandleExecuteJavaScript(const std::string& code);
  std::string HandleTakeScreenshot();

  // HTTP helpers
  static std::string ParseHttpMethod(const std::string& request);
  static std::string ParseHttpPath(const std::string& request);
  static std::string ParseHttpBody(const std::string& request);
  static std::string BuildHttpResponse(int status_code,
                                       const std::string& status_text,
                                       const std::string& body);

  // GLib callbacks (static, use user_data for 'this' pointer)
  static gboolean OnServerReadable(GIOChannel* source,
                                   GIOCondition condition,
                                   gpointer user_data);
  static gboolean OnClientReadable(GIOChannel* source,
                                   GIOCondition condition,
                                   gpointer user_data);
};

}  // namespace runtime
}  // namespace athena

#endif  // ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_H_

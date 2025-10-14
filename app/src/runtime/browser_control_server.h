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
 * - Calls browser control methods (via function callbacks)
 * - Returns HTTP responses
 *
 * Runs in GTK main thread using GLib I/O watches (no threading needed).
 *
 * Lifecycle:
 * 1. Create server with config
 * 2. SetBrowserWindow() to register browser instance
 * 3. Initialize() to start listening
 * 4. GTK main loop handles requests
 * 5. Shutdown() to stop server
 *
 * Thread Safety:
 * - All methods must be called from GTK main thread
 * - I/O callbacks run in GTK main thread (via g_io_add_watch)
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
   * @param window GtkWindow instance (not owned, must outlive this server)
   */
  void SetBrowserWindow(platform::GtkWindow* window);

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

  // Browser window (not owned)
  platform::GtkWindow* window_;

  // Socket file descriptor
  int server_fd_;

  // GLib I/O watch source IDs
  guint server_watch_id_;

  // State
  bool running_;

  // Internal methods
  bool AcceptConnection();
  bool HandleRequest(int client_fd);
  std::string ProcessRequest(const std::string& request);

  // Request handlers
  std::string HandleOpenUrl(const std::string& url);
  std::string HandleGetUrl();
  std::string HandleGetTabCount();

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
};

}  // namespace runtime
}  // namespace athena

#endif  // ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_H_

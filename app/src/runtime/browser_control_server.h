/**
 * Browser Control Server
 *
 * Internal HTTP server that exposes browser control endpoints over Unix socket.
 * Allows the Node.js agent to control the browser via HTTP requests.
 *
 * This server runs on the main UI thread using platform-specific I/O mechanisms,
 * avoiding the need for additional threading.
 *
 * Implementation Files:
 * - browser_control_server.cpp: Core server lifecycle and connection management
 * - browser_control_server_routing.cpp: HTTP request parsing and routing
 * - browser_control_handlers_navigation.cpp: Navigation and history handlers
 * - browser_control_handlers_tabs.cpp: Tab management handlers
 * - browser_control_handlers_content.cpp: HTML, JavaScript, screenshot handlers
 * - browser_control_handlers_extraction.cpp: Advanced content extraction handlers
 * - browser_control_server_internal.h: Shared utilities and constants
 */

#ifndef ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_H_
#define ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_H_

#include "utils/error.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declare Qt types
class QSocketNotifier;

namespace athena {
namespace platform {
class QtMainWindow;
}
}  // namespace athena

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
 * - Calls browser control methods directly (on UI main thread)
 * - Returns HTTP responses
 *
 * Runs entirely on UI main thread using non-blocking I/O.
 *
 * Lifecycle:
 * 1. Create server with config
 * 2. SetBrowserWindow() to register browser instance
 * 3. Initialize() to start listening
 * 4. Main loop handles requests via platform-specific I/O watches
 * 5. Shutdown() to stop server
 *
 * Thread Safety & Threading Model:
 * - ALL operations run on Qt's main UI thread (required by CEF)
 * - No threading introduced - operations are truly synchronous
 * - Non-blocking sockets prevent blocking the UI thread during I/O
 * - QSocketNotifier integrates Unix socket I/O with Qt's event loop
 * - WaitForLoadToComplete() processes Qt events via QCoreApplication::processEvents()
 *   to prevent UI freezing during navigation waits
 * - JavaScript execution uses CefDoMessageLoopWork() to pump CEF events
 * - All CEF browser operations execute on the main thread (CEF requirement)
 *
 * Performance Optimizations:
 * - Socket buffer management parses headers only once (cached position)
 * - JavaScript execution returns objects directly (no double JSON encoding)
 * - Request size limited to 1MB to prevent DoS attacks
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

  // Non-copyable, non-movable
  BrowserControlServer(const BrowserControlServer&) = delete;
  BrowserControlServer& operator=(const BrowserControlServer&) = delete;
  BrowserControlServer(BrowserControlServer&&) = delete;
  BrowserControlServer& operator=(BrowserControlServer&&) = delete;

  /**
   * Set the browser window to control.
   * Must be called before Initialize().
   *
   * @param window Shared pointer to QtMainWindow (server stores a weak reference)
   */
  void SetBrowserWindow(const std::shared_ptr<platform::QtMainWindow>& window);

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
  std::weak_ptr<platform::QtMainWindow> window_;

  // Socket file descriptor
  int server_fd_;

  // Qt socket notifier for accepting connections
  QSocketNotifier* server_watch_id_;

  // Active client connections (opaque pointer - implementation detail)
  std::vector<void*> active_clients_;

  // State
  bool running_;

  // Internal methods
  void AcceptConnection();
  bool HandleClientData(void* client);
  void CloseClient(void* client);
  std::string ProcessRequest(const std::string& request);

  // Request handlers (run synchronously on UI main thread)
  std::string HandleOpenUrl(const std::string& url);
  std::string HandleGetUrl(std::optional<size_t> tab_index);
  std::string HandleGetTabCount();
  std::string HandleGetPageHtml(std::optional<size_t> tab_index);
  std::string HandleExecuteJavaScript(const std::string& code, std::optional<size_t> tab_index);
  std::string HandleTakeScreenshot(std::optional<size_t> tab_index, std::optional<bool> full_page);
  std::string HandleNavigate(const std::string& url, std::optional<size_t> tab_index);
  std::string HandleHistory(const std::string& action, std::optional<size_t> tab_index);
  std::string HandleReload(std::optional<size_t> tab_index, std::optional<bool> ignore_cache);
  std::string HandleCreateTab(const std::string& url);
  std::string HandleCloseTab(size_t tab_index);
  std::string HandleSwitchTab(size_t tab_index);
  std::string HandleTabInfo();

  // Context-efficient content extraction handlers
  std::string HandleGetPageSummary(std::optional<size_t> tab_index);
  std::string HandleGetInteractiveElements(std::optional<size_t> tab_index);
  std::string HandleGetAccessibilityTree(std::optional<size_t> tab_index);
  std::string HandleQueryContent(const std::string& query_type, std::optional<size_t> tab_index);
  std::string HandleGetAnnotatedScreenshot(std::optional<size_t> tab_index);

  // HTTP helpers
  static std::string ParseHttpMethod(const std::string& request);
  static std::string ParseHttpPath(const std::string& request);
  static std::string ParseHttpBody(const std::string& request);
  static std::string BuildHttpResponse(int status_code,
                                       const std::string& status_text,
                                       const std::string& body);
};

}  // namespace runtime
}  // namespace athena

#endif  // ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_H_

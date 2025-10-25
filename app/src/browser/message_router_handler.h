#ifndef ATHENA_BROWSER_MESSAGE_ROUTER_HANDLER_H_
#define ATHENA_BROWSER_MESSAGE_ROUTER_HANDLER_H_

#include "include/cef_browser.h"
#include "include/wrapper/cef_message_router.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace athena {
namespace browser {

/**
 * Handler for CefMessageRouter JS↔C++ bridge.
 *
 * This provides a promise-based JavaScript API using CEF's battle-tested
 * CefMessageRouterBrowserSide. This complements the custom IPC system
 * (RequestJavaScriptEvaluation) which is used for simple cases.
 *
 * JavaScript Usage:
 *   window.athena.query({
 *     request: 'operation_name',
 *     data: {...}
 *   }).then(response => {
 *     console.log('Success:', response);
 *   }).catch(error => {
 *     console.error('Failed:', error);
 *   });
 *
 * Benefits over custom IPC:
 * - Promise-based (no polling)
 * - Handles renderer crashes gracefully
 * - Standard pattern across CEF apps
 * - Automatic cleanup of pending requests
 *
 * Design:
 * - Non-copyable, non-movable
 * - Uses dependency injection for query handlers
 * - Thread-safe for CEF UI thread operations
 */
class MessageRouterHandler : public CefMessageRouterBrowserSide::Handler {
 public:
  /**
   * Query handler function type.
   * Parameters: browser, frame, query_id, request_json, persistent, callback
   * Returns: true if handled, false to let other handlers try
   */
  using QueryHandler = std::function<bool(CefRefPtr<CefBrowser>,
                                          CefRefPtr<CefFrame>,
                                          int64_t,
                                          const std::string&,
                                          bool,
                                          CefRefPtr<CefMessageRouterBrowserSide::Callback>)>;

  MessageRouterHandler();
  ~MessageRouterHandler() override;

  // Non-copyable, non-movable
  MessageRouterHandler(const MessageRouterHandler&) = delete;
  MessageRouterHandler& operator=(const MessageRouterHandler&) = delete;
  MessageRouterHandler(MessageRouterHandler&&) = delete;
  MessageRouterHandler& operator=(MessageRouterHandler&&) = delete;

  /**
   * Register a handler for a specific query type.
   * Query type is extracted from JSON: { "request": "type_name", ... }
   */
  void RegisterQueryHandler(const std::string& query_type, QueryHandler handler);

  /**
   * Unregister a handler for a specific query type.
   */
  void UnregisterQueryHandler(const std::string& query_type);

  // CefMessageRouterBrowserSide::Handler methods
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override;

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64_t query_id) override;

 private:
  std::unordered_map<std::string, QueryHandler> handlers_;
};

}  // namespace browser
}  // namespace athena

#endif  // ATHENA_BROWSER_MESSAGE_ROUTER_HANDLER_H_

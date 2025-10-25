#include "browser/message_router_handler.h"

#include "include/wrapper/cef_helpers.h"
#include "utils/logging.h"

#include <nlohmann/json.hpp>

namespace athena {
namespace browser {

static utils::Logger logger("MessageRouterHandler");

MessageRouterHandler::MessageRouterHandler() = default;

MessageRouterHandler::~MessageRouterHandler() = default;

void MessageRouterHandler::RegisterQueryHandler(const std::string& query_type,
                                                QueryHandler handler) {
  handlers_[query_type] = std::move(handler);
  logger.Info("Registered query handler for: {}", query_type);
}

void MessageRouterHandler::UnregisterQueryHandler(const std::string& query_type) {
  handlers_.erase(query_type);
  logger.Info("Unregistered query handler for: {}", query_type);
}

bool MessageRouterHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   int64_t query_id,
                                   const CefString& request,
                                   bool persistent,
                                   CefRefPtr<Callback> callback) {
  CEF_REQUIRE_UI_THREAD();

  const std::string request_str = request.ToString();
  logger.Debug(
      "OnQuery: query_id={}, persistent={}, request={}", query_id, persistent, request_str);

  // Parse request JSON to extract query type
  try {
    auto request_json = nlohmann::json::parse(request_str);

    if (!request_json.contains("request") || !request_json["request"].is_string()) {
      logger.Warn("OnQuery: Missing or invalid 'request' field in query");
      callback->Failure(-1, "Missing or invalid 'request' field");
      return true;
    }

    const std::string query_type = request_json["request"].get<std::string>();

    // Look up handler for this query type
    auto it = handlers_.find(query_type);
    if (it == handlers_.end()) {
      logger.Debug("OnQuery: No handler registered for query type: {}", query_type);
      return false;  // Let other handlers try
    }

    // Invoke the handler
    logger.Debug("OnQuery: Dispatching to handler for: {}", query_type);
    return it->second(browser, frame, query_id, request_str, persistent, callback);

  } catch (const nlohmann::json::exception& e) {
    logger.Error("OnQuery: Failed to parse request JSON: {}", e.what());
    callback->Failure(-2, std::string("Invalid JSON: ") + e.what());
    return true;
  }
}

void MessageRouterHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                                           CefRefPtr<CefFrame> frame,
                                           int64_t query_id) {
  (void)browser;
  (void)frame;
  CEF_REQUIRE_UI_THREAD();

  logger.Debug("OnQueryCanceled: query_id={}", query_id);

  // Note: We don't need to do anything here for most use cases.
  // The callback is automatically invalidated by CEF when the query is canceled.
  // Specific handlers can track query_id if they need cleanup on cancellation.
}

}  // namespace browser
}  // namespace athena

/**
 * Browser Control Server - HTTP Routing
 *
 * Handles HTTP request parsing and routing to appropriate handlers.
 */

#include "runtime/browser_control_server.h"
#include "runtime/browser_control_server_internal.h"
#include "utils/logging.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>

namespace athena {
namespace runtime {

static utils::Logger logger("BrowserControlServer");

// ============================================================================
// HTTP Parsing Utilities
// ============================================================================

std::string BrowserControlServer::ParseHttpMethod(const std::string& request) {
  size_t space_pos = request.find(' ');
  if (space_pos == std::string::npos) {
    return "";
  }
  return request.substr(0, space_pos);
}

std::string BrowserControlServer::ParseHttpPath(const std::string& request) {
  size_t first_space = request.find(' ');
  if (first_space == std::string::npos) {
    return "";
  }

  size_t second_space = request.find(' ', first_space + 1);
  if (second_space == std::string::npos) {
    return "";
  }

  return request.substr(first_space + 1, second_space - first_space - 1);
}

std::string BrowserControlServer::ParseHttpBody(const std::string& request) {
  size_t body_start = request.find("\r\n\r\n");
  if (body_start == std::string::npos) {
    return "";
  }

  return request.substr(body_start + 4);
}

std::string BrowserControlServer::BuildHttpResponse(int status_code,
                                                    const std::string& status_text,
                                                    const std::string& body) {
  std::ostringstream response;
  response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
  response << "Content-Type: application/json\r\n";
  response << "Content-Length: " << body.size() << "\r\n";
  response << "Connection: close\r\n";
  response << "\r\n";
  response << body;

  return response.str();
}

// ============================================================================
// Request Routing (runs on Qt main thread)
// ============================================================================

std::string BrowserControlServer::ProcessRequest(const std::string& request) {
  std::string method = ParseHttpMethod(request);
  std::string path = ParseHttpPath(request);
  std::string body = ParseHttpBody(request);

  logger.Debug("Processing " + method + " " + path);

  auto parse_json = [&](nlohmann::json& json_out) -> bool {
    try {
      if (body.empty()) {
        json_out = nlohmann::json::object();
      } else {
        json_out = nlohmann::json::parse(body);
      }
      return true;
    } catch (const nlohmann::json::exception& e) {
      logger.Error("JSON parsing error: " + std::string(e.what()));
      return false;
    }
  };

  // Route to handlers - all run synchronously on main thread
  if (method == "POST" && path == "/internal/open_url") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("url") || !json["url"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing url parameter"})");
    }
    std::string url = json["url"].get<std::string>();
    return BuildHttpResponse(200, "OK", HandleOpenUrl(url));

  } else if ((method == "GET" || method == "POST") && path == "/internal/get_url") {
    std::optional<size_t> tab_index;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleGetUrl(tab_index));

  } else if (method == "GET" && path == "/internal/tab_count") {
    return BuildHttpResponse(200, "OK", HandleGetTabCount());

  } else if ((method == "GET" || method == "POST") && path == "/internal/get_html") {
    std::optional<size_t> tab_index;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleGetPageHtml(tab_index));

  } else if (method == "POST" && path == "/internal/execute_js") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("code") || !json["code"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing code parameter"})");
    }
    std::optional<size_t> tab_index;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    std::string code = json["code"].get<std::string>();
    return BuildHttpResponse(200, "OK", HandleExecuteJavaScript(code, tab_index));

  } else if ((method == "GET" || method == "POST") && path == "/internal/screenshot") {
    std::optional<size_t> tab_index;
    std::optional<bool> full_page;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
      if (json.contains("fullPage") && json["fullPage"].is_boolean()) {
        full_page = json["fullPage"].get<bool>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleTakeScreenshot(tab_index, full_page));

  } else if (method == "POST" && path == "/internal/navigate") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("url") || !json["url"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing url parameter"})");
    }
    std::optional<size_t> tab_index;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    return BuildHttpResponse(200, "OK", HandleNavigate(json["url"].get<std::string>(), tab_index));

  } else if (method == "POST" && path == "/internal/history") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("action") || !json["action"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing action parameter"})");
    }
    std::optional<size_t> tab_index;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    return BuildHttpResponse(200, "OK",
                             HandleHistory(json["action"].get<std::string>(), tab_index));

  } else if (method == "POST" && path == "/internal/reload") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    std::optional<size_t> tab_index;
    std::optional<bool> ignore_cache;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    if (json.contains("ignoreCache") && json["ignoreCache"].is_boolean()) {
      ignore_cache = json["ignoreCache"].get<bool>();
    }
    return BuildHttpResponse(200, "OK", HandleReload(tab_index, ignore_cache));

  } else if (method == "POST" && path == "/internal/tab/create") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("url") || !json["url"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing url parameter"})");
    }
    return BuildHttpResponse(200, "OK", HandleCreateTab(json["url"].get<std::string>()));

  } else if (method == "POST" && path == "/internal/tab/close") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("tabIndex") || !json["tabIndex"].is_number_unsigned()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing tabIndex parameter"})");
    }
    return BuildHttpResponse(200, "OK", HandleCloseTab(json["tabIndex"].get<size_t>()));

  } else if (method == "POST" && path == "/internal/tab/switch") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("tabIndex") || !json["tabIndex"].is_number_unsigned()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing tabIndex parameter"})");
    }
    return BuildHttpResponse(200, "OK", HandleSwitchTab(json["tabIndex"].get<size_t>()));

  } else if (method == "GET" && path == "/internal/tab_info") {
    return BuildHttpResponse(200, "OK", HandleTabInfo());

  } else if ((method == "GET" || method == "POST") && path == "/internal/get_page_summary") {
    std::optional<size_t> tab_index;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleGetPageSummary(tab_index));

  } else if ((method == "GET" || method == "POST") && path == "/internal/get_interactive_elements") {
    std::optional<size_t> tab_index;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleGetInteractiveElements(tab_index));

  } else if ((method == "GET" || method == "POST") && path == "/internal/get_accessibility_tree") {
    std::optional<size_t> tab_index;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleGetAccessibilityTree(tab_index));

  } else if (method == "POST" && path == "/internal/query_content") {
    nlohmann::json json;
    if (!parse_json(json)) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Invalid JSON"})");
    }
    if (!json.contains("queryType") || !json["queryType"].is_string()) {
      return BuildHttpResponse(400, "Bad Request",
                               R"({"success":false,"error":"Missing queryType parameter"})");
    }
    std::optional<size_t> tab_index;
    if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
      tab_index = json["tabIndex"].get<size_t>();
    }
    return BuildHttpResponse(200, "OK",
                             HandleQueryContent(json["queryType"].get<std::string>(), tab_index));

  } else if ((method == "GET" || method == "POST") && path == "/internal/get_annotated_screenshot") {
    std::optional<size_t> tab_index;
    if (method == "POST") {
      nlohmann::json json;
      if (!parse_json(json)) {
        return BuildHttpResponse(400, "Bad Request",
                                 R"({"success":false,"error":"Invalid JSON"})");
      }
      if (json.contains("tabIndex") && json["tabIndex"].is_number_unsigned()) {
        tab_index = json["tabIndex"].get<size_t>();
      }
    }
    return BuildHttpResponse(200, "OK", HandleGetAnnotatedScreenshot(tab_index));

  } else {
    logger.Warn("Unknown endpoint: " + path);
    return BuildHttpResponse(404, "Not Found",
                             R"({"success":false,"error":"Endpoint not found"})");
  }
}

}  // namespace runtime
}  // namespace athena

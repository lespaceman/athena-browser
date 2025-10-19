/**
 * Browser Control Server - Content Extraction Handlers
 *
 * Handlers for advanced content extraction using JavaScript:
 * - Page summaries
 * - Interactive elements
 * - Accessibility tree
 * - Content queries (forms, tables, media, etc.)
 * - Annotated screenshots
 */

#include "runtime/browser_control_server.h"
#include "runtime/browser_control_server_internal.h"
#include "runtime/js_execution_utils.h"
#include "platform/qt_mainwindow.h"
#include "utils/logging.h"
#include <nlohmann/json.hpp>
#include <map>

namespace athena {
namespace runtime {

static utils::Logger logger("BrowserControlServer");

// ============================================================================
// Content Extraction Handlers
// ============================================================================

std::string BrowserControlServer::HandleGetPageSummary(std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"}}.dump();
    }

    QString js = R"(
      function getVisibleText(element) {
        var clone = element.cloneNode(true);
        var toRemove = clone.querySelectorAll('style, script, noscript, iframe, svg');
        for (var i = 0; i < toRemove.length; i++) {
          toRemove[i].remove();
        }
        return clone.textContent || clone.innerText || '';
      }

      var mainElement = document.querySelector('main') || document.querySelector('article') ||
                       document.querySelector('[role="main"]') || document.querySelector('.content');
      var mainText = mainElement ? getVisibleText(mainElement) : getVisibleText(document.body);

      return {
        title: document.title,
        url: window.location.href,
        headings: Array.from(document.querySelectorAll('h1,h2,h3')).map(function(h) { return h.textContent.trim(); }).slice(0, 10),
        mainText: mainText.trim().substring(0, 500),
        forms: document.querySelectorAll('form').length,
        links: document.querySelectorAll('a').length,
        buttons: document.querySelectorAll('button, input[type="button"], input[type="submit"]').length,
        inputs: document.querySelectorAll('input, textarea, select').length,
        images: document.querySelectorAll('img').length
      };
    )";

    QString result = window->ExecuteJavaScript(js);
    logger.Info("Raw JS result: {}", result.toStdString());
    std::string parse_error;
    auto exec = ParseJsExecutionResultString(result.toStdString(), parse_error);
    if (!exec.has_value()) {
      logger.Error("Page summary parsing failed: {}", parse_error);
      return nlohmann::json{
          {"success", false},
          {"error", parse_error.empty() ? "Failed to parse page summary response" : parse_error}}.dump();
    }

    logger.Info("Parsed exec result - success: {}, type: {}, value: {}", exec->success, exec->type, exec->value.dump());

    if (!exec->success) {
      logger.Warn("Page summary script execution failed: {}", exec->error_message);
      return nlohmann::json{
          {"success", false},
          {"error", exec->error_message.empty() ? "Failed to extract page summary" : exec->error_message}}.dump();
    }

    nlohmann::json summary = exec->value;
    logger.Info("Summary value: {}", summary.dump());

    if (!summary.is_object()) {
      logger.Error("Page summary result is not an object. Type: {}, Value: {}",
                   summary.type_name(), summary.dump());
      return nlohmann::json{
          {"success", false},
          {"error", "Invalid response format - expected object"}}.dump();
    }

    return nlohmann::json{
        {"success", true},
        {"summary", summary},
        {"tabIndex", static_cast<int>(target_tab)}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleGetInteractiveElements(std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"}}.dump();
    }

    QString js = R"(
      return (function() {
        const elements = [];
        const selectors = 'a, button, input, select, textarea, [role="button"], [onclick], [tabindex="0"]';

        document.querySelectorAll(selectors).forEach((el, idx) => {
          const rect = el.getBoundingClientRect();

          // Only include visible elements
          if (rect.width > 0 && rect.height > 0 &&
              rect.top < window.innerHeight &&
              rect.bottom > 0 &&
              getComputedStyle(el).visibility !== 'hidden' &&
              getComputedStyle(el).display !== 'none') {

            let text = el.textContent?.trim().substring(0, 100) || '';
            if (text.length === 0) {
              // Try alternative text sources
              text = el.getAttribute('aria-label') ||
                     el.getAttribute('title') ||
                     el.getAttribute('placeholder') ||
                     el.value || '';
            }

            elements.push({
              index: idx,
              tag: el.tagName.toLowerCase(),
              type: el.type || '',
              id: el.id || '',
              className: el.className || '',
              text: text,
              href: el.href || '',
              name: el.name || '',
              placeholder: el.placeholder || '',
              value: el.value || '',
              ariaLabel: el.getAttribute('aria-label') || '',
              role: el.getAttribute('role') || '',
              disabled: el.disabled || false,
              checked: el.checked || false,
              bounds: {
                x: Math.round(rect.x),
                y: Math.round(rect.y),
                width: Math.round(rect.width),
                height: Math.round(rect.height)
              }
            });
          }
        });

        return JSON.stringify(elements);
      })();
    )";

    QString result = window->ExecuteJavaScript(js);
    std::string parse_error;
    auto exec = ParseJsExecutionResultString(result.toStdString(), parse_error);
    if (!exec.has_value()) {
      logger.Error("Interactive elements parsing failed: {}", parse_error);
      return nlohmann::json{
          {"success", false},
          {"error", parse_error.empty() ? "Failed to parse interactive elements response" : parse_error}}.dump();
    }

    if (!exec->success) {
      logger.Warn("Interactive elements script execution failed: {}", exec->error_message);
      return nlohmann::json{
          {"success", false},
          {"error", exec->error_message.empty() ? "Failed to extract interactive elements" : exec->error_message}}.dump();
    }

    nlohmann::json elements = exec->value;
    if (elements.is_string() && JsonStringLooksLikeObject(elements)) {
      try {
        elements = nlohmann::json::parse(elements.get<std::string>());
      } catch (const nlohmann::json::parse_error& e) {
        logger.Error("Failed to parse interactive elements payload: {}", e.what());
        return nlohmann::json{
            {"success", false},
            {"error", "Failed to parse interactive elements"}}.dump();
      }
    }

    if (!elements.is_array()) {
      logger.Error("Interactive elements result is not an array");
      return nlohmann::json{
          {"success", false},
          {"error", "Invalid response format - expected array"}}.dump();
    }

    return nlohmann::json{
        {"success", true},
        {"elements", elements},
        {"count", elements.size()},
        {"tabIndex", static_cast<int>(target_tab)}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleGetAccessibilityTree(std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"}}.dump();
    }

    QString js = R"(
      return (function() {
        function buildA11yTree(element, depth = 0, maxDepth = 3) {
          if (depth > maxDepth) return null;
          if (!element) return null;

          const tagName = element.tagName.toLowerCase();

          // Skip non-semantic elements
          if (['script', 'style', 'noscript', 'meta', 'link'].includes(tagName)) {
            return null;
          }

          const role = element.getAttribute('role') || tagName;
          const rect = element.getBoundingClientRect();

          // Skip invisible elements at top level
          if (depth === 0 && (rect.width === 0 || rect.height === 0)) {
            return null;
          }

          const node = {
            role: role,
            tag: tagName
          };

          // Add text content for leaf nodes
          const text = element.getAttribute('aria-label') ||
                       (element.childNodes.length === 1 && element.childNodes[0].nodeType === 3
                         ? element.textContent?.trim().substring(0, 50)
                         : '');
          if (text) node.name = text;

          // Add relevant attributes
          if (element.id) node.id = element.id;
          if (element.href) node.href = element.href;
          if (element.type) node.type = element.type;
          if (element.value) node.value = element.value;
          if (element === document.activeElement) node.focused = true;
          if (element.disabled) node.disabled = true;
          if (element.getAttribute('aria-hidden') === 'true') node.hidden = true;

          // Recursively get children for containers
          const containerTags = ['main', 'nav', 'header', 'footer', 'section', 'article', 'aside', 'form', 'div', 'ul', 'ol'];
          if (containerTags.includes(tagName) || role === 'navigation' || role === 'main') {
            const children = Array.from(element.children)
              .map(child => buildA11yTree(child, depth + 1, maxDepth))
              .filter(Boolean);

            if (children.length > 0) {
              node.children = children;
            }
          }

          return node;
        }

        return JSON.stringify(buildA11yTree(document.body));
      })();
    )";

    QString result = window->ExecuteJavaScript(js);
    std::string parse_error;
    auto exec = ParseJsExecutionResultString(result.toStdString(), parse_error);
    if (!exec.has_value()) {
      logger.Error("Accessibility tree parsing failed: {}", parse_error);
      return nlohmann::json{
          {"success", false},
          {"error", parse_error.empty() ? "Failed to parse accessibility tree response" : parse_error}}.dump();
    }

    if (!exec->success) {
      logger.Warn("Accessibility tree execution failed: {}", exec->error_message);
      return nlohmann::json{
          {"success", false},
          {"error", exec->error_message.empty() ? "Failed to extract accessibility tree" : exec->error_message}}.dump();
    }

    nlohmann::json tree = exec->value;
    if (tree.is_string() && JsonStringLooksLikeObject(tree)) {
      try {
        tree = nlohmann::json::parse(tree.get<std::string>());
      } catch (const nlohmann::json::parse_error& e) {
        logger.Error("Failed to parse accessibility tree JSON: {}", e.what());
        return nlohmann::json{
            {"success", false},
            {"error", "Failed to parse accessibility tree"}}.dump();
      }
    }

    return nlohmann::json{
        {"success", true},
        {"tree", tree},
        {"tabIndex", static_cast<int>(target_tab)}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleQueryContent(const std::string& query_type,
                                                     std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"}}.dump();
    }

    // Define query types
    std::map<std::string, std::string> queries = {
      {"forms", R"(JSON.stringify(Array.from(document.querySelectorAll('form')).map((f, idx) => ({index: idx, action: f.action, method: f.method, name: f.name || '', id: f.id || '', fields: Array.from(f.elements).map(e => ({name: e.name || '', type: e.type || '', id: e.id || '', placeholder: e.placeholder || '', required: e.required || false, value: e.value || '', options: e.tagName.toLowerCase() === 'select' ? Array.from(e.options).map(o => ({text: o.text, value: o.value})) : []}))}))))"},

      {"navigation", R"(JSON.stringify(Array.from(document.querySelectorAll('nav a, header a, [role="navigation"] a')).map(a => ({text: a.textContent.trim(), href: a.href, title: a.title || ''}))))"},

      {"article", R"(JSON.stringify({title: document.title, heading: document.querySelector('h1')?.textContent.trim() || '', content: (document.querySelector('article, main, [role="main"]')?.textContent || document.body.textContent).trim().substring(0, 2000), author: document.querySelector('[rel="author"], .author, .byline')?.textContent.trim() || '', published: document.querySelector('time, [itemprop="datePublished"]')?.textContent.trim() || ''}))"},

      {"tables", R"(JSON.stringify(Array.from(document.querySelectorAll('table')).slice(0, 5).map(table => ({caption: table.caption?.textContent.trim() || '', headers: Array.from(table.querySelectorAll('th')).map(th => th.textContent.trim()), rows: Array.from(table.querySelectorAll('tbody tr')).slice(0, 10).map(tr => Array.from(tr.querySelectorAll('td')).map(td => td.textContent.trim()))}))))"},

      {"media", R"(JSON.stringify({images: Array.from(document.querySelectorAll('img')).slice(0, 20).map(img => ({src: img.src, alt: img.alt || '', title: img.title || ''})), videos: Array.from(document.querySelectorAll('video')).map(v => ({src: v.src || v.currentSrc, poster: v.poster || ''}))}))"}
    };

    auto it = queries.find(query_type);
    if (it == queries.end()) {
      return nlohmann::json{
          {"success", false},
          {"error", "Unknown query type. Available: forms, navigation, article, tables, media"}}.dump();
    }

    QString js = QString::fromStdString("return (function() { return " + it->second + "; })();");
    logger.Info("Query content ({}) - Executing JS (first 200 chars): {}", query_type, js.toStdString().substr(0, 200));
    QString result = window->ExecuteJavaScript(js);
    logger.Info("Query content ({}) - Raw result (first 500 chars): {}", query_type, result.toStdString().substr(0, 500));
    std::string parse_error;
    auto exec = ParseJsExecutionResultString(result.toStdString(), parse_error);
    if (!exec.has_value()) {
      logger.Error("Query content ({}) parse error: {}", query_type, parse_error);
      return nlohmann::json{
          {"success", false},
          {"error", parse_error.empty() ? "Failed to parse query response" : parse_error}}.dump();
    }

    if (!exec->success) {
      logger.Warn("Query content ({}) execution failed: {}", query_type, exec->error_message);
      return nlohmann::json{
          {"success", false},
          {"error", exec->error_message.empty() ? "Failed to execute query" : exec->error_message}}.dump();
    }

    nlohmann::json data = exec->value;
    logger.Info("Query content ({}) - exec->value type: {}, is_string: {}, is_null: {}, dump: {}",
                query_type, exec->type, data.is_string(), data.is_null(),
                data.dump().substr(0, 200));
    if (data.is_string() && JsonStringLooksLikeObject(data)) {
      try {
        data = nlohmann::json::parse(data.get<std::string>());
      } catch (const nlohmann::json::parse_error& e) {
        logger.Error("Failed to parse query content JSON for {}: {}", query_type, e.what());
        return nlohmann::json{
            {"success", false},
            {"error", "Failed to parse query result"}}.dump();
      }
    }

    return nlohmann::json{
        {"success", true},
        {"queryType", query_type},
        {"data", data},
        {"tabIndex", static_cast<int>(target_tab)}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleGetAnnotatedScreenshot(std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    std::string error;
    if (!SwitchToRequestedTab(window, tab_index, error)) {
      return nlohmann::json{
          {"success", false},
          {"error", error}}.dump();
    }

    size_t target_tab = window->GetActiveTabIndex();
    if (!window->WaitForLoadToComplete(target_tab, kDefaultContentTimeoutMs)) {
      return nlohmann::json{
          {"success", false},
          {"error", "Page is still loading"}}.dump();
    }

    // Get screenshot
    QString screenshot_base64 = window->TakeScreenshot();
    if (screenshot_base64.isEmpty()) {
      return nlohmann::json{
          {"success", false},
          {"error", "Failed to capture screenshot"}}.dump();
    }

    // Get interactive element positions
    QString js = R"(
      return (function() {
        const elements = [];
        const selectors = 'a, button, input, select, textarea, [role="button"]';

        document.querySelectorAll(selectors).forEach((el, idx) => {
          const rect = el.getBoundingClientRect();

          // Only include visible elements in viewport
          if (rect.width > 0 && rect.height > 0 &&
              rect.top < window.innerHeight &&
              rect.bottom > 0 &&
              rect.left < window.innerWidth &&
              rect.right > 0 &&
              getComputedStyle(el).visibility !== 'hidden' &&
              getComputedStyle(el).display !== 'none') {

            const text = (el.textContent?.trim() ||
                         el.getAttribute('aria-label') ||
                         el.getAttribute('title') ||
                         el.placeholder ||
                         el.value || '').substring(0, 30);

            elements.push({
              index: idx,
              x: Math.round(rect.x),
              y: Math.round(rect.y),
              width: Math.round(rect.width),
              height: Math.round(rect.height),
              tag: el.tagName.toLowerCase(),
              text: text,
              type: el.type || ''
            });
          }
        });

        return JSON.stringify(elements.slice(0, 50)); // Limit to 50 elements
      })();
    )";

    QString elements_result = window->ExecuteJavaScript(js);
    nlohmann::json elements_json = nlohmann::json::array();

    std::string parse_error;
    auto exec = ParseJsExecutionResultString(elements_result.toStdString(), parse_error);
    if (!exec.has_value()) {
      logger.Warn("Annotated screenshot element parse error: {}", parse_error);
    } else if (!exec->success) {
      logger.Warn("Annotated screenshot element execution failed: {}", exec->error_message);
    } else {
      nlohmann::json tmp = exec->value;
      if (tmp.is_string() && JsonStringLooksLikeObject(tmp)) {
        try {
          tmp = nlohmann::json::parse(tmp.get<std::string>());
        } catch (const nlohmann::json::parse_error& e) {
          logger.Error("Failed to parse annotated screenshot elements JSON: {}", e.what());
          tmp = nlohmann::json::array();
        }
      }

      if (tmp.is_array()) {
        elements_json = tmp;
      } else {
        logger.Warn("Annotated screenshot elements result is not an array");
      }
    }

    return nlohmann::json{
        {"success", true},
        {"screenshot", screenshot_base64.toStdString()},
        {"elements", elements_json},
        {"tabIndex", static_cast<int>(target_tab)}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

}  // namespace runtime
}  // namespace athena

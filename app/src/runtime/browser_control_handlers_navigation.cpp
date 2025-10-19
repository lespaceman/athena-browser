/**
 * Browser Control Server - Navigation Handlers
 *
 * Handlers for URL navigation, history, reload, and tab count operations.
 */

#include "runtime/browser_control_server.h"
#include "runtime/browser_control_server_internal.h"
#include "platform/qt_mainwindow.h"
#include "utils/logging.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace athena {
namespace runtime {

static utils::Logger logger("BrowserControlServer");

// ============================================================================
// Navigation Handlers
// ============================================================================

std::string BrowserControlServer::HandleOpenUrl(const std::string& url) {
  logger.Info("Opening URL: " + url);

  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    const auto start = std::chrono::steady_clock::now();
    size_t target_tab = 0;
    bool created_tab = false;

    size_t tab_count = window->GetTabCount();
    if (tab_count == 0) {
      int tab_index = window->CreateTab(QString::fromStdString(url));
      if (tab_index < 0) {
        return nlohmann::json{
            {"success", false},
            {"error", "Failed to create tab"}}.dump();
      }

      target_tab = static_cast<size_t>(tab_index);
      created_tab = true;
    } else {
      target_tab = window->GetActiveTabIndex();
      window->LoadURL(QString::fromStdString(url));
    }

    bool loaded = window->WaitForLoadToComplete(target_tab, kDefaultNavigationTimeoutMs);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    if (!loaded) {
      return nlohmann::json{
          {"success", false},
          {"error", "Navigation timed out"},
          {"tabIndex", static_cast<int>(target_tab)},
          {"loadTimeMs", elapsed}}.dump();
    }

    const std::string final_url = window->GetCurrentUrl().toStdString();
    return nlohmann::json{
        {"success", true},
        {"tabIndex", static_cast<int>(target_tab)},
        {"finalUrl", final_url.empty() ? url : final_url},
        {"createdTab", created_tab},
        {"loadTimeMs", elapsed}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleGetUrl(std::optional<size_t> tab_index) {
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

    QString url = window->GetCurrentUrl();
    return nlohmann::json{
        {"success", true},
        {"url", url.toStdString()},
        {"tabIndex", static_cast<int>(window->GetActiveTabIndex())}}.dump();
  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleGetTabCount() {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  try {
    size_t count = window->GetTabCount();
    return nlohmann::json{
        {"success", true},
        {"count", count}}.dump();

  } catch (const std::exception& e) {
    return nlohmann::json{
        {"success", false},
        {"error", e.what()}}.dump();
  }
}

std::string BrowserControlServer::HandleNavigate(const std::string& url,
                                                 std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  if (window->GetTabCount() == 0) {
    return HandleOpenUrl(url);
  }

  std::string error;
  if (!SwitchToRequestedTab(window, tab_index, error)) {
    return nlohmann::json{
        {"success", false},
        {"error", error}}.dump();
  }

  size_t target_tab = tab_index.has_value()
      ? *tab_index
      : window->GetActiveTabIndex();

  const auto start = std::chrono::steady_clock::now();
  window->LoadURL(QString::fromStdString(url));

  bool loaded = window->WaitForLoadToComplete(target_tab, kDefaultNavigationTimeoutMs);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();

  if (!loaded) {
    return nlohmann::json{
        {"success", false},
        {"error", "Navigation timed out"},
        {"tabIndex", static_cast<int>(target_tab)},
        {"loadTimeMs", elapsed}}.dump();
  }

  const std::string final_url = window->GetCurrentUrl().toStdString();
  return nlohmann::json{
      {"success", true},
      {"tabIndex", static_cast<int>(target_tab)},
      {"finalUrl", final_url.empty() ? url : final_url},
      {"loadTimeMs", elapsed}}.dump();
}

std::string BrowserControlServer::HandleHistory(const std::string& action,
                                                std::optional<size_t> tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  std::string error;
  if (!SwitchToRequestedTab(window, tab_index, error)) {
    return nlohmann::json{
        {"success", false},
        {"error", error}}.dump();
  }

  std::string action_lower = action;
  std::transform(
      action_lower.begin(),
      action_lower.end(),
      action_lower.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

  size_t target_tab = window->GetActiveTabIndex();
  const auto start = std::chrono::steady_clock::now();

  if (action_lower == "back") {
    window->GoBack();
  } else if (action_lower == "forward") {
    window->GoForward();
  } else {
    return nlohmann::json{
        {"success", false},
        {"error", "Invalid history action"}}.dump();
  }
  bool loaded = window->WaitForLoadToComplete(target_tab, kDefaultNavigationTimeoutMs);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();

  if (!loaded) {
    return nlohmann::json{
        {"success", false},
        {"error", "Navigation timed out"},
        {"action", action_lower},
        {"tabIndex", static_cast<int>(target_tab)},
        {"loadTimeMs", elapsed}}.dump();
  }

  const std::string final_url = window->GetCurrentUrl().toStdString();
  return nlohmann::json{
      {"success", true},
      {"action", action_lower},
      {"tabIndex", static_cast<int>(target_tab)},
      {"finalUrl", final_url},
      {"loadTimeMs", elapsed}}.dump();
}

std::string BrowserControlServer::HandleReload(std::optional<size_t> tab_index,
                                               std::optional<bool> ignore_cache) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  std::string error;
  if (!SwitchToRequestedTab(window, tab_index, error)) {
    return nlohmann::json{
        {"success", false},
        {"error", error}}.dump();
  }

  size_t target_tab = window->GetActiveTabIndex();
  const auto start = std::chrono::steady_clock::now();

  window->Reload(ignore_cache.value_or(false));
  bool loaded = window->WaitForLoadToComplete(target_tab, kDefaultNavigationTimeoutMs);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();

  if (!loaded) {
    return nlohmann::json{
        {"success", false},
        {"error", "Reload timed out"},
        {"tabIndex", static_cast<int>(target_tab)},
        {"ignoreCache", ignore_cache.value_or(false)},
        {"loadTimeMs", elapsed}}.dump();
  }

  return nlohmann::json{
      {"success", true},
      {"tabIndex", static_cast<int>(target_tab)},
      {"ignoreCache", ignore_cache.value_or(false)},
      {"loadTimeMs", elapsed}}.dump();
}

}  // namespace runtime
}  // namespace athena

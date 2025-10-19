/**
 * Browser Control Server - Tab Management Handlers
 *
 * Handlers for tab creation, closing, switching, and information.
 */

#include "runtime/browser_control_server.h"
#include "runtime/browser_control_server_internal.h"
#include "platform/qt_mainwindow.h"
#include "utils/logging.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace athena {
namespace runtime {

static utils::Logger logger("BrowserControlServer");

// ============================================================================
// Tab Management Handlers
// ============================================================================

std::string BrowserControlServer::HandleCreateTab(const std::string& url) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  const auto start = std::chrono::steady_clock::now();
  int tab_index = window->CreateTab(QString::fromStdString(url));
  if (tab_index < 0) {
    return nlohmann::json{
        {"success", false},
        {"error", "Failed to create tab"}}.dump();
  }

  bool loaded = window->WaitForLoadToComplete(static_cast<size_t>(tab_index),
                                              kDefaultNavigationTimeoutMs);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();

  if (!loaded) {
    return nlohmann::json{
        {"success", false},
        {"error", "Tab creation timed out"},
        {"tabIndex", tab_index},
        {"loadTimeMs", elapsed}}.dump();
  }

  const std::string final_url = window->GetCurrentUrl().toStdString();
  return nlohmann::json{
      {"success", true},
      {"tabIndex", tab_index},
      {"url", url},
      {"finalUrl", final_url.empty() ? url : final_url},
      {"loadTimeMs", elapsed}}.dump();
}

std::string BrowserControlServer::HandleCloseTab(size_t tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  size_t count = window->GetTabCount();
  if (tab_index >= count) {
    return nlohmann::json{
        {"success", false},
        {"error", "Invalid tab index"}}.dump();
  }

  window->CloseTab(tab_index);
  return nlohmann::json{
      {"success", true},
      {"tabIndex", static_cast<int>(tab_index)}}.dump();
}

std::string BrowserControlServer::HandleSwitchTab(size_t tab_index) {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  size_t count = window->GetTabCount();
  if (tab_index >= count) {
    return nlohmann::json{
        {"success", false},
        {"error", "Invalid tab index"}}.dump();
  }

  window->SwitchToTab(tab_index);
  return nlohmann::json{
      {"success", true},
      {"tabIndex", static_cast<int>(window->GetActiveTabIndex())}}.dump();
}

std::string BrowserControlServer::HandleTabInfo() {
  auto window = window_.lock();
  if (!running_ || !window) {
    return nlohmann::json{
        {"success", false},
        {"error", "Server is shutting down"}}.dump();
  }

  size_t count = window->GetTabCount();
  size_t active = window->GetActiveTabIndex();
  return nlohmann::json{
      {"success", true},
      {"count", count},
      {"activeTabIndex", active}}.dump();
}

}  // namespace runtime
}  // namespace athena

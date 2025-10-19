/**
 * Browser Control Server Internal Utilities
 *
 * Shared utilities and constants used across browser control server implementation files.
 * This header is NOT part of the public API - use browser_control_server.h instead.
 */

#ifndef ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_INTERNAL_H_
#define ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_INTERNAL_H_

#include <memory>
#include <optional>
#include <string>
#include "platform/qt_mainwindow.h"
#include "utils/logging.h"

namespace athena {
namespace runtime {

// ============================================================================
// Constants
// ============================================================================

// Maximum size for HTTP requests (1MB to prevent DoS attacks)
static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;

// Default timeout for navigation operations (15 seconds)
static constexpr int kDefaultNavigationTimeoutMs = 15000;

// Default timeout for content extraction operations (5 seconds)
static constexpr int kDefaultContentTimeoutMs = 5000;

// ============================================================================
// Shared Utilities
// ============================================================================

/**
 * Switch to the requested tab if tab_index is provided.
 * If tab_index is not provided, uses the currently active tab.
 *
 * @param window Main window instance
 * @param tab_index Optional tab index to switch to
 * @param error_message Output parameter for error message
 * @return true on success, false on error (check error_message)
 */
inline bool SwitchToRequestedTab(const std::shared_ptr<platform::QtMainWindow>& window,
                                  std::optional<size_t> tab_index,
                                  std::string& error_message) {
  if (!tab_index.has_value()) {
    return true;
  }

  size_t count = window->GetTabCount();
  if (*tab_index >= count) {
    error_message = "Invalid tab index";
    return false;
  }

  if (window->GetActiveTabIndex() != *tab_index) {
    window->SwitchToTab(*tab_index);
  }
  return true;
}

}  // namespace runtime
}  // namespace athena

#endif  // ATHENA_RUNTIME_BROWSER_CONTROL_SERVER_INTERNAL_H_

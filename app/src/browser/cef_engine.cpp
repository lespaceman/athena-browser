#include "browser/cef_engine.h"

#include "include/cef_browser.h"
#include "include/cef_request_context.h"
#include "include/wrapper/cef_helpers.h"
#include "utils/logging.h"

#include <iostream>
#include <limits.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>

namespace athena {
namespace browser {

// Static logger for this module
static utils::Logger logger("CefEngine");

// Helper: Get executable path for subprocess
static std::string GetExecutablePath() {
  char path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len != -1) {
    path[len] = '\0';
    return std::string(path);
  }
  return "";
}

namespace {

bool CanBindLocalPort(uint16_t port) {
  if (port == 0) {
    return true;
  }

  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    logger.Warn("Failed to create socket to probe port {}: {}", port, std::strerror(errno));
    return false;
  }

  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);

  int result = ::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  int saved_errno = errno;
  ::close(sock);

  if (result == 0) {
    return true;
  }

  if (saved_errno != EADDRINUSE && saved_errno != EACCES) {
    logger.Warn("Unexpected error probing port {}: {}", port, std::strerror(saved_errno));
  }

  return false;
}

bool WaitForPortAvailability(uint16_t port, int timeout_ms) {
  if (port == 0) {
    return true;
  }

  const int clamped_timeout = std::max(timeout_ms, 0);
  if (CanBindLocalPort(port)) {
    return true;
  }

  if (clamped_timeout == 0) {
    return false;
  }

  logger.Info("Remote debugging port {} is busy; waiting up to {} ms for release", port,
              clamped_timeout);

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(clamped_timeout);

  while (std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (CanBindLocalPort(port)) {
      logger.Info("Remote debugging port {} is now free", port);
      return true;
    }
  }

  return CanBindLocalPort(port);
}

}  // namespace

CefEngine::CefEngine(CefRefPtr<::CefApp> app, const CefMainArgs* main_args)
    : app_(app),
      main_args_(main_args),
      initialized_(false),
      next_id_(1),
      remote_debugging_port_(0),
      remote_debugging_wait_timeout_ms_(3000) {}

CefEngine::~CefEngine() {
  if (initialized_) {
    Shutdown();
  }
}

// ============================================================================
// Lifecycle Management
// ============================================================================

utils::Result<void> CefEngine::Initialize(const EngineConfig& config) {
  if (initialized_) {
    return utils::Err<void>("CEF engine already initialized");
  }

  remote_debugging_port_ = config.remote_debugging_port;
  remote_debugging_wait_timeout_ms_ =
      std::clamp(config.remote_debugging_port_wait_timeout_ms, 0, 60000);

  if (remote_debugging_port_ > 0) {
    if (!WaitForPortAvailability(remote_debugging_port_, remote_debugging_wait_timeout_ms_)) {
      return utils::Err<void>("Remote debugging port " +
                              std::to_string(remote_debugging_port_) + " is still in use after " +
                              std::to_string(remote_debugging_wait_timeout_ms_) + " ms");
    }
    logger.Info("Remote debugging enabled on fixed port {}", remote_debugging_port_);
  } else {
    logger.Info("Remote debugging port set to dynamic allocation");
  }

  // Configure CEF settings
  CefSettings settings;
  settings.no_sandbox = !config.enable_sandbox;
  settings.multi_threaded_message_loop = false;
  settings.external_message_pump = false;
  settings.windowless_rendering_enabled = config.enable_windowless_rendering;

  // Enable remote debugging on localhost only for security
  // When remote_debugging_port_ is 0, CEF will request a dynamic port.
  settings.remote_debugging_port = static_cast<int>(remote_debugging_port_);

  // Set cache path
  if (!config.cache_path.empty()) {
    CefString(&settings.cache_path).FromString(config.cache_path);
  }

  // Set subprocess path
  std::string subprocess_path = config.subprocess_path;
  if (subprocess_path.empty()) {
    subprocess_path = GetExecutablePath();
  }
  if (!subprocess_path.empty()) {
    CefString(&settings.browser_subprocess_path).FromString(subprocess_path);
  }

  // Initialize CEF (using the CefMainArgs passed to constructor, if available)
  if (main_args_) {
    if (!CefInitialize(*main_args_, settings, app_, nullptr)) {
      return utils::Err<void>("CefInitialize failed");
    }
  } else {
    // Fallback: create CefMainArgs if not provided
    CefMainArgs default_main_args(0, nullptr);
    if (!CefInitialize(default_main_args, settings, app_, nullptr)) {
      return utils::Err<void>("CefInitialize failed");
    }
  }

  initialized_ = true;
  logger.Info("CEF initialized successfully");
  return utils::Ok();
}

void CefEngine::Shutdown() {
  if (!initialized_) {
    return;
  }

  // Close all browsers
  for (auto& pair : browsers_) {
    if (pair.second.browser) {
      pair.second.browser->GetHost()->CloseBrowser(true);
    }
  }
  browsers_.clear();

  // Shutdown CEF
  CefShutdown();
  initialized_ = false;

  if (remote_debugging_port_ > 0) {
    if (!WaitForPortAvailability(remote_debugging_port_, remote_debugging_wait_timeout_ms_)) {
      logger.Warn("Remote debugging port {} did not become available within {} ms; "
                  "a lingering process may still be holding it",
                  remote_debugging_port_, remote_debugging_wait_timeout_ms_);
    }
  }
  remote_debugging_port_ = 0;

  logger.Info("CEF shutdown complete");
}

// ============================================================================
// Browser Management
// ============================================================================

utils::Result<BrowserId> CefEngine::CreateBrowser(const BrowserConfig& config) {
  if (!initialized_) {
    return utils::Err<BrowserId>("CEF engine not initialized");
  }

  if (!config.gl_renderer) {
    return utils::Err<BrowserId>("gl_renderer is required");
  }

  // Generate unique ID
  BrowserId id = GenerateId();

  // Create CEF client
  CefRefPtr<CefClient> client = new CefClient(config.native_window_handle, config.gl_renderer);

  // Initialize message router for JS↔C++ bridge
  client->InitializeMessageRouter();

  client->SetDeviceScaleFactor(config.device_scale_factor);
  client->SetSize(config.width, config.height);

  // Create CEF browser
  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);  // 0 = no parent window handle

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = 60;

  // Create RequestContext for per-tab cookie/cache isolation (if requested)
  CefRefPtr<::CefRequestContext> request_context = nullptr;
  if (config.isolate_cookies) {
    // Create a new context that shares storage with the global context
    // This provides cookie/cache isolation while avoiding separate disk caches
    CefRequestContextSettings context_settings;
    request_context =
        CefRequestContext::CreateContext(CefRequestContext::GetGlobalContext(), nullptr);
    logger.Debug("Browser {}: Created isolated RequestContext for cookie/cache isolation", id);
  } else {
    // Use global context (shared cookies/cache across all tabs)
    request_context = nullptr;  // nullptr = use global context
    logger.Debug("Browser {}: Using global RequestContext (shared cookies/cache)", id);
  }

  // Store browser info (browser will be set in OnAfterCreated callback)
  BrowserInfo info;
  info.id = id;
  info.client = client;
  info.browser = nullptr;  // Will be set after creation
  info.request_context = request_context;
  browsers_[id] = info;

  // Create browser asynchronously
  if (!CefBrowserHost::CreateBrowser(
          window_info, client, config.url, browser_settings, nullptr, request_context)) {
    browsers_.erase(id);
    return utils::Err<BrowserId>("CefBrowserHost::CreateBrowser failed");
  }

  logger.Info("Browser {} created with URL: {}", id, config.url);

  // Return by value
  return utils::Result<BrowserId>(id);
}

void CefEngine::CloseBrowser(BrowserId id, bool force_close) {
  BrowserInfo* info = FindBrowser(id);
  if (!info) {
    return;
  }

  if (info->client && info->client->GetBrowser()) {
    info->client->GetBrowser()->GetHost()->CloseBrowser(force_close);
  }

  browsers_.erase(id);
}

bool CefEngine::HasBrowser(BrowserId id) const {
  return FindBrowser(id) != nullptr;
}

// ============================================================================
// Navigation
// ============================================================================

void CefEngine::LoadURL(BrowserId id, const std::string& url) {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  if (browser) {
    browser->GetMainFrame()->LoadURL(url);
  }
}

void CefEngine::GoBack(BrowserId id) {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  if (browser && browser->CanGoBack()) {
    browser->GoBack();
  }
}

void CefEngine::GoForward(BrowserId id) {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  if (browser && browser->CanGoForward()) {
    browser->GoForward();
  }
}

void CefEngine::Reload(BrowserId id, bool ignore_cache) {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  if (browser) {
    if (ignore_cache) {
      browser->ReloadIgnoreCache();
    } else {
      browser->Reload();
    }
  }
}

void CefEngine::StopLoad(BrowserId id) {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  if (browser) {
    browser->StopLoad();
  }
}

// ============================================================================
// Browser State
// ============================================================================

bool CefEngine::CanGoBack(BrowserId id) const {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  return browser && browser->CanGoBack();
}

bool CefEngine::CanGoForward(BrowserId id) const {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  return browser && browser->CanGoForward();
}

bool CefEngine::IsLoading(BrowserId id) const {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  return browser && browser->IsLoading();
}

std::string CefEngine::GetURL(BrowserId id) const {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  if (browser) {
    return browser->GetMainFrame()->GetURL().ToString();
  }
  return "";
}

// ============================================================================
// Rendering & Display
// ============================================================================

void CefEngine::SetSize(BrowserId id, int width, int height) {
  CefRefPtr<CefClient> client = GetCefClient(id);
  if (client) {
    client->SetSize(width, height);
  }
}

void CefEngine::SetDeviceScaleFactor(BrowserId id, float scale_factor) {
  CefRefPtr<CefClient> client = GetCefClient(id);
  if (client) {
    client->SetDeviceScaleFactor(scale_factor);
  }
}

void CefEngine::Invalidate(BrowserId id) {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  if (browser) {
    browser->GetHost()->Invalidate(PET_VIEW);
  }
}

// ============================================================================
// Input Events
// ============================================================================

void CefEngine::SetFocus(BrowserId id, bool focus) {
  CefRefPtr<::CefBrowser> browser = GetCefBrowser(id);
  if (browser) {
    browser->GetHost()->SetFocus(focus);

    // Update CefClient's focus state for CEF #3870 workaround
    CefRefPtr<CefClient> client = GetCefClient(id);
    if (client) {
      client->SetFocus(focus);
    }
  }
}

// ============================================================================
// Message Loop Integration
// ============================================================================

void CefEngine::DoMessageLoopWork() {
  if (initialized_) {
    CefDoMessageLoopWork();
  }
}

void CefEngine::ShowDevTools(BrowserId id) {
  if (!initialized_) {
    logger.Warn("ShowDevTools: CEF engine not initialized");
    return;
  }

  CefRefPtr<CefClient> client = GetCefClient(id);
  if (!client) {
    logger.Warn("ShowDevTools: No client found for browser ID {}", id);
    return;
  }

  client->ShowDevTools();
}

// ============================================================================
// CEF-specific API
// ============================================================================

CefRefPtr<::CefBrowser> CefEngine::GetCefBrowser(BrowserId id) const {
  const BrowserInfo* info = FindBrowser(id);
  if (!info || !info->client) {
    return nullptr;
  }
  return info->client->GetBrowser();
}

CefRefPtr<CefClient> CefEngine::GetCefClient(BrowserId id) const {
  const BrowserInfo* info = FindBrowser(id);
  return info ? info->client : nullptr;
}

// ============================================================================
// Private helpers
// ============================================================================

BrowserId CefEngine::GenerateId() {
  return next_id_++;
}

CefEngine::BrowserInfo* CefEngine::FindBrowser(BrowserId id) {
  auto it = browsers_.find(id);
  return (it != browsers_.end()) ? &it->second : nullptr;
}

const CefEngine::BrowserInfo* CefEngine::FindBrowser(BrowserId id) const {
  auto it = browsers_.find(id);
  return (it != browsers_.end()) ? &it->second : nullptr;
}

}  // namespace browser
}  // namespace athena

#include "browser/cef_engine.h"
#include "include/cef_browser.h"
#include "include/wrapper/cef_helpers.h"
#include <iostream>
#include <unistd.h>
#include <limits.h>

namespace athena {
namespace browser {

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

CefEngine::CefEngine(CefRefPtr<::CefApp> app, const CefMainArgs* main_args)
    : app_(app),
      main_args_(main_args),
      initialized_(false),
      next_id_(1) {
}

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

  // Configure CEF settings
  CefSettings settings;
  settings.no_sandbox = !config.enable_sandbox;
  settings.multi_threaded_message_loop = false;
  settings.external_message_pump = false;
  settings.windowless_rendering_enabled = config.enable_windowless_rendering;

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
  std::cout << "[CefEngine::Initialize] CEF initialized successfully" << std::endl;
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
  std::cout << "[CefEngine::Shutdown] CEF shutdown complete" << std::endl;
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
  CefRefPtr<CefClient> client = new CefClient(
      config.native_window_handle,
      config.gl_renderer);

  client->SetDeviceScaleFactor(config.device_scale_factor);
  client->SetSize(config.width, config.height);

  // Create CEF browser
  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);  // 0 = no parent window handle

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = 60;

  // Store browser info (browser will be set in OnAfterCreated callback)
  BrowserInfo info;
  info.id = id;
  info.client = client;
  info.browser = nullptr;  // Will be set after creation
  browsers_[id] = info;

  // Create browser asynchronously
  if (!CefBrowserHost::CreateBrowser(window_info, client, config.url,
                                      browser_settings, nullptr, nullptr)) {
    browsers_.erase(id);
    return utils::Err<BrowserId>("CefBrowserHost::CreateBrowser failed");
  }

  std::cout << "[CefEngine::CreateBrowser] Browser " << id
            << " created with URL: " << config.url << std::endl;

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

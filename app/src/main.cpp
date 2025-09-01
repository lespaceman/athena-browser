#include "cef_app.h"
#include "cef_browser.h"
#include "cef_client.h"
#include "cef_command_line.h"
#include "wrapper/cef_helpers.h"
#ifdef _WIN32
#include "cef_sandbox_win.h"
#endif
#include "app_handler.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <limits.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

class SimpleHandler : public CefClient, public CefLifeSpanHandler, public CefRequestHandler {
 public:
  SimpleHandler() = default;

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = browser;
  }

  bool DoClose(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    return false; // Allow close
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = nullptr;
  }

  // Block unsafe schemes and optionally route external links.
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    CEF_REQUIRE_UI_THREAD();
    const std::string url = request->GetURL();
    // Allow app:// and http(s) only; block file://, chrome://, devtools://, etc.
    if (url.rfind("app://", 0) == 0 || url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
      return false; // allow
    }
    return true; // cancel navigation
  }

 private:
  CefRefPtr<CefBrowser> browser_;
  IMPLEMENT_REFCOUNTING(SimpleHandler);
};

static std::string GetEnv(const char* key) {
  const char* v = std::getenv(key);
  return v ? std::string(v) : std::string();
}

static std::string GetExecutableDir() {
#if defined(_WIN32)
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
  std::string p(exe_path);
  auto pos = p.find_last_of("/\\");
  return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buf(size + 1);
  if (_NSGetExecutablePath(buf.data(), &size) != 0) return ".";
  std::string p(buf.data());
  auto pos = p.find_last_of('/');
  return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#else
  char exe_path[PATH_MAX] = {0};
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len > 0) exe_path[len] = '\0';
  std::string p = len > 0 ? std::string(exe_path) : std::string(".");
  auto pos = p.find_last_of('/');
  return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#endif
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
  CefEnableHighDPISupport();
#endif

  CefMainArgs main_args(argc, argv);
  CefRefPtr<AppHandler> app(new AppHandler());

  const int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    return exit_code; // Child/subprocess exit.
  }

  CefSettings settings;
  settings.no_sandbox = true; // Enable sandbox later per-platform.
#if !defined(NDEBUG)
  settings.remote_debugging_port = 9222;
#endif

  // Point CEF to resource and locale directories to fix ICU/resource loading.
  const std::string exe_dir = GetExecutableDir();
#if defined(_WIN32)
  CefString(&settings.resources_dir_path) = exe_dir;
  CefString(&settings.locales_dir_path) = exe_dir + "\\locales";
#else
  CefString(&settings.resources_dir_path) = exe_dir;
  CefString(&settings.locales_dir_path) = exe_dir + "/locales";
#endif

  if (!CefInitialize(main_args, settings, app, nullptr)) {
    return 1;
  }

  CefWindowInfo window_info;
#ifdef _WIN32
  window_info.SetAsPopup(NULL, "Athena Browser");
#endif

  CefBrowserSettings browser_settings;

  // Determine URL based on environment
  std::string url = GetEnv("DEV_URL");
  if (url.empty()) {
    // Production: Use custom app:// scheme
    url = "app://index.html";
  }
  // Else: Development mode using DEV_URL (e.g., http://localhost:5173)

  CefRefPtr<SimpleHandler> handler(new SimpleHandler());
  CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings, nullptr, nullptr);

  CefRunMessageLoop();
  CefShutdown();
  return 0;
}

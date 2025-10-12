#ifndef ATHENA_BROWSER_CEF_ENGINE_H_
#define ATHENA_BROWSER_CEF_ENGINE_H_

#include "browser/browser_engine.h"
#include "browser/cef_client.h"
#include "include/cef_app.h"
#include <map>
#include <memory>

namespace athena {
namespace browser {

/**
 * CEF implementation of the BrowserEngine interface.
 *
 * This class wraps the Chromium Embedded Framework (CEF) and provides
 * a clean, testable interface for browser operations.
 *
 * Design:
 * - RAII: Initialization in Initialize(), cleanup in Shutdown()
 * - Thread-safe: All public methods check CEF threading requirements
 * - ID management: Generates unique IDs for browsers
 * - Lifetime management: Tracks all browser instances
 */
class CefEngine : public BrowserEngine {
 public:
  /**
   * Construct a CEF engine.
   * Note: Does not initialize CEF. Call Initialize() explicitly.
   *
   * @param app CEF application handler (optional, for custom handlers)
   * @param main_args CEF main arguments (optional, will be passed to Initialize)
   */
  explicit CefEngine(CefRefPtr<::CefApp> app = nullptr, const CefMainArgs* main_args = nullptr);

  ~CefEngine() override;

  // Non-copyable, non-movable
  CefEngine(const CefEngine&) = delete;
  CefEngine& operator=(const CefEngine&) = delete;
  CefEngine(CefEngine&&) = delete;
  CefEngine& operator=(CefEngine&&) = delete;

  // ============================================================================
  // BrowserEngine interface
  // ============================================================================

  utils::Result<void> Initialize(const EngineConfig& config) override;
  void Shutdown() override;
  bool IsInitialized() const override { return initialized_; }

  utils::Result<BrowserId> CreateBrowser(const BrowserConfig& config) override;
  void CloseBrowser(BrowserId id, bool force_close = false) override;
  bool HasBrowser(BrowserId id) const override;

  void LoadURL(BrowserId id, const std::string& url) override;
  void GoBack(BrowserId id) override;
  void GoForward(BrowserId id) override;
  void Reload(BrowserId id, bool ignore_cache = false) override;
  void StopLoad(BrowserId id) override;

  bool CanGoBack(BrowserId id) const override;
  bool CanGoForward(BrowserId id) const override;
  bool IsLoading(BrowserId id) const override;
  std::string GetURL(BrowserId id) const override;

  void SetSize(BrowserId id, int width, int height) override;
  void SetDeviceScaleFactor(BrowserId id, float scale_factor) override;
  void Invalidate(BrowserId id) override;

  void SetFocus(BrowserId id, bool focus) override;

  void DoMessageLoopWork() override;

  // ============================================================================
  // CEF-specific API (for internal use)
  // ============================================================================

  /**
   * Get the CEF browser instance for a given ID.
   * @return Browser reference, or nullptr if not found
   */
  CefRefPtr<::CefBrowser> GetCefBrowser(BrowserId id) const;

  /**
   * Get the CEF client for a given browser ID.
   * @return Client reference, or nullptr if not found
   */
  virtual CefRefPtr<CefClient> GetCefClient(BrowserId id) const;

 private:
  /**
   * Browser instance info.
   */
  struct BrowserInfo {
    BrowserId id;
    CefRefPtr<CefClient> client;
    CefRefPtr<::CefBrowser> browser;
  };

  /**
   * Generate a unique browser ID.
   */
  BrowserId GenerateId();

  /**
   * Find browser info by ID.
   */
  BrowserInfo* FindBrowser(BrowserId id);
  const BrowserInfo* FindBrowser(BrowserId id) const;

  CefRefPtr<::CefApp> app_;           // CEF application handler
  const CefMainArgs* main_args_;      // CEF main arguments (non-owning)
  bool initialized_;                  // Initialization state
  BrowserId next_id_;                 // Next browser ID to assign
  std::map<BrowserId, BrowserInfo> browsers_;  // Active browsers
};

}  // namespace browser
}  // namespace athena

#endif  // ATHENA_BROWSER_CEF_ENGINE_H_

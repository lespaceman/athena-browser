#ifndef ATHENA_BROWSER_BROWSER_ENGINE_H_
#define ATHENA_BROWSER_BROWSER_ENGINE_H_

#include "utils/error.h"

#include <cstdint>
#include <string>

namespace athena {
namespace rendering {
class GLRenderer;
}

namespace browser {

// Forward declarations
using BrowserId = uint64_t;
constexpr BrowserId kInvalidBrowserId = 0;

/**
 * Configuration for initializing the browser engine.
 */
struct EngineConfig {
  std::string cache_path;
  std::string subprocess_path;
  bool enable_sandbox = false;
  bool enable_windowless_rendering = true;
  int windowless_frame_rate = 60;
};

/**
 * Configuration for creating a browser instance.
 */
struct BrowserConfig {
  std::string url;
  int width = 1200;
  int height = 800;
  float device_scale_factor = 1.0f;
  rendering::GLRenderer* gl_renderer = nullptr;  // Non-owning pointer
  void* native_window_handle = nullptr;          // Platform-specific (QWidget*, HWND, etc.)
};

/**
 * Abstract browser engine interface.
 *
 * This interface abstracts the underlying browser engine (CEF, WebKit, etc.)
 * to make the codebase more testable and maintainable.
 *
 * Lifecycle:
 *   1. Create engine
 *   2. Initialize() - must be called before any other operations
 *   3. CreateBrowser() - create browser instances
 *   4. Navigation/interaction operations
 *   5. CloseBrowser() - close browsers
 *   6. Shutdown() - clean shutdown
 *   7. Destroy engine
 */
class BrowserEngine {
 public:
  virtual ~BrowserEngine() = default;

  // ============================================================================
  // Lifecycle Management
  // ============================================================================

  /**
   * Initialize the browser engine.
   * Must be called before any other operations.
   *
   * @param config Engine configuration
   * @return Ok on success, error on failure
   */
  virtual utils::Result<void> Initialize(const EngineConfig& config) = 0;

  /**
   * Shutdown the browser engine.
   * All browsers must be closed before calling this.
   */
  virtual void Shutdown() = 0;

  /**
   * Check if the engine is initialized.
   */
  virtual bool IsInitialized() const = 0;

  // ============================================================================
  // Browser Management
  // ============================================================================

  /**
   * Create a new browser instance.
   *
   * @param config Browser configuration
   * @return Browser ID on success, error on failure
   */
  virtual utils::Result<BrowserId> CreateBrowser(const BrowserConfig& config) = 0;

  /**
   * Close a browser instance.
   *
   * @param id Browser ID
   * @param force_close If true, close immediately without waiting for cleanup
   */
  virtual void CloseBrowser(BrowserId id, bool force_close = false) = 0;

  /**
   * Check if a browser exists.
   */
  virtual bool HasBrowser(BrowserId id) const = 0;

  // ============================================================================
  // Navigation
  // ============================================================================

  /**
   * Load a URL in the browser.
   */
  virtual void LoadURL(BrowserId id, const std::string& url) = 0;

  /**
   * Navigate back in history.
   */
  virtual void GoBack(BrowserId id) = 0;

  /**
   * Navigate forward in history.
   */
  virtual void GoForward(BrowserId id) = 0;

  /**
   * Reload the current page.
   *
   * @param ignore_cache If true, bypass cache
   */
  virtual void Reload(BrowserId id, bool ignore_cache = false) = 0;

  /**
   * Stop loading the current page.
   */
  virtual void StopLoad(BrowserId id) = 0;

  // ============================================================================
  // Browser State
  // ============================================================================

  /**
   * Check if the browser can go back.
   */
  virtual bool CanGoBack(BrowserId id) const = 0;

  /**
   * Check if the browser can go forward.
   */
  virtual bool CanGoForward(BrowserId id) const = 0;

  /**
   * Check if the browser is loading.
   */
  virtual bool IsLoading(BrowserId id) const = 0;

  /**
   * Get the current URL.
   */
  virtual std::string GetURL(BrowserId id) const = 0;

  // ============================================================================
  // Rendering & Display
  // ============================================================================

  /**
   * Notify the browser that the view size changed.
   *
   * @param id Browser ID
   * @param width New width in logical pixels
   * @param height New height in logical pixels
   */
  virtual void SetSize(BrowserId id, int width, int height) = 0;

  /**
   * Set the device scale factor (for HiDPI displays).
   *
   * @param id Browser ID
   * @param scale_factor Scale factor (1.0 = normal, 2.0 = Retina, etc.)
   */
  virtual void SetDeviceScaleFactor(BrowserId id, float scale_factor) = 0;

  /**
   * Notify the browser to repaint.
   */
  virtual void Invalidate(BrowserId id) = 0;

  // ============================================================================
  // Input Events (Platform-independent)
  // ============================================================================

  /**
   * Set focus state.
   */
  virtual void SetFocus(BrowserId id, bool focus) = 0;

  // ============================================================================
  // Message Loop Integration
  // ============================================================================

  /**
   * Perform a single iteration of the message loop.
   * Should be called regularly from the platform's main loop.
   */
  virtual void DoMessageLoopWork() = 0;
};

}  // namespace browser
}  // namespace athena

#endif  // ATHENA_BROWSER_BROWSER_ENGINE_H_

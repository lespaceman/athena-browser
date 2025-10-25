#ifndef ATHENA_PLATFORM_QT_WINDOW_SYSTEM_H_
#define ATHENA_PLATFORM_QT_WINDOW_SYSTEM_H_

#include "platform/window_system.h"

#include <memory>

// Forward declarations for Qt classes (in global namespace)
class QApplication;
class QTimer;

namespace athena {
namespace browser {
class BrowserEngine;
}

namespace platform {

class QtMainWindow;

/**
 * Qt-based window system implementation.
 *
 * Manages Qt initialization and the main event loop.
 * Integrates CEF message loop with Qt's event loop using QTimer.
 */
class QtWindowSystem : public WindowSystem {
 public:
  QtWindowSystem();
  ~QtWindowSystem() override;

  // Disable copy and move
  QtWindowSystem(const QtWindowSystem&) = delete;
  QtWindowSystem& operator=(const QtWindowSystem&) = delete;
  QtWindowSystem(QtWindowSystem&&) = delete;
  QtWindowSystem& operator=(QtWindowSystem&&) = delete;

  // ============================================================================
  // Lifecycle Management
  // ============================================================================

  utils::Result<void> Initialize(int& argc, char* argv[], browser::BrowserEngine* engine) override;
  void Shutdown() override;
  bool IsInitialized() const override;

  // ============================================================================
  // Window Management
  // ============================================================================

  utils::Result<std::shared_ptr<Window>> CreateWindow(const WindowConfig& config,
                                                      const WindowCallbacks& callbacks) override;

  // ============================================================================
  // Event Loop
  // ============================================================================

  void Run() override;
  void Quit() override;
  bool IsRunning() const override;

 private:
  bool initialized_;
  bool running_;
  browser::BrowserEngine* engine_;        // Non-owning
  QApplication* app_;                     // Qt application instance (owned)
  QTimer* cef_timer_;                     // CEF message pump timer (owned by app_)
  std::shared_ptr<QtMainWindow> window_;  // Main window instance
};

}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_QT_WINDOW_SYSTEM_H_

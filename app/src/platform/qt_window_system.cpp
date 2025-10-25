/**
 * QtWindowSystem Implementation
 *
 * Qt-based window system managing initialization and the main event loop.
 * Integrates CEF message loop with Qt's event loop.
 */

#include "platform/qt_window_system.h"

#include "browser/browser_engine.h"
#include "include/cef_app.h"
#include "platform/qt_mainwindow.h"
#include "utils/logging.h"

#include <QApplication>
#include <QTimer>

namespace athena {
namespace platform {

using namespace utils;

static Logger logger("QtWindowSystem");

QtWindowSystem::QtWindowSystem()
    : initialized_(false),
      running_(false),
      engine_(nullptr),
      app_(nullptr),
      cef_timer_(nullptr),
      window_(nullptr) {}

QtWindowSystem::~QtWindowSystem() {
  Shutdown();
}

Result<void> QtWindowSystem::Initialize(int& argc, char* argv[], browser::BrowserEngine* engine) {
  if (initialized_) {
    return Error("WindowSystem already initialized");
  }

  if (!engine) {
    return Error("BrowserEngine cannot be null");
  }

  logger.Info("Initializing Qt window system");

  // Create Qt application
  app_ = new QApplication(argc, argv);
  app_->setApplicationName("Athena Browser");
  app_->setApplicationVersion("1.0");

  // Store engine
  engine_ = engine;

  initialized_ = true;

  logger.Info("Qt window system initialized");
  return Ok();
}

void QtWindowSystem::Shutdown() {
  if (!initialized_)
    return;

  logger.Info("Shutting down Qt window system");

  // Remove CEF message loop callback
  if (cef_timer_) {
    cef_timer_->stop();
    delete cef_timer_;
    cef_timer_ = nullptr;
  }

  window_.reset();

  if (app_) {
    delete app_;
    app_ = nullptr;
  }

  initialized_ = false;
  running_ = false;
  engine_ = nullptr;

  logger.Info("Qt window system shut down");
}

bool QtWindowSystem::IsInitialized() const {
  return initialized_;
}

Result<std::shared_ptr<Window>> QtWindowSystem::CreateWindow(const WindowConfig& config,
                                                             const WindowCallbacks& callbacks) {
  if (!initialized_) {
    return Error("WindowSystem not initialized");
  }

  logger.Info("Creating window");

  window_ = std::make_shared<QtMainWindow>(config, callbacks, engine_);

  return std::static_pointer_cast<Window>(window_);
}

void QtWindowSystem::Run() {
  if (!initialized_) {
    logger.Error("Cannot run: WindowSystem not initialized");
    return;
  }

  logger.Info("Starting Qt event loop");
  running_ = true;

  // ====================================================================
  // CRITICAL: CEF Message Pump Integration
  // ====================================================================
  // CEF requires CefDoMessageLoopWork() to be called regularly to
  // process browser events (painting, navigation, JS execution, etc.)
  //
  // We use a QTimer that fires every 10ms to call CefDoMessageLoopWork()
  // ====================================================================

  cef_timer_ = new QTimer(app_);
  QObject::connect(cef_timer_, &QTimer::timeout, []() {
    // Process CEF events on every timer tick
    CefDoMessageLoopWork();
  });
  cef_timer_->start(10);  // 10ms = ~100 FPS max

  logger.Info("CEF message pump started (10ms interval)");

  // Show window (InitializeBrowser will be called from showEvent)
  if (window_) {
    window_->Show();
  }

  // Run Qt event loop (blocks until quit)
  int exitCode = app_->exec();

  running_ = false;
  logger.Info("Qt event loop exited with code " + std::to_string(exitCode));
}

void QtWindowSystem::Quit() {
  if (running_ && app_) {
    app_->quit();
    running_ = false;
  }
}

bool QtWindowSystem::IsRunning() const {
  return running_;
}

}  // namespace platform
}  // namespace athena

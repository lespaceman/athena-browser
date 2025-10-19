#ifndef ATHENA_TESTS_MOCKS_MOCK_WINDOW_SYSTEM_H_
#define ATHENA_TESTS_MOCKS_MOCK_WINDOW_SYSTEM_H_

#include "platform/window_system.h"

#include <gmock/gmock.h>
#include <map>
#include <memory>

namespace athena {
namespace platform {
namespace testing {

/**
 * Mock Window implementation for testing.
 */
class MockWindow : public Window {
 public:
  MockWindow(const WindowConfig& config,
             const WindowCallbacks& callbacks,
             browser::BrowserEngine* engine)
      : config_(config),
        callbacks_(callbacks),
        engine_(engine),
        visible_(false),
        has_focus_(false),
        closed_(false),
        browser_id_(0),
        scale_factor_(1.0f),
        native_handle_(reinterpret_cast<void*>(0x12345678)),
        render_widget_(reinterpret_cast<void*>(0x87654321)) {}

  // Window Properties
  std::string GetTitle() const override { return config_.title; }

  void SetTitle(const std::string& title) override { config_.title = title; }

  core::Size GetSize() const override { return config_.size; }

  void SetSize(const core::Size& size) override {
    config_.size = size;
    if (callbacks_.on_resize) {
      callbacks_.on_resize(size.width, size.height);
    }
  }

  float GetScaleFactor() const override { return scale_factor_; }

  void SetScaleFactor(float scale) { scale_factor_ = scale; }

  void* GetNativeHandle() const override { return native_handle_; }

  void* GetRenderWidget() const override { return render_widget_; }

  rendering::GLRenderer* GetGLRenderer() const override {
    // Mock implementation - return fake pointer for testing
    // In real tests that need a renderer, this could be set via a setter
    return reinterpret_cast<rendering::GLRenderer*>(0xABCDEF00);
  }

  // Window State
  bool IsVisible() const override { return visible_; }

  void Show() override {
    visible_ = true;
    // Simulate tab creation like GtkWindow::OnRealize()
    // Create a browser if we don't already have one
    if (browser_id_ == 0 && engine_ && engine_->IsInitialized()) {
      browser::BrowserConfig browser_config;
      browser_config.url = config_.url;
      browser_config.width = config_.size.width;
      browser_config.height = config_.size.height;
      browser_config.device_scale_factor = scale_factor_;
      browser_config.gl_renderer = GetGLRenderer();
      browser_config.native_window_handle = render_widget_;

      auto result = engine_->CreateBrowser(browser_config);
      if (result.IsOk()) {
        browser_id_ = result.Value();

        // Load initial URL if provided
        if (!config_.url.empty()) {
          engine_->LoadURL(browser_id_, config_.url);
        }
      }
    }
  }

  void Hide() override { visible_ = false; }

  bool HasFocus() const override { return has_focus_; }

  void Focus() override {
    has_focus_ = true;
    if (callbacks_.on_focus_changed) {
      callbacks_.on_focus_changed(true);
    }
  }

  // Browser Integration
  void SetBrowser(browser::BrowserId browser_id) override { browser_id_ = browser_id; }

  browser::BrowserId GetBrowser() const override { return browser_id_; }

  // Lifecycle
  void Close(bool force) override {
    if (!force && callbacks_.on_close) {
      callbacks_.on_close();
    }
    closed_ = true;
    if (callbacks_.on_destroy) {
      callbacks_.on_destroy();
    }
  }

  bool IsClosed() const override { return closed_; }

  // Test helpers
  void SimulateFocusChange(bool focused) {
    has_focus_ = focused;
    if (callbacks_.on_focus_changed) {
      callbacks_.on_focus_changed(focused);
    }
  }

  void SimulateResize(int width, int height) {
    config_.size = {width, height};
    if (callbacks_.on_resize) {
      callbacks_.on_resize(width, height);
    }
  }

 private:
  WindowConfig config_;
  WindowCallbacks callbacks_;
  browser::BrowserEngine* engine_;  // Non-owning
  bool visible_;
  bool has_focus_;
  bool closed_;
  browser::BrowserId browser_id_;
  float scale_factor_;
  void* native_handle_;
  void* render_widget_;
};

/**
 * Mock WindowSystem implementation for testing.
 */
class MockWindowSystem : public WindowSystem {
 public:
  MockWindowSystem() : initialized_(false), running_(false), engine_(nullptr) {}

  // Lifecycle Management
  utils::Result<void> Initialize(int argc, char* argv[], browser::BrowserEngine* engine) override {
    (void)argc;
    (void)argv;
    if (initialized_) {
      return utils::Error("WindowSystem already initialized");
    }
    engine_ = engine;
    initialized_ = true;
    return utils::Ok();
  }

  void Shutdown() override {
    if (!initialized_) {
      return;
    }
    windows_.clear();
    initialized_ = false;
    engine_ = nullptr;
  }

  bool IsInitialized() const override { return initialized_; }

  // Window Management
  utils::Result<std::shared_ptr<Window>> CreateWindow(const WindowConfig& config,
                                                      const WindowCallbacks& callbacks) override {
    if (!initialized_) {
      return utils::Error("WindowSystem not initialized");
    }

    auto window = std::make_shared<MockWindow>(config, callbacks, engine_);
    auto* window_ptr = window.get();
    windows_[window_ptr] = window_ptr;

    return std::static_pointer_cast<Window>(window);
  }

  // Event Loop
  void Run() override {
    if (!initialized_) {
      return;
    }
    running_ = true;
    // Mock implementation - just set the flag
    // In real tests, you would call Quit() explicitly
  }

  void Quit() override { running_ = false; }

  bool IsRunning() const override { return running_; }

  // Test helpers
  browser::BrowserEngine* GetEngine() const { return engine_; }

  size_t GetWindowCount() const { return windows_.size(); }

 private:
  bool initialized_;
  bool running_;
  browser::BrowserEngine* engine_;
  std::map<void*, MockWindow*> windows_;  // Non-owning pointers for tracking
};

}  // namespace testing
}  // namespace platform
}  // namespace athena

#endif  // ATHENA_TESTS_MOCKS_MOCK_WINDOW_SYSTEM_H_

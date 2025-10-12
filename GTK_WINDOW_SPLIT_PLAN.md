# Plan: Split gtk_window.cpp into Manageable Files

**Created:** 2025-10-12
**Status:** 🟦 Ready to Start
**Estimated Time:** 4 hours
**Target Completion:** TBD

---

## 📊 Progress Tracker

### Overall Progress

| Phase | Status | Estimated | Actual | Tasks | Progress |
|-------|--------|-----------|--------|-------|----------|
| **Phase 1: Preparation** | ✅ Complete | 1h | 0.5h | 3/5 | 60% |
| **Phase 2: Extract GtkWindowSystem** | ✅ Complete | 30m | 0.2h | 5/5 | 100% |
| **Phase 3: Extract Tab Management** | ✅ Complete | 45m | 0.3h | 5/5 | 100% |
| **Phase 4: Extract GTK Callbacks** | ⬜ Not Started | 1h | — | 0/7 | 0% |
| **Phase 5: Verification** | ⬜ Not Started | 30m | — | 0/4 | 0% |
| **Phase 6: Merge & Cleanup** | ⬜ Not Started | 15m | — | 0/3 | 0% |
| **TOTAL** | 🟦 **45%** | **4h** | **1.0h** | **13/29** | **45%** |

**Legend:**
- ⬜ Not Started | 🟦 In Progress | ✅ Complete | ⚠️ Blocked | ❌ Failed

---

## 🎯 Current Status

| Metric | Value | Target | Status |
|--------|-------|--------|--------|
| **gtk_window.cpp** | 1,028 LOC | 800 LOC | ⚠️ +28% over |
| **gtk_window_tabs.cpp** | 381 LOC (NEW) | 800 LOC | ✅ 52% under limit |
| **gtk_window_system.cpp** | 112 LOC (NEW) | 800 LOC | ✅ 86% under limit |
| **gtk_window.h** | 374 LOC | 800 LOC | ✅ Within limit |
| **Complexity** | Medium | Medium | 🟦 Improving |
| **Test Status** | 274/274 passing | 274/274 | ✅ All tests pass |

**Problem:** The file handles too many concerns:
- GTK event callbacks (16 static functions)
- Window lifecycle
- Tab management
- Input event helpers
- Address bar updates
- GtkWindowSystem implementation

---

## 📐 Proposed Split Strategy

### Target Architecture

```
gtk_window.cpp (1,076 LOC)
    ↓
Split into 4 files
    ↓
├── gtk_window.cpp (380 LOC) - Core window management ✅ 52% under limit
├── gtk_window_callbacks.cpp (300 LOC) - GTK event handlers ✅ 62% under limit
├── gtk_window_tabs.cpp (320 LOC) - Tab operations ✅ 60% under limit
└── gtk_window_system.cpp (95 LOC) - Window system ✅ 88% under limit
```

**Benefits:**
- ✅ All files under 400 LOC (50% under 800 LOC limit)
- ✅ Clear separation of concerns
- ✅ Easier to navigate and maintain
- ✅ Better testability (can test tab logic separately)
- ✅ No API changes (all public methods stay in GtkWindow)

---

## 📋 Detailed Implementation Plan

### Phase 1: Preparation (1h)

| # | Task | Status | Time Est. | Time Actual | Notes |
|---|------|--------|-----------|-------------|-------|
| 1.1 | Enable tab tests | ⬜ | 15m | — | Remove DISABLED_ prefix |
| 1.2 | Create backup files | ⬜ | 5m | — | Save gtk_window.{cpp,h} |
| 1.3 | Create git branch | ⬜ | 5m | — | refactor/split-gtk-window |
| 1.4 | Analyze line ranges | ⬜ | 15m | — | Document exact splits |
| 1.5 | Write smoke tests | ⬜ | 20m | — | Verify split doesn't break |

<details>
<summary><b>Step 1.1: Enable Tab Tests (15 min)</b></summary>

```bash
# Verify all bugs are actually fixed
sed -i 's/DISABLED_//g' app/tests/platform/gtk_window_tabs_test.cpp

# Build and test
./scripts/build.sh
GDK_BACKEND=x11 ./build/release/app/tests/gtk_window_tabs_test

# Expected: All tests should PASS ✅
```

**Success Criteria:**
- [ ] All 7 tab tests passing
- [ ] No crashes or segfaults
- [ ] Test output shows all assertions pass

</details>

<details>
<summary><b>Step 1.2: Create Backup (5 min)</b></summary>

```bash
cp app/src/platform/gtk_window.cpp app/src/platform/gtk_window.cpp.backup
cp app/src/platform/gtk_window.h app/src/platform/gtk_window.h.backup
```

**Success Criteria:**
- [ ] Backup files created
- [ ] Original files unchanged

</details>

<details>
<summary><b>Step 1.3: Create Branch (5 min)</b></summary>

```bash
git checkout -b refactor/split-gtk-window
git add app/tests/platform/gtk_window_tabs_test.cpp
git commit -m "test: enable gtk_window tab tests (all bugs fixed)"
```

**Success Criteria:**
- [ ] Branch created
- [ ] Test file committed
- [ ] Git status clean

</details>

<details>
<summary><b>Step 1.4: Analyze Line Ranges (15 min)</b></summary>

Create `SPLIT_LINE_RANGES.md`:

```markdown
# GTK Window Split - Line Ranges

## gtk_window.cpp (current: 1,076 LOC)

### gtk_window_callbacks.cpp (lines 16-527)
- Anonymous namespace with all static callbacks
- Helper functions (GetCefModifiers, GetWindowsKeyCode)
- Input event handlers (mouse, keyboard, focus)
- Window lifecycle handlers
- Navigation button handlers
- Tab event handlers
- Address bar update helpers

### gtk_window_tabs.cpp (lines 697-705, 1060-1376)
- CreateBrowser (legacy method)
- CreateTab
- CloseTab
- CloseTabByBrowserId
- SwitchToTab
- GetTabCount
- GetActiveTabIndex
- GetActiveTab
- OnTabSwitch
- OnNewTabClicked
- OnCloseTabClicked

### gtk_window_system.cpp (lines 1381-1474)
- GtkWindowSystem::GtkWindowSystem()
- GtkWindowSystem::~GtkWindowSystem()
- Initialize()
- Shutdown()
- IsInitialized()
- CreateWindow()
- Run()
- Quit()
- IsRunning()
- OnCefMessageLoopWork()

### gtk_window.cpp (remaining core methods)
- Constructor/Destructor (lines 533-566)
- InitializeWindow (lines 568-615)
- CreateToolbar (lines 617-660)
- SetupEventHandlers (lines 662-695)
- GetCefClient (lines 711-717)
- Window properties (lines 719-758)
- Window state (lines 764-793)
- Browser integration (lines 799-819)
- Lifecycle (lines 825-838)
- GTK callbacks (lines 844-952)
- Navigation methods (lines 958-991)
- Address bar updates (lines 997-1054)
```

**Success Criteria:**
- [ ] All line ranges documented
- [ ] No overlaps between files
- [ ] All methods accounted for

</details>

<details>
<summary><b>Step 1.5: Write Smoke Tests (20 min)</b></summary>

Create `app/tests/platform/gtk_window_split_test.cpp`:

```cpp
/**
 * Smoke test to verify gtk_window.cpp split doesn't break functionality
 */
#include "platform/gtk_window.h"
#include "mocks/mock_browser_engine.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace athena::platform;
using namespace athena::browser;
using ::testing::_;
using ::testing::Return;

class GtkWindowSplitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int argc = 0;
    char** argv = nullptr;
    gtk_init(&argc, &argv);

    ON_CALL(mock_engine_, IsInitialized())
        .WillByDefault(Return(true));
  }

  testing::MockBrowserEngine mock_engine_;
};

TEST_F(GtkWindowSplitTest, WindowCreationWorks) {
  WindowConfig config;
  config.title = "Test";
  config.size = {800, 600};

  WindowCallbacks callbacks;

  GtkWindow window(config, callbacks, &mock_engine_);

  EXPECT_EQ(window.GetTitle(), "Test");
  EXPECT_FALSE(window.IsVisible());
}

TEST_F(GtkWindowSplitTest, TabMethodsAccessible) {
  WindowConfig config;
  WindowCallbacks callbacks;

  GtkWindow window(config, callbacks, &mock_engine_);

  // Verify tab methods are accessible
  EXPECT_EQ(window.GetTabCount(), 0);
  EXPECT_EQ(window.GetActiveTabIndex(), 0);
  EXPECT_EQ(window.GetActiveTab(), nullptr);
}

TEST_F(GtkWindowSplitTest, NavigationMethodsAccessible) {
  WindowConfig config;
  WindowCallbacks callbacks;

  GtkWindow window(config, callbacks, &mock_engine_);

  // These should not crash even without a browser
  window.LoadURL("https://example.com");
  window.GoBack();
  window.GoForward();
  window.Reload();
  window.StopLoad();

  SUCCEED();
}
```

Add to `app/tests/CMakeLists.txt`:

```cmake
add_athena_test(gtk_window_split_test
  platform/gtk_window_split_test.cpp
  ../src/platform/gtk_window.cpp
  ../src/platform/gtk_window_system.cpp
  ../src/platform/gtk_window_tabs.cpp
  ../src/platform/gtk_window_callbacks.cpp
)
```

**Success Criteria:**
- [ ] Test file compiles
- [ ] All 3 tests pass
- [ ] No crashes or segfaults

</details>

---

### Phase 2: Extract GtkWindowSystem (30m)

| # | Task | Status | Time Est. | Time Actual | Notes |
|---|------|--------|-----------|-------------|-------|
| 2.1 | Create new file | ⬜ | 10m | — | gtk_window_system.cpp |
| 2.2 | Update CMakeLists.txt | ⬜ | 5m | — | Add new source file |
| 2.3 | Remove from original | ⬜ | 5m | — | Delete lines 1381-1474 |
| 2.4 | Test build | ⬜ | 10m | — | Verify 267 tests pass |
| 2.5 | Commit changes | ⬜ | 5m | — | Git commit |

<details>
<summary><b>Step 2.1: Create gtk_window_system.cpp (10 min)</b></summary>

```bash
cat > app/src/platform/gtk_window_system.cpp << 'EOF'
/**
 * GtkWindowSystem Implementation
 *
 * Manages GTK initialization and the main event loop.
 * Integrates CEF message loop with GTK's event loop.
 */
#include "platform/gtk_window.h"
#include "browser/browser_engine.h"
#include "include/cef_app.h"
#include <iostream>

namespace athena {
namespace platform {

// ============================================================================
// GtkWindowSystem Implementation
// ============================================================================

GtkWindowSystem::GtkWindowSystem()
    : initialized_(false),
      running_(false),
      engine_(nullptr),
      message_loop_source_id_(0) {}

GtkWindowSystem::~GtkWindowSystem() {
  Shutdown();
}

utils::Result<void> GtkWindowSystem::Initialize(int argc, char* argv[],
                                                 browser::BrowserEngine* engine) {
  if (initialized_) {
    return utils::Error("WindowSystem already initialized");
  }

  if (!engine) {
    return utils::Error("BrowserEngine cannot be null");
  }

  // Disable GTK setlocale (CEF requirement)
  gtk_disable_setlocale();

  // Initialize GTK
  gtk_init(&argc, &argv);

  engine_ = engine;
  initialized_ = true;

  // Setup CEF message loop integration
  message_loop_source_id_ = g_idle_add(OnCefMessageLoopWork, this);

  return utils::Ok();
}

void GtkWindowSystem::Shutdown() {
  if (!initialized_) return;

  // Remove CEF message loop callback
  if (message_loop_source_id_ != 0) {
    g_source_remove(message_loop_source_id_);
    message_loop_source_id_ = 0;
  }

  initialized_ = false;
  running_ = false;
  engine_ = nullptr;
}

bool GtkWindowSystem::IsInitialized() const {
  return initialized_;
}

utils::Result<std::unique_ptr<Window>> GtkWindowSystem::CreateWindow(
    const WindowConfig& config,
    const WindowCallbacks& callbacks) {
  if (!initialized_) {
    return utils::Error("WindowSystem not initialized");
  }

  auto window = std::make_unique<GtkWindow>(config, callbacks, engine_);
  return std::unique_ptr<Window>(std::move(window));
}

void GtkWindowSystem::Run() {
  if (!initialized_) {
    std::cerr << "[GtkWindowSystem] Cannot run: WindowSystem not initialized" << std::endl;
    return;
  }

  running_ = true;
  gtk_main();
  running_ = false;
}

void GtkWindowSystem::Quit() {
  if (running_) {
    gtk_main_quit();
    running_ = false;
  }
}

bool GtkWindowSystem::IsRunning() const {
  return running_;
}

gboolean GtkWindowSystem::OnCefMessageLoopWork(gpointer data) {
  (void)data;
  CefDoMessageLoopWork();
  return G_SOURCE_CONTINUE;
}

}  // namespace platform
}  // namespace athena
EOF
```

**Success Criteria:**
- [ ] File created with 95 LOC
- [ ] All methods from original included
- [ ] Proper namespace structure
- [ ] Copyright notice added

</details>

<details>
<summary><b>Step 2.2: Update CMakeLists.txt (5 min)</b></summary>

Edit `app/CMakeLists.txt`:

```cmake
# Find the ATHENA_PLATFORM_SOURCES section and update:
set(ATHENA_PLATFORM_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window_system.cpp  # NEW
)
```

**Success Criteria:**
- [ ] CMakeLists.txt updated
- [ ] New source file listed
- [ ] Syntax correct (no cmake errors)

</details>

<details>
<summary><b>Step 2.3: Remove from Original (5 min)</b></summary>

```bash
# Edit app/src/platform/gtk_window.cpp
# Delete lines 1381-1474 (GtkWindowSystem implementation)
# Keep everything else intact
```

**Success Criteria:**
- [ ] Lines removed from gtk_window.cpp
- [ ] File now ~981 LOC
- [ ] No syntax errors introduced

</details>

<details>
<summary><b>Step 2.4: Test Build (10 min)</b></summary>

```bash
# Clean build to catch any issues
./scripts/build.sh

# Run all tests
ctest --test-dir build/release --output-on-failure

# Expected: All 267 tests pass ✅
```

**Success Criteria:**
- [ ] Build succeeds with zero errors
- [ ] All 267 tests passing
- [ ] Test runtime <2s
- [ ] No new warnings

</details>

<details>
<summary><b>Step 2.5: Commit Changes (5 min)</b></summary>

```bash
git add app/src/platform/gtk_window_system.cpp \
        app/CMakeLists.txt \
        app/src/platform/gtk_window.cpp

git commit -m "refactor: extract GtkWindowSystem to separate file

- Move GtkWindowSystem implementation to gtk_window_system.cpp
- Reduces gtk_window.cpp from 1,076 to 981 LOC (-9%)
- All 267 tests still passing
- Zero regressions, zero API changes"
```

**Success Criteria:**
- [ ] Clean git status
- [ ] Commit message descriptive
- [ ] Changes staged correctly

</details>

---

### Phase 3: Extract Tab Management (45m)

| # | Task | Status | Time Est. | Time Actual | Notes |
|---|------|--------|-----------|-------------|-------|
| 3.1 | Create new file | ⬜ | 15m | — | gtk_window_tabs.cpp |
| 3.2 | Update CMakeLists.txt | ⬜ | 5m | — | Add new source file |
| 3.3 | Remove from original | ⬜ | 5m | — | Delete tab methods |
| 3.4 | Test build | ⬜ | 15m | — | Verify tabs work |
| 3.5 | Commit changes | ⬜ | 5m | — | Git commit |

<details>
<summary><b>Step 3.1: Create gtk_window_tabs.cpp (15 min)</b></summary>

```bash
cat > app/src/platform/gtk_window_tabs.cpp << 'EOF'
/**
 * GtkWindow Tab Management Implementation
 *
 * Handles all tab-related operations:
 * - Creating new tabs
 * - Closing tabs
 * - Switching between tabs
 * - Tab callbacks (address change, loading state, title)
 */
#include "platform/gtk_window.h"
#include "browser/cef_engine.h"
#include "browser/cef_client.h"
#include <iostream>

namespace athena {
namespace platform {

// ============================================================================
// Tab Management Methods
// ============================================================================

utils::Result<void> GtkWindow::CreateBrowser(const std::string& url) {
  // Legacy method - for backward compatibility, create a tab
  // Modern code should use CreateTab() directly
  int tab_index = CreateTab(url);
  if (tab_index >= 0) {
    return utils::Ok();
  }
  return utils::Error("Failed to create tab");
}

int GtkWindow::CreateTab(const std::string& url) {
  if (!gl_renderer_) {
    std::cerr << "[GtkWindow::CreateTab] GLRenderer not initialized" << std::endl;
    return -1;
  }

  if (!engine_) {
    std::cerr << "[GtkWindow::CreateTab] BrowserEngine not available" << std::endl;
    return -1;
  }

  std::cout << "[GtkWindow::CreateTab] Creating tab with URL: " << url << std::endl;

  // Create Tab structure
  Tab tab;
  tab.url = url;
  tab.title = "New Tab";
  tab.is_loading = true;
  tab.can_go_back = false;
  tab.can_go_forward = false;

  // Create browser instance
  float scale_factor = static_cast<float>(gtk_widget_get_scale_factor(gl_area_));

  browser::BrowserConfig browser_config;
  browser_config.url = url;
  browser_config.width = config_.size.width;
  browser_config.height = config_.size.height;
  browser_config.device_scale_factor = scale_factor;
  browser_config.gl_renderer = gl_renderer_.get();
  browser_config.native_window_handle = gl_area_;

  auto result = engine_->CreateBrowser(browser_config);
  if (!result) {
    std::cerr << "[GtkWindow::CreateTab] Failed to create browser: "
              << result.GetError().Message() << std::endl;
    return -1;
  }

  tab.browser_id = result.Value();

  // Get the CEF client
  auto* cef_engine = dynamic_cast<browser::CefEngine*>(engine_);
  if (cef_engine) {
    auto client = cef_engine->GetCefClient(tab.browser_id);
    if (client) {
      tab.cef_client = client.get();

      // Wire up callbacks for this tab
      // Use browser_id instead of index to avoid stale references after tab closure
      browser::BrowserId bid = tab.browser_id;

      tab.cef_client->SetAddressChangeCallback([this, bid](const std::string& url) {
        std::lock_guard<std::mutex> lock(tabs_mutex_);
        auto it = std::find_if(tabs_.begin(), tabs_.end(),
          [bid](const Tab& t) { return t.browser_id == bid; });
        if (it != tabs_.end()) {
          it->url = url;
          size_t tab_index = std::distance(tabs_.begin(), it);
          if (tab_index == active_tab_index_) {
            this->UpdateAddressBar(url);
          }
        }
      });

      tab.cef_client->SetLoadingStateChangeCallback([this, bid](bool is_loading, bool can_go_back, bool can_go_forward) {
        std::lock_guard<std::mutex> lock(tabs_mutex_);
        auto it = std::find_if(tabs_.begin(), tabs_.end(),
          [bid](const Tab& t) { return t.browser_id == bid; });
        if (it != tabs_.end()) {
          it->is_loading = is_loading;
          it->can_go_back = can_go_back;
          it->can_go_forward = can_go_forward;
          size_t tab_index = std::distance(tabs_.begin(), it);
          if (tab_index == active_tab_index_) {
            this->UpdateNavigationButtons(is_loading, can_go_back, can_go_forward);
          }
        }
      });

      tab.cef_client->SetTitleChangeCallback([this, bid](const std::string& title) {
        std::lock_guard<std::mutex> lock(tabs_mutex_);
        auto it = std::find_if(tabs_.begin(), tabs_.end(),
          [bid](const Tab& t) { return t.browser_id == bid; });
        if (it != tabs_.end()) {
          it->title = title;
          // Update the tab label on the GTK main thread
          g_idle_add([](gpointer user_data) -> gboolean {
            auto* data = static_cast<std::pair<GtkWidget*, std::string>*>(user_data);
            gtk_label_set_text(GTK_LABEL(data->first), data->second.c_str());
            delete data;
            return G_SOURCE_REMOVE;
          }, new std::pair<GtkWidget*, std::string>(it->tab_label, title));
        }
      });

      std::cout << "[GtkWindow::CreateTab] Callbacks wired for browser_id " << bid << std::endl;
    }
  }

  // Create tab label with close button
  GtkWidget* tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget* label = gtk_label_new(tab.title.c_str());
  GtkWidget* close_btn = gtk_button_new_with_label("✕");
  gtk_widget_set_size_request(close_btn, 20, 20);

  gtk_box_pack_start(GTK_BOX(tab_box), label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tab_box), close_btn, FALSE, FALSE, 0);
  gtk_widget_show_all(tab_box);

  // Enable middle-click to close on the tab box
  GtkWidget* event_box = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(event_box), tab_box);
  gtk_widget_add_events(event_box, GDK_BUTTON_PRESS_MASK);
  g_object_set_data(G_OBJECT(event_box), "window", this);
  g_object_set_data(G_OBJECT(event_box), "browser_id",
    reinterpret_cast<gpointer>(static_cast<uintptr_t>(tab.browser_id)));
  gtk_widget_show(event_box);

  tab.tab_label = label;
  tab.close_button = close_btn;

  // Add empty page to notebook (we don't need content, just the tab)
  GtkWidget* empty_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), empty_page, event_box);

  // Store tab
  tabs_.push_back(tab);
  size_t new_tab_index = tabs_.size() - 1;

  // Connect close button signal
  g_object_set_data(G_OBJECT(close_btn), "window", this);
  g_object_set_data(G_OBJECT(close_btn), "browser_id",
    reinterpret_cast<gpointer>(static_cast<uintptr_t>(tab.browser_id)));

  // Switch to the new tab
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), new_tab_index);

  std::cout << "[GtkWindow::CreateTab] Tab created successfully, index: " << new_tab_index << std::endl;
  return static_cast<int>(new_tab_index);
}

void GtkWindow::CloseTab(size_t index) {
  browser::BrowserId browser_to_close = 0;
  size_t new_active_index = 0;
  bool should_close_window = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (index >= tabs_.size()) {
      std::cerr << "[GtkWindow::CloseTab] Invalid tab index: " << index << std::endl;
      return;
    }

    std::cout << "[GtkWindow::CloseTab] Closing tab " << index << std::endl;

    browser_to_close = tabs_[index].browser_id;

    // Remove the notebook page
    gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_), index);

    // Remove from tabs vector
    tabs_.erase(tabs_.begin() + index);

    // Check if we closed the last tab
    should_close_window = tabs_.empty();

    // Adjust active tab index if needed
    if (!should_close_window && active_tab_index_ >= tabs_.size()) {
      active_tab_index_ = tabs_.size() - 1;
    }

    new_active_index = active_tab_index_;
  }

  // Close the browser instance (outside lock)
  if (engine_ && browser_to_close != 0) {
    engine_->CloseBrowser(browser_to_close, false);
  }

  // If we closed the last tab, close the window
  if (should_close_window) {
    std::cout << "[GtkWindow::CloseTab] No tabs left, closing window" << std::endl;
    Close();
    return;
  }

  // Switch to the new active tab (outside lock to avoid deadlock)
  SwitchToTab(new_active_index);
}

void GtkWindow::CloseTabByBrowserId(browser::BrowserId browser_id) {
  size_t index_to_close = 0;
  bool found = false;
  size_t new_active_index = 0;
  bool should_close_window = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
      [browser_id](const Tab& t) { return t.browser_id == browser_id; });

    if (it != tabs_.end()) {
      found = true;
      index_to_close = std::distance(tabs_.begin(), it);
      std::cout << "[GtkWindow::CloseTabByBrowserId] Found tab at index " << index_to_close
                << " for browser_id " << browser_id << std::endl;

      // Remove the notebook page
      gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_), index_to_close);

      // Remove from tabs vector
      tabs_.erase(it);

      // Check if we closed the last tab
      should_close_window = tabs_.empty();

      // Adjust active tab index if needed
      if (!should_close_window && active_tab_index_ >= tabs_.size()) {
        active_tab_index_ = tabs_.size() - 1;
      }

      new_active_index = active_tab_index_;
    }
  }

  if (!found) {
    std::cerr << "[GtkWindow::CloseTabByBrowserId] Tab with browser_id " << browser_id
              << " not found" << std::endl;
    return;
  }

  // Close the browser instance (outside lock)
  if (engine_ && browser_id != 0) {
    engine_->CloseBrowser(browser_id, false);
  }

  // If we closed the last tab, close the window
  if (should_close_window) {
    std::cout << "[GtkWindow::CloseTabByBrowserId] No tabs left, closing window" << std::endl;
    Close();
    return;
  }

  // Switch to the new active tab (outside lock to avoid deadlock)
  SwitchToTab(new_active_index);
}

void GtkWindow::SwitchToTab(size_t index) {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  if (index >= tabs_.size()) {
    std::cerr << "[GtkWindow::SwitchToTab] Invalid tab index: " << index << std::endl;
    return;
  }

  std::cout << "[GtkWindow::SwitchToTab] Switching to tab " << index << std::endl;

  active_tab_index_ = index;
  Tab& tab = tabs_[index];

  // Update address bar and navigation buttons
  UpdateAddressBar(tab.url);
  UpdateNavigationButtons(tab.is_loading, tab.can_go_back, tab.can_go_forward);

  // Set focus to the browser
  if (tab.cef_client && tab.cef_client->GetBrowser()) {
    tab.cef_client->GetBrowser()->GetHost()->SetFocus(has_focus_);
  }

  // Request a render to show the new tab's content
  if (gl_area_) {
    gtk_gl_area_queue_render(GTK_GL_AREA(gl_area_));
  }

  std::cout << "[GtkWindow::SwitchToTab] Switched to tab " << index
            << ", URL: " << tab.url << std::endl;
}

size_t GtkWindow::GetTabCount() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return tabs_.size();
}

size_t GtkWindow::GetActiveTabIndex() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return active_tab_index_;
}

Tab* GtkWindow::GetActiveTab() {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  if (active_tab_index_ < tabs_.size()) {
    return &tabs_[active_tab_index_];
  }
  return nullptr;
}

void GtkWindow::OnTabSwitch(int page_num) {
  std::cout << "[GtkWindow::OnTabSwitch] Tab switched to page: " << page_num << std::endl;
  if (page_num >= 0) {
    size_t tab_count = GetTabCount();  // Thread-safe
    if (static_cast<size_t>(page_num) < tab_count) {
      SwitchToTab(static_cast<size_t>(page_num));
    }
  }
}

void GtkWindow::OnNewTabClicked() {
  std::cout << "[GtkWindow::OnNewTabClicked] Creating new tab" << std::endl;
  CreateTab("https://www.google.com");
}

void GtkWindow::OnCloseTabClicked(size_t tab_index) {
  std::cout << "[GtkWindow::OnCloseTabClicked] Closing tab: " << tab_index << std::endl;
  CloseTab(tab_index);
}

}  // namespace platform
}  // namespace athena
EOF
```

**Success Criteria:**
- [ ] File created with ~320 LOC
- [ ] All tab methods included
- [ ] Thread-safety preserved (std::lock_guard)
- [ ] browser_id-based callbacks (not index-based)

</details>

<details>
<summary><b>Step 3.2: Update CMakeLists.txt (5 min)</b></summary>

```cmake
set(ATHENA_PLATFORM_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window_system.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window_tabs.cpp  # NEW
)
```

**Success Criteria:**
- [ ] CMakeLists.txt updated
- [ ] New source file added
- [ ] Proper ordering maintained

</details>

<details>
<summary><b>Step 3.3: Remove from Original (5 min)</b></summary>

```bash
# Edit app/src/platform/gtk_window.cpp
# Delete lines 697-705 (CreateBrowser legacy method)
# Delete lines 1060-1376 (all tab methods)
```

**Success Criteria:**
- [ ] Tab methods removed
- [ ] File now ~665 LOC
- [ ] No syntax errors

</details>

<details>
<summary><b>Step 3.4: Test Build (15 min)</b></summary>

```bash
./scripts/build.sh
ctest --test-dir build/release --output-on-failure

# Run tab-specific tests
GDK_BACKEND=x11 ./build/release/app/tests/gtk_window_tabs_test

# Manual test: Run browser and try tabs
./scripts/run.sh
# Try: Ctrl+T (new tab), Ctrl+W (close tab), Ctrl+Tab (switch)
```

**Success Criteria:**
- [ ] Build succeeds
- [ ] All 267+ tests passing
- [ ] Tab tests pass (7 tests)
- [ ] Browser tabs work correctly
- [ ] Keyboard shortcuts work

</details>

<details>
<summary><b>Step 3.5: Commit Changes (5 min)</b></summary>

```bash
git add app/src/platform/gtk_window_tabs.cpp \
        app/CMakeLists.txt \
        app/src/platform/gtk_window.cpp

git commit -m "refactor: extract tab management to separate file

- Move all tab methods to gtk_window_tabs.cpp
- Reduces gtk_window.cpp from 981 to 665 LOC (-32%)
- All 267+ tests still passing
- Tab functionality fully preserved
- Thread-safety maintained with std::mutex
- browser_id-based callbacks prevent index bugs"
```

**Success Criteria:**
- [ ] Clean git status
- [ ] Commit includes all changed files
- [ ] Commit message descriptive

</details>

---

### Phase 4: Extract GTK Callbacks (1h)

| # | Task | Status | Time Est. | Time Actual | Notes |
|---|------|--------|-----------|-------------|-------|
| 4.1 | Create callback header | ⬜ | 15m | — | gtk_window_callbacks.h |
| 4.2 | Create callback implementation | ⬜ | 20m | — | gtk_window_callbacks.cpp |
| 4.3 | Update SetupEventHandlers | ⬜ | 10m | — | Use registration helpers |
| 4.4 | Update CMakeLists.txt | ⬜ | 5m | — | Add callback files |
| 4.5 | Remove from original | ⬜ | 5m | — | Delete anonymous namespace |
| 4.6 | Test build | ⬜ | 10m | — | Verify all events work |
| 4.7 | Commit changes | ⬜ | 5m | — | Final commit |

<details>
<summary><b>Step 4.1: Create gtk_window_callbacks.h (15 min)</b></summary>

```bash
cat > app/src/platform/gtk_window_callbacks.h << 'EOF'
/**
 * GTK Window Event Callbacks
 *
 * Internal header for GTK event callback registration.
 * All callbacks forward to GtkWindow methods via user_data pointer.
 */
#ifndef ATHENA_PLATFORM_GTK_WINDOW_CALLBACKS_H_
#define ATHENA_PLATFORM_GTK_WINDOW_CALLBACKS_H_

#include <gtk/gtk.h>

namespace athena {
namespace platform {

class GtkWindow;

/**
 * Callback registration helpers.
 * Groups related callbacks for easier setup.
 */
namespace callbacks {

/**
 * Register window lifecycle callbacks.
 * - delete-event
 * - destroy
 * - key-press-event (for Ctrl+T, Ctrl+W, etc.)
 */
void RegisterWindowCallbacks(GtkWidget* window, GtkWindow* self);

/**
 * Register OpenGL callbacks.
 * - realize (GL context creation)
 * - render (frame rendering)
 * - size-allocate (window resize)
 */
void RegisterGLCallbacks(GtkWidget* gl_area, GtkWindow* self);

/**
 * Register input event callbacks.
 * - button-press-event
 * - button-release-event
 * - motion-notify-event
 * - scroll-event
 * - leave-notify-event
 * - key-press-event
 * - key-release-event
 * - focus-in-event
 * - focus-out-event
 */
void RegisterInputCallbacks(GtkWidget* gl_area, GtkWindow* self);

/**
 * Register toolbar button callbacks.
 * - back, forward, reload, stop buttons
 * - address bar activation
 * - new tab button
 */
void RegisterToolbarCallbacks(GtkWidget* back_btn,
                               GtkWidget* forward_btn,
                               GtkWidget* reload_btn,
                               GtkWidget* stop_btn,
                               GtkWidget* address_entry,
                               GtkWidget* new_tab_btn,
                               GtkWindow* self);

/**
 * Register tab event callbacks.
 * - switch-page (tab switching)
 */
void RegisterTabCallbacks(GtkWidget* notebook, GtkWindow* self);

}  // namespace callbacks
}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_GTK_WINDOW_CALLBACKS_H_
EOF
```

**Success Criteria:**
- [ ] Header created with registration functions
- [ ] Clear documentation for each group
- [ ] Include guards correct
- [ ] Forward declarations present

</details>

<details>
<summary><b>Step 4.2: Create gtk_window_callbacks.cpp (20 min)</b></summary>

Create `app/src/platform/gtk_window_callbacks.cpp` with:
- All static callback functions from anonymous namespace
- Helper functions (GetCefModifiers, GetWindowsKeyCode)
- Registration function implementations

**Success Criteria:**
- [ ] File created with ~300 LOC
- [ ] All 16+ callbacks included
- [ ] Helper functions included
- [ ] Registration functions implemented
- [ ] All callbacks forward to GtkWindow methods

</details>

<details>
<summary><b>Step 4.3: Update SetupEventHandlers (10 min)</b></summary>

Edit `gtk_window.cpp` to use registration helpers:

```cpp
#include "platform/gtk_window_callbacks.h"

void GtkWindow::SetupEventHandlers() {
  callbacks::RegisterWindowCallbacks(window_, this);
  callbacks::RegisterGLCallbacks(gl_area_, this);
  callbacks::RegisterTabCallbacks(notebook_, this);

  if (config_.enable_input) {
    callbacks::RegisterInputCallbacks(gl_area_, this);
  }

  callbacks::RegisterToolbarCallbacks(
    back_button_, forward_button_, reload_button_,
    stop_button_, address_entry_, new_tab_button_, this);
}
```

**Success Criteria:**
- [ ] SetupEventHandlers simplified
- [ ] All callbacks still registered
- [ ] Conditional input setup preserved

</details>

<details>
<summary><b>Step 4.4: Update CMakeLists.txt (5 min)</b></summary>

```cmake
set(ATHENA_PLATFORM_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window_callbacks.cpp  # NEW
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window_system.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gtk_window_tabs.cpp
)
```

**Success Criteria:**
- [ ] Callbacks file added to build
- [ ] Proper ordering maintained

</details>

<details>
<summary><b>Step 4.5: Remove from Original (5 min)</b></summary>

```bash
# Edit app/src/platform/gtk_window.cpp
# Delete lines 16-527 (anonymous namespace with all static callbacks)
# Add: #include "platform/gtk_window_callbacks.h"
```

**Success Criteria:**
- [ ] Anonymous namespace removed
- [ ] File now ~380 LOC ✅
- [ ] Include added for callbacks

</details>

<details>
<summary><b>Step 4.6: Test Build (10 min)</b></summary>

```bash
./scripts/build.sh
ctest --test-dir build/release --output-on-failure

# Run browser and test all inputs
./scripts/run.sh
# Test: mouse clicks, keyboard input, scroll, navigation buttons, tabs
```

**Success Criteria:**
- [ ] Build succeeds
- [ ] All 267+ tests passing
- [ ] Mouse input works
- [ ] Keyboard input works
- [ ] Navigation buttons work
- [ ] Tab shortcuts work (Ctrl+T, Ctrl+W, etc.)

</details>

<details>
<summary><b>Step 4.7: Commit Changes (5 min)</b></summary>

```bash
git add app/src/platform/gtk_window_callbacks.{h,cpp} \
        app/CMakeLists.txt \
        app/src/platform/gtk_window.cpp

git commit -m "refactor: extract GTK callbacks to separate file

- Move all static callbacks to gtk_window_callbacks.cpp
- Add callback registration helpers for clarity
- Reduces gtk_window.cpp from 665 to 380 LOC (-43%)
- All 267+ tests still passing
- gtk_window.cpp now UNDER 800 LOC limit (52% under) ✅

Split complete:
- gtk_window.cpp: 380 LOC (core)
- gtk_window_callbacks.cpp: 300 LOC (events)
- gtk_window_tabs.cpp: 320 LOC (tabs)
- gtk_window_system.cpp: 95 LOC (system)
Total: ~1,095 LOC (was 1,076 LOC)"
```

**Success Criteria:**
- [ ] All files committed
- [ ] Commit message comprehensive
- [ ] Metrics documented

</details>

---

### Phase 5: Verification & Documentation (30m)

| # | Task | Status | Time Est. | Time Actual | Notes |
|---|------|--------|-----------|-------------|-------|
| 5.1 | Run full test suite | ⬜ | 10m | — | All automated tests |
| 5.2 | Check code metrics | ⬜ | 5m | — | Verify LOC counts |
| 5.3 | Update documentation | ⬜ | 10m | — | REFACTOR_PROGRESS.md |
| 5.4 | Manual testing | ⬜ | 5m | — | Run browser, test features |

<details>
<summary><b>Step 5.1: Run Full Test Suite (10 min)</b></summary>

```bash
# Clean build
rm -rf build/release
./scripts/build.sh

# Run all tests
ctest --test-dir build/release --output-on-failure --verbose

# Run tab tests specifically
GDK_BACKEND=x11 ./build/release/app/tests/gtk_window_tabs_test

# Run split smoke test
./build/release/app/tests/gtk_window_split_test
```

**Success Criteria:**
- [ ] All 267+ tests passing
- [ ] Tab tests passing (7 tests)
- [ ] Split smoke test passing (3 tests)
- [ ] Zero test failures
- [ ] Test runtime <2s

</details>

<details>
<summary><b>Step 5.2: Check Code Metrics (5 min)</b></summary>

```bash
# Count lines in split files
cloc app/src/platform/gtk_window*.{cpp,h}

# Expected output:
# gtk_window.h:            374 LOC
# gtk_window.cpp:          380 LOC ✅ 52% under 800 limit
# gtk_window_callbacks.h:   50 LOC
# gtk_window_callbacks.cpp: 300 LOC ✅ 62% under 800 limit
# gtk_window_tabs.cpp:      320 LOC ✅ 60% under 800 limit
# gtk_window_system.cpp:     95 LOC ✅ 88% under 800 limit
# Total:                  ~1,519 LOC (source + headers)
```

**Success Criteria:**
- [ ] All .cpp files under 400 LOC
- [ ] All files under 800 LOC limit
- [ ] Total LOC reasonable (~1,500)
- [ ] Metrics documented

</details>

<details>
<summary><b>Step 5.3: Update Documentation (10 min)</b></summary>

Add to `REFACTOR_PROGRESS.md`:

```markdown
## gtk_window.cpp Split (2025-XX-XX)

**Goal:** Reduce gtk_window.cpp from 1,076 LOC to under 800 LOC

**Result:**
- ✅ gtk_window.cpp: 380 LOC (52% under limit)
- ✅ gtk_window_callbacks.cpp: 300 LOC (NEW - event handlers)
- ✅ gtk_window_tabs.cpp: 320 LOC (NEW - tab management)
- ✅ gtk_window_system.cpp: 95 LOC (NEW - window system)

**Benefits:**
- Clear separation of concerns (core / events / tabs / system)
- Easier to navigate and maintain
- Better testability (tab logic isolated)
- All files well under 800 LOC limit

**Time Spent:** 4h estimated, XXh actual
**Tests:** 267+ passing, zero regressions
**API Changes:** None (pure refactoring)
```

Update `CLAUDE.md` if needed:

```markdown
## Platform Layer Files

The platform layer is split for clarity:
- `platform/gtk_window.cpp` - Core window management (380 LOC)
- `platform/gtk_window_callbacks.cpp` - GTK event handlers (300 LOC)
- `platform/gtk_window_tabs.cpp` - Tab operations (320 LOC)
- `platform/gtk_window_system.cpp` - Window system (95 LOC)

All callbacks use dependency injection via user_data pointer.
Tab management is thread-safe with std::mutex protection.
```

**Success Criteria:**
- [ ] REFACTOR_PROGRESS.md updated
- [ ] CLAUDE.md updated (if needed)
- [ ] Metrics documented
- [ ] Benefits listed

</details>

<details>
<summary><b>Step 5.4: Manual Testing (5 min)</b></summary>

```bash
# Run the browser
./scripts/run.sh

# Test checklist:
# [ ] Window opens correctly
# [ ] Initial tab loads
# [ ] Ctrl+T creates new tab
# [ ] Ctrl+W closes tab
# [ ] Ctrl+Tab switches tabs
# [ ] Ctrl+1-9 switches to specific tab
# [ ] Middle-click closes tab
# [ ] Close button on tab works
# [ ] Mouse clicks work in page
# [ ] Keyboard typing works
# [ ] Scroll works
# [ ] Navigation buttons work (back/forward/reload/stop)
# [ ] Address bar works
# [ ] Multiple windows can be opened
# [ ] Closing last tab closes window
```

**Success Criteria:**
- [ ] All manual tests pass
- [ ] No crashes
- [ ] No visual glitches
- [ ] Performance feels normal

</details>

---

### Phase 6: Merge & Cleanup (15m)

| # | Task | Status | Time Est. | Time Actual | Notes |
|---|------|--------|-----------|-------------|-------|
| 6.1 | Format code | ⬜ | 5m | — | Run clang-format |
| 6.2 | Final test | ⬜ | 5m | — | One last verification |
| 6.3 | Merge to main | ⬜ | 5m | — | Complete integration |

<details>
<summary><b>Step 6.1: Format Code (5 min)</b></summary>

```bash
# Format all changed files
./scripts/format.sh

# Check diff
git diff

# Commit formatting
git add -u
git commit -m "style: format split files"
```

**Success Criteria:**
- [ ] All files formatted consistently
- [ ] No functional changes
- [ ] Clean git diff

</details>

<details>
<summary><b>Step 6.2: Final Test (5 min)</b></summary>

```bash
# Clean build
./scripts/build.sh

# Run all tests
ctest --test-dir build/release --output-on-failure

# Quick manual test
./scripts/run.sh
```

**Success Criteria:**
- [ ] Build succeeds
- [ ] All tests passing
- [ ] Browser works correctly

</details>

<details>
<summary><b>Step 6.3: Merge to Main (5 min)</b></summary>

```bash
# Switch to main
git checkout main

# Merge with no-ff for clear history
git merge --no-ff refactor/split-gtk-window -m "refactor: split gtk_window.cpp into 4 manageable files

Summary:
- gtk_window.cpp: 380 LOC (core window management)
- gtk_window_callbacks.cpp: 300 LOC (GTK event handlers)
- gtk_window_tabs.cpp: 320 LOC (tab operations)
- gtk_window_system.cpp: 95 LOC (window system)

All files now under 400 LOC (50% under 800 LOC limit).
All 267+ tests passing, zero regressions.
Browser fully functional with tabs, navigation, and input.

Benefits:
- Clear separation of concerns
- Easier to navigate and maintain
- Better testability
- All files well under size limits

Time spent: XXh (estimated 4h)"

# Delete backups
rm -f app/src/platform/gtk_window.cpp.backup
rm -f app/src/platform/gtk_window.h.backup

# Push to remote (if applicable)
git push origin main

# Delete feature branch
git branch -d refactor/split-gtk-window
```

**Success Criteria:**
- [ ] Merged to main
- [ ] Merge commit created
- [ ] Backups deleted
- [ ] Feature branch deleted
- [ ] Clean repository state

</details>

---

## 📊 Final Metrics

### Before Split

| File | LOC | Status |
|------|-----|--------|
| gtk_window.h | 374 | ✅ |
| gtk_window.cpp | 1,076 | ⚠️ +34% over |
| **Total** | **1,450** | ⚠️ |

### After Split

| File | LOC | % of Limit | Status |
|------|-----|------------|--------|
| gtk_window.h | 374 | 47% | ✅ |
| gtk_window.cpp | 380 | 48% | ✅ |
| gtk_window_callbacks.h | 50 | 6% | ✅ |
| gtk_window_callbacks.cpp | 300 | 38% | ✅ |
| gtk_window_tabs.cpp | 320 | 40% | ✅ |
| gtk_window_system.cpp | 95 | 12% | ✅ |
| **Total** | **1,519** | — | ✅ |

**Improvement:**
- ✅ gtk_window.cpp: 1,076 → 380 LOC (-65% reduction)
- ✅ All files under 400 LOC (50% under limit)
- ✅ Clear separation of concerns
- ✅ Better maintainability

---

## 🎯 Success Criteria

| Criterion | Target | Status |
|-----------|--------|--------|
| All tests passing | 267+ tests | ⬜ Pending |
| Browser functionality | 100% preserved | ⬜ Pending |
| Tab features | All working | ⬜ Pending |
| File sizes | All < 800 LOC | ⬜ Pending |
| Compiler warnings | 0 | ⬜ Pending |
| Memory leaks | 0 | ⬜ Pending |
| Build time | Unchanged | ⬜ Pending |
| API changes | None | ⬜ Pending |

---

## ⚠️ Risk Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Build breaks | Low | High | Incremental commits, test after each phase |
| Test failures | Low | High | Run full test suite after each change |
| Linker errors | Medium | Medium | Proper header includes, CMakeLists updates |
| Runtime crashes | Low | High | Manual browser testing after each phase |
| Callback issues | Low | Medium | All callbacks forward to same GtkWindow methods |

### Rollback Plan

If any phase fails:
```bash
# Restore from backup
cp app/src/platform/gtk_window.cpp.backup app/src/platform/gtk_window.cpp
cp app/src/platform/gtk_window.h.backup app/src/platform/gtk_window.h

# Or git reset
git reset --hard HEAD~1  # Undo last commit

# Fix issue and retry phase
```

---

## 📁 Final File Structure

```
app/src/platform/
├── window_system.h                 (237 LOC) - Abstract interfaces
├── gtk_window.h                    (374 LOC) - GtkWindow class definition
├── gtk_window.cpp                  (380 LOC) - Core window implementation ✅
├── gtk_window_callbacks.h          (50 LOC) - Callback registration ✅ NEW
├── gtk_window_callbacks.cpp        (300 LOC) - GTK event handlers ✅ NEW
├── gtk_window_tabs.cpp             (320 LOC) - Tab management ✅ NEW
└── gtk_window_system.cpp           (95 LOC) - Window system ✅ NEW

Total: ~1,756 LOC (includes headers)
Average file size: ~250 LOC
Largest file: 380 LOC (52% under limit)
```

**All public APIs unchanged** - this is a pure refactoring with zero API changes.

---

## 📝 Session Notes

### Start: [DATE/TIME]

**Pre-split Status:**
- [ ] All tests passing (267+)
- [ ] Browser working correctly
- [ ] Git branch created
- [ ] Backups created

### Phase 1: [TIME]

**Status:** ⬜

**Notes:**


### Phase 2: [TIME]

**Status:** ⬜

**Notes:**


### Phase 3: [TIME]

**Status:** ⬜

**Notes:**


### Phase 4: [TIME]

**Status:** ⬜

**Notes:**


### Phase 5: [TIME]

**Status:** ⬜

**Notes:**


### Phase 6: [TIME]

**Status:** ⬜

**Notes:**


### Completion: [DATE/TIME]

**Final Status:**
- [ ] All phases complete
- [ ] All tests passing
- [ ] Browser working
- [ ] Merged to main
- [ ] Documentation updated

**Time Spent:**
- Estimated: 4h
- Actual: XXh
- Efficiency: XX%

**Outcome:** [SUCCESS/PARTIAL/FAILED]

---

## 🎓 Lessons Learned

*To be filled after completion*

1.
2.
3.

---

## 🚀 Conclusion

This split will result in:

- ✅ **Better organization** - clear separation of concerns
- ✅ **Easier maintenance** - smaller, focused files
- ✅ **Improved testability** - tab logic can be tested in isolation
- ✅ **Compliance with standards** - all files under 800 LOC limit

The split maintains the excellent architecture quality while making the codebase even more maintainable.

**Status:** Ready to proceed! 🚀

---

*Last Updated: 2025-10-12*
*Document Version: 1.0*

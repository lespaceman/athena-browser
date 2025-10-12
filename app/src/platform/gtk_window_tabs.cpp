#include "platform/gtk_window.h"
#include "browser/browser_engine.h"
#include "browser/cef_engine.h"
#include "browser/cef_client.h"
#include "rendering/gl_renderer.h"
#include "include/cef_render_handler.h"

#include <iostream>
#include <algorithm>

namespace athena {
namespace platform {

namespace {

// ============================================================================
// Tab Management Callbacks (Internal)
// ============================================================================

static void on_close_tab_button_clicked(GtkButton* button, gpointer user_data) {
  (void)user_data;
  // Retrieve the window and browser_id from the button's object data
  GtkWindow* window = static_cast<GtkWindow*>(g_object_get_data(G_OBJECT(button), "window"));
  browser::BrowserId browser_id = static_cast<browser::BrowserId>(
    reinterpret_cast<uintptr_t>(g_object_get_data(G_OBJECT(button), "browser_id")));

  if (window) {
    // Find the tab with this browser_id and close it
    for (size_t i = 0; i < window->GetTabCount(); ++i) {
      // We need to access tabs_ safely, but we can't do it from here
      // Instead, add a new method CloseTabByBrowserId
      // For now, we'll assume the window has a way to find it
      // Actually, let's just iterate and find the right tab
      window->CloseTabByBrowserId(browser_id);
      break;
    }
  }
}

static gboolean on_tab_button_press(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
  (void)widget;
  (void)user_data;
  // Check for middle-click (button 2)
  if (event->button == 2) {
    // Retrieve the window and browser_id from the widget's object data
    GtkWindow* window = static_cast<GtkWindow*>(g_object_get_data(G_OBJECT(widget), "window"));
    browser::BrowserId browser_id = static_cast<browser::BrowserId>(
      reinterpret_cast<uintptr_t>(g_object_get_data(G_OBJECT(widget), "browser_id")));

    if (window) {
      window->CloseTabByBrowserId(browser_id);
      return TRUE;  // Handled
    }
  }

  return FALSE;  // Not handled, allow normal processing
}

}  // anonymous namespace

// ============================================================================
// Tab Management Implementation
// ============================================================================

int GtkWindow::CreateTab(const std::string& url) {
  if (!gl_area_) {
    std::cerr << "[GtkWindow::CreateTab] GL area not initialized" << std::endl;
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
  tab.cef_client = nullptr;  // Initialize to nullptr to avoid undefined behavior
  tab.tab_label = nullptr;   // Initialize GTK pointers too
  tab.close_button = nullptr;

  // Each tab owns its own GL renderer so it can maintain an independent
  // backing surface that stays valid while the tab is inactive.
  tab.renderer = std::make_unique<rendering::GLRenderer>();
  auto renderer_init = tab.renderer->Initialize(gl_area_);
  if (!renderer_init) {
    std::cerr << "[GtkWindow::CreateTab] Failed to initialize GL surface: "
              << renderer_init.GetError().Message() << std::endl;
    return -1;
  }
  GtkAllocation allocation;
  gtk_widget_get_allocation(gl_area_, &allocation);
  int surface_width = allocation.width > 0 ? allocation.width : config_.size.width;
  int surface_height = allocation.height > 0 ? allocation.height : config_.size.height;
  tab.renderer->SetViewSize(surface_width, surface_height);

  // Create browser instance
  float scale_factor = static_cast<float>(gtk_widget_get_scale_factor(gl_area_));

  browser::BrowserConfig browser_config;
  browser_config.url = url;
  browser_config.width = surface_width;
  browser_config.height = surface_height;
  browser_config.device_scale_factor = scale_factor;
  browser_config.gl_renderer = tab.renderer.get();
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
          // CRITICAL FIX: Don't capture raw widget pointer (it->tab_label)!
          // If tab is closed before idle callback runs, widget will be destroyed.
          // Instead, capture browser_id and window pointer to re-lookup the tab.
          struct TitleUpdateData {
            GtkWindow* window;
            browser::BrowserId browser_id;
            std::string title;
          };

          g_idle_add([](gpointer user_data) -> gboolean {
            auto* data = static_cast<TitleUpdateData*>(user_data);

            // Re-lookup the tab by browser_id to ensure it still exists
            std::lock_guard<std::mutex> lock(data->window->tabs_mutex_);
            auto it = std::find_if(data->window->tabs_.begin(),
                                   data->window->tabs_.end(),
                                   [data](const Tab& t) {
                                     return t.browser_id == data->browser_id;
                                   });

            // Only update if tab still exists
            if (it != data->window->tabs_.end() && it->tab_label != nullptr) {
              gtk_label_set_text(GTK_LABEL(it->tab_label), data->title.c_str());
            }

            delete data;
            return G_SOURCE_REMOVE;
          }, new TitleUpdateData{this, bid, title});
        }
      });

      tab.cef_client->SetRenderInvalidatedCallback(
          [this, bid](CefRenderHandler::PaintElementType type) {
            this->HandleTabRenderInvalidated(bid, type);
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
  g_object_set_data(G_OBJECT(event_box), "browser_id", reinterpret_cast<gpointer>(static_cast<uintptr_t>(tab.browser_id)));
  g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_tab_button_press), nullptr);
  gtk_widget_show(event_box);

  tab.tab_label = label;
  tab.close_button = close_btn;

  // IMPORTANT: Store tab BEFORE adding to notebook
  // gtk_notebook_append_page() may trigger "switch-page" signal immediately
  // which calls SwitchToTab() and needs tabs_[index] to exist
  //
  // CRITICAL: Lock tabs_mutex_ before modifying tabs_ vector
  // Callbacks (SetAddressChangeCallback, etc.) lock this mutex when reading tabs_
  // Without this lock, we have a data race: CreateTab() writes, callbacks read
  size_t new_tab_index;
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    tabs_.push_back(std::move(tab));
    new_tab_index = tabs_.size() - 1;
  }

  // Connect close button signal
  // Note: We store the browser_id as user data to identify which tab to close
  // This is critical - using tab_index would become stale after tab closures
  g_object_set_data(G_OBJECT(close_btn), "window", this);
  g_object_set_data(G_OBJECT(close_btn), "browser_id", reinterpret_cast<gpointer>(static_cast<uintptr_t>(tab.browser_id)));
  g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_tab_button_clicked), nullptr);

  // Add empty page to notebook (we don't need content, just the tab)
  // This may trigger "switch-page" signal, so tab must already be in tabs_
  GtkWidget* empty_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show(empty_page);  // Must show the page widget for tab to appear
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), empty_page, event_box);

  // Switch to the new tab
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), new_tab_index);

  std::cout << "[GtkWindow::CreateTab] Tab created successfully, index: " << new_tab_index << std::endl;
  return static_cast<int>(new_tab_index);
}

void GtkWindow::CloseTab(size_t index) {
  browser::BrowserId browser_to_close = 0;
  size_t new_active_index = 0;
  bool should_close_window = false;
  std::unique_ptr<rendering::GLRenderer> renderer_to_destroy;
  browser::CefClient* client_to_hide = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (index >= tabs_.size()) {
      std::cerr << "[GtkWindow::CloseTab] Invalid tab index: " << index << std::endl;
      return;
    }

    std::cout << "[GtkWindow::CloseTab] Closing tab " << index << std::endl;

    browser_to_close = tabs_[index].browser_id;
    renderer_to_destroy = std::move(tabs_[index].renderer);
    client_to_hide = tabs_[index].cef_client;

    // CRITICAL: Block "switch-page" signal to prevent reentrant locking
    // gtk_notebook_remove_page() triggers switch-page, which calls SwitchToTab()
    // which tries to lock tabs_mutex_ again -> deadlock/crash
    g_signal_handlers_block_matched(notebook_,
                                     G_SIGNAL_MATCH_DATA,
                                     0, 0, nullptr, nullptr, this);

    // Remove the notebook page
    gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_), index);

    // Unblock the signal
    g_signal_handlers_unblock_matched(notebook_,
                                       G_SIGNAL_MATCH_DATA,
                                       0, 0, nullptr, nullptr, this);

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

  if (client_to_hide && client_to_hide->GetBrowser()) {
    client_to_hide->GetBrowser()->GetHost()->WasHidden(true);
  }

  if (renderer_to_destroy) {
    renderer_to_destroy->Cleanup();
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
  std::unique_ptr<rendering::GLRenderer> renderer_to_destroy;
  browser::CefClient* client_to_hide = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
      [browser_id](const Tab& t) { return t.browser_id == browser_id; });

    if (it != tabs_.end()) {
      found = true;
      index_to_close = std::distance(tabs_.begin(), it);
      std::cout << "[GtkWindow::CloseTabByBrowserId] Found tab at index " << index_to_close
                << " for browser_id " << browser_id << std::endl;

      // CRITICAL: Block "switch-page" signal to prevent reentrant locking
      g_signal_handlers_block_matched(notebook_,
                                       G_SIGNAL_MATCH_DATA,
                                       0, 0, nullptr, nullptr, this);

      // Remove the notebook page
      gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_), index_to_close);

      // Unblock the signal
      g_signal_handlers_unblock_matched(notebook_,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, nullptr, nullptr, this);

      renderer_to_destroy = std::move(it->renderer);
      client_to_hide = it->cef_client;

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

  if (client_to_hide && client_to_hide->GetBrowser()) {
    client_to_hide->GetBrowser()->GetHost()->WasHidden(true);
  }

  if (renderer_to_destroy) {
    renderer_to_destroy->Cleanup();
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
  browser::CefClient* client_to_show = nullptr;
  browser::CefClient* client_to_hide = nullptr;
  std::string url;
  bool is_loading = false;
  bool can_go_back = false;
  bool can_go_forward = false;
  bool index_changed = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (index >= tabs_.size()) {
      std::cerr << "[GtkWindow::SwitchToTab] Invalid tab index: " << index << std::endl;
      return;
    }

    size_t previous_index = active_tab_index_;
    if (previous_index < tabs_.size()) {
      client_to_hide = tabs_[previous_index].cef_client;
    }

    std::cout << "[GtkWindow::SwitchToTab] Switching to tab " << index << std::endl;

    active_tab_index_ = index;
    Tab& tab = tabs_[index];

    client_to_show = tab.cef_client;
    url = tab.url;
    is_loading = tab.is_loading;
    can_go_back = tab.can_go_back;
    can_go_forward = tab.can_go_forward;

    index_changed = (previous_index != index);
  }

  UpdateAddressBar(url);
  UpdateNavigationButtons(is_loading, can_go_back, can_go_forward);

  if (index_changed && client_to_hide && client_to_hide != client_to_show) {
    if (auto browser = client_to_hide->GetBrowser()) {
      browser->GetHost()->WasHidden(true);
    }
  }

  if (client_to_show && client_to_show->GetBrowser()) {
    auto host = client_to_show->GetBrowser()->GetHost();
    host->WasHidden(false);
    host->SetFocus(has_focus_);
  }

  if (gl_area_ && config_.enable_input) {
    gtk_widget_grab_focus(gl_area_);
  }

  if (gl_area_) {
    gtk_gl_area_queue_render(GTK_GL_AREA(gl_area_));
  }

  std::cout << "[GtkWindow::SwitchToTab] Switched to tab " << index
            << ", URL: " << url << std::endl;
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

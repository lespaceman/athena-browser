/**
 * QtMainWindow Implementation
 *
 * Qt-based main window with multi-tab browser support.
 * Maintains clean architecture: zero globals, RAII, dependency injection.
 */

#include "platform/qt_mainwindow.h"

#include "browser/browser_engine.h"
#include "browser/cef_client.h"
#include "browser/cef_engine.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "platform/qt_agent_panel.h"
#include "platform/qt_browserwidget.h"
#include "rendering/gl_renderer.h"
#include "utils/logging.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMetaObject>
#include <QPalette>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <thread>
#include <utility>

namespace athena {
namespace platform {

using namespace browser;
using namespace rendering;
using namespace utils;

static Logger logger("QtMainWindow");

// ============================================================================
// QtMainWindow Implementation
// ============================================================================

QtMainWindow::QtMainWindow(const WindowConfig& config,
                           const WindowCallbacks& callbacks,
                           BrowserEngine* engine,
                           QWidget* parent)
    : QMainWindow(parent),
      config_(config),
      callbacks_(callbacks),
      engine_(engine),
      node_runtime_(config.node_runtime),
      closed_(false),
      visible_(false),
      has_focus_(false),
      browser_initialized_(false),
      toolbar_(nullptr),
      addressBar_(nullptr),
      backButton_(nullptr),
      forwardButton_(nullptr),
      reloadButton_(nullptr),
      stopButton_(nullptr),
      newTabButton_(nullptr),
      agentButton_(nullptr),
      tabWidget_(nullptr),
      agentPanel_(nullptr),
      active_tab_index_(0),
      current_url_(QString::fromStdString(config.url)) {
  logger.Info("Creating Qt main window");

  setWindowTitle(QString::fromStdString(config_.title));
  resize(config_.size.width, config_.size.height);

  setupUI();
  connectSignals();

  logger.Info("Qt main window created successfully");
}

QtMainWindow::~QtMainWindow() {
  logger.Info("Destroying Qt main window");
  closed_ = true;

  // Clean up tabs before destroying window
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    for (auto& tab : tabs_) {
      tab.renderer.reset();  // Explicit cleanup while GL context is valid
    }
    tabs_.clear();
  }

  // Qt automatically destroys all child widgets (toolbar_, addressBar_, etc.)
  logger.Info("Qt main window destroyed");
}

// ============================================================================
// Setup Methods
// ============================================================================

void QtMainWindow::setupUI() {
  createToolbar();
  createCentralWidget();
}

void QtMainWindow::createToolbar() {
  toolbar_ = addToolBar(tr("Navigation"));
  toolbar_->setMovable(false);
  toolbar_->setIconSize(QSize(24, 24));

  // Back button
  backButton_ = new QPushButton(this);
  backButton_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
  backButton_->setFlat(true);
  backButton_->setToolTip(tr("Back (Alt+Left)"));
  backButton_->setEnabled(false);
  toolbar_->addWidget(backButton_);

  // Forward button
  forwardButton_ = new QPushButton(this);
  forwardButton_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
  forwardButton_->setFlat(true);
  forwardButton_->setToolTip(tr("Forward (Alt+Right)"));
  forwardButton_->setEnabled(false);
  toolbar_->addWidget(forwardButton_);

  // Reload button
  reloadButton_ = new QPushButton(this);
  reloadButton_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
  reloadButton_->setFlat(true);
  reloadButton_->setToolTip(tr("Reload (Ctrl+R)"));
  toolbar_->addWidget(reloadButton_);

  // Stop button
  stopButton_ = new QPushButton(this);
  stopButton_->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));
  stopButton_->setFlat(true);
  stopButton_->setToolTip(tr("Stop (Esc)"));
  stopButton_->setEnabled(false);
  toolbar_->addWidget(stopButton_);

  // Address bar
  addressBar_ = new QLineEdit(this);
  addressBar_->setPlaceholderText(tr("Enter URL or search..."));
  addressBar_->setText(current_url_);
  addressBar_->setStyleSheet(R"(
    QLineEdit {
      border: 1px solid #ccc;
      border-radius: 4px;
      padding: 6px 8px;
      margin: 4px;
      font-size: 13px;
    }
    QLineEdit:focus {
      border: 2px solid #2196F3;
      padding: 5px 7px;
    }
  )");
  toolbar_->addWidget(addressBar_);

  // New tab button
  newTabButton_ = new QPushButton(this);
  newTabButton_->setText("+");
  newTabButton_->setToolTip(tr("New Tab (Ctrl+T)"));
  newTabButton_->setMaximumWidth(30);
  toolbar_->addWidget(newTabButton_);

  // Add separator
  toolbar_->addSeparator();

  // Agent sidebar toggle button
  agentButton_ = new QPushButton(this);
  agentButton_->setText(tr("Ask AI"));
  agentButton_->setToolTip(tr("Toggle Agent Sidebar (Ctrl+Shift+C)"));
  agentButton_->setCheckable(true);
  agentButton_->setChecked(true);  // Sidebar visible by default
  toolbar_->addWidget(agentButton_);
}

void QtMainWindow::createCentralWidget() {
  // Create tab widget (Phase 2: multi-tab support)
  tabWidget_ = new QTabWidget(this);
  tabWidget_->setTabsClosable(true);  // Show "×" button on each tab
  tabWidget_->setMovable(true);       // Allow dragging tabs to reorder
  tabWidget_->setDocumentMode(true);  // Cleaner look

  // Create Agent chat panel
  agentPanel_ = new AgentPanel(this, this);
  agentPanel_->SetNodeRuntime(node_runtime_);
  agentPanel_->setMinimumWidth(300);
  agentPanel_->setMaximumWidth(500);  // Prevent sidebar from taking too much space

  // Create horizontal splitter: browser tabs on left, Agent sidebar on right
  QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->addWidget(tabWidget_);   // Browser tabs (left)
  splitter->addWidget(agentPanel_);  // Agent sidebar (right)

  // Set stretch factors: 70% browser, 30% sidebar (more balanced)
  splitter->setStretchFactor(0, 7);  // Browser gets 7x weight
  splitter->setStretchFactor(1, 3);  // Sidebar gets 3x weight

  // Set initial sizes: For a 1920px window, sidebar should be ~400px
  QList<int> sizes;
  sizes << 1400 << 400;  // Browser: 1400px, Sidebar: 400px
  splitter->setSizes(sizes);

  // Allow user to resize the splitter, but prevent collapse
  splitter->setChildrenCollapsible(false);
  splitter->setHandleWidth(0);

  const QColor splitterColor = QApplication::palette().color(QPalette::Window);
  splitter->setStyleSheet(
      QStringLiteral(
          "QSplitter::handle { background-color: %1; border: none; margin: 0; padding: 0; }")
          .arg(splitterColor.name(QColor::HexRgb)));

  setCentralWidget(splitter);

  logger.Info("Central widget created with Agent sidebar");
}

void QtMainWindow::connectSignals() {
  // Toolbar signals → slots
  connect(backButton_, &QPushButton::clicked, this, &QtMainWindow::onBackClicked);

  connect(forwardButton_, &QPushButton::clicked, this, &QtMainWindow::onForwardClicked);

  connect(reloadButton_, &QPushButton::clicked, this, &QtMainWindow::onReloadClicked);

  connect(stopButton_, &QPushButton::clicked, this, &QtMainWindow::onStopClicked);

  connect(addressBar_, &QLineEdit::returnPressed, this, &QtMainWindow::onAddressBarReturnPressed);

  connect(newTabButton_, &QPushButton::clicked, this, &QtMainWindow::onNewTabClicked);

  // Tab widget signals
  connect(tabWidget_, &QTabWidget::tabCloseRequested, this, &QtMainWindow::onTabCloseRequested);

  connect(tabWidget_, &QTabWidget::currentChanged, this, &QtMainWindow::onCurrentTabChanged);

  if (QTabBar* tabBar = tabWidget_->tabBar()) {
    connect(tabBar, &QTabBar::tabMoved, this, &QtMainWindow::onTabMoved);
  }

  // Agent button
  connect(agentButton_, &QPushButton::clicked, this, &QtMainWindow::onAgentButtonClicked);

  // Keyboard shortcut: Ctrl+Shift+C (or Cmd+Shift+C on macOS)
  QShortcut* agentShortcut = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
  connect(agentShortcut, &QShortcut::activated, this, &QtMainWindow::onAgentButtonClicked);
}

void QtMainWindow::InitializeBrowser() {
  if (browser_initialized_) {
    logger.Warn("Browser already initialized");
    return;
  }

  logger.Info("Initializing browser (Phase 2: multi-tab)");

  // Create the first tab with the initial URL
  int tab_index = CreateTab(current_url_);

  if (tab_index >= 0) {
    browser_initialized_ = true;
    logger.Info("Browser initialized successfully with first tab");
  } else {
    logger.Error("Failed to initialize browser: could not create first tab");
  }
}

// ============================================================================
// Qt Event Handlers
// ============================================================================

void QtMainWindow::closeEvent(QCloseEvent* event) {
  logger.Info("Window close event");
  closed_ = true;

  // Collect all CEF clients while holding the lock
  std::vector<CefClient*> clients_to_close;
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    for (auto& tab : tabs_) {
      if (tab.cef_client && tab.cef_client->GetBrowser()) {
        clients_to_close.push_back(tab.cef_client);
      }
    }
  }

  // Close browsers outside the lock to avoid deadlock
  for (auto* client : clients_to_close) {
    if (client && client->GetBrowser()) {
      client->GetBrowser()->GetHost()->CloseBrowser(false);
    }
  }

  // Call user callback
  if (callbacks_.on_close) {
    callbacks_.on_close();
  }

  event->accept();
  QApplication::quit();
}

void QtMainWindow::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);

  // Notify browser of size change (resize active tab's browser widget)
  // Extract data first, then call OnBrowserSizeChanged outside the lock to avoid recursive locking
  size_t tab_index_to_resize = 0;
  int widget_width = 0;
  int widget_height = 0;
  bool should_resize = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (active_tab_index_ < tabs_.size() && tabs_[active_tab_index_].browser_widget) {
      BrowserWidget* widget = tabs_[active_tab_index_].browser_widget;
      tab_index_to_resize = active_tab_index_;
      widget_width = widget->width();
      widget_height = widget->height();
      should_resize = true;
    }
  }

  // Call OnBrowserSizeChanged outside the lock
  if (should_resize) {
    OnBrowserSizeChanged(tab_index_to_resize, widget_width, widget_height);
  }

  // Call user callback
  if (callbacks_.on_resize) {
    callbacks_.on_resize(event->size().width(), event->size().height());
  }
}

void QtMainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);

  if (!visible_) {
    visible_ = true;
    logger.Info("Window shown");

    // Initialize browser after window is shown and GL context is ready
    // This is deferred to ensure GL context is available
    QTimer::singleShot(100, this, &QtMainWindow::InitializeBrowser);
  }
}

void QtMainWindow::OnBrowserSizeChanged(size_t tab_index, int width, int height) {
  // Called from BrowserWidget when resized
  // In Phase 2, each tab has its own BrowserWidget, so we resize THAT specific tab
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tab_index < tabs_.size()) {
    QtTab& tab = tabs_[tab_index];

    if (tab.cef_client) {
      tab.cef_client->SetSize(width, height);
    }

    if (tab.renderer) {
      tab.renderer->SetViewSize(width, height);
    }
  }
}

// ============================================================================
// UI Event Slots
// ============================================================================

void QtMainWindow::onBackClicked() {
  GoBack();
}

void QtMainWindow::onForwardClicked() {
  GoForward();
}

void QtMainWindow::onReloadClicked() {
  Reload();
}

void QtMainWindow::onStopClicked() {
  StopLoad();
}

void QtMainWindow::onAddressBarReturnPressed() {
  QString url = addressBar_->text().trimmed();

  // Add scheme if missing
  if (!url.contains("://")) {
    if (url.contains('.') && !url.contains(' ')) {
      url = "https://" + url;
    } else {
      // Treat as search query
      url = "https://www.google.com/search?q=" + QString(QUrl::toPercentEncoding(url));
    }
  }

  LoadURL(url);
}

void QtMainWindow::onNewTabClicked() {
  logger.Info("New tab button clicked");
  OnNewTabClicked();
}

void QtMainWindow::onTabCloseRequested(int index) {
  logger.Info("Tab close requested: " + std::to_string(index));
  OnCloseTabClicked(index);
}

void QtMainWindow::onCurrentTabChanged(int index) {
  logger.Info("Current tab changed to: " + std::to_string(index));
  if (index >= 0) {
    OnTabSwitch(index);
  }
}

void QtMainWindow::onAgentButtonClicked() {
  if (agentPanel_) {
    agentPanel_->ToggleVisibility();

    // Update button state
    agentButton_->setChecked(agentPanel_->IsVisible());

    logger.Info(agentPanel_->IsVisible() ? "Agent sidebar shown" : "Agent sidebar hidden");
  }
}

void QtMainWindow::onTabMoved(int from, int to) {
  logger.Info("Tab moved from " + std::to_string(from) + " to " + std::to_string(to));

  if (from == to) {
    return;
  }

  std::lock_guard<std::mutex> lock(tabs_mutex_);

  const int tab_count = static_cast<int>(tabs_.size());
  if (from < 0 || to < 0 || from >= tab_count || to >= tab_count) {
    logger.Warn("onTabMoved: indices out of range");
    return;
  }

  QtTab moved_tab = std::move(tabs_[from]);
  tabs_.erase(tabs_.begin() + from);
  tabs_.insert(tabs_.begin() + to, std::move(moved_tab));

  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].browser_widget) {
      tabs_[i].browser_widget->SetTabIndex(i);
    }
  }

  active_tab_index_ = static_cast<size_t>(tabWidget_->currentIndex());
}

// ============================================================================
// Public Interface Methods
// ============================================================================

void QtMainWindow::LoadURL(const QString& url) {
  CefClient* client = nullptr;
  size_t tab_index = 0;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);

    current_url_ = url;

    QtTab* tab = GetActiveTab();
    if (!tab) {
      logger.Warn("LoadURL: No active tab found");
      UpdateAddressBar(url);
      return;
    }

    if (!tab->cef_client) {
      logger.Warn("LoadURL: Active tab has no CEF client yet");
      UpdateAddressBar(url);
      return;
    }

    if (!tab->cef_client->GetBrowser()) {
      logger.Warn("LoadURL: Active tab's CEF client has no browser yet (still initializing?)");
      UpdateAddressBar(url);
      return;
    }

    // Save client pointer and update tab URL
    client = tab->cef_client;
    tab->url = url;
    tab->is_loading = true;
    tab_index = active_tab_index_;
  }

  // Call CEF outside the lock to avoid deadlock if CEF calls back into our code
  logger.Info("Loading URL in tab " + std::to_string(tab_index) + ": " + url.toStdString());
  client->GetBrowser()->GetMainFrame()->LoadURL(url.toStdString());

  UpdateAddressBar(url);
}

void QtMainWindow::UpdateAddressBar(const QString& url) {
  // Thread-safe: can be called from CEF thread
  QMetaObject::invokeMethod(
      this,
      [this, url]() {
        if (!closed_) {
          addressBar_->setText(url);
          current_url_ = url;
        }
      },
      Qt::QueuedConnection);
}

void QtMainWindow::UpdateNavigationButtons(bool is_loading, bool can_go_back, bool can_go_forward) {
  // Thread-safe: can be called from CEF thread
  QMetaObject::invokeMethod(
      this,
      [this, is_loading, can_go_back, can_go_forward]() {
        if (!closed_) {
          backButton_->setEnabled(can_go_back);
          forwardButton_->setEnabled(can_go_forward);
          reloadButton_->setEnabled(!is_loading);
          stopButton_->setEnabled(is_loading);
        }
      },
      Qt::QueuedConnection);
}

void QtMainWindow::GoBack() {
  CefClient* client = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    QtTab* tab = GetActiveTab();
    if (tab && tab->cef_client && tab->cef_client->GetBrowser()) {
      client = tab->cef_client;
      tab->is_loading = true;
    }
  }

  // Call CEF outside the lock
  if (client && client->GetBrowser()) {
    client->GetBrowser()->GoBack();
  }
}

void QtMainWindow::GoForward() {
  CefClient* client = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    QtTab* tab = GetActiveTab();
    if (tab && tab->cef_client && tab->cef_client->GetBrowser()) {
      client = tab->cef_client;
      tab->is_loading = true;
    }
  }

  // Call CEF outside the lock
  if (client && client->GetBrowser()) {
    client->GetBrowser()->GoForward();
  }
}

void QtMainWindow::Reload(bool ignore_cache) {
  CefClient* client = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    QtTab* tab = GetActiveTab();
    if (tab && tab->cef_client && tab->cef_client->GetBrowser()) {
      client = tab->cef_client;
      tab->is_loading = true;
    }
  }

  // Call CEF outside the lock
  if (client && client->GetBrowser()) {
    if (ignore_cache) {
      client->GetBrowser()->ReloadIgnoreCache();
    } else {
      client->GetBrowser()->Reload();
    }
  }
}

void QtMainWindow::StopLoad() {
  CefClient* client = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    QtTab* tab = GetActiveTab();
    if (tab && tab->cef_client && tab->cef_client->GetBrowser()) {
      client = tab->cef_client;
      tab->is_loading = false;
    }
  }

  // Call CEF outside the lock
  if (client && client->GetBrowser()) {
    client->GetBrowser()->StopLoad();
  }
}

// ============================================================================
// Accessors for BrowserControlServer
// ============================================================================

CefClient* QtMainWindow::GetCefClient() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tabs_.empty()) {
    return nullptr;
  }

  return tabs_[active_tab_index_].cef_client;
}

CefClient* QtMainWindow::GetCefClientForTab(size_t tab_index) const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tab_index >= tabs_.size()) {
    return nullptr;
  }

  return tabs_[tab_index].cef_client;
}

GLRenderer* QtMainWindow::GetGLRenderer() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tabs_.empty()) {
    return nullptr;
  }

  return tabs_[active_tab_index_].renderer.get();
}

QString QtMainWindow::GetCurrentUrl() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return current_url_;
}

QString QtMainWindow::GetPageHTML() const {
  QtTab* tab = nullptr;
  CefRefPtr<CefBrowser> browser;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    tab = const_cast<QtMainWindow*>(this)->GetActiveTab();
    if (!tab || !tab->cef_client || !tab->cef_client->GetBrowser()) {
      return QString();
    }
    browser = tab->cef_client->GetBrowser();
  }

  auto main_frame = browser->GetMainFrame();
  if (!main_frame) {
    logger.Error("No main frame available");
    return QString();
  }

  // Helper class for synchronous HTML retrieval
  class HtmlVisitor : public CefStringVisitor {
   public:
    HtmlVisitor() : complete_(false) {}

    void Visit(const CefString& string) override {
      std::lock_guard<std::mutex> lock(mutex_);
      html_ = string.ToString();
      complete_.store(true, std::memory_order_release);
    }

    bool WaitForHtml(std::string& html, int timeout_ms = 5000) {
      auto start = std::chrono::steady_clock::now();

      // Poll and pump CEF message loop instead of blocking
      while (!complete_.load(std::memory_order_acquire)) {
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        if (elapsed >= timeout_ms) {
          return false;
        }

        // Pump CEF message loop to allow callbacks to run
        CefDoMessageLoopWork();

        // Small sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      std::lock_guard<std::mutex> lock(mutex_);
      html = html_;
      return true;
    }

   private:
    std::mutex mutex_;
    std::string html_;
    std::atomic<bool> complete_;
    IMPLEMENT_REFCOUNTING(HtmlVisitor);
  };

  // Request HTML source
  CefRefPtr<HtmlVisitor> visitor = new HtmlVisitor();
  main_frame->GetSource(visitor);

  // Wait for result
  std::string html;
  if (visitor->WaitForHtml(html, 5000)) {
    logger.Info("Retrieved HTML (" + std::to_string(html.length()) + " bytes)");
    return QString::fromStdString(html);
  } else {
    logger.Error("Timeout waiting for HTML");
    return QString();
  }
}

QString QtMainWindow::ExecuteJavaScript(const QString& code) const {
  browser::CefClient* cef_client = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);

    QtTab* tab = const_cast<QtMainWindow*>(this)->GetActiveTab();
    if (!tab || !tab->cef_client || !tab->cef_client->GetBrowser()) {
      logger.Error("ExecuteJavaScript: No active CEF client or browser");
      return QString(R"({"success":false,"error":{"message":"No active browser"}})");
    }

    cef_client = tab->cef_client;
  }

  auto request_id_opt = cef_client->RequestJavaScriptEvaluation(code.toStdString());
  if (!request_id_opt.has_value()) {
    logger.Error("ExecuteJavaScript: Failed to dispatch request");
    return QString(
        R"({"success":false,"error":{"message":"Failed to dispatch JavaScript to renderer"}})");
  }

  const std::string request_id = request_id_opt.value();
  const int timeout_ms = 5000;
  auto start = std::chrono::steady_clock::now();

  while (true) {
    auto result = cef_client->TryConsumeJavaScriptResult(request_id);
    if (result.has_value()) {
      logger.Info("JavaScript executed ({} bytes)", result->size());
      return QString::fromStdString(result.value());
    }

    if (closed_) {
      logger.Error("ExecuteJavaScript aborted: window closed while waiting");
      cef_client->CancelJavaScriptEvaluation(request_id);
      return QString(
          R"({"success":false,"error":{"message":"Window closed while waiting for result"}})");
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    if (elapsed >= timeout_ms) {
      logger.Error("ExecuteJavaScript timed out after {}ms", timeout_ms);
      cef_client->CancelJavaScriptEvaluation(request_id);
      return QString(
          R"({"success":false,"error":{"message":"Timeout waiting for JavaScript result"},"type":"timeout"})");
    }

    CefDoMessageLoopWork();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

QString QtMainWindow::TakeScreenshot() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  QtTab* tab = const_cast<QtMainWindow*>(this)->GetActiveTab();
  if (!tab || !tab->renderer) {
    logger.Error("TakeScreenshot: No active tab or renderer");
    return QString();
  }

  // Call the GLRenderer's TakeScreenshot method (automatically scaled to 0.5 for AI analysis)
  std::string base64_png = tab->renderer->TakeScreenshot();
  if (base64_png.empty()) {
    logger.Error("TakeScreenshot: Failed to capture screenshot");
    return QString();
  }

  logger.Info("Screenshot captured successfully");
  return QString::fromStdString(base64_png);
}

// ============================================================================
// Tab Management (Phase 2: Full Multi-Tab Support)
// ============================================================================

int QtMainWindow::CreateTab(const QString& url) {
  if (!tabWidget_) {
    logger.Error("Tab widget not initialized");
    return -1;
  }

  if (!engine_) {
    logger.Error("BrowserEngine not available");
    return -1;
  }

  logger.Info("Creating tab with URL: " + url.toStdString());

  // Create Tab structure
  QtTab tab;
  tab.browser_id = 0;  // Will be set after browser creation
  tab.cef_client = nullptr;
  tab.browser_widget = nullptr;  // Will be set below
  tab.url = url;
  tab.title = "New Tab";
  tab.is_loading = true;
  tab.can_go_back = false;
  tab.can_go_forward = false;

  // Each tab owns its own GL renderer
  tab.renderer = std::make_unique<GLRenderer>();

  // Add tab to tabs_ vector FIRST to get the correct index
  size_t new_tab_index;
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    tabs_.push_back(std::move(tab));
    new_tab_index = tabs_.size() - 1;
  }

  // Create BrowserWidget for this tab with the correct tab index
  BrowserWidget* browserWidget = new BrowserWidget(this, new_tab_index, tabWidget_);
  browserWidget->setFocusPolicy(Qt::StrongFocus);

  // Update the tab's browser_widget pointer
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    tabs_[new_tab_index].browser_widget = browserWidget;
  }

  // Initialize GLRenderer for this tab (just sets the pointer, doesn't initialize yet)
  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    browserWidget->InitializeBrowser(tabs_[new_tab_index].renderer.get());
  }

  // Connect signal to create browser when GL context is ready
  // IMPORTANT: Must capture new_tab_index by value, not by reference
  connect(browserWidget, &BrowserWidget::glContextReady, this, [this, new_tab_index]() {
    logger.Info("GL context ready for tab " + std::to_string(new_tab_index) + ", creating browser");
    createBrowserForTab(new_tab_index);
  });

  // Add to tab widget (must happen AFTER tabs_ vector is updated)
  int qt_index = tabWidget_->addTab(browserWidget, "New Tab");

  // Switch to the new tab
  tabWidget_->setCurrentIndex(qt_index);

  logger.Info("Tab widget created, index: " + std::to_string(new_tab_index) +
              " (browser will be created when GL is ready)");
  return static_cast<int>(new_tab_index);
}

void QtMainWindow::createBrowserForTab(size_t tab_index) {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tab_index >= tabs_.size()) {
    logger.Error("Invalid tab index: " + std::to_string(tab_index));
    return;
  }

  QtTab& tab = tabs_[tab_index];
  BrowserWidget* browserWidget = tab.browser_widget;

  if (!browserWidget) {
    logger.Error("BrowserWidget is null for tab " + std::to_string(tab_index));
    return;
  }

  logger.Info("Creating CEF browser for tab " + std::to_string(tab_index));

  // Create CEF browser instance
  float scale_factor = devicePixelRatioF();

  browser::BrowserConfig browser_config;
  browser_config.url = tab.url.toStdString();
  browser_config.width = browserWidget->width() > 0 ? browserWidget->width() : width();
  browser_config.height = browserWidget->height() > 0 ? browserWidget->height() : height();
  browser_config.device_scale_factor = scale_factor;
  browser_config.gl_renderer = tab.renderer.get();
  browser_config.native_window_handle = browserWidget;

  auto result = engine_->CreateBrowser(browser_config);
  if (!result.IsOk()) {
    logger.Error("Failed to create browser: " + result.GetError().Message());
    return;
  }

  tab.browser_id = result.Value();
  logger.Info("Browser created with ID: " + std::to_string(tab.browser_id));

  // Get the CEF client
  auto* cef_engine = dynamic_cast<browser::CefEngine*>(engine_);
  if (cef_engine) {
    auto client = cef_engine->GetCefClient(tab.browser_id);
    if (client) {
      tab.cef_client = client.get();

      // Wire up CEF callbacks for this tab
      browser::BrowserId bid = tab.browser_id;

      tab.cef_client->SetAddressChangeCallback([this, bid](const std::string& url_str) {
        std::lock_guard<std::mutex> lock(tabs_mutex_);
        auto it = std::find_if(
            tabs_.begin(), tabs_.end(), [bid](const QtTab& t) { return t.browser_id == bid; });
        if (it != tabs_.end()) {
          it->url = QString::fromStdString(url_str);
          size_t tab_idx = std::distance(tabs_.begin(), it);
          if (tab_idx == active_tab_index_) {
            this->UpdateAddressBar(QString::fromStdString(url_str));
          }
        }
      });

      tab.cef_client->SetLoadingStateChangeCallback(
          [this, bid](bool is_loading, bool can_go_back, bool can_go_forward) {
            std::lock_guard<std::mutex> lock(tabs_mutex_);
            auto it = std::find_if(
                tabs_.begin(), tabs_.end(), [bid](const QtTab& t) { return t.browser_id == bid; });
            if (it != tabs_.end()) {
              it->is_loading = is_loading;
              it->can_go_back = can_go_back;
              it->can_go_forward = can_go_forward;
              size_t tab_idx = std::distance(tabs_.begin(), it);
              if (tab_idx == active_tab_index_) {
                this->UpdateNavigationButtons(is_loading, can_go_back, can_go_forward);
              }
            }
          });

      tab.cef_client->SetTitleChangeCallback([this, bid](const std::string& title_str) {
        std::lock_guard<std::mutex> lock(tabs_mutex_);
        auto it = std::find_if(
            tabs_.begin(), tabs_.end(), [bid](const QtTab& t) { return t.browser_id == bid; });
        if (it != tabs_.end()) {
          it->title = QString::fromStdString(title_str);

          // Update tab title on Qt main thread
          QMetaObject::invokeMethod(
              this,
              [this, bid, title_str]() {
                if (closed_)
                  return;

                std::lock_guard<std::mutex> lock(tabs_mutex_);
                auto it2 = std::find_if(tabs_.begin(), tabs_.end(), [bid](const QtTab& t) {
                  return t.browser_id == bid;
                });

                if (it2 != tabs_.end()) {
                  size_t tab_idx = std::distance(tabs_.begin(), it2);
                  tabWidget_->setTabText(tab_idx, QString::fromStdString(title_str));
                }
              },
              Qt::QueuedConnection);
        }
      });

      // CRITICAL: Wire up render invalidation callback
      // This tells the widget to repaint when CEF has new content
      tab.cef_client->SetRenderInvalidatedCallback(
          [this, bid](CefRenderHandler::PaintElementType type) {
            (void)type;  // Unused parameter
            std::lock_guard<std::mutex> lock(tabs_mutex_);
            auto it = std::find_if(
                tabs_.begin(), tabs_.end(), [bid](const QtTab& t) { return t.browser_id == bid; });
            if (it != tabs_.end() && it->browser_widget) {
              // Schedule a repaint of the widget on the Qt main thread
              QMetaObject::invokeMethod(
                  it->browser_widget,
                  [widget = it->browser_widget]() {
                    if (widget) {
                      widget->update();
                    }
                  },
                  Qt::QueuedConnection);
            }
          });

      logger.Info("Callbacks wired for browser_id: " + std::to_string(bid));
    }
  }
}

void QtMainWindow::CloseTab(size_t index) {
  browser::BrowserId browser_to_close = 0;
  size_t new_active_index = 0;
  bool should_close_window = false;
  std::unique_ptr<GLRenderer> renderer_to_destroy;
  CefClient* client_to_hide = nullptr;
  BrowserWidget* widget_to_delete = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (index >= tabs_.size()) {
      logger.Error("Invalid tab index: " + std::to_string(index));
      return;
    }

    logger.Info("Closing tab: " + std::to_string(index));

    browser_to_close = tabs_[index].browser_id;
    renderer_to_destroy = std::move(tabs_[index].renderer);
    client_to_hide = tabs_[index].cef_client;
    widget_to_delete = tabs_[index].browser_widget;

    // Remove from tabs vector while holding the lock to keep state consistent
    tabs_.erase(tabs_.begin() + index);

    // Update browser widget tab indices so they stay in sync with tabs_
    for (size_t i = 0; i < tabs_.size(); ++i) {
      if (tabs_[i].browser_widget) {
        tabs_[i].browser_widget->SetTabIndex(i);
      }
    }

    // Check if we closed the last tab
    should_close_window = tabs_.empty();

    // Adjust active tab index if needed
    if (should_close_window) {
      active_tab_index_ = 0;
      new_active_index = 0;
    } else {
      size_t previous_active = active_tab_index_;
      size_t max_index = tabs_.empty() ? 0 : tabs_.size() - 1;
      active_tab_index_ = std::min(previous_active, max_index);
      new_active_index = active_tab_index_;
    }
  }

  // Remove the tab page outside the lock to avoid re-entrant signal handling deadlocks
  {
    QSignalBlocker blocker(tabWidget_);
    tabWidget_->removeTab(static_cast<int>(index));
  }

  if (widget_to_delete) {
    widget_to_delete->deleteLater();
  }

  // Hide browser (outside lock)
  if (client_to_hide && client_to_hide->GetBrowser()) {
    client_to_hide->GetBrowser()->GetHost()->WasHidden(true);
  }

  // Cleanup renderer
  if (renderer_to_destroy) {
    renderer_to_destroy->Cleanup();
  }

  // Close the browser instance (outside lock)
  if (engine_ && browser_to_close != 0) {
    engine_->CloseBrowser(browser_to_close, false);
  }

  // If we closed the last tab, close the window
  if (should_close_window) {
    logger.Info("No tabs left, closing window");
    Close();
    return;
  }

  // Switch to the new active tab
  SwitchToTab(new_active_index);
}

void QtMainWindow::CloseTabByBrowserId(browser::BrowserId browser_id) {
  size_t index_to_close = 0;
  bool found = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [browser_id](const QtTab& t) {
      return t.browser_id == browser_id;
    });

    if (it != tabs_.end()) {
      found = true;
      index_to_close = std::distance(tabs_.begin(), it);
      logger.Info("Found tab at index " + std::to_string(index_to_close) + " for browser_id " +
                  std::to_string(browser_id));
    }
  }

  if (!found) {
    logger.Error("Tab with browser_id " + std::to_string(browser_id) + " not found");
    return;
  }

  CloseTab(index_to_close);
}

void QtMainWindow::SwitchToTab(size_t index) {
  CefClient* client_to_show = nullptr;
  CefClient* client_to_hide = nullptr;
  BrowserWidget* widget_to_update = nullptr;
  QString url;
  bool is_loading = false;
  bool can_go_back = false;
  bool can_go_forward = false;
  bool index_changed = false;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    if (index >= tabs_.size()) {
      logger.Error("Invalid tab index: " + std::to_string(index));
      return;
    }

    size_t previous_index = active_tab_index_;
    if (previous_index < tabs_.size()) {
      client_to_hide = tabs_[previous_index].cef_client;
    }

    logger.Info("Switching to tab: " + std::to_string(index));

    active_tab_index_ = index;
    QtTab& tab = tabs_[index];

    client_to_show = tab.cef_client;
    widget_to_update = tab.browser_widget;
    url = tab.url;
    is_loading = tab.is_loading;
    can_go_back = tab.can_go_back;
    can_go_forward = tab.can_go_forward;

    index_changed = (previous_index != index);
  }

  // Update UI
  UpdateAddressBar(url);
  UpdateNavigationButtons(is_loading, can_go_back, can_go_forward);

  // Hide previous browser (if different)
  if (index_changed && client_to_hide && client_to_hide != client_to_show) {
    if (auto browser = client_to_hide->GetBrowser()) {
      browser->GetHost()->WasHidden(true);
    }
  }

  // Show new browser
  if (client_to_show && client_to_show->GetBrowser()) {
    auto host = client_to_show->GetBrowser()->GetHost();
    host->WasHidden(false);
    host->SetFocus(has_focus_);

    // Force CEF to send a paint event immediately
    host->Invalidate(PET_VIEW);
  }

  // Trigger repaint (use the widget pointer we saved inside the lock)
  if (widget_to_update) {
    widget_to_update->update();
  }

  logger.Info("Switched to tab " + std::to_string(index) + ", URL: " + url.toStdString());
}

void QtMainWindow::OnTabSwitch(int index) {
  logger.Info("Tab switched to: " + std::to_string(index));
  if (index >= 0 && static_cast<size_t>(index) < GetTabCount()) {
    SwitchToTab(static_cast<size_t>(index));
  }
}

void QtMainWindow::OnNewTabClicked() {
  logger.Info("Creating new tab");
  CreateTab("https://www.google.com");
}

void QtMainWindow::OnCloseTabClicked(int index) {
  logger.Info("Closing tab: " + std::to_string(index));
  if (index >= 0) {
    CloseTab(static_cast<size_t>(index));
  }
}

size_t QtMainWindow::GetTabCount() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return tabs_.size();
}

size_t QtMainWindow::GetActiveTabIndex() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return active_tab_index_;
}

QtTab* QtMainWindow::GetActiveTab() {
  // No lock here - caller must lock
  if (tabs_.empty() || active_tab_index_ >= tabs_.size()) {
    return nullptr;
  }

  return &tabs_[active_tab_index_];
}

bool QtMainWindow::WaitForLoadToComplete(size_t tab_index, int timeout_ms) const {
  auto start = std::chrono::steady_clock::now();

  while (true) {
    bool ready = false;
    {
      std::lock_guard<std::mutex> lock(tabs_mutex_);
      if (tab_index >= tabs_.size()) {
        logger.Warn("WaitForLoadToComplete: invalid tab index " + std::to_string(tab_index));
        return false;
      }

      const QtTab& tab = tabs_[tab_index];
      ready = (tab.cef_client != nullptr) && !tab.is_loading;
    }

    if (ready) {
      return true;
    }

    if (closed_) {
      logger.Warn("WaitForLoadToComplete aborted because window is closed");
      return false;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    if (elapsed >= timeout_ms) {
      logger.Warn("WaitForLoadToComplete timed out after " + std::to_string(timeout_ms) +
                  "ms for tab " + std::to_string(tab_index));
      return false;
    }

    CefDoMessageLoopWork();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

// ============================================================================
// Window Interface Implementation
// ============================================================================

std::string QtMainWindow::GetTitle() const {
  return windowTitle().toStdString();
}

void QtMainWindow::SetTitle(const std::string& title) {
  setWindowTitle(QString::fromStdString(title));
}

core::Size QtMainWindow::GetSize() const {
  return {width(), height()};
}

void QtMainWindow::SetSize(const core::Size& size) {
  resize(size.width, size.height);
}

float QtMainWindow::GetScaleFactor() const {
  return devicePixelRatioF();
}

void* QtMainWindow::GetNativeHandle() const {
  return (void*)winId();
}

void* QtMainWindow::GetRenderWidget() const {
  // Phase 2: return active tab's browser widget
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  if (active_tab_index_ < tabs_.size() && tabs_[active_tab_index_].browser_widget) {
    return (void*)tabs_[active_tab_index_].browser_widget;
  }
  return nullptr;
}

bool QtMainWindow::IsVisible() const {
  return visible_;
}

void QtMainWindow::Show() {
  show();
  visible_ = true;
}

void QtMainWindow::Hide() {
  hide();
  visible_ = false;
}

bool QtMainWindow::HasFocus() const {
  return has_focus_;
}

void QtMainWindow::Focus() {
  activateWindow();
  raise();
}

void QtMainWindow::SetBrowser(BrowserId browser_id) {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  for (auto& tab : tabs_) {
    if (tab.browser_id == browser_id) {
      logger.Info("Browser set for tab");
      return;
    }
  }

  logger.Warn("Browser ID not found in tabs");
}

BrowserId QtMainWindow::GetBrowser() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);

  if (tabs_.empty()) {
    return 0;
  }

  return tabs_[active_tab_index_].browser_id;
}

void QtMainWindow::Close(bool force) {
  if (force) {
    closed_ = true;
    close();
  } else {
    // Trigger close event which may be cancelled
    close();
  }
}

bool QtMainWindow::IsClosed() const {
  return closed_;
}

// ============================================================================
// QtWindowSystem Implementation
// ============================================================================

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

Result<void> QtWindowSystem::Initialize(int& argc, char* argv[], BrowserEngine* engine) {
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

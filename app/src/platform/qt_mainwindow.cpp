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
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QPalette>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
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
// UI Event Slot - Agent Panel
// ============================================================================

void QtMainWindow::onAgentButtonClicked() {
  if (agentPanel_) {
    agentPanel_->ToggleVisibility();

    // Update button state
    agentButton_->setChecked(agentPanel_->IsVisible());

    logger.Info(agentPanel_->IsVisible() ? "Agent sidebar shown" : "Agent sidebar hidden");
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

}  // namespace platform
}  // namespace athena

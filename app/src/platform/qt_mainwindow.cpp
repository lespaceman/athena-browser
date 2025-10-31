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
      splitter_(nullptr),
      agent_panel_last_width_(360),
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
  agentPanel_->setMinimumWidth(300);  // Only while visible

  // Create horizontal splitter: browser tabs on left, Agent sidebar on right
  splitter_ = new QSplitter(Qt::Horizontal, this);
  splitter_->addWidget(tabWidget_);   // Browser tabs (left)
  splitter_->addWidget(agentPanel_);  // Agent sidebar (right)

  // Set minimum constraints to prevent extreme positions
  tabWidget_->setMinimumWidth(600);  // Browser needs reasonable space for usability

  // Set stretch factors: browser gets most space (expanding), panel gets some (preferred)
  splitter_->setStretchFactor(0, 1);  // Browser expands to fill available space
  splitter_->setStretchFactor(1, 0);  // Panel stays at preferred size

  // Allow children to collapse so panel can hide completely
  splitter_->setChildrenCollapsible(true);

  // Make handle visible and draggable (3px width)
  splitter_->setHandleWidth(3);
  // Enable opaque resize for continuous, smooth updates during drag
  splitter_->setOpaqueResize(true);

  const QColor splitterColor = QApplication::palette().color(QPalette::Mid);
  splitter_->setStyleSheet(QStringLiteral("QSplitter::handle {"
                                          "  background-color: %1;"
                                          "  border: none;"
                                          "  margin: 0;"
                                          "  padding: 0;"
                                          "}"
                                          "QSplitter::handle:hover {"
                                          "  background-color: %2;"
                                          "}")
                               .arg(splitterColor.name(QColor::HexRgb),
                                    splitterColor.lighter(120).name(QColor::HexRgb)));

  setCentralWidget(splitter_);

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

  // Agent panel visibility changes
  connect(agentPanel_, &AgentPanel::visibilityChanged, this,
          &QtMainWindow::onAgentPanelVisibilityChanged);

  // Splitter signals - handle manual resize events
  connect(splitter_, &QSplitter::splitterMoved, this, &QtMainWindow::onSplitterMoved);

  // Keyboard shortcut: Ctrl+Shift+C (or Cmd+Shift+C on macOS)
  QShortcut* agentShortcut = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
  connect(agentShortcut, &QShortcut::activated, this, &QtMainWindow::onAgentButtonClicked);

  // Keyboard shortcut: F12 to open DevTools (same as Chrome/Firefox)
  QShortcut* devToolsShortcut = new QShortcut(QKeySequence(Qt::Key_F12), this);
  connect(devToolsShortcut, &QShortcut::activated, this, &QtMainWindow::ShowDevTools);
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

  logger.Debug("Window resized: {}x{} -> {}x{}, maximized={}",
               event->oldSize().width(), event->oldSize().height(),
               event->size().width(), event->size().height(),
               isMaximized());

  // IMPORTANT: Do NOT call OnBrowserSizeChanged() here!
  // Qt's layout system will automatically call resizeGL() on the BrowserWidget
  // when it resizes, and resizeGL() already handles the CEF notification with
  // the correct browser widget dimensions.
  //
  // Calling OnBrowserSizeChanged() here would:
  // 1. Use the wrong dimensions (window size instead of widget size)
  // 2. Create a double-resize (resizeGL already notified CEF)
  // 3. Cause rendering artifacts during maximize/restore
  //
  // See: qt_browserwidget.cpp:184-225 for the correct resize handling

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

void QtMainWindow::onAgentPanelVisibilityChanged(bool visible) {
  // Simple, predictable splitter behavior: show/hide + explicit setSizes

  if (!splitter_ || !tabWidget_ || !agentPanel_) {
    return;
  }

  QList<int> sizes = splitter_->sizes();
  int total_width = 0;
  if (sizes.size() >= 2) {
    total_width = sizes[0] + sizes[1];
  }
  if (total_width <= 0) {
    total_width = splitter_->width();
  }

  if (visible) {
    // Show panel: restore minimum width and use cached sidebar width when available
    agentPanel_->setMinimumWidth(300);
    agentPanel_->show();

    const int min_browser_width = tabWidget_->minimumWidth();
    const int min_sidebar_width = agentPanel_->minimumWidth();
    const int max_sidebar_width = std::max(0, total_width - min_browser_width);

    // Choose last remembered width or default to 30% if first time.
    if (agent_panel_last_width_ <= 0) {
      agent_panel_last_width_ = std::max(min_sidebar_width, total_width * 30 / 100);
    }
    int sidebar_width = 0;
    int browser_width = 0;

    if (max_sidebar_width <= 0) {
      sidebar_width = 0;
      browser_width = total_width;
    } else if (max_sidebar_width < min_sidebar_width) {
      sidebar_width = max_sidebar_width;
      browser_width = std::max(total_width - sidebar_width, min_browser_width);
    } else {
      sidebar_width =
          std::clamp(agent_panel_last_width_, min_sidebar_width, max_sidebar_width);
      browser_width = std::max(total_width - sidebar_width, min_browser_width);
      if (browser_width + sidebar_width != total_width) {
        sidebar_width = std::max(0, total_width - browser_width);
      }
    }

    splitter_->setSizes({browser_width, sidebar_width});
    agent_panel_last_width_ = sidebar_width;

    logger.Info("Agent panel shown - browser={}px, sidebar={}px", browser_width, sidebar_width);
  } else {
    // Hide panel: give all space to browser
    if (sizes.size() >= 2 && sizes[1] > 0) {
      agent_panel_last_width_ = sizes[1];
    }

    agentPanel_->hide();
    agentPanel_->setMinimumWidth(0);

    splitter_->setSizes({total_width, 0});

    logger.Info("Agent panel hidden - browser gets all space: {}px", total_width);
  }

  // Event-driven resize sync handles the rest automatically:
  // 1. Qt calls resizeGL() on the browser widget
  // 2. resizeGL() updates viewport and calls CEF WasResized()
  // 3. CEF eventually calls OnPaint with new dimensions
  // 4. OnCefPaint() checks size match and triggers update()
  // No timers needed!
}

void QtMainWindow::onSplitterMoved(int pos, int index) {
  // Handle manual splitter resize
  // With opaque resize enabled, Qt automatically calls resizeGL() during drag.
  // Event-driven resize sync handles everything automatically:
  // 1. Qt calls resizeGL() continuously during drag
  // 2. resizeGL() updates viewport and calls CEF WasResized()
  // 3. CEF eventually calls OnPaint with new dimensions
  // 4. OnCefPaint() checks size match and triggers update()
  // No manual intervention needed!

  if (splitter_) {
    QList<int> sizes = splitter_->sizes();
    if (sizes.size() >= 2 && sizes[1] > 0) {
      agent_panel_last_width_ = sizes[1];
    }
  }

  logger.Debug("Splitter moved to position {} (index {})", pos, index);
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

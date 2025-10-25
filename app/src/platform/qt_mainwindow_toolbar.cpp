/**
 * QtMainWindow Toolbar Implementation
 *
 * Handles toolbar creation and navigation button event handlers.
 * Maintains clean separation: toolbar UI creation and navigation actions.
 */

#include "browser/cef_client.h"
#include "platform/qt_mainwindow.h"
#include "utils/logging.h"

#include <QPushButton>
#include <QStyle>
#include <QUrl>

namespace athena {
namespace platform {

using namespace browser;
using namespace utils;

static Logger logger("QtMainWindow::Toolbar");

// ============================================================================
// Toolbar Creation
// ============================================================================

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

// ============================================================================
// Navigation Button Event Handlers
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

// ============================================================================
// Navigation Methods
// ============================================================================

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

void QtMainWindow::ShowDevTools() {
  CefClient* client = nullptr;

  {
    std::lock_guard<std::mutex> lock(tabs_mutex_);
    QtTab* tab = GetActiveTab();
    if (tab && tab->cef_client) {
      client = tab->cef_client;
    }
  }

  // Call CEF outside the lock
  if (client) {
    client->ShowDevTools();
    logger.Info("DevTools opened for active tab");
  } else {
    logger.Warn("ShowDevTools: No active tab with CEF client");
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

QString QtMainWindow::GetCurrentUrl() const {
  std::lock_guard<std::mutex> lock(tabs_mutex_);
  return current_url_;
}

}  // namespace platform
}  // namespace athena

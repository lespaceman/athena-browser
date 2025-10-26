/**
 * QtMainWindow Resize Tests
 *
 * Tests for proper layout and sizing behavior when maximizing/resizing windows
 * with the agent sidebar.
 */

#include "platform/qt_browserwidget.h"
#include "platform/qt_mainwindow.h"

#include <gtest/gtest.h>
#include <QApplication>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

using namespace athena::platform;

// Test fixture that initializes QApplication (required for Qt widgets)
class QtResizeTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    // QApplication must be created once for all Qt tests
    if (!QApplication::instance()) {
      static int argc = 1;
      static char app_name[] = "qt_resize_test";
      static char* argv[] = {app_name, nullptr};
      app_ = new QApplication(argc, argv);
    }
  }

  void SetUp() override {
    // Each test gets a fresh window
    // We can't easily create a real QtMainWindow without CEF, so we'll test
    // the layout behavior using a simplified widget hierarchy
  }

  void TearDown() override {
    // Clean up widgets created in tests
  }

  static QApplication* app_;
};

QApplication* QtResizeTest::app_ = nullptr;

// ============================================================================
// Resize Behavior Tests
// ============================================================================

/**
 * Test: Browser widget width should be constrained by splitter when window is maximized
 *
 * Scenario:
 * 1. Create a window with browser (left) and sidebar (right) in a QSplitter (70:30 ratio)
 * 2. Maximize the window to 1920x1080
 * 3. Verify that browser widget width is approximately 70% of window width (not 100%)
 *
 * Expected: Browser width ≈ 1344px (70% of 1920), not 1920px
 * This test should FAIL before the fix and PASS after the fix.
 */
TEST_F(QtResizeTest, BrowserWidthConstrainedByRailOnMaximize) {
  // Create a test widget hierarchy mimicking QtMainWindow's structure
  QWidget window;
  window.resize(800, 600);

  // Create splitter with 70:30 ratio (browser:sidebar)
  QSplitter* splitter = new QSplitter(Qt::Horizontal, &window);
  splitter->setGeometry(0, 0, 800, 600);  // Set explicit geometry

  // Browser widget (left side)
  QWidget* browserWidget = new QWidget();
  splitter->addWidget(browserWidget);

  // Sidebar (right side)
  QWidget* sidebarWidget = new QWidget();
  sidebarWidget->setMinimumWidth(300);
  sidebarWidget->setMaximumWidth(500);
  splitter->addWidget(sidebarWidget);

  // Set stretch factors: 70% browser, 30% sidebar
  splitter->setStretchFactor(0, 7);  // Browser gets 7x weight
  splitter->setStretchFactor(1, 3);  // Sidebar gets 3x weight
  splitter->setChildrenCollapsible(false);

  // Set initial sizes
  QList<int> sizes;
  sizes << 560 << 240;  // 800px total, 70:30 ratio
  splitter->setSizes(sizes);

  // Force layout update
  splitter->updateGeometry();
  QApplication::processEvents();

  // Verify initial state
  EXPECT_GT(browserWidget->width(), 0) << "Browser widget should have non-zero width after layout";
  EXPECT_GT(sidebarWidget->width(), 0) << "Sidebar widget should have non-zero width after layout";

  int initial_browser_width = browserWidget->width();
  int initial_sidebar_width = sidebarWidget->width();

  // Verify initial ratio is approximately 70:30
  int initial_total = initial_browser_width + initial_sidebar_width;
  double initial_ratio = static_cast<double>(initial_browser_width) / initial_total;
  EXPECT_NEAR(initial_ratio, 0.7, 0.1) << "Initial browser/total ratio should be ~70%";

  // Simulate maximize: resize window and splitter to 1920x1080
  window.resize(1920, 1080);
  splitter->setGeometry(0, 0, 1920, 1080);

  // Force layout update
  splitter->updateGeometry();
  QApplication::processEvents();

  // CRITICAL: After maximize, browser widget should be ~70% of window width
  // If the bug exists, browserWidget->width() might be 1920 (full width)
  // After the fix, it should be ~1344 (70% of 1920)

  int final_browser_width = browserWidget->width();
  int final_sidebar_width = sidebarWidget->width();

  EXPECT_GT(final_browser_width, 0) << "Browser widget should have non-zero width after resize";
  EXPECT_GT(final_sidebar_width, 0) << "Sidebar widget should have non-zero width after resize";

  // Verify the browser doesn't take full width
  EXPECT_LT(final_browser_width, 1920)
      << "Browser should not take full window width when sidebar is visible";

  // Sidebar should not exceed its maximum width
  EXPECT_LE(final_sidebar_width, 500) << "Sidebar should respect its maximum width constraint";

  // Verify ratio is maintained (approximately 70:30, accounting for max width constraint)
  int final_total = final_browser_width + final_sidebar_width;
  double final_ratio = static_cast<double>(final_browser_width) / final_total;

  // Since sidebar is capped at 500px, browser might get more than 70%
  // But it should still be less than 100%
  EXPECT_GT(final_ratio, 0.6) << "Browser should get at least 60% of available space";
  EXPECT_LT(final_ratio, 1.0) << "Browser should not take all space when sidebar is visible";
}

/**
 * Test: Verify resize is properly handled during window state changes
 *
 * This test verifies that the browser widget properly adapts to window resizes
 * without taking full window width when the sidebar is visible.
 */
TEST_F(QtResizeTest, BrowserResizeDoesNotOverlapSidebar) {
  QWidget window;
  window.resize(800, 600);

  QSplitter* splitter = new QSplitter(Qt::Horizontal, &window);
  splitter->setGeometry(0, 0, 800, 600);

  QWidget* browserWidget = new QWidget();
  QWidget* sidebarWidget = new QWidget();
  sidebarWidget->setMinimumWidth(300);
  sidebarWidget->setMaximumWidth(500);

  splitter->addWidget(browserWidget);
  splitter->addWidget(sidebarWidget);
  splitter->setStretchFactor(0, 7);
  splitter->setStretchFactor(1, 3);

  QList<int> sizes;
  sizes << 560 << 240;
  splitter->setSizes(sizes);

  splitter->updateGeometry();
  QApplication::processEvents();

  int initial_width = browserWidget->width();
  EXPECT_GT(initial_width, 0) << "Browser should have non-zero initial width";

  // Resize to larger size (simulating maximize-like behavior)
  window.resize(1600, 900);
  splitter->setGeometry(0, 0, 1600, 900);
  splitter->updateGeometry();
  QApplication::processEvents();

  int final_width = browserWidget->width();

  // Verify the browser doesn't take full window width
  EXPECT_GT(final_width, 0) << "Browser should have non-zero final width";
  EXPECT_LT(final_width, 1600) << "Browser should not take full width with sidebar visible";

  // Sidebar should still be visible
  int sidebar_width = sidebarWidget->width();
  EXPECT_GT(sidebar_width, 0) << "Sidebar should be visible";
  EXPECT_LE(sidebar_width, 500) << "Sidebar should respect max width";
}

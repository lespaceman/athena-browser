#include "platform/qt_chat_bubble.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMargins>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QVBoxLayout>
#include <QWidget>

using namespace athena::platform;

namespace {

AgentPanelPalette MakeTestPalette() {
  AgentPanelPalette palette;
  palette.dark = false;
  palette.accent = QColor("#2563EB");

  palette.userBubble.background = QColor("#2563EB");
  palette.userBubble.text = QColor("#F8FAFC");
  palette.userBubble.label = QColor("#F8FAFC");
  palette.userBubble.codeBackground = QColor("#1D4ED8");
  palette.userBubble.codeText = QColor("#F8FAFC");

  palette.assistantBubble.background = QColor("#1F2937");
  palette.assistantBubble.text = QColor("#F9FAFB");
  palette.assistantBubble.label = QColor("#E2E8F0");
  palette.assistantBubble.codeBackground = QColor("#111827");
  palette.assistantBubble.codeText = QColor("#E2E8F0");

  return palette;
}

void PumpQtEvents(int iterations = 6) {
  for (int i = 0; i < iterations; ++i) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
}

QString LongTestMessage() {
  return QStringLiteral(
             "This is a deliberately long single-line message that should wrap naturally within "
             "the chat bubble regardless of how narrow the panel becomes, ensuring there is no "
             "horizontal scrolling required by the user even when the layout is constrained to "
             "compact sizes. ")
      .append(QStringLiteral(
          "It continues with additional descriptive text to guarantee a noticeable difference in "
          "wrapping behavior when the bubble width is reduced, making it easy to detect "
          "regressions in the geometry logic."));
}

}  // namespace

class QtChatBubbleTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (!QApplication::instance()) {
      static int argc = 1;
      static char app_name[] = "qt_chat_bubble_test";
      static char* argv[] = {app_name, nullptr};

      qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

      app_ = new QApplication(argc, argv);
    }
  }

  static QApplication* app_;
};

QApplication* QtChatBubbleTest::app_ = nullptr;

TEST_F(QtChatBubbleTest, WrapsLongLinesWithinBubbleWidth) {
  QWidget parent;
  parent.resize(240, 240);
  auto* layout = new QVBoxLayout(&parent);
  layout->setContentsMargins(0, 0, 0, 0);

  AgentPanelPalette palette = MakeTestPalette();

  auto* bubble = new ChatBubble(ChatBubble::Role::Assistant, LongTestMessage(), palette, &parent);
  bubble->setMaximumWidth(parent.width());
  layout->addWidget(bubble);

  parent.show();
  bubble->show();

  PumpQtEvents();

  auto* content = bubble->findChild<QTextEdit*>();
  ASSERT_NE(content, nullptr);

  QTextDocument* document = content->document();
  ASSERT_NE(document, nullptr);

  int maxLines = 0;
  for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
    if (QTextLayout* layout = block.layout()) {
      maxLines = std::max(maxLines, layout->lineCount());
    }
  }

  EXPECT_GE(maxLines, 2) << "Text should wrap to multiple lines inside a narrow bubble";
  EXPECT_FALSE(content->horizontalScrollBar()->isVisible())
      << "Horizontal scrollbar should remain hidden";
}

TEST_F(QtChatBubbleTest, LineCountIncreasesAsBubbleNarrows) {
  QWidget parent;
  parent.resize(360, 260);
  auto* layout = new QVBoxLayout(&parent);
  layout->setContentsMargins(0, 0, 0, 0);

  AgentPanelPalette palette = MakeTestPalette();

  auto* bubble = new ChatBubble(ChatBubble::Role::Assistant, LongTestMessage(), palette, &parent);
  bubble->setMaximumWidth(parent.width());
  layout->addWidget(bubble);

  parent.show();
  bubble->show();

  PumpQtEvents();

  auto* content = bubble->findChild<QTextEdit*>();
  ASSERT_NE(content, nullptr);

  QTextDocument* document = content->document();
  ASSERT_NE(document, nullptr);

  auto maxLineCount = [&]() {
    int maxLines = 0;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
      if (QTextLayout* layout = block.layout()) {
        maxLines = std::max(maxLines, layout->lineCount());
      }
    }
    return maxLines;
  };

  const int initialLines = maxLineCount();
  EXPECT_GE(initialLines, 2) << "Initial layout should already wrap long content";
  const int initialBubbleWidth = bubble->width();

  parent.resize(180, 260);
  bubble->setMaximumWidth(parent.width());
  bubble->updateGeometry();
  PumpQtEvents();

  const int narrowLines = maxLineCount();
  const int narrowBubbleWidth = bubble->width();

  SCOPED_TRACE(testing::Message() << "initialWidth=" << initialBubbleWidth << " narrowWidth="
                                  << narrowBubbleWidth << " initialLines=" << initialLines
                                  << " narrowLines=" << narrowLines);

  EXPECT_GE(narrowLines, initialLines)
      << "Line count should not decrease when the bubble gets narrower";
  EXPECT_LT(narrowBubbleWidth, initialBubbleWidth)
      << "Bubble width should contract with the parent layout";

  EXPECT_EQ(narrowBubbleWidth, parent.width())
      << "Bubble width should track the parent layout width";
  EXPECT_LE(content->viewport()->width(), narrowBubbleWidth)
      << "Viewport should remain within the bubble bounds";

  EXPECT_FALSE(content->horizontalScrollBar()->isVisible())
      << "Horizontal scrollbar should remain hidden after resizing";
}

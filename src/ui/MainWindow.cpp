#include "ui/MainWindow.h"

#include <QLabel>
#include <QSplitter>

#include "ui/shell/NavigationPane.h"

namespace ngks::ui {

MainWindow::MainWindow()
{
    setWindowTitle("NGKsMailcpp - Phase 1");
    resize(1200, 700);

    auto* splitter = new QSplitter(this);

    auto* navigationPane = new ngks::ui::shell::NavigationPane(splitter);

    auto* messageListPane = new QLabel("MessageList (select a folder)", splitter);
    auto* readingPane = new QLabel("ReadingPane", splitter);

    messageListPane->setAlignment(Qt::AlignCenter);
    readingPane->setAlignment(Qt::AlignCenter);

    splitter->addWidget(navigationPane);
    splitter->addWidget(messageListPane);
    splitter->addWidget(readingPane);
    splitter->setSizes({260, 360, 580});

    setCentralWidget(splitter);

    connect(navigationPane, &ngks::ui::shell::NavigationPane::FolderSelected, this,
        [messageListPane](int, int, const QString& folderRole, const QString& folderName) {
            Q_UNUSED(folderRole);
            messageListPane->setText(QString("%1 (0)").arg(folderName));
        }
    );
}

} // namespace ngks::ui
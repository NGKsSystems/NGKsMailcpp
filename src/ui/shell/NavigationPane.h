#pragma once

#include <QWidget>

class QLabel;
class QStackedLayout;
class QTreeView;

namespace ngks::ui::models { class FolderTreeModel; }

namespace ngks::ui::shell {

class NavigationPane final : public QWidget {
    Q_OBJECT

public:
    explicit NavigationPane(QWidget* parent = nullptr);

    void Refresh();

signals:
    void FolderSelected(int accountId, int folderId, QString folderRole, QString folderName);

private:
    QStackedLayout* stack_ = nullptr;
    QLabel* emptyState_ = nullptr;
    QTreeView* tree_ = nullptr;
    ngks::ui::models::FolderTreeModel* model_ = nullptr;

    void WireSignals();
};

} // namespace ngks::ui::shell
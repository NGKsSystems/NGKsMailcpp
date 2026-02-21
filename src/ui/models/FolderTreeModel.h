#pragma once

#include <QStandardItemModel>

namespace ngks::ui::models {

class FolderTreeModel final : public QStandardItemModel {
public:
    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        FolderIdRole,
        FolderRoleRole,
        IsAccountNodeRole
    };

    explicit FolderTreeModel(QObject* parent = nullptr);

    // Loads accounts/folders from the default QSqlDatabase connection.
    // If DB is not available, the model remains empty.
    void Reload();

    bool HasResolvedAccounts() const;
    QModelIndex FirstInboxIndex() const;

private:
    bool hasResolvedAccounts_ = false;
    QModelIndex firstInboxIndex_;
};

} // namespace ngks::ui::models
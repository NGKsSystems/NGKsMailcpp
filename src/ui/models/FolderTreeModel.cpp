#include "ui/models/FolderTreeModel.hpp"

#include "ui/models/MailUiService.hpp"

namespace ngks::ui::models {

FolderTreeModel::FolderTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

QModelIndex FolderTreeModel::index(int row, int column, const QModelIndex& parentIndex) const
{
    if (parentIndex.isValid() || row < 0 || row >= rows_.size() || column != 0) {
        return QModelIndex();
    }
    return createIndex(row, column);
}

QModelIndex FolderTreeModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int FolderTreeModel::rowCount(const QModelIndex& parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0;
    }
    return rows_.size();
}

int FolderTreeModel::columnCount(const QModelIndex& parentIndex) const
{
    Q_UNUSED(parentIndex);
    return 1;
}

QVariant FolderTreeModel::data(const QModelIndex& modelIndex, int role) const
{
    if (!modelIndex.isValid() || modelIndex.row() < 0 || modelIndex.row() >= rows_.size()) {
        return {};
    }

    const auto& row = rows_[modelIndex.row()];
    switch (role) {
    case Qt::DisplayRole:
        return row.displayName;
    case AccountIdRole:
        return row.accountId;
    case FolderIdRole:
        return row.folderId;
    case FolderRoleRole:
        return row.specialUse;
    case IsAccountNodeRole:
        return false;
    default:
        return {};
    }
}

QVariant FolderTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0) {
        return "Folders";
    }
    return {};
}

void FolderTreeModel::SetService(const std::shared_ptr<IMailUiService>& service)
{
    service_ = service;
}

void FolderTreeModel::SetAccountId(int accountId)
{
    accountId_ = accountId;
}

void FolderTreeModel::Reload()
{
    beginResetModel();
    rows_.clear();
    firstInboxIndex_ = QModelIndex();
    if (service_ && accountId_ > 0) {
        rows_ = service_->ListFolders(accountId_);
        for (int i = 0; i < rows_.size(); ++i) {
            const QString role = rows_[i].specialUse.toLower();
            const QString name = rows_[i].displayName.toLower();
            if (role == "\\inbox" || name == "inbox") {
                firstInboxIndex_ = index(i, 0, {});
                break;
            }
        }
        if (!firstInboxIndex_.isValid() && !rows_.isEmpty()) {
            firstInboxIndex_ = index(0, 0, {});
        }
    }
    endResetModel();
}

bool FolderTreeModel::HasResolvedAccounts() const
{
    return !rows_.isEmpty();
}

QModelIndex FolderTreeModel::FirstInboxIndex() const
{
    return firstInboxIndex_;
}

} // namespace ngks::ui::models
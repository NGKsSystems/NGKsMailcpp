#pragma once

#include <QAbstractItemModel>
#include <QModelIndex>

#include <memory>

#include "ui/models/MailUiService.hpp"

namespace ngks::ui::models {

class IMailUiService;

class FolderTreeModel final : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        FolderIdRole,
        FolderRoleRole,
        IsAccountNodeRole,
    };

    explicit FolderTreeModel(QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void SetService(const std::shared_ptr<IMailUiService>& service);
    void SetAccountId(int accountId);
    int AccountId() const { return accountId_; }
    void Reload();
    bool HasResolvedAccounts() const;
    QModelIndex FirstInboxIndex() const;

private:
    std::shared_ptr<IMailUiService> service_;
    QVector<FolderItem> rows_;
    int accountId_ = 0;
    QModelIndex firstInboxIndex_;
};

} // namespace ngks::ui::models

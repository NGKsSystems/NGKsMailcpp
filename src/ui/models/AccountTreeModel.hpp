#pragma once

#include <QAbstractItemModel>

#include <memory>

#include "ui/models/MailUiService.hpp"

namespace ngks::ui::models {

class IMailUiService;

class AccountTreeModel final : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        ProviderRole,
        EmailRole,
    };

    explicit AccountTreeModel(QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void SetService(const std::shared_ptr<IMailUiService>& service);
    void Reload();

private:
    std::shared_ptr<IMailUiService> service_;
    QVector<AccountItem> rows_;
};

} // namespace ngks::ui::models

#pragma once

#include <QAbstractTableModel>

#include <memory>

#include "ui/models/MailUiService.hpp"

namespace ngks::ui::models {

class IMailUiService;

class MessageListModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Columns {
        FromColumn = 0,
        SubjectColumn,
        DateColumn,
        ProviderColumn,
        ReadStateColumn,
        ColumnCount
    };

    explicit MessageListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void SetService(const std::shared_ptr<IMailUiService>& service);
    void SetContext(int accountId, int folderId);
    void Reload();

    int MessageIdAt(int row) const;
    bool IsUnreadAt(int row) const;
    MessageItem ItemAt(int row) const;
    bool MarkReadAt(int row, bool read);
    bool DeleteAt(int row);

    QString PreviewHtmlAt(int row) const;

private:
    std::shared_ptr<IMailUiService> service_;
    QVector<MessageItem> rows_;
    int accountId_ = 0;
    int folderId_ = 0;
};

} // namespace ngks::ui::models

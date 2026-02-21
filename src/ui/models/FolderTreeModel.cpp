#include "ui/models/FolderTreeModel.h"

#include <QHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

namespace ngks::ui::models {

FolderTreeModel::FolderTreeModel(QObject* parent)
    : QStandardItemModel(parent)
{
    setHorizontalHeaderLabels({ "Folders" });
}

static QStandardItem* MakeItem(const QString& text)
{
    auto* it = new QStandardItem(text);
    it->setEditable(false);
    return it;
}

static QString NormalizeName(const QString& name)
{
    QString trimmed = name;
    if (trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.size() >= 2) {
        trimmed = trimmed.mid(1, trimmed.size() - 2);
    }
    return trimmed;
}

void FolderTreeModel::Reload()
{
    clear();
    setHorizontalHeaderLabels({ "Folders" });
    hasResolvedAccounts_ = false;
    firstInboxIndex_ = QModelIndex();

    QSqlDatabase db = QSqlDatabase::database(); // default connection
    if (!db.isValid() || !db.isOpen()) {
        return;
    }

    QSqlQuery qa(db);
    if (!qa.exec("SELECT id, email, provider FROM accounts WHERE status='RESOLVED' ORDER BY id ASC")) {
        return;
    }

    while (qa.next()) {
        hasResolvedAccounts_ = true;

        const int accountId = qa.value(0).toInt();
        const QString email = qa.value(1).toString();
        const QString provider = qa.value(2).toString();

        auto* accountItem = MakeItem(QString("%1 (%2)").arg(email, provider));
        accountItem->setData(accountId, AccountIdRole);
        accountItem->setData(true, IsAccountNodeRole);
        invisibleRootItem()->appendRow(accountItem);

        QSqlQuery qf(db);
        qf.prepare(
            "SELECT id, remote_name, display_name, delimiter, attrs_json, special_use "
            "FROM folders WHERE account_id = :aid ORDER BY id ASC");
        qf.bindValue(":aid", accountId);
        if (!qf.exec()) {
            continue;
        }

        QHash<QString, QStandardItem*> pathIndex;

        while (qf.next()) {
            const int folderId = qf.value(0).toInt();
            const QString remoteName = NormalizeName(qf.value(1).toString());
            const QString displayName = qf.value(2).toString();
            QString delimiter = qf.value(3).toString();
            if (delimiter.isEmpty()) {
                delimiter = "/";
            }
            const QString attrsJson = qf.value(4).toString();
            const QString specialUse = qf.value(5).toString();

            QStringList parts = remoteName.split(delimiter, Qt::SkipEmptyParts);
            if (parts.isEmpty()) {
                parts = QStringList{displayName.isEmpty() ? remoteName : displayName};
            }

            QString currentPath;
            QStandardItem* parent = accountItem;
            for (int i = 0; i < parts.size(); ++i) {
                if (!currentPath.isEmpty()) {
                    currentPath += delimiter;
                }
                currentPath += parts[i];

                QStandardItem* node = pathIndex.value(currentPath, nullptr);
                if (!node) {
                    const bool isLeaf = (i == parts.size() - 1);
                    node = MakeItem(isLeaf && !displayName.isEmpty() ? displayName : parts[i]);
                    node->setData(accountId, AccountIdRole);
                    node->setData(isLeaf ? folderId : -1, FolderIdRole);
                    node->setData(isLeaf ? specialUse.toLower() : QString(), FolderRoleRole);
                    node->setData(false, IsAccountNodeRole);
                    if (isLeaf) {
                        node->setData(remoteName, Qt::ToolTipRole);
                    }
                    parent->appendRow(node);
                    pathIndex.insert(currentPath, node);
                }
                parent = node;
            }

            if (firstInboxIndex_.isValid()) {
                continue;
            }
            if (specialUse.compare("\\inbox", Qt::CaseInsensitive) == 0 || remoteName.compare("INBOX", Qt::CaseInsensitive) == 0) {
                firstInboxIndex_ = parent->index();
            }
            Q_UNUSED(attrsJson);
        }
    }
}

bool FolderTreeModel::HasResolvedAccounts() const
{
    return hasResolvedAccounts_;
}

QModelIndex FolderTreeModel::FirstInboxIndex() const
{
    return firstInboxIndex_;
}

} // namespace ngks::ui::models
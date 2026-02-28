#pragma once

#include <QSqlDatabase>
#include <QString>

namespace ngks::core::mail::providers::imap {

class MessageSyncService {
public:
    static bool SyncFolder(QSqlDatabase& db, int accountId, int folderId, int limit, QString& outError);
};

} // namespace ngks::core::mail::providers::imap

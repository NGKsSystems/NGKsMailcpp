#include "core/mail/providers/imap/FolderMirrorService.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

#include "core/storage/Db.h"

namespace ngks::core::mail::providers::imap {

bool FolderMirrorService::MirrorResolvedAccount(
    ngks::core::storage::Db& db,
    const ResolveRequest& request,
    const QString& credentialRef,
    const QVector<ResolvedFolder>& folders,
    int& outAccountId,
    QString& outError,
    const QString& providerId)
{
    outAccountId = -1;
    outError.clear();

    if (!db.IsOpen()) {
        outError = "DB is not open";
        return false;
    }

    auto& sqlDb = db.Handle();
    if (!sqlDb.transaction()) {
        outError = "Failed to start transaction";
        return false;
    }

    QSqlQuery upsert(sqlDb);
    upsert.prepare(
        "INSERT INTO accounts(email, provider, imap_host, imap_port, tls_mode, auth_method, credential_ref, status, sync_state, created_at) "
        "VALUES(:email, :provider, :host, :port, :tls, :auth, :cred, 'RESOLVED', '', datetime('now')) "
        "ON CONFLICT(email) DO UPDATE SET "
        "provider=excluded.provider, imap_host=excluded.imap_host, imap_port=excluded.imap_port, "
        "tls_mode=excluded.tls_mode, auth_method=excluded.auth_method, credential_ref=excluded.credential_ref, "
        "status='RESOLVED'");
    upsert.bindValue(":email", request.email);
    upsert.bindValue(":provider", providerId.trimmed().isEmpty() ? QString("imap") : providerId.trimmed());
    upsert.bindValue(":host", request.host);
    upsert.bindValue(":port", request.port);
    upsert.bindValue(":tls", request.tls ? "TLS" : "PLAIN");
    upsert.bindValue(":auth", "PASSWORD");
    upsert.bindValue(":cred", credentialRef);

    if (!upsert.exec()) {
        outError = upsert.lastError().text();
        sqlDb.rollback();
        return false;
    }

    QSqlQuery accountLookup(sqlDb);
    accountLookup.prepare("SELECT id FROM accounts WHERE email=:email LIMIT 1");
    accountLookup.bindValue(":email", request.email);
    if (!accountLookup.exec() || !accountLookup.next()) {
        outError = "Failed to retrieve resolved account id";
        sqlDb.rollback();
        return false;
    }

    outAccountId = accountLookup.value(0).toInt();

    QSqlQuery clearFolders(sqlDb);
    clearFolders.prepare("DELETE FROM folders WHERE account_id=:aid");
    clearFolders.bindValue(":aid", outAccountId);
    if (!clearFolders.exec()) {
        outError = clearFolders.lastError().text();
        sqlDb.rollback();
        return false;
    }

    QSqlQuery insertFolder(sqlDb);
    insertFolder.prepare(
        "INSERT INTO folders(account_id, remote_name, display_name, delimiter, attrs_json, special_use, sync_state, created_at) "
        "VALUES(:aid, :remote, :display, :delim, :attrs, :special, '', datetime('now'))");

    for (const auto& folder : folders) {
        insertFolder.bindValue(":aid", outAccountId);
        insertFolder.bindValue(":remote", folder.remoteName);
        insertFolder.bindValue(":display", folder.displayName);
        insertFolder.bindValue(":delim", folder.delimiter.isEmpty() ? "/" : folder.delimiter);
        insertFolder.bindValue(":attrs", folder.attrsJson);
        insertFolder.bindValue(":special", folder.specialUse.isNull() ? QString("") : folder.specialUse);
        if (!insertFolder.exec()) {
            outError = insertFolder.lastError().text();
            sqlDb.rollback();
            return false;
        }
    }

    if (!sqlDb.commit()) {
        outError = "Failed to commit mirror transaction";
        return false;
    }

    return true;
}

}

#include "db/BasicCredentialStore.hpp"

#include <QSqlError>
#include <QSqlQuery>

namespace ngks::db {

bool BasicCredentialStore::Upsert(ngks::core::storage::Db& db, const BasicCredentialRecord& rec, QString& outError)
{
    outError.clear();

    if (!db.IsOpen()) {
        outError = "db-not-open";
        return false;
    }

    QSqlQuery q(db.Handle());
    q.prepare(
        "INSERT INTO basic_credentials(provider, email, username, secret_enc, created_at) "
        "VALUES(:provider, :email, :username, :secret, datetime('now')) "
        "ON CONFLICT(provider, email) DO UPDATE SET "
        "username=excluded.username, secret_enc=excluded.secret_enc, created_at=datetime('now')");

    q.bindValue(":provider", rec.providerId.trimmed());
    q.bindValue(":email", rec.email.trimmed());
    q.bindValue(":username", rec.username.trimmed());
    q.bindValue(":secret", rec.secret); // TODO: encrypt at rest when keystore support is added.

    if (!q.exec()) {
        outError = q.lastError().text();
        return false;
    }

    return true;
}

} // namespace ngks::db

// src/core/auth/OAuthStore.cpp
#include "core/auth/OAuthStore.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ngks::core::auth {

static qint64 NowUtcSeconds()
{
    return QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
}

bool OAuthStore::EnsureTables(ngks::core::storage::Db& db, QString& outError)
{
    outError.clear();

    QSqlDatabase handle = db.Handle();
    if (!handle.isValid() || !handle.isOpen()) {
        outError = "OAuthStore::EnsureTables: DB handle invalid or not open";
        return false;
    }

    QSqlQuery q(handle);

    // Keep schema minimal and stable for Phase 2.
    // NOTE: refresh_token is required for Gmail IMAP OAuth (offline access).
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS oauth_tokens ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  provider TEXT NOT NULL,"
        "  email TEXT NOT NULL,"
        "  refresh_token TEXT NOT NULL,"
        "  access_token TEXT,"
        "  expires_at_utc INTEGER NOT NULL DEFAULT 0,"
        "  created_at_utc INTEGER NOT NULL,"
        "  updated_at_utc INTEGER NOT NULL,"
        "  UNIQUE(provider, email)"
        ");";

    if (!q.exec(QString::fromUtf8(ddl))) {
        outError = QString("OAuthStore::EnsureTables: %1").arg(q.lastError().text());
        return false;
    }

    return true;
}

bool OAuthStore::UpsertToken(ngks::core::storage::Db& db, const OAuthTokenRecord& rec, QString& outError)
{
    outError.clear();

    if (rec.provider.trimmed().isEmpty() || rec.email.trimmed().isEmpty()) {
        outError = "OAuthStore::UpsertToken: provider/email required";
        return false;
    }
    if (rec.refreshToken.isEmpty()) {
        outError = "OAuthStore::UpsertToken: refresh_token required";
        return false;
    }

    QSqlDatabase handle = db.Handle();
    if (!handle.isValid() || !handle.isOpen()) {
        outError = "OAuthStore::UpsertToken: DB handle invalid or not open";
        return false;
    }

    const qint64 now = NowUtcSeconds();

    // SQLite UPSERT. Works on modern SQLite (should be present in Qt builds).
    QSqlQuery q(handle);
    q.prepare(
        "INSERT INTO oauth_tokens("
        "  provider, email, refresh_token, access_token, expires_at_utc, created_at_utc, updated_at_utc"
        ") VALUES ("
        "  :provider, :email, :refresh_token, :access_token, :expires_at_utc, :created_at_utc, :updated_at_utc"
        ") "
        "ON CONFLICT(provider, email) DO UPDATE SET "
        "  refresh_token = excluded.refresh_token, "
        "  access_token = excluded.access_token, "
        "  expires_at_utc = excluded.expires_at_utc, "
        "  updated_at_utc = excluded.updated_at_utc"
    );

    q.bindValue(":provider", rec.provider.trimmed());
    q.bindValue(":email", rec.email.trimmed());
    q.bindValue(":refresh_token", rec.refreshToken);
    q.bindValue(":access_token", rec.accessToken);
    q.bindValue(":expires_at_utc", static_cast<qint64>(rec.expiresAtUtc));
    q.bindValue(":created_at_utc", now);
    q.bindValue(":updated_at_utc", now);

    if (!q.exec()) {
        outError = QString("OAuthStore::UpsertToken: %1").arg(q.lastError().text());
        return false;
    }

    return true;
}

bool OAuthStore::GetRefreshToken(ngks::core::storage::Db& db,
                                const QString& provider,
                                const QString& email,
                                QString& outRefreshToken,
                                QString& outError)
{
    outError.clear();
    outRefreshToken.clear();

    QSqlDatabase handle = db.Handle();
    if (!handle.isValid() || !handle.isOpen()) {
        outError = "OAuthStore::GetRefreshToken: DB handle invalid or not open";
        return false;
    }

    const QString p = provider.trimmed();
    const QString e = email.trimmed();
    if (p.isEmpty() || e.isEmpty()) {
        outError = "OAuthStore::GetRefreshToken: provider/email required";
        return false;
    }

    QSqlQuery q(handle);
    q.prepare("SELECT refresh_token FROM oauth_tokens WHERE provider = :provider AND email = :email LIMIT 1");
    q.bindValue(":provider", p);
    q.bindValue(":email", e);

    if (!q.exec()) {
        outError = QString("OAuthStore::GetRefreshToken: %1").arg(q.lastError().text());
        return false;
    }

    if (!q.next()) {
        // Not found is not an error; leave outError empty to distinguish.
        return false;
    }

    outRefreshToken = q.value(0).toString();
    if (outRefreshToken.isEmpty()) {
        outError = "OAuthStore::GetRefreshToken: refresh_token empty in DB";
        return false;
    }

    return true;
}

} // namespace ngks::core::auth
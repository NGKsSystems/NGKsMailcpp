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

static bool TableHasColumn(QSqlDatabase& db, const QString& table, const QString& column)
{
    QSqlQuery q(db);
    q.prepare(QString("PRAGMA table_info(%1)").arg(table));
    if (!q.exec()) {
        return false;
    }
    while (q.next()) {
        const QString colName = q.value(1).toString();
        if (colName.compare(column, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

static bool EnsureColumn(QSqlDatabase& db, const QString& table, const QString& column, const QString& ddlType, QString& outError)
{
    if (TableHasColumn(db, table, column)) {
        return true;
    }
    QSqlQuery q(db);
    const QString sql = QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, ddlType);
    if (!q.exec(sql)) {
        outError = QString("OAuthStore::EnsureTables: %1").arg(q.lastError().text());
        return false;
    }
    return true;
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

    // Forward migrations: add provider config columns if missing.
    // These are intentionally nullable/optional.
    if (!EnsureColumn(handle, "oauth_tokens", "client_id", "TEXT", outError)) {
        return false;
    }
    if (!EnsureColumn(handle, "oauth_tokens", "client_secret", "TEXT", outError)) {
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

    // SQLite UPSERT.
    QSqlQuery q(handle);
    q.prepare(
        "INSERT INTO oauth_tokens("
        "  provider, email, refresh_token, access_token, expires_at_utc, client_id, client_secret, created_at_utc, updated_at_utc"
        ") VALUES ("
        "  :provider, :email, :refresh_token, :access_token, :expires_at_utc, :client_id, :client_secret, :created_at_utc, :updated_at_utc"
        ") "
        "ON CONFLICT(provider, email) DO UPDATE SET "
        "  refresh_token = excluded.refresh_token, "
        "  access_token = excluded.access_token, "
        "  expires_at_utc = excluded.expires_at_utc, "
        "  client_id = CASE "
        "      WHEN excluded.client_id IS NOT NULL AND length(excluded.client_id) > 0 THEN excluded.client_id "
        "      ELSE oauth_tokens.client_id "
        "    END, "
        "  client_secret = CASE "
        "      WHEN excluded.client_secret IS NOT NULL AND length(excluded.client_secret) > 0 THEN excluded.client_secret "
        "      ELSE oauth_tokens.client_secret "
        "    END, "
        "  updated_at_utc = excluded.updated_at_utc"
    );

    q.bindValue(":provider", rec.provider.trimmed());
    q.bindValue(":email", rec.email.trimmed());
    q.bindValue(":refresh_token", rec.refreshToken);
    q.bindValue(":access_token", rec.accessToken);
    q.bindValue(":expires_at_utc", static_cast<qint64>(rec.expiresAtUtc));
    q.bindValue(":client_id", rec.clientId.trimmed());
    q.bindValue(":client_secret", rec.clientSecret.trimmed());
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

bool OAuthStore::GetProviderClientCredentials(ngks::core::storage::Db& db,
                                              const QString& provider,
                                              QString& outClientId,
                                              QString& outClientSecret,
                                              QString& outError)
{
    outError.clear();
    outClientId.clear();
    outClientSecret.clear();

    QSqlDatabase handle = db.Handle();
    if (!handle.isValid() || !handle.isOpen()) {
        outError = "OAuthStore::GetProviderClientCredentials: DB handle invalid or not open";
        return false;
    }

    const QString p = provider.trimmed();
    if (p.isEmpty()) {
        outError = "OAuthStore::GetProviderClientCredentials: provider required";
        return false;
    }

    // If columns don't exist (older DB), treat as "not found" not fatal.
    if (!TableHasColumn(handle, "oauth_tokens", "client_id") || !TableHasColumn(handle, "oauth_tokens", "client_secret")) {
        return false;
    }

    QSqlQuery q(handle);
    q.prepare(
        "SELECT client_id, client_secret "
        "FROM oauth_tokens "
        "WHERE provider = :provider "
        "ORDER BY updated_at_utc DESC "
        "LIMIT 1"
    );
    q.bindValue(":provider", p);

    if (!q.exec()) {
        outError = QString("OAuthStore::GetProviderClientCredentials: %1").arg(q.lastError().text());
        return false;
    }

    if (!q.next()) {
        // Not found is not an error; leave outError empty.
        return false;
    }

    outClientId = q.value(0).toString().trimmed();
    outClientSecret = q.value(1).toString().trimmed();

    if (outClientId.isEmpty() || outClientSecret.isEmpty()) {
        // Treat empty creds as not found (caller can fall back to env / fail).
        outClientId.clear();
        outClientSecret.clear();
        return false;
    }

    return true;
}

} // namespace ngks::core::auth
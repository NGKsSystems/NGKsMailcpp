#include "core/storage/Schema.h"

#include <QSqlQuery>
#include <QVariant>

#include "core/storage/Db.h"

namespace ngks::core::storage {

namespace {
constexpr int kSchemaVersion = 2;
}

Schema::Schema(Db& db)
    : db_(db)
{
}

bool Schema::Ensure()
{
    if (!db_.IsOpen()) {
        return false;
    }

    if (!EnsureMeta()) {
        return false;
    }

    const int version = CurrentVersion();
    if (version < 0) {
        return false;
    }

    if (version < 2) {
        if (!MigrateToV2()) {
            return false;
        }
        if (!SetVersion(kSchemaVersion)) {
            return false;
        }
    }

    if (!EnsureTables()) {
        return false;
    }

    if (version == 0) {
        if (!SetVersion(kSchemaVersion)) {
            return false;
        }
    }

    return true;
}

bool Schema::EnsureMeta()
{
    QSqlQuery query(db_.Handle());

    if (!query.exec("CREATE TABLE IF NOT EXISTS app_meta (k TEXT PRIMARY KEY, v TEXT NOT NULL)")) {
        return false;
    }

    if (!query.exec("INSERT OR IGNORE INTO app_meta(k, v) VALUES('schema_version', '0')")) {
        return false;
    }

    return true;
}

bool Schema::EnsureTables()
{
    QSqlQuery query(db_.Handle());

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS accounts ("
            "  id INTEGER PRIMARY KEY,"
            "  email TEXT NOT NULL UNIQUE,"
            "  provider TEXT NOT NULL,"
            "  imap_host TEXT NOT NULL,"
            "  imap_port INTEGER NOT NULL,"
            "  tls_mode TEXT NOT NULL,"
            "  auth_method TEXT NOT NULL,"
            "  credential_ref TEXT NOT NULL,"
            "  status TEXT NOT NULL,"
            "  sync_state TEXT NOT NULL DEFAULT '',"
            "  created_at TEXT NOT NULL"
            ")")) {
        return false;
    }

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS folders ("
            "  id INTEGER PRIMARY KEY,"
            "  account_id INTEGER NOT NULL,"
            "  remote_name TEXT NOT NULL,"
            "  display_name TEXT NOT NULL,"
            "  delimiter TEXT NOT NULL,"
            "  attrs_json TEXT NOT NULL,"
            "  special_use TEXT NOT NULL,"
            "  sync_state TEXT NOT NULL DEFAULT '',"
            "  created_at TEXT NOT NULL,"
            "  FOREIGN KEY(account_id) REFERENCES accounts(id)"
            ")")) {
        return false;
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_accounts_email ON accounts(email)")) {
        return false;
    }
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_folders_account ON folders(account_id)")) {
        return false;
    }
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_folders_remote_name ON folders(account_id, remote_name)")) {
        return false;
    }

    return true;
}

int Schema::CurrentVersion() const
{
    QSqlQuery query(db_.Handle());
    if (!query.exec("SELECT v FROM app_meta WHERE k='schema_version' LIMIT 1")) {
        return -1;
    }
    if (!query.next()) {
        return 0;
    }
    bool ok = false;
    const int version = query.value(0).toInt(&ok);
    return ok ? version : 0;
}

bool Schema::SetVersion(int version)
{
    QSqlQuery query(db_.Handle());
    query.prepare("UPDATE app_meta SET v=:v WHERE k='schema_version'");
    query.bindValue(":v", QString::number(version));
    if (!query.exec()) {
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Schema::MigrateToV2()
{
    QSqlQuery query(db_.Handle());
    if (!query.exec("DROP TABLE IF EXISTS folders")) {
        return false;
    }
    if (!query.exec("DROP TABLE IF EXISTS accounts")) {
        return false;
    }
    return EnsureTables();
}

} // namespace ngks::core::storage
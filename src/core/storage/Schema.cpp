#include "core/storage/Schema.h"

#include <QSqlQuery>
#include <QVariant>

#include "core/storage/Db.h"

namespace ngks::core::storage {

namespace {
constexpr int kSchemaVersion = 4;
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
        if (!SetVersion(2)) {
            return false;
        }
    }

    if (version < 3) {
        if (!MigrateToV3()) {
            return false;
        }
        if (!SetVersion(3)) {
            return false;
        }
    }

    if (version < 4) {
        if (!MigrateToV4()) {
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

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS basic_credentials ("
            "  provider TEXT NOT NULL,"
            "  email TEXT NOT NULL,"
            "  username TEXT NOT NULL,"
            "  secret_enc TEXT NOT NULL,"
            "  created_at TEXT NOT NULL,"
            "  PRIMARY KEY(provider, email)"
            ")")) {
        return false;
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_basic_credentials_provider_email ON basic_credentials(provider, email)")) {
        return false;
    }

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS messages ("
            "  id INTEGER PRIMARY KEY,"
            "  account_id INTEGER NOT NULL,"
            "  folder_id INTEGER NOT NULL,"
            "  provider TEXT NOT NULL DEFAULT '',"
            "  remote_uid INTEGER NOT NULL DEFAULT 0,"
            "  message_id_hdr TEXT NOT NULL DEFAULT '',"
            "  from_display TEXT NOT NULL DEFAULT '',"
            "  subject TEXT NOT NULL DEFAULT '',"
            "  date_utc TEXT NOT NULL DEFAULT '',"
            "  body_text TEXT NOT NULL DEFAULT '',"
            "  body_html TEXT NOT NULL DEFAULT '',"
            "  attachments_json TEXT NOT NULL DEFAULT '[]',"
            "  is_read INTEGER NOT NULL DEFAULT 0,"
            "  created_at TEXT NOT NULL DEFAULT (datetime('now'))"
            ")")) {
        return false;
    }

    if (!query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_messages_folder_uid ON messages(account_id, folder_id, remote_uid)")) {
        return false;
    }
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_messages_folder_date ON messages(account_id, folder_id, date_utc DESC, id DESC)")) {
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

bool Schema::MigrateToV3()
{
    return EnsureTables();
}

bool Schema::MigrateToV4()
{
    return EnsureTables();
}

} // namespace ngks::core::storage
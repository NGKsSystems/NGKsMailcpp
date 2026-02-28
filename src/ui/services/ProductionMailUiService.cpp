#include "ui/services/ProductionMailUiService.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

#include "core/logging/AuditLog.h"
#include "core/mail/providers/imap/MessageSyncService.h"

namespace ngks::ui::services {

namespace {

int FieldIndex(const QSqlRecord& rec, const QStringList& candidates)
{
    for (const auto& name : candidates) {
        const int idx = rec.indexOf(name);
        if (idx >= 0) {
            return idx;
        }
    }
    return -1;
}

QString ReadStringByCandidates(const QSqlRecord& rec, const QSqlQuery& q, const QStringList& names)
{
    const int idx = FieldIndex(rec, names);
    return idx >= 0 ? q.value(idx).toString() : QString();
}

int ReadIntByCandidates(const QSqlRecord& rec, const QSqlQuery& q, const QStringList& names, int fallback = 0)
{
    const int idx = FieldIndex(rec, names);
    return idx >= 0 ? q.value(idx).toInt() : fallback;
}

bool ReadBoolByCandidates(const QSqlRecord& rec, const QSqlQuery& q, const QStringList& names, bool fallback = false)
{
    const int idx = FieldIndex(rec, names);
    if (idx < 0) {
        return fallback;
    }
    const QVariant v = q.value(idx);
    if (v.typeId() == QMetaType::Bool) {
        return v.toBool();
    }
    const QString s = v.toString().trimmed().toLower();
    if (s == "1" || s == "true" || s == "yes") return true;
    if (s == "0" || s == "false" || s == "no") return false;
    return fallback;
}

QDateTime ParseUtc(const QString& text)
{
    if (text.isEmpty()) {
        return QDateTime::currentDateTimeUtc();
    }
    QDateTime dt = QDateTime::fromString(text, Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(text, "yyyy-MM-dd HH:mm:ss");
        dt.setTimeSpec(Qt::UTC);
    }
    if (!dt.isValid()) {
        return QDateTime::currentDateTimeUtc();
    }
    if (dt.timeSpec() == Qt::LocalTime) {
        dt = dt.toUTC();
    }
    return dt;
}

QStringList ParseAttachmentNames(const QString& json)
{
    if (json.trimmed().isEmpty()) {
        return {};
    }
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return {};
    }
    QStringList out;
    const auto arr = doc.array();
    for (const auto& v : arr) {
        if (v.isObject()) {
            const auto o = v.toObject();
            const QString name = o.value("name").toString();
            if (!name.isEmpty()) out.push_back(name);
        } else if (v.isString()) {
            out.push_back(v.toString());
        }
    }
    return out;
}

bool HasMessagesTable(QSqlDatabase& db)
{
    QSqlQuery q(db);
    if (!q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='messages' LIMIT 1")) {
        return false;
    }
    return q.next();
}

} // namespace

QVector<ngks::ui::models::AccountItem> ProductionMailUiService::ListAccounts()
{
    QVector<ngks::ui::models::AccountItem> out;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return out;
    }

    QSqlQuery q(db);
    if (!q.exec("SELECT id, email, provider FROM accounts ORDER BY id ASC")) {
        return out;
    }

    while (q.next()) {
        out.push_back({q.value(0).toInt(), q.value(1).toString(), q.value(2).toString()});
    }
    return out;
}

QVector<ngks::ui::models::FolderItem> ProductionMailUiService::ListFolders(int accountId)
{
    QVector<ngks::ui::models::FolderItem> out;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen() || accountId <= 0) {
        return out;
    }

    QSqlQuery q(db);
    q.prepare("SELECT id, account_id, display_name, special_use FROM folders WHERE account_id=:aid ORDER BY id ASC");
    q.bindValue(":aid", accountId);
    if (!q.exec()) {
        return out;
    }

    while (q.next()) {
        out.push_back({q.value(0).toInt(), q.value(1).toInt(), q.value(2).toString(), q.value(3).toString()});
    }
    return out;
}

QVector<ngks::ui::models::MessageItem> ProductionMailUiService::ListMessages(int accountId, int folderId)
{
    QVector<ngks::ui::models::MessageItem> out;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen() || accountId <= 0 || folderId <= 0 || !HasMessagesTable(db)) {
        return out;
    }

    QString syncError;
    if (!ngks::core::mail::providers::imap::MessageSyncService::SyncFolder(db, accountId, folderId, 80, syncError) &&
        !syncError.trimmed().isEmpty()) {
        ngks::core::logging::AuditLog::Event(
            "UI_SYNC_WARN",
            QString("{\"account_id\":%1,\"folder_id\":%2,\"reason\":\"%3\"}")
                .arg(accountId)
                .arg(folderId)
                .arg(syncError)
                .toStdString());
    }

    QSqlQuery q(db);
    q.prepare("SELECT * FROM messages WHERE account_id=:aid AND folder_id=:fid ORDER BY id DESC LIMIT 1000");
    q.bindValue(":aid", accountId);
    q.bindValue(":fid", folderId);
    if (!q.exec()) {
        return out;
    }

    const QSqlRecord rec = q.record();
    while (q.next()) {
        ngks::ui::models::MessageItem row;
        row.messageId = ReadIntByCandidates(rec, q, {"id", "message_id"});
        row.accountId = ReadIntByCandidates(rec, q, {"account_id"}, accountId);
        row.folderId = ReadIntByCandidates(rec, q, {"folder_id"}, folderId);
        row.from = ReadStringByCandidates(rec, q, {"from_display", "from_name", "from_addr", "sender", "from"});
        row.subject = ReadStringByCandidates(rec, q, {"subject", "title"});
        row.provider = ReadStringByCandidates(rec, q, {"provider"});
        row.unread = !ReadBoolByCandidates(rec, q, {"is_read", "read"}, false);

        const QString dateRaw = ReadStringByCandidates(rec, q, {"date_utc", "sent_at", "received_at", "created_at"});
        row.dateUtc = ParseUtc(dateRaw);

        row.bodyHtml = ReadStringByCandidates(rec, q, {"body_html", "html"});
        row.bodyText = ReadStringByCandidates(rec, q, {"body_text", "text"});
        row.attachments = ParseAttachmentNames(ReadStringByCandidates(rec, q, {"attachments_json", "attachments"}));
        row.previewHtml = !row.bodyHtml.trimmed().isEmpty() ? row.bodyHtml : row.bodyText.toHtmlEscaped();
        out.push_back(row);
    }

    return out;
}

bool ProductionMailUiService::MarkRead(int messageId, bool read)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen() || messageId <= 0 || !HasMessagesTable(db)) {
        return false;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE messages SET is_read=:r WHERE id=:id");
    q.bindValue(":r", read ? 1 : 0);
    q.bindValue(":id", messageId);
    if (!q.exec()) {
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool ProductionMailUiService::DeleteMessage(int messageId)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen() || messageId <= 0 || !HasMessagesTable(db)) {
        return false;
    }

    QSqlQuery q(db);
    q.prepare("DELETE FROM messages WHERE id=:id");
    q.bindValue(":id", messageId);
    if (!q.exec()) {
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace ngks::ui::services

#include "core/mail/providers/imap/MessageSyncService.h"

#include <QDateTime>
#include <QDir>
#include <QProcessEnvironment>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <algorithm>

#include "core/auth/OAuthStore.h"
#include "core/mail/mime/MimeParser.h"
#include "core/mail/providers/imap/ImapClient.h"
#include "core/oauth/OAuthBroker.h"
#include "platform/common/Paths.h"

namespace ngks::core::mail::providers::imap {

namespace {

QString Esc(const QString& v)
{
    QString out = v;
    out.replace("\\", "\\\\");
    out.replace("\"", "\\\"");
    return out;
}

QString MakeTag(int i)
{
    return QString("S%1").arg(i, 4, 10, QChar('0'));
}

bool IsTaggedOk(const QStringList& lines, const QString& tag)
{
    for (const auto& line : lines) {
        if (line.startsWith(tag + " OK", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool IsTaggedOkRaw(const QList<QByteArray>& lines, const QString& tag)
{
    const QByteArray prefix = (tag + " OK").toUtf8();
    for (const auto& line : lines) {
        if (line.trimmed().startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

bool ParseUidSearch(const QStringList& lines, QVector<qint64>& outUids)
{
    outUids.clear();
    QRegularExpression re("^\\*\\s+SEARCH\\s*(.*)$", QRegularExpression::CaseInsensitiveOption);
    for (const auto& line : lines) {
        const auto m = re.match(line);
        if (!m.hasMatch()) {
            continue;
        }
        const QString rest = m.captured(1).trimmed();
        if (rest.isEmpty()) {
            continue;
        }
        const auto parts = rest.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        for (const auto& p : parts) {
            bool ok = false;
            const qint64 uid = p.toLongLong(&ok);
            if (ok && uid > 0) {
                outUids.push_back(uid);
            }
        }
    }
    return true;
}

QString ExtractLiteralMessage(const QList<QByteArray>& lines)
{
    int start = -1;
    int literalLen = -1;

    QRegularExpression reLiteral("\\{(\\d+)\\}\\s*$");
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = QString::fromUtf8(lines[i]).trimmed();
        if (!line.contains("BODY[]", Qt::CaseInsensitive)) {
            continue;
        }
        const auto m = reLiteral.match(line);
        if (m.hasMatch()) {
            literalLen = m.captured(1).toInt();
            start = i + 1;
            break;
        }
    }

    if (start < 0 || literalLen <= 0) {
        return {};
    }

    QByteArray payload;
    payload.reserve(literalLen + 64);
    for (int i = start; i < lines.size() && payload.size() < literalLen; ++i) {
        payload += lines[i];
    }

    if (payload.size() < literalLen) {
        return {};
    }

    payload = payload.left(literalLen);
    return QString::fromUtf8(payload);
}

bool ResolveLatestOAuthByEmail(QSqlDatabase& db,
                               const QString& email,
                               QString& outProvider,
                               QString& outRefreshToken,
                               QString& outAccessToken,
                               qint64& outExpiresAtUtc,
                               QString& outClientId,
                               QString& outClientSecret,
                               QString& outError)
{
    QSqlQuery q(db);
    q.prepare("SELECT provider, refresh_token, access_token, expires_at_utc, client_id, client_secret "
              "FROM oauth_tokens WHERE email=:e ORDER BY updated_at_utc DESC LIMIT 1");
    q.bindValue(":e", email.trimmed());
    if (!q.exec() || !q.next()) {
        outError = q.lastError().text().isEmpty() ? "missing-oauth-tokens" : q.lastError().text();
        return false;
    }

    outProvider = q.value(0).toString().trimmed();
    outRefreshToken = q.value(1).toString();
    outAccessToken = q.value(2).toString();
    outExpiresAtUtc = q.value(3).toLongLong();
    outClientId = q.value(4).toString().trimmed();
    outClientSecret = q.value(5).toString().trimmed();
    return true;
}

bool HasSeenFlag(const QStringList& lines)
{
    for (const auto& line : lines) {
        if (line.contains("FLAGS", Qt::CaseInsensitive) && line.contains("\\Seen", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool HasSeenFlagRaw(const QList<QByteArray>& lines)
{
    for (const auto& raw : lines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.contains("FLAGS", Qt::CaseInsensitive) && line.contains("\\Seen", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QString HeaderFallback(const QString& rawMime, const QString& name)
{
    QRegularExpression re(QString("^%1\\s*:\\s*(.+)$").arg(QRegularExpression::escape(name)),
                          QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    const auto m = re.match(rawMime);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

QByteArray BuildXoauth2Raw(const QString& user, const QString& accessToken)
{
    QString token = accessToken.trimmed();
    const QString bearerPrefix = "Bearer ";
    while (token.startsWith(bearerPrefix, Qt::CaseInsensitive)) {
        token = token.mid(bearerPrefix.size()).trimmed();
    }

    QByteArray raw;
    raw.reserve(user.toUtf8().size() + token.toUtf8().size() + 32);
    raw += "user=";
    raw += user.toUtf8();
    raw.append(char(0x01));
    raw += "auth=Bearer ";
    raw += token.toUtf8();
    raw.append(char(0x01));
    raw.append(char(0x01));
    return raw;
}

QString OAuthTokenEndpointForProvider(const QString& provider)
{
    const QString p = provider.trimmed().toLower();
    if (p == "gmail") {
        return "https://oauth2.googleapis.com/token";
    }
    if (p == "ms_graph") {
        return "https://login.microsoftonline.com/common/oauth2/v2.0/token";
    }
    if (p == "yahoo") {
        return "https://api.login.yahoo.com/oauth2/get_token";
    }
    return {};
}

struct AuthContext {
    bool useXoauth2 = false;
    QString username;
    QString password;
    QString accessToken;
};

bool ResolveOAuthContext(QSqlDatabase& db,
                         const QString& provider,
                         const QString& email,
                         const QString& credentialRef,
                         AuthContext& out,
                         QString& outError)
{
    QString queryProvider = provider;
    QString queryEmail = email;
    if (credentialRef.startsWith("oauth_tokens:", Qt::CaseInsensitive)) {
        const auto parts = credentialRef.split(':');
        if (parts.size() >= 3) {
            queryProvider = parts[1].trimmed();
            queryEmail = parts[2].trimmed();
        }
    }

    QString tokenProvider = queryProvider;
    QString refreshToken;
    QString storedAccessToken;
    qint64 expiresAtUtc = 0;
    QString clientId;
    QString clientSecret;

    {
        QSqlQuery q(db);
        q.prepare("SELECT refresh_token, access_token, expires_at_utc, client_id, client_secret "
                  "FROM oauth_tokens WHERE provider=:p AND email=:e LIMIT 1");
        q.bindValue(":p", queryProvider);
        q.bindValue(":e", queryEmail);
        if (q.exec() && q.next()) {
            refreshToken = q.value(0).toString();
            storedAccessToken = q.value(1).toString();
            expiresAtUtc = q.value(2).toLongLong();
            clientId = q.value(3).toString().trimmed();
            clientSecret = q.value(4).toString().trimmed();
        } else {
            QString fallbackErr;
            if (!ResolveLatestOAuthByEmail(db, queryEmail, tokenProvider, refreshToken, storedAccessToken, expiresAtUtc, clientId, clientSecret, fallbackErr)) {
                outError = fallbackErr;
                return false;
            }
        }
    }

    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    QString accessToken = storedAccessToken;

    const bool hasRefreshToken = !refreshToken.trimmed().isEmpty();
    if (hasRefreshToken) {
        const QString tokenEndpoint = OAuthTokenEndpointForProvider(tokenProvider);
        if (tokenEndpoint.isEmpty()) {
            outError = "unsupported-oauth-provider";
            return false;
        }

        ngks::core::oauth::OAuthConfig cfg;
        cfg.provider = tokenProvider;
        cfg.email = queryEmail;
        cfg.clientId = clientId;
        cfg.clientSecret = clientSecret;
        cfg.tokenEndpoint = tokenEndpoint;

        ngks::core::oauth::OAuthResult refreshed;
        QString refreshError;
        if (!ngks::core::oauth::OAuthBroker::RefreshAccessToken(cfg, refreshToken, refreshed, refreshError)) {
            if (storedAccessToken.isEmpty()) {
                outError = refreshError;
                return false;
            }
            accessToken = storedAccessToken;
        } else {
            accessToken = refreshed.accessToken;

            QSqlQuery update(db);
            update.prepare("UPDATE oauth_tokens SET access_token=:a, expires_at_utc=:x, updated_at_utc=:u WHERE provider=:p AND email=:e");
            update.bindValue(":a", refreshed.accessToken);
            update.bindValue(":x", refreshed.expiresAtUtc);
            update.bindValue(":u", QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
            update.bindValue(":p", tokenProvider);
            update.bindValue(":e", queryEmail);
            update.exec();
        }
    } else if (accessToken.isEmpty() || expiresAtUtc <= (now + 60)) {
        outError = "missing-refresh-token";
        return false;
    }

    if (accessToken.trimmed().isEmpty()) {
        outError = "empty-access-token";
        return false;
    }

    out.useXoauth2 = true;
    out.username = queryEmail;
    out.accessToken = accessToken;
    out.password.clear();
    return true;
}

bool AuthenticateImap(ImapClient& client, int& commandIndex, const AuthContext& auth, QString& outError)
{
    const QString authTag = MakeTag(commandIndex++);
    if (auth.useXoauth2) {
        const QString beginCmd = QString("%1 AUTHENTICATE XOAUTH2").arg(authTag);
        if (!client.SendCommand(beginCmd)) {
            outError = client.LastError();
            return false;
        }

        const QString first = client.ReadLine();
        if (first.isEmpty()) {
            outError = client.LastError().isEmpty() ? "xoauth2-missing-continuation" : client.LastError();
            return false;
        }

        if (!first.trimmed().startsWith('+')) {
            QStringList rest = client.ReadResponseUntilTag(authTag);
            rest.prepend(first);
            outError = "imap-auth-xoauth2-rejected";
            return false;
        }

        const QString b64 = QString::fromLatin1(BuildXoauth2Raw(auth.username, auth.accessToken).toBase64());
        if (!client.SendRawLine(b64, "<XOAUTH2_B64_REDACTED>")) {
            outError = client.LastError();
            return false;
        }

        const QString firstAfterPayload = client.ReadLine();
        QStringList authLines;
        if (firstAfterPayload.isEmpty()) {
            outError = client.LastError().isEmpty() ? "imap-auth-xoauth2-empty-response" : client.LastError();
            return false;
        }

        if (firstAfterPayload.trimmed().startsWith('+')) {
            if (!client.SendRawLine(QString(), "<XOAUTH2_CANCEL>")) {
                outError = client.LastError();
                return false;
            }
            authLines = client.ReadResponseUntilTag(authTag);
            authLines.prepend(firstAfterPayload);
        } else {
            authLines = client.ReadResponseUntilTag(authTag);
            authLines.prepend(firstAfterPayload);
        }

        if (!IsTaggedOk(authLines, authTag)) {
            bool sawCapability = false;
            for (const auto& line : authLines) {
                if (line.startsWith("* CAPABILITY", Qt::CaseInsensitive)) {
                    sawCapability = true;
                    break;
                }
            }

            const bool timedOut = client.LastError().contains("timeout waiting for IMAP response", Qt::CaseInsensitive);
            if (sawCapability && timedOut) {
                const QString probeTag = MakeTag(commandIndex++);
                if (client.SendCommand(probeTag + " NOOP")) {
                    const QStringList probeLines = client.ReadResponseUntilTag(probeTag);
                    if (IsTaggedOk(probeLines, probeTag)) {
                        return true;
                    }
                }
            }

            outError = "imap-auth-xoauth2-failed";
            return false;
        }
        return true;
    }

    if (!client.SendCommand(QString("%1 LOGIN \"%2\" \"%3\"").arg(authTag, Esc(auth.username), Esc(auth.password)),
                            QString("%1 LOGIN \"%2\" \"<REDACTED>\"").arg(authTag, Esc(auth.username)))) {
        outError = client.LastError();
        return false;
    }
    const QStringList loginLines = client.ReadResponseUntilTag(authTag);
    if (!IsTaggedOk(loginLines, authTag)) {
        outError = "imap-login-failed";
        return false;
    }
    return true;
}

bool LoadAccountContext(QSqlDatabase& db,
                        int accountId,
                        int folderId,
                        QString& outProvider,
                        QString& outEmail,
                        QString& outImapHost,
                        int& outImapPort,
                        bool& outTls,
                        QString& outCredentialRef,
                        QString& outAuthMethod,
                        QString& outRemoteFolder,
                        QString& outError)
{
    QSqlQuery q(db);
    q.prepare(
                            "SELECT a.provider, a.email, a.imap_host, a.imap_port, a.tls_mode, a.credential_ref, a.auth_method, f.remote_name "
        "FROM accounts a JOIN folders f ON f.account_id=a.id "
        "WHERE a.id=:aid AND f.id=:fid LIMIT 1");
    q.bindValue(":aid", accountId);
    q.bindValue(":fid", folderId);
    if (!q.exec() || !q.next()) {
        outError = q.lastError().text().isEmpty() ? "account-folder-not-found" : q.lastError().text();
        return false;
    }

    outProvider = q.value(0).toString().trimmed();
    outEmail = q.value(1).toString().trimmed();
    outImapHost = q.value(2).toString().trimmed();
    outImapPort = q.value(3).toInt();
    outTls = q.value(4).toString().trimmed().compare("TLS", Qt::CaseInsensitive) == 0;
    outCredentialRef = q.value(5).toString().trimmed();
    outAuthMethod = q.value(6).toString().trimmed();
    outRemoteFolder = q.value(7).toString().trimmed();

    if (outImapHost.isEmpty() || outImapPort <= 0 || outRemoteFolder.isEmpty()) {
        outError = "invalid-account-context";
        return false;
    }
    return true;
}

bool ResolveBasicCredentials(QSqlDatabase& db,
                             const QString& provider,
                             const QString& email,
                             const QString& credentialRef,
                             QString& outUsername,
                             QString& outPassword,
                             QString& outError)
{
    outUsername.clear();
    outPassword.clear();

    QString queryProvider = provider;
    QString queryEmail = email;

    if (credentialRef.compare("GENERIC_IMAP_ENV", Qt::CaseInsensitive) == 0) {
        QSqlQuery persisted(db);
        persisted.prepare("SELECT username, secret_enc FROM basic_credentials WHERE provider=:p AND email=:e LIMIT 1");
        persisted.bindValue(":p", provider.trimmed());
        persisted.bindValue(":e", email.trimmed());
        if (persisted.exec() && persisted.next()) {
            outUsername = persisted.value(0).toString();
            outPassword = persisted.value(1).toString();
            if (!outUsername.isEmpty() && !outPassword.isEmpty()) {
                return true;
            }
        }

        const auto env = QProcessEnvironment::systemEnvironment();
        outUsername = env.value("NGKS_GENERIC_IMAP_USERNAME").trimmed();
        outPassword = env.value("NGKS_GENERIC_IMAP_PASSWORD");
        if (outUsername.isEmpty()) {
            outUsername = email.trimmed();
        }
        if (outUsername.isEmpty() || outPassword.isEmpty()) {
            outError = "missing-generic-imap-env-credentials";
            return false;
        }
        return true;
    }

    if (credentialRef.startsWith("basic_credentials:", Qt::CaseInsensitive)) {
        const auto parts = credentialRef.split(':');
        if (parts.size() >= 3) {
            queryProvider = parts[1].trimmed();
            queryEmail = parts[2].trimmed();
        }
    }

    QSqlQuery q(db);
    q.prepare("SELECT username, secret_enc FROM basic_credentials WHERE provider=:p AND email=:e LIMIT 1");
    q.bindValue(":p", queryProvider);
    q.bindValue(":e", queryEmail);
    if (!q.exec() || !q.next()) {
        outError = q.lastError().text().isEmpty() ? "missing-basic-credentials" : q.lastError().text();
        return false;
    }

    outUsername = q.value(0).toString();
    outPassword = q.value(1).toString();
    if (outUsername.isEmpty() || outPassword.isEmpty()) {
        outError = "empty-basic-credentials";
        return false;
    }

    return true;
}

QJsonArray BuildAttachmentArray(const std::vector<ngks::core::mail::mime::Attachment>& items)
{
    QJsonArray arr;
    for (const auto& item : items) {
        QJsonObject o;
        o.insert("name", QString::fromStdString(item.name));
        o.insert("content_type", QString::fromStdString(item.contentType));
        o.insert("inline", item.isInline);
        o.insert("content_id", QString::fromStdString(item.contentId));
        arr.push_back(o);
    }
    return arr;
}

} // namespace

bool MessageSyncService::SyncFolder(QSqlDatabase& db, int accountId, int folderId, int limit, QString& outError)
{
    outError.clear();
    if (!db.isOpen() || accountId <= 0 || folderId <= 0) {
        outError = "invalid-sync-input";
        return false;
    }

    QString provider;
    QString email;
    QString host;
    int port = 993;
    bool tls = true;
    QString credentialRef;
    QString authMethod;
    QString remoteFolder;

    if (!LoadAccountContext(db, accountId, folderId, provider, email, host, port, tls, credentialRef, authMethod, remoteFolder, outError)) {
        return false;
    }

    AuthContext auth;
    const bool preferOAuth = authMethod.compare("XOAUTH2", Qt::CaseInsensitive) == 0 ||
                             credentialRef.startsWith("oauth_tokens:", Qt::CaseInsensitive) ||
                             credentialRef.compare("OAUTH_DB_REFRESH", Qt::CaseInsensitive) == 0;
    if (preferOAuth) {
        if (!ResolveOAuthContext(db, provider, email, credentialRef, auth, outError)) {
            auth = AuthContext{};
        }
    }
    if (!auth.useXoauth2) {
        QString basicErr;
        if (!ResolveBasicCredentials(db, provider, email, credentialRef, auth.username, auth.password, basicErr)) {
            QString oauthErr;
            const QString providerLower = provider.trimmed().toLower();
            const bool canFallbackToOAuth = (providerLower == "gmail"
                                            || providerLower == "ms_graph"
                                            || providerLower == "yahoo"
                                            || providerLower == "imap");
            if (!canFallbackToOAuth || !ResolveOAuthContext(db, provider, email, credentialRef, auth, oauthErr)) {
                outError = !basicErr.trimmed().isEmpty() ? basicErr : oauthErr;
                return false;
            }
        }
    }

    const auto imapLogRoot = ngks::platform::common::ArtifactsDir() / "logs" / "imap";
    QDir().mkpath(QString::fromStdString(imapLogRoot.string()));

    const auto transcriptFs = imapLogRoot /
                              QString("sync_%1_%2_%3.txt")
                                  .arg(accountId)
                                  .arg(folderId)
                                  .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz"))
                                  .toStdString();

    ImapClient client;
    if (!client.Connect(host, port, tls, QString::fromStdString(transcriptFs.string()))) {
        outError = client.LastError();
        return false;
    }

    client.ReadGreeting();

    int commandIndex = 1;
    if (!AuthenticateImap(client, commandIndex, auth, outError)) {
        client.Disconnect();
        return false;
    }

    const QString selectTag = MakeTag(commandIndex++);
    if (!client.SendCommand(QString("%1 SELECT \"%2\"").arg(selectTag, Esc(remoteFolder)))) {
        outError = client.LastError();
        client.Disconnect();
        return false;
    }
    const QStringList selectLines = client.ReadResponseUntilTag(selectTag);
    if (!IsTaggedOk(selectLines, selectTag)) {
        outError = "imap-select-failed";
        client.Disconnect();
        return false;
    }

    const QString searchTag = MakeTag(commandIndex++);
    if (!client.SendCommand(searchTag + " UID SEARCH ALL")) {
        outError = client.LastError();
        client.Disconnect();
        return false;
    }
    const QStringList searchLines = client.ReadResponseUntilTag(searchTag);
    if (!IsTaggedOk(searchLines, searchTag)) {
        outError = "imap-search-failed";
        client.Disconnect();
        return false;
    }

    QVector<qint64> uids;
    ParseUidSearch(searchLines, uids);
    std::sort(uids.begin(), uids.end());
    if (uids.size() > limit) {
        uids = uids.mid(uids.size() - limit);
    }

    if (!db.transaction()) {
        outError = "sync-transaction-start-failed";
        client.Disconnect();
        return false;
    }

    QSqlQuery upsert(db);
    upsert.prepare(
        "INSERT INTO messages(account_id, folder_id, provider, remote_uid, message_id_hdr, from_display, subject, date_utc, body_text, body_html, attachments_json, is_read, created_at) "
        "VALUES(:aid, :fid, :provider, :uid, :mid, :fromd, :subject, :date, :text, :html, :attachments, :is_read, datetime('now')) "
        "ON CONFLICT(account_id, folder_id, remote_uid) DO UPDATE SET "
        "provider=excluded.provider, message_id_hdr=excluded.message_id_hdr, from_display=excluded.from_display, subject=excluded.subject, date_utc=excluded.date_utc, body_text=excluded.body_text, body_html=excluded.body_html, attachments_json=excluded.attachments_json, is_read=excluded.is_read");

    for (const auto uid : uids) {
        const QString fetchTag = MakeTag(commandIndex++);
        if (!client.SendCommand(QString("%1 UID FETCH %2 (FLAGS BODY.PEEK[])").arg(fetchTag).arg(uid))) {
            outError = client.LastError();
            db.rollback();
            client.Disconnect();
            return false;
        }

        const QList<QByteArray> fetchRawLines = client.ReadResponseUntilTagRaw(fetchTag);
        if (!IsTaggedOkRaw(fetchRawLines, fetchTag)) {
            continue;
        }

        const QString rawMime = ExtractLiteralMessage(fetchRawLines);
        if (rawMime.trimmed().isEmpty()) {
            continue;
        }

        ngks::core::mail::mime::MimeParseResult parsed;
        ngks::core::mail::mime::MimeParser parser;
        const bool parsedOk = parser.Parse(rawMime.toStdString(), parsed);
        if (!parsedOk) {
            continue;
        }

        const QString fromDisplay = !QString::fromStdString(parsed.from).trimmed().isEmpty()
                                        ? QString::fromStdString(parsed.from)
                                        : HeaderFallback(rawMime, "From");
        const QString subject = !QString::fromStdString(parsed.subject).trimmed().isEmpty()
                                    ? QString::fromStdString(parsed.subject)
                                    : HeaderFallback(rawMime, "Subject");
        const QString dateRaw = !QString::fromStdString(parsed.date).trimmed().isEmpty()
                                    ? QString::fromStdString(parsed.date)
                                    : HeaderFallback(rawMime, "Date");

        QDateTime dt = QDateTime::fromString(dateRaw, Qt::RFC2822Date);
        if (!dt.isValid()) {
            dt = QDateTime::currentDateTimeUtc();
        }
        dt = dt.toUTC();

        const QString messageIdHdr = !QString::fromStdString(parsed.messageId).trimmed().isEmpty()
                                         ? QString::fromStdString(parsed.messageId)
                                         : HeaderFallback(rawMime, "Message-ID");

        const QString bodyText = QString::fromStdString(parsed.text);
        const QString bodyHtml = QString::fromStdString(parsed.html);
        const QString safeMessageId = messageIdHdr.isNull() ? QString() : messageIdHdr;
        const QString safeFrom = fromDisplay.isNull() ? QString() : fromDisplay;
        const QString safeSubject = subject.isNull() ? QString() : subject;
        const QString safeBodyText = bodyText.isNull() ? QString() : bodyText;
        const QString safeBodyHtml = bodyHtml.isNull() ? QString() : bodyHtml;
        const QString attachmentsJson = QString::fromUtf8(QJsonDocument(BuildAttachmentArray(parsed.attachments)).toJson(QJsonDocument::Compact));

        upsert.bindValue(":aid", accountId);
        upsert.bindValue(":fid", folderId);
        upsert.bindValue(":provider", provider);
        upsert.bindValue(":uid", uid);
        upsert.bindValue(":mid", safeMessageId);
        upsert.bindValue(":fromd", safeFrom);
        upsert.bindValue(":subject", safeSubject);
        upsert.bindValue(":date", dt.toString(Qt::ISODate));
        upsert.bindValue(":text", safeBodyText);
        upsert.bindValue(":html", safeBodyHtml);
        upsert.bindValue(":attachments", attachmentsJson);
        upsert.bindValue(":is_read", HasSeenFlagRaw(fetchRawLines) ? 1 : 0);
        if (!upsert.exec()) {
            outError = upsert.lastError().text();
            db.rollback();
            client.Disconnect();
            return false;
        }
    }

    QSqlQuery markSync(db);
    markSync.prepare("UPDATE folders SET sync_state=:s WHERE id=:fid");
    markSync.bindValue(":s", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    markSync.bindValue(":fid", folderId);
    markSync.exec();

    if (!db.commit()) {
        outError = "sync-transaction-commit-failed";
        client.Disconnect();
        return false;
    }

    const QString logoutTag = MakeTag(commandIndex++);
    client.SendCommand(logoutTag + " LOGOUT");
    client.ReadResponseUntilTag(logoutTag);
    client.Disconnect();
    return true;
}

} // namespace ngks::core::mail::providers::imap

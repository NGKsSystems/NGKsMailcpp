#include "providers/icloud/ICloudAuthDriver.hpp"

#include "core/logging/AuditLog.h"
#include "core/mail/providers/imap/FolderMirrorService.h"
#include "core/mail/providers/imap/ImapProvider.h"
#include "db/BasicCredentialStore.hpp"
#include "providers/icloud/icloud_env.hpp"

namespace ngks::providers::icloud {

namespace {

QString RedactEmail(const QString& email)
{
    const int at = email.indexOf('@');
    if (at > 0 && at < email.size() - 1) {
        return QString("%1***@%2").arg(email.left(1), email.mid(at + 1));
    }
    if (email.isEmpty()) {
        return "";
    }
    return QString("%1***").arg(email.left(1));
}

QString SanitizeReason(const QString& reason)
{
    QString out = reason;
    out.replace("\r", " ");
    out.replace("\n", " ");
    out.replace("\"", "'");
    if (out.size() > 240) {
        out = out.left(240);
    }
    return out;
}

bool ParsePort(const QString& value, int& outPort)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok || parsed <= 0 || parsed > 65535) {
        return false;
    }
    outPort = parsed;
    return true;
}

} // namespace

ICloudAuthDriver::ICloudAuthDriver(ngks::core::storage::Db& db)
    : db_(db)
{
}

ngks::auth::AuthResult ICloudAuthDriver::BeginConnect(const QString& email)
{
    ngks::auth::AuthResult result;
    result.providerId = profile_.ProviderId();

    QString effectiveEmail = email.trimmed();
    if (effectiveEmail.isEmpty()) {
        effectiveEmail = env::ReadOptional(env::kEmail).value_or(QString());
    }

    const QString username = env::ReadOptional(env::kUsername).value_or(QString());
    const QString appPassword = env::ReadOptional(env::kAppPassword).value_or(QString());

    const QString imapHost = env::ReadOptional(env::kImapHost).value_or(QString("imap.mail.me.com"));
    const QString imapPortRaw = env::ReadOptional(env::kImapPort).value_or(QString("993"));
    const QString imapTlsRaw = env::ReadOptional(env::kImapTls).value_or(QString("true"));

    const QString smtpHost = env::ReadOptional(env::kSmtpHost).value_or(QString("smtp.mail.me.com"));
    const QString smtpPortRaw = env::ReadOptional(env::kSmtpPort).value_or(QString("587"));
    const QString smtpTlsRaw = env::ReadOptional(env::kSmtpTls).value_or(QString("true"));

    if (effectiveEmail.isEmpty()) {
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kEmail);
        result.exitCode = 71;
        return result;
    }
    if (username.isEmpty()) {
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kUsername);
        result.exitCode = 71;
        return result;
    }
    if (appPassword.isEmpty()) {
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kAppPassword);
        result.exitCode = 71;
        return result;
    }

    int imapPort = 993;
    if (!ParsePort(imapPortRaw, imapPort)) {
        result.detail = QString("%1 invalid-port %2").arg(env::LOG_PREFIX, env::kImapPort);
        result.exitCode = 71;
        return result;
    }

    int smtpPort = 587;
    if (!ParsePort(smtpPortRaw, smtpPort)) {
        result.detail = QString("%1 invalid-port %2").arg(env::LOG_PREFIX, env::kSmtpPort);
        result.exitCode = 71;
        return result;
    }

    bool imapTls = true;
    if (!env::ParseBoolText(imapTlsRaw, imapTls)) {
        result.detail = QString("%1 invalid-bool %2").arg(env::LOG_PREFIX, env::kImapTls);
        result.exitCode = 71;
        return result;
    }

    bool smtpTls = true;
    if (!env::ParseBoolText(smtpTlsRaw, smtpTls)) {
        result.detail = QString("%1 invalid-bool %2").arg(env::LOG_PREFIX, env::kSmtpTls);
        result.exitCode = 71;
        return result;
    }

    (void)smtpHost;
    (void)smtpPort;
    (void)smtpTls;

    ngks::core::mail::providers::imap::ResolveRequest request;
    request.email = effectiveEmail;
    request.host = imapHost;
    request.port = imapPort;
    request.tls = imapTls;
    request.username = username;
    request.password = appPassword;
    request.useXoauth2 = false;
    request.oauthAccessToken.clear();

    ngks::core::mail::providers::imap::ImapProvider imap;
    QVector<ngks::core::mail::providers::imap::ResolvedFolder> folders;
    QString resolveError;
    QString transcriptPath;
    if (!imap.ResolveAccount(request, folders, resolveError, transcriptPath)) {
        const QString sanitized = SanitizeReason(resolveError);
        ngks::core::logging::AuditLog::Event(
            "ICLOUD_CONNECT",
            QString("{\"provider\":\"icloud\",\"event\":\"CONNECT_FAIL\",\"reason\":\"%1\"}")
                .arg(sanitized)
                .toStdString());
        result.detail = QString("%1 imap-check-failed: %2").arg(env::LOG_PREFIX, sanitized);
        result.exitCode = 71;
        return result;
    }

    ngks::db::BasicCredentialRecord cred;
    cred.providerId = profile_.ProviderId();
    cred.email = effectiveEmail;
    cred.username = username;
    cred.secret = appPassword;

    QString credErr;
    if (!ngks::db::BasicCredentialStore::Upsert(db_, cred, credErr)) {
        const QString sanitized = SanitizeReason(credErr);
        ngks::core::logging::AuditLog::Event(
            "ICLOUD_CONNECT",
            QString("{\"provider\":\"icloud\",\"event\":\"CONNECT_FAIL\",\"reason\":\"%1\"}")
                .arg(sanitized)
                .toStdString());
        result.detail = QString("%1 credential-store-failed: %2").arg(env::LOG_PREFIX, sanitized);
        result.exitCode = 72;
        return result;
    }

    ngks::core::mail::providers::imap::FolderMirrorService mirror;
    int accountId = -1;
    QString mirrorError;
    const QString credentialRef = QString("basic_credentials:%1:%2").arg(profile_.ProviderId(), effectiveEmail);
    if (!mirror.MirrorResolvedAccount(db_, request, credentialRef, folders, accountId, mirrorError, profile_.ProviderId())) {
        const QString sanitized = SanitizeReason(mirrorError);
        ngks::core::logging::AuditLog::Event(
            "ICLOUD_CONNECT",
            QString("{\"provider\":\"icloud\",\"event\":\"CONNECT_FAIL\",\"reason\":\"%1\"}")
                .arg(sanitized)
                .toStdString());
        result.detail = QString("%1 db-mirror-failed: %2").arg(env::LOG_PREFIX, sanitized);
        result.exitCode = 72;
        return result;
    }

    ngks::core::logging::AuditLog::Event(
        "ICLOUD_CONNECT",
        QString("{\"provider\":\"icloud\",\"event\":\"CONNECT_OK\",\"email\":\"%1\"}")
            .arg(RedactEmail(effectiveEmail))
            .toStdString());

    result.ok = true;
    result.exitCode = 0;
    result.detail = QString("%1 connect-ok").arg(env::LOG_PREFIX);
    return result;
}

} // namespace ngks::providers::icloud

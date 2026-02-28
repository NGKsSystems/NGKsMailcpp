#include "providers/yahoo/YahooAuthDriver.hpp"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

#include "core/mail/providers/imap/FolderMirrorService.h"
#include "core/mail/providers/imap/ImapProvider.h"
#include "core/oauth/OAuthBroker.h"

namespace ngks::providers::yahoo {

YahooAuthDriver::YahooAuthDriver(ngks::core::storage::Db& db)
    : db_(db)
{
}

ngks::auth::AuthResult YahooAuthDriver::BeginConnect(const QString& email)
{
    ngks::auth::AuthResult result;
    result.providerId = profile_.ProviderId();

    const QString trimmedEmail = email.trimmed();
    if (trimmedEmail.isEmpty()) {
        result.detail = QString("%1 missing-email").arg(kLogPrefix);
        result.exitCode = 70;
        return result;
    }

    QString refreshToken;
    QString storedAccessToken;
    qint64 expiresAtUtc = 0;
    QString clientId;
    QString clientSecret;

    QSqlQuery tokenQ(db_.Handle());
    tokenQ.prepare("SELECT refresh_token, access_token, expires_at_utc, client_id, client_secret "
                   "FROM oauth_tokens WHERE provider=:p AND email=:e LIMIT 1");
    tokenQ.bindValue(":p", profile_.ProviderId());
    tokenQ.bindValue(":e", trimmedEmail);
    if (!tokenQ.exec() || !tokenQ.next()) {
        result.detail = QString("%1 legacy-oauth-token-not-found; use yahoo_app_password").arg(kLogPrefix);
        result.exitCode = 71;
        return result;
    }

    refreshToken = tokenQ.value(0).toString();
    storedAccessToken = tokenQ.value(1).toString();
    expiresAtUtc = tokenQ.value(2).toLongLong();
    clientId = tokenQ.value(3).toString().trimmed();
    clientSecret = tokenQ.value(4).toString().trimmed();

    QString accessToken = storedAccessToken;
    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const bool tokenExpiringSoon = (expiresAtUtc > 0 && expiresAtUtc <= (now + 60));

    if (!refreshToken.trimmed().isEmpty() && (accessToken.trimmed().isEmpty() || tokenExpiringSoon)) {
        ngks::core::oauth::OAuthConfig cfg;
        cfg.provider = profile_.ProviderId();
        cfg.email = trimmedEmail;
        cfg.clientId = clientId;
        cfg.clientSecret = clientSecret;
        cfg.tokenEndpoint = profile_.OAuth().tokenUrl;

        ngks::core::oauth::OAuthResult refreshed;
        QString refreshError;
        if (ngks::core::oauth::OAuthBroker::RefreshAccessToken(cfg, refreshToken, refreshed, refreshError)) {
            accessToken = refreshed.accessToken;

            QSqlQuery updateQ(db_.Handle());
            updateQ.prepare("UPDATE oauth_tokens SET access_token=:a, expires_at_utc=:x, updated_at_utc=:u WHERE provider=:p AND email=:e");
            updateQ.bindValue(":a", refreshed.accessToken);
            updateQ.bindValue(":x", refreshed.expiresAtUtc);
            updateQ.bindValue(":u", QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
            updateQ.bindValue(":p", profile_.ProviderId());
            updateQ.bindValue(":e", trimmedEmail);
            updateQ.exec();
        } else if (accessToken.trimmed().isEmpty()) {
            result.detail = QString("%1 refresh-failed: %2").arg(kLogPrefix, refreshError);
            result.exitCode = 71;
            return result;
        }
    }

    if (accessToken.trimmed().isEmpty()) {
        result.detail = QString("%1 empty-access-token; use yahoo_app_password").arg(kLogPrefix);
        result.exitCode = 71;
        return result;
    }

    ngks::core::mail::providers::imap::ResolveRequest request;
    request.email = trimmedEmail;
    request.host = "imap.mail.yahoo.com";
    request.port = 993;
    request.tls = true;
    request.username = trimmedEmail;
    request.password.clear();
    request.useXoauth2 = true;
    request.oauthAccessToken = accessToken;

    ngks::core::mail::providers::imap::ImapProvider imap;
    QVector<ngks::core::mail::providers::imap::ResolvedFolder> folders;
    QString resolveError;
    QString transcriptPath;
    if (!imap.ResolveAccount(request, folders, resolveError, transcriptPath)) {
        result.detail = QString("%1 legacy-oauth-imap-check-failed: %2").arg(kLogPrefix, resolveError);
        result.exitCode = 71;
        return result;
    }

    ngks::core::mail::providers::imap::FolderMirrorService mirror;
    int accountId = -1;
    QString mirrorError;
    const QString credentialRef = QString("oauth_tokens:%1:%2").arg(profile_.ProviderId(), trimmedEmail);
    if (!mirror.MirrorResolvedAccount(db_, request, credentialRef, folders, accountId, mirrorError, profile_.ProviderId())) {
        result.detail = QString("%1 legacy-oauth-db-mirror-failed: %2").arg(kLogPrefix, mirrorError);
        result.exitCode = 72;
        return result;
    }

    result.ok = true;
    result.exitCode = 0;
    result.detail = QString("%1 legacy-oauth-connect-ok").arg(kLogPrefix);
    result.accessToken = accessToken;
    result.refreshToken = refreshToken;
    result.expiresAtUtc = expiresAtUtc;
    return result;
}

} // namespace ngks::providers::yahoo

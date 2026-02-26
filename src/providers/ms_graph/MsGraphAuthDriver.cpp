#include "providers/ms_graph/MsGraphAuthDriver.hpp"

#include "core/auth/OAuthStore.h"
#include "core/oauth/OAuthBroker.h"
#include "db/TokenStore.hpp"
#include "providers/ms_graph/ms_graph_env.hpp"

namespace ngks::providers::ms_graph {

MsGraphAuthDriver::MsGraphAuthDriver(ngks::core::storage::Db& db)
    : db_(db)
{
}

ngks::auth::AuthResult MsGraphAuthDriver::BeginConnect(const QString& email)
{
    ngks::auth::AuthResult result;
    result.providerId = profile_.ProviderId();

    const QString trimmedEmail = email.trimmed();
    if (trimmedEmail.isEmpty()) {
        result.detail = "[MS_GRAPH] missing-email";
        result.exitCode = 70;
        return result;
    }

    QString clientId = QString::fromUtf8(qgetenv(env::kClientId)).trimmed();
    QString clientSecret = QString::fromUtf8(qgetenv(env::kClientSecret)).trimmed();

    if (clientId.isEmpty() || clientSecret.isEmpty()) {
        QString dbClientId;
        QString dbClientSecret;
        QString dbErr;
        if (ngks::core::auth::OAuthStore::GetProviderClientCredentials(db_, profile_.ProviderId(), dbClientId, dbClientSecret, dbErr)) {
            if (clientId.isEmpty()) clientId = dbClientId;
            if (clientSecret.isEmpty()) clientSecret = dbClientSecret;
        }
    }

    if (clientId.isEmpty()) {
        result.detail = "[MS_GRAPH] missing-client-id";
        result.exitCode = 71;
        return result;
    }

    ngks::core::oauth::OAuthConfig cfg;
    cfg.provider = profile_.ProviderId();
    cfg.email = trimmedEmail;
    cfg.clientId = clientId;
    cfg.clientSecret = clientSecret;
    cfg.authEndpoint = profile_.OAuth().authorizeUrl;
    cfg.tokenEndpoint = profile_.OAuth().tokenUrl;
    cfg.scope = profile_.Scopes().join(" ");
    cfg.sendClientSecretInCodeExchange = false;
    cfg.includeScopeInCodeExchange = true;
    cfg.listenPort = 0;
    cfg.timeoutSeconds = 180;

    ngks::core::oauth::OAuthResult oauth;
    QString connectError;
    QString brokerProofPath;
    if (!ngks::core::oauth::OAuthBroker::ConnectAndFetchTokens(cfg, oauth, connectError, brokerProofPath)) {
        result.detail = QString("%1 connect-failed: %2").arg(kLogPrefix, connectError);
        result.brokerProofPath = brokerProofPath;
        result.exitCode = 71;
        return result;
    }

    ngks::db::TokenRecord tokenRec;
    tokenRec.providerId = profile_.ProviderId();
    tokenRec.email = trimmedEmail;
    tokenRec.refreshToken = oauth.refreshToken;
    tokenRec.accessToken = oauth.accessToken;
    tokenRec.expiresAtUtc = oauth.expiresAtUtc;
    tokenRec.clientId = clientId;
    tokenRec.clientSecret = clientSecret;

    QString storeErr;
    if (!ngks::db::TokenStore::Store(db_, tokenRec, storeErr)) {
        result.detail = QString("%1 store-failed: %2").arg(kLogPrefix, storeErr);
        result.brokerProofPath = brokerProofPath;
        result.exitCode = 72;
        return result;
    }

    result.ok = true;
    result.exitCode = 0;
    result.detail = QString("%1 connect-ok").arg(kLogPrefix);
    result.brokerProofPath = brokerProofPath;
    result.accessToken = oauth.accessToken;
    result.refreshToken = oauth.refreshToken;
    result.expiresAtUtc = oauth.expiresAtUtc;
    return result;
}

} // namespace ngks::providers::ms_graph

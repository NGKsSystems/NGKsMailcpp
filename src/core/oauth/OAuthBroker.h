#pragma once

#include <QString>

namespace ngks::core::oauth {

struct OAuthConfig {
    QString provider;          // "gmail"
    QString email;             // for storage / display
    QString clientId;          // from Google Cloud OAuth client (Desktop)
    QString clientSecret;      // from Google Cloud OAuth client
    QString authEndpoint;      // https://accounts.google.com/o/oauth2/v2/auth
    QString tokenEndpoint;     // https://oauth2.googleapis.com/token
    QString scope;             // https://mail.google.com/
    int listenPort;            // 0 = auto
    int timeoutSeconds;        // e.g. 180
};

struct OAuthResult {
    QString refreshToken;
    QString accessToken;
    qint64 expiresAtUtc;       // unix seconds
};

class OAuthBroker {
public:
    static bool ConnectAndFetchTokens(const OAuthConfig& cfg, OAuthResult& out, QString& outError, QString& outProofPath);
    static bool RefreshAccessToken(const OAuthConfig& cfg, const QString& refreshToken, OAuthResult& out, QString& outError);
};

} // namespace ngks::core::oauth
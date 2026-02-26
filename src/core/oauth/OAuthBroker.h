#pragma once

#include <QString>

namespace ngks::core::oauth {

struct OAuthConfig {
    QString provider;          // provider id
    QString email;             // for storage / display
    QString clientId;          // provider client id
    QString clientSecret;      // provider client secret
    QString authEndpoint;      // provider authorize endpoint
    QString tokenEndpoint;     // provider token endpoint
    QString scope;             // provider-owned scope string
    bool sendClientSecretInCodeExchange = true;
    bool includeScopeInCodeExchange = false;
    int listenPort;            // 0 = auto
    int timeoutSeconds;        // e.g. 180
    bool listenUseHttps = false;
    QString redirectScheme = "http";    // "http" or "https"
    QString redirectHost = "127.0.0.1"; // use "localhost" for https
    QString certPath;                    // optional; default artifacts/certs/localhost.crt.pem
    QString keyPath;                     // optional; default artifacts/certs/localhost.key.pem
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
    static bool OAuthHttpsSelftest(const OAuthConfig& cfg, QString& outRedirectUri, QString& outError);
};

} // namespace ngks::core::oauth
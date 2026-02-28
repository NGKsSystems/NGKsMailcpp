#include "core/oauth/OAuthBroker.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace ngks::core::oauth {

static QString Base64Url(const QByteArray& in)
{
    QByteArray b = in.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QString::fromUtf8(b);
}

static QString RandomUrlSafe(int bytes)
{
    QByteArray buf;
    buf.resize(bytes);
    for (int i = 0; i < bytes; ++i) {
        buf[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    }
    return Base64Url(buf);
}

static bool WriteProofLine(QFile& f, const QString& s)
{
    if (!f.isOpen()) return false;
    QByteArray line = (s + "\n").toUtf8();
    return f.write(line) == line.size();
}

static bool PostForm(QNetworkAccessManager& nam, const QUrl& url, const QUrlQuery& form, QJsonObject& outJson, QString& outErr)
{
    outErr.clear();
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray body = form.query(QUrl::FullyEncoded).toUtf8();
    QNetworkReply* reply = nam.post(req, body);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    reply->deleteLater();

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        outErr = QString("token-json-parse-failed http=%1 err=%2 raw=%3")
                     .arg(http)
                     .arg(pe.errorString())
                     .arg(QString::fromUtf8(raw.left(300)));
        return false;
    }

    outJson = doc.object();
    if (http < 200 || http >= 300) {
        outErr = QString("token-http-failed http=%1").arg(http);
        if (outJson.contains("error")) {
            outErr += QString(" error=%1").arg(outJson.value("error").toString());
        }
        if (outJson.contains("error_description")) {
            outErr += QString(" desc=%1").arg(outJson.value("error_description").toString());
        }
        return false;
    }

    return true;
}

static QString DefaultCertPath()
{
    return "artifacts/certs/localhost.crt.pem";
}

static QString DefaultKeyPath()
{
    return "artifacts/certs/localhost.key.pem";
}

static QString BuildMissingTlsError(const QString& certPath, const QString& keyPath)
{
    return QString(
        "missing_tls_cert\n"
        "CERT_PATH=%1\n"
        "KEY_PATH=%2\n"
        "openssl req -x509 -newkey rsa:2048 -nodes ^\n"
        "  -keyout artifacts/certs/localhost.key.pem ^\n"
        "  -out artifacts/certs/localhost.crt.pem ^\n"
        "  -days 365 ^\n"
        "  -subj \"/CN=localhost\"")
        .arg(certPath, keyPath);
}

static bool ValidateLoopbackConfig(const OAuthConfig& cfg, QString& outErr)
{
    outErr.clear();
    const QString scheme = cfg.redirectScheme.trimmed().toLower();
    const QString host = cfg.redirectHost.trimmed().toLower();

    if (cfg.listenUseHttps) {
        if (scheme != "https") {
            outErr = "invalid-https-config redirectScheme must be https";
            return false;
        }
        if (host != "localhost") {
            outErr = "invalid-https-config redirectHost must be localhost";
            return false;
        }
        if (cfg.listenPort <= 0) {
            outErr = "invalid-https-config listenPort must be fixed and > 0";
            return false;
        }
    }

    if (!cfg.listenUseHttps && scheme != "http") {
        outErr = "invalid-http-config redirectScheme must be http";
        return false;
    }

    if (host.isEmpty()) {
        outErr = "invalid-redirect-host";
        return false;
    }
    return true;
}

static QString BuildRedirectUri(const OAuthConfig& cfg, int port)
{
    return QString("%1://%2:%3/callback")
        .arg(cfg.redirectScheme.trimmed().isEmpty() ? "http" : cfg.redirectScheme,
             cfg.redirectHost.trimmed().isEmpty() ? "127.0.0.1" : cfg.redirectHost)
        .arg(port);
}

static bool LoadTlsMaterial(const OAuthConfig& cfg, QSslCertificate& outCert, QSslKey& outKey, QString& outErr)
{
    outErr.clear();
    const QString certPath = cfg.certPath.trimmed().isEmpty() ? DefaultCertPath() : cfg.certPath.trimmed();
    const QString keyPath = cfg.keyPath.trimmed().isEmpty() ? DefaultKeyPath() : cfg.keyPath.trimmed();

    QFile certFile(certPath);
    QFile keyFile(keyPath);
    if (!certFile.exists() || !keyFile.exists()) {
        outErr = BuildMissingTlsError(certPath, keyPath);
        return false;
    }
    if (!certFile.open(QIODevice::ReadOnly) || !keyFile.open(QIODevice::ReadOnly)) {
        outErr = BuildMissingTlsError(certPath, keyPath);
        return false;
    }

    outCert = QSslCertificate(certFile.readAll(), QSsl::Pem);
    outKey = QSslKey(keyFile.readAll(), QSsl::Rsa, QSsl::Pem);
    if (outCert.isNull() || outKey.isNull()) {
        outErr = BuildMissingTlsError(certPath, keyPath);
        return false;
    }
    return true;
}

static bool WaitForOAuthCode(const OAuthConfig& cfg, QTcpServer& server, int timeoutSeconds, QString& outCode, QString& outErr, QFile* proof)
{
    outErr.clear();
    outCode.clear();

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&server, &QTcpServer::newConnection, &loop, &QEventLoop::quit);

    timer.start(timeoutSeconds * 1000);
    loop.exec();

    if (!server.hasPendingConnections()) {
        outErr = "oauth-timeout";
        if (proof) WriteProofLine(*proof, "OAUTH_WAIT=timeout");
        return false;
    }

    QTcpSocket* plainSock = server.nextPendingConnection();
    if (!plainSock) {
        outErr = "oauth-no-socket";
        return false;
    }

    QTcpSocket* activeSock = plainSock;
    if (cfg.listenUseHttps) {
        QSslCertificate cert;
        QSslKey key;
        if (!LoadTlsMaterial(cfg, cert, key, outErr)) {
            if (proof) WriteProofLine(*proof, "ERROR=" + outErr);
            plainSock->disconnectFromHost();
            plainSock->deleteLater();
            return false;
        }

        auto* sslSock = new QSslSocket();
        if (!sslSock->setSocketDescriptor(plainSock->socketDescriptor())) {
            outErr = "oauth-https-socket-descriptor-failed";
            plainSock->disconnectFromHost();
            plainSock->deleteLater();
            sslSock->deleteLater();
            return false;
        }
        plainSock->deleteLater();
        sslSock->setLocalCertificate(cert);
        sslSock->setPrivateKey(key);
        sslSock->startServerEncryption();
        if (!sslSock->waitForEncrypted(10000)) {
            outErr = "oauth-https-handshake-failed";
            sslSock->disconnectFromHost();
            sslSock->deleteLater();
            return false;
        }
        activeSock = sslSock;
    }

    activeSock->waitForReadyRead(3000);
    const QByteArray req = activeSock->readAll();

    // Very small HTTP parse: first line "GET /callback?code=... HTTP/1.1"
    const QList<QByteArray> lines = req.split('\n');
    if (lines.isEmpty()) {
        outErr = "oauth-bad-http";
        activeSock->disconnectFromHost();
        activeSock->deleteLater();
        return false;
    }

    const QByteArray first = lines[0].trimmed();
    const QList<QByteArray> parts = first.split(' ');
    if (parts.size() < 2) {
        outErr = "oauth-bad-http-line";
        activeSock->disconnectFromHost();
        activeSock->deleteLater();
        return false;
    }

    const QByteArray pathPart = parts[1];
    QUrl url(QString("http://localhost%1").arg(QString::fromUtf8(pathPart)));
    QUrlQuery q(url);
    const QString code = q.queryItemValue("code");
    const QString err = q.queryItemValue("error");

    QByteArray resp;
    if (!err.isEmpty()) {
        resp =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
            "<html><body><h3>OAuth failed</h3><p>You can close this tab.</p></body></html>";
        outErr = QString("oauth-error=%1").arg(err);
    } else if (code.isEmpty()) {
        resp =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
            "<html><body><h3>OAuth missing code</h3><p>You can close this tab.</p></body></html>";
        outErr = "oauth-missing-code";
    } else {
        resp =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
            "<html><body><h3>OAuth OK</h3><p>You can close this tab and return to NGKsMailcpp.</p></body></html>";
        outCode = code;
    }

    activeSock->write(resp);
    activeSock->flush();
    activeSock->disconnectFromHost();
    activeSock->deleteLater();

    if (proof) {
        WriteProofLine(*proof, QString("OAUTH_HTTP_FIRSTLINE=%1").arg(QString::fromUtf8(first)));
        WriteProofLine(*proof, outCode.isEmpty() ? "OAUTH_CODE=EMPTY" : "OAUTH_CODE=RECEIVED");
    }

    return !outCode.isEmpty();
}

bool OAuthBroker::ConnectAndFetchTokens(const OAuthConfig& cfg, OAuthResult& out, QString& outError, QString& outProofPath)
{
    outError.clear();
    out = {};
    outProofPath.clear();

    // proof file (no secrets)
    const QString proofName = QString("30_oauth_connect_%1_%2.txt")
        .arg(cfg.provider, QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz"));
    outProofPath = QString("artifacts/_proof/%1").arg(proofName);
    QFile proof(outProofPath);
    proof.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);

    WriteProofLine(proof, "=== 30 OAUTH CONNECT ===");
    WriteProofLine(proof, "TIMESTAMP_UTC=" + QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    WriteProofLine(proof, "PROVIDER=" + cfg.provider);
    WriteProofLine(proof, "EMAIL=" + cfg.email);
    WriteProofLine(proof, "SCOPE=" + cfg.scope);

    if (!ValidateLoopbackConfig(cfg, outError)) {
        WriteProofLine(proof, "ERROR=" + outError);
        return false;
    }

    if (cfg.clientId.trimmed().isEmpty()) {
        outError = "missing-client-id";
        WriteProofLine(proof, "ERROR=missing-client-id");
        return false;
    }
    if (cfg.sendClientSecretInCodeExchange && cfg.clientSecret.trimmed().isEmpty()) {
        outError = "missing-client-secret";
        WriteProofLine(proof, "ERROR=missing-client-secret");
        return false;
    }

    // Loopback server
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, cfg.listenPort <= 0 ? 0 : static_cast<quint16>(cfg.listenPort))) {
        outError = "oauth-listen-failed";
        WriteProofLine(proof, "ERROR=oauth-listen-failed");
        return false;
    }
    const int port = server.serverPort();
    const QString redirectUri = BuildRedirectUri(cfg, port);

    WriteProofLine(proof, "REDIRECT_URI=" + redirectUri);

    // PKCE
    const QString codeVerifier = RandomUrlSafe(48);
    const QByteArray sha = QCryptographicHash::hash(codeVerifier.toUtf8(), QCryptographicHash::Sha256);
    const QString codeChallenge = Base64Url(sha);

    // State
    const QString state = RandomUrlSafe(16);

    // Auth URL
    QUrl authUrl(cfg.authEndpoint);
    QUrlQuery aq;
    aq.addQueryItem("client_id", cfg.clientId);
    aq.addQueryItem("redirect_uri", redirectUri);
    aq.addQueryItem("response_type", "code");
    aq.addQueryItem("scope", cfg.scope);
    aq.addQueryItem("access_type", "offline");
    aq.addQueryItem("prompt", "consent");
    aq.addQueryItem("state", state);
    aq.addQueryItem("code_challenge", codeChallenge);
    aq.addQueryItem("code_challenge_method", "S256");
    authUrl.setQuery(aq);

    WriteProofLine(proof, "AUTH_URL_OPENED=yes");
    QDesktopServices::openUrl(authUrl);

    // Wait for code
    QString code;
    if (!WaitForOAuthCode(cfg, server, cfg.timeoutSeconds, code, outError, &proof)) {
        WriteProofLine(proof, "ERROR=" + outError);
        return false;
    }

    // Exchange code for tokens
    QNetworkAccessManager nam;
    QJsonObject json;
    QUrlQuery form;
    form.addQueryItem("client_id", cfg.clientId);
    if (cfg.sendClientSecretInCodeExchange && !cfg.clientSecret.trimmed().isEmpty()) {
        form.addQueryItem("client_secret", cfg.clientSecret);
    }
    form.addQueryItem("code", code);
    form.addQueryItem("code_verifier", codeVerifier);
    form.addQueryItem("redirect_uri", redirectUri);
    form.addQueryItem("grant_type", "authorization_code");
    if (cfg.includeScopeInCodeExchange && !cfg.scope.trimmed().isEmpty()) {
        form.addQueryItem("scope", cfg.scope);
    }

    if (!PostForm(nam, QUrl(cfg.tokenEndpoint), form, json, outError)) {
        WriteProofLine(proof, "ERROR=token-exchange-failed");
        WriteProofLine(proof, "DETAIL=" + outError);
        return false;
    }

    const QString accessToken = json.value("access_token").toString();
    const QString refreshToken = json.value("refresh_token").toString();
    const int expiresIn = json.value("expires_in").toInt();

    if (accessToken.isEmpty()) {
        outError = "missing-access-token";
        WriteProofLine(proof, "ERROR=missing-access-token");
        return false;
    }
    if (refreshToken.isEmpty()) {
        outError = "missing-refresh-token";
        WriteProofLine(proof, "ERROR=missing-refresh-token");
        WriteProofLine(proof, "HINT=Did Google already grant offline? Remove app access and retry; prompt=consent should force it.");
        return false;
    }

    out.accessToken = accessToken;
    out.refreshToken = refreshToken;
    out.expiresAtUtc = QDateTime::currentDateTimeUtc().addSecs(expiresIn > 0 ? expiresIn : 0).toSecsSinceEpoch();

    WriteProofLine(proof, "TOKEN_OK=yes");
    WriteProofLine(proof, "ACCESS_TOKEN_PRESENT=yes");
    WriteProofLine(proof, "REFRESH_TOKEN_PRESENT=yes");
    WriteProofLine(proof, "EXPIRES_AT_UTC=" + QString::number(out.expiresAtUtc));

    return true;
}

bool OAuthBroker::OAuthHttpsSelftest(const OAuthConfig& cfg, QString& outRedirectUri, QString& outError)
{
    outRedirectUri.clear();
    outError.clear();

    if (!ValidateLoopbackConfig(cfg, outError)) {
        return false;
    }

    if (cfg.listenUseHttps) {
        QSslCertificate cert;
        QSslKey key;
        if (!LoadTlsMaterial(cfg, cert, key, outError)) {
            return false;
        }
    }

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, cfg.listenPort <= 0 ? 0 : static_cast<quint16>(cfg.listenPort))) {
        outError = "oauth-listen-failed";
        return false;
    }

    outRedirectUri = BuildRedirectUri(cfg, server.serverPort());
    server.close();
    return true;
}

bool OAuthBroker::RefreshAccessToken(const OAuthConfig& cfg, const QString& refreshToken, OAuthResult& out, QString& outError)
{
    outError.clear();
    out = {};

    if (cfg.clientId.trimmed().isEmpty()) {
        outError = "missing-client-id";
        return false;
    }
    if (refreshToken.trimmed().isEmpty()) {
        outError = "missing-refresh-token";
        return false;
    }

    QNetworkAccessManager nam;
    QJsonObject json;
    QUrlQuery form;
    form.addQueryItem("client_id", cfg.clientId);
    if (!cfg.clientSecret.trimmed().isEmpty()) {
        form.addQueryItem("client_secret", cfg.clientSecret);
    }
    form.addQueryItem("refresh_token", refreshToken);
    form.addQueryItem("grant_type", "refresh_token");

    if (!PostForm(nam, QUrl(cfg.tokenEndpoint), form, json, outError)) {
        return false;
    }

    const QString accessToken = json.value("access_token").toString();
    const int expiresIn = json.value("expires_in").toInt();

    if (accessToken.isEmpty()) {
        outError = "missing-access-token";
        return false;
    }

    out.accessToken = accessToken;
    out.refreshToken = refreshToken;
    out.expiresAtUtc = QDateTime::currentDateTimeUtc().addSecs(expiresIn > 0 ? expiresIn : 0).toSecsSinceEpoch();
    return true;
}

} // namespace ngks::core::oauth
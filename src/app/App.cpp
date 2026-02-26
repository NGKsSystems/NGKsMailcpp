// src/app/App.cpp
#include "app/App.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QTimer>

#include <iostream>

#include "core/auth/OAuthStore.h"
#include "core/logging/AuditLog.h"
#include "core/mail/providers/imap/FolderMirrorService.h"
#include "core/mail/providers/imap/ImapProvider.h"
#include "core/oauth/OAuthBroker.h"
#include "core/storage/Db.h"
#include "core/storage/Schema.h"
#include "platform/common/Paths.h"
#include "providers/core/ProviderRegistry.hpp"
#include "providers/yahoo/YahooProfile.hpp"
#include "ui/MainWindow.h"

namespace ngks::app {

namespace {

QString JsonEscape(const QString& value)
{
    QString out = value;
    out.replace("\\", "\\\\");
    out.replace("\"", "\\\"");
    return out;
}

QString RedactUsername(const QString& username)
{
    const int at = username.indexOf('@');
    if (at > 0 && at < username.size() - 1) {
        return QString("%1***@%2").arg(username.left(1), username.mid(at + 1));
    }
    if (username.isEmpty()) {
        return "";
    }
    return QString("%1***").arg(username.left(1));
}

bool IsLocalhostHost(const QString& host)
{
    const QString lowered = host.trimmed().toLower();
    return lowered == "localhost" || lowered == "127.0.0.1" || lowered == "::1";
}

struct Xoauth2FailMeta {
    bool isXoauth2 = false;
    bool shapeOk = false;
    QString imapLastReply;
    QString shapeReason;
    QString imapPhase;
    QString imapTag;
    QString imapLastTagged;
    QString imapLastUntagged;
    bool sawPlusContinuation = false;
    int authBearerCount = -1;
    int firstAuthBearerPos = -1;
    int secondAuthBearerPos = -1;
    int rawLen = -1;
};

Xoauth2FailMeta ParseXoauth2FailMeta(const QString& resolveError)
{
    Xoauth2FailMeta meta;
    if (!resolveError.startsWith("XOAUTH2 failed")) {
        return meta;
    }

    meta.isXoauth2 = true;
    const QStringList parts = resolveError.split('|', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part.startsWith("imap_last_reply=")) {
            meta.imapLastReply = part.mid(QString("imap_last_reply=").size());
        } else if (part.startsWith("xoauth2_shape_ok=")) {
            const QString v = part.mid(QString("xoauth2_shape_ok=").size()).trimmed().toLower();
            meta.shapeOk = (v == "true" || v == "1");
        } else if (part.startsWith("xoauth2_shape_reason=")) {
            meta.shapeReason = part.mid(QString("xoauth2_shape_reason=").size());
        } else if (part.startsWith("auth_bearer_count=")) {
            meta.authBearerCount = part.mid(QString("auth_bearer_count=").size()).toInt();
        } else if (part.startsWith("first_auth_bearer_pos=")) {
            meta.firstAuthBearerPos = part.mid(QString("first_auth_bearer_pos=").size()).toInt();
        } else if (part.startsWith("second_auth_bearer_pos=")) {
            meta.secondAuthBearerPos = part.mid(QString("second_auth_bearer_pos=").size()).toInt();
        } else if (part.startsWith("raw_len=")) {
            meta.rawLen = part.mid(QString("raw_len=").size()).toInt();
        } else if (part.startsWith("imap_phase=")) {
            meta.imapPhase = part.mid(QString("imap_phase=").size());
        } else if (part.startsWith("imap_last_tagged=")) {
            meta.imapLastTagged = part.mid(QString("imap_last_tagged=").size());
        } else if (part.startsWith("imap_tag=")) {
            meta.imapTag = part.mid(QString("imap_tag=").size());
        } else if (part.startsWith("imap_last_untagged=")) {
            meta.imapLastUntagged = part.mid(QString("imap_last_untagged=").size());
        } else if (part.startsWith("saw_plus_continuation=")) {
            const QString v = part.mid(QString("saw_plus_continuation=").size()).trimmed().toLower();
            meta.sawPlusContinuation = (v == "true" || v == "1");
        }
    }
    return meta;
}

int DumpFoldersToProof(const std::filesystem::path& dbPath, int limit)
{
    const auto proofPath = ngks::platform::common::ArtifactsDir() / "_proof" / "29_db_dump_folders.txt";
    QFile outFile(QString::fromStdString(proofPath.string()));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return 30;
    }

    QTextStream out(&outFile);
    out << "=== 29 DB DUMP FOLDERS ===\n";
    out << "TIMESTAMP: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "\n";
    out << "DB_PATH: " << QString::fromStdString(dbPath.string()) << "\n";
    out << "LIMIT: " << limit << "\n\n";

    ngks::core::storage::Db dumpDb;
    if (!dumpDb.Open(dbPath)) {
        out << "ERROR: failed to open db\n";
        return 31;
    }

    QSqlQuery countQuery(dumpDb.Handle());
    if (!countQuery.exec("select count(*) from folders")) {
        out << "ERROR: count query failed: " << countQuery.lastError().text() << "\n";
        return 32;
    }
    if (countQuery.next()) {
        out << "FOLDERS_COUNT: " << countQuery.value(0).toInt() << "\n\n";
    } else {
        out << "ERROR: count query returned no rows\n";
        return 33;
    }

    QSqlQuery listQuery(dumpDb.Handle());
    if (!listQuery.exec(QString("select id, remote_name, display_name, special_use from folders order by remote_name limit %1").arg(limit))) {
        out << "ERROR: list query failed: " << listQuery.lastError().text() << "\n";
        return 34;
    }

    out << "id\tremote_name\tdisplay_name\tparent_id\tspecial_use\n";
    int rows = 0;
    while (listQuery.next()) {
        const QString parentId = "N/A";
        out << listQuery.value(0).toInt() << '\t'
            << listQuery.value(1).toString() << '\t'
            << listQuery.value(2).toString() << '\t'
            << parentId << '\t'
            << listQuery.value(3).toString() << '\n';
        ++rows;
    }
    out << "\nROWS_WRITTEN: " << rows << "\n";
    out.flush();
    return 0;
}

int DumpOAuthToProof(const std::filesystem::path& dbPath, int limit)
{
    const auto proofPath = ngks::platform::common::ArtifactsDir() / "_proof" / "30_db_dump_oauth.txt";
    QFile outFile(QString::fromStdString(proofPath.string()));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return 35;
    }

    QTextStream out(&outFile);
    out << "=== 30 DB DUMP OAUTH ===\n";
    out << "TIMESTAMP: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "\n";
    out << "DB_PATH: " << QString::fromStdString(dbPath.string()) << "\n";
    out << "LIMIT: " << limit << "\n\n";

    ngks::core::storage::Db dumpDb;
    if (!dumpDb.Open(dbPath)) {
        out << "ERROR: failed to open db\n";
        return 36;
    }

    QSqlQuery countQuery(dumpDb.Handle());
    if (!countQuery.exec("select count(*) from oauth_tokens")) {
        out << "ERROR: count query failed: " << countQuery.lastError().text() << "\n";
        return 37;
    }
    if (countQuery.next()) {
        out << "OAUTH_TOKENS_COUNT: " << countQuery.value(0).toInt() << "\n\n";
    } else {
        out << "ERROR: count query returned no rows\n";
        return 38;
    }

    bool hasAccessExpiresUtc = false;
    bool hasExpiresAtUtc = false;
    QSqlQuery colsQuery(dumpDb.Handle());
    if (colsQuery.exec("PRAGMA table_info(oauth_tokens)")) {
        while (colsQuery.next()) {
            const QString col = colsQuery.value(1).toString();
            if (col.compare("access_expires_utc", Qt::CaseInsensitive) == 0) {
                hasAccessExpiresUtc = true;
            }
            if (col.compare("expires_at_utc", Qt::CaseInsensitive) == 0) {
                hasExpiresAtUtc = true;
            }
        }
    }

    QString expiresExpr = "NULL";
    if (hasAccessExpiresUtc) {
        expiresExpr = "access_expires_utc";
    } else if (hasExpiresAtUtc) {
        expiresExpr = "expires_at_utc";
    }

    QSqlQuery listQuery(dumpDb.Handle());
    if (!listQuery.exec(QString(
            "select provider, email, "
            "case when refresh_token is not null and length(refresh_token) > 0 then 1 else 0 end as has_refresh_token, "
            "%1 as access_expires_utc "
            "from oauth_tokens order by provider, email limit %2")
                             .arg(expiresExpr)
                             .arg(limit))) {
        out << "ERROR: list query failed: " << listQuery.lastError().text() << "\n";
        return 39;
    }

    out << "provider\temail\thas_refresh_token\taccess_expires_utc\n";
    int rows = 0;
    while (listQuery.next()) {
        const QString expires = listQuery.value(3).isNull() ? "NULL" : listQuery.value(3).toString();
        out << listQuery.value(0).toString() << '\t'
            << listQuery.value(1).toString() << '\t'
            << listQuery.value(2).toInt() << '\t'
            << expires << '\n';
        ++rows;
    }

    out << "\nROWS_WRITTEN: " << rows << "\n";
    out.flush();
    return 0;
}

int CountOAuthRows(ngks::core::storage::Db& db)
{
    QSqlQuery countQuery(db.Handle());
    if (!countQuery.exec("select count(*) from oauth_tokens")) {
        return -1;
    }
    if (!countQuery.next()) {
        return -1;
    }
    return countQuery.value(0).toInt();
}

int WriteOAuthConnectProof(const QString& status, const QString& emailMasked, const QString& detail, const QString& brokerProofPath, int dbRowsAfter)
{
    const auto proofDir = ngks::platform::common::ArtifactsDir() / "_proof";
    const auto proofPath = proofDir / QString("52_oauth_connect_result_%1.txt")
                                        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz"))
                                        .toStdString();

    QFile outFile(QString::fromStdString(proofPath.string()));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return 60;
    }

    QTextStream out(&outFile);
    out << "=== 30 OAUTH CONNECT ===\n";
    out << "TIMESTAMP: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "\n";
    out << "STATUS: " << status << "\n";
    out << "EMAIL: " << emailMasked << "\n";
    out << "DB_ROWS_AFTER: " << dbRowsAfter << "\n";
    if (!detail.isEmpty()) {
        out << "DETAIL: " << detail << "\n";
    }
    if (!brokerProofPath.isEmpty()) {
        out << "BROKER_PROOF_PATH: " << brokerProofPath << "\n";
    }
    out.flush();
    return 0;
}

} // namespace

void MainWindowDeleter::operator()(ngks::ui::MainWindow* p) noexcept
{
    delete p;
}

int App::Run(int argc, char* argv[])
{
    QApplication qtApp(argc, argv);

    QCoreApplication::setOrganizationName("NGKsSystems");
    QCoreApplication::setApplicationName("NGKsMailcpp");

    if (!ngks::platform::common::EnsureAppDirectories()) {
        return 2;
    }

    ngks::core::storage::Db db;
    if (!db.Open(ngks::platform::common::DbFilePath())) {
        return 3;
    }

    ngks::core::storage::Schema schema(db);
    if (!schema.Ensure()) {
        return 4;
    }

    // Phase 2 prerequisite: ensure OAuth tables exist before anything uses them.
    // Keep this immediately after schema.Ensure(), before any OAuth path runs.
    {
        QString oauthErr;
        if (!ngks::core::auth::OAuthStore::EnsureTables(db, oauthErr)) {
            ngks::core::logging::AuditLog::Init(ngks::platform::common::AuditLogFilePath().string());
            ngks::core::logging::AuditLog::Event(
                "OAUTH_FAIL",
                QString("{\"reason\":\"%1\"}").arg(JsonEscape(oauthErr)).toStdString());
            return 40;
        }
    }

    ngks::core::logging::AuditLog::Init(ngks::platform::common::AuditLogFilePath().string());
    ngks::core::logging::AuditLog::AppStart(ngks::platform::common::DbFilePath().string(), 1);
    QObject::connect(&qtApp, &QCoreApplication::aboutToQuit, []() {
        ngks::core::logging::AuditLog::AppExit(1);
    });

    QCommandLineParser parser;
    parser.setApplicationDescription("NGKsMailcpp");
    parser.addHelpOption();

    const QCommandLineOption resolveOpt("resolve-test", "Run IMAP account resolve + mirror and exit.");
    const QCommandLineOption oauthConnectOpt("oauth-connect", "Run Gmail OAuth connect flow and store refresh token.");
    const QCommandLineOption gmailOauthConnectOpt("gmail-oauth-connect", "Run Gmail OAuth connect flow via provider-owned driver.");
    const QCommandLineOption msOauthConnectOpt("ms-oauth-connect", "Run Microsoft OAuth connect flow via provider-owned driver.");
    const QCommandLineOption icloudConnectOpt("icloud-connect", "Run iCloud app-password connect flow via provider-owned driver.");
    const QCommandLineOption yahooConnectOpt("yahoo-connect", "Run Yahoo app-password connect flow via provider-owned driver.");
    const QCommandLineOption genericImapConnectOpt("generic-imap-connect", "Run Generic IMAP connect flow via provider-owned driver.");
    const QCommandLineOption genericImapConnectInteractiveOpt("generic-imap-connect-interactive", "Run Generic IMAP connect with interactive terminal prompts.");
    const QCommandLineOption emailOpt("email", "Account email", "email");
    const QCommandLineOption hostOpt("host", "IMAP host", "host");
    const QCommandLineOption portOpt("port", "IMAP port", "port", "993");
    const QCommandLineOption tlsOpt("tls", "TLS mode true/false", "tls", "true");
    const QCommandLineOption userOpt("username", "IMAP username", "username");
    const QCommandLineOption passOpt("password", "IMAP password", "password");
    const QCommandLineOption allowLocalhostOpt("allow-localhost", "Allow localhost/loopback target in resolve-test mode.");
    const QCommandLineOption dbDumpFoldersOpt("db-dump-folders", "Dump folders table to artifacts/_proof/29_db_dump_folders.txt and exit.");
    const QCommandLineOption dbDumpOAuthOpt("db-dump-oauth", "Dump oauth_tokens table to artifacts/_proof/30_db_dump_oauth.txt and exit.");
    const QCommandLineOption auditSelftestOpt("audit-selftest", "Emit provider-tagged audit events (gmail/ms_graph) and exit.");
    const QCommandLineOption oauthHttpsSelftestOpt("oauth-https-selftest", "Run OAuth HTTPS loopback listener selftest (Yahoo scaffold) and exit.");
    const QCommandLineOption limitOpt("limit", "Limit for --db-dump-folders rows.", "limit", "200");

    parser.addOption(resolveOpt);
    parser.addOption(oauthConnectOpt);
    parser.addOption(gmailOauthConnectOpt);
    parser.addOption(msOauthConnectOpt);
    parser.addOption(icloudConnectOpt);
    parser.addOption(yahooConnectOpt);
    parser.addOption(genericImapConnectOpt);
    parser.addOption(genericImapConnectInteractiveOpt);
    parser.addOption(emailOpt);
    parser.addOption(hostOpt);
    parser.addOption(portOpt);
    parser.addOption(tlsOpt);
    parser.addOption(userOpt);
    parser.addOption(passOpt);
    parser.addOption(allowLocalhostOpt);
    parser.addOption(dbDumpFoldersOpt);
    parser.addOption(dbDumpOAuthOpt);
    parser.addOption(auditSelftestOpt);
    parser.addOption(oauthHttpsSelftestOpt);
    parser.addOption(limitOpt);
    parser.process(qtApp);

    bool ok = false;
    int limit = parser.value(limitOpt).toInt(&ok);
    if (!ok || limit <= 0) {
        limit = 200;
    }
    if (limit > 5000) {
        limit = 5000;
    }

    if (parser.isSet(dbDumpFoldersOpt)) {
        return DumpFoldersToProof(ngks::platform::common::DbFilePath(), limit);
    }

    if (parser.isSet(dbDumpOAuthOpt)) {
        return DumpOAuthToProof(ngks::platform::common::DbFilePath(), limit);
    }

    if (parser.isSet(auditSelftestOpt)) {
        ngks::core::logging::AuditLog::Event(
            "AUDIT_SELFTEST",
            "{\"provider\":\"gmail\",\"kind\":\"selftest\"}");
        ngks::core::logging::AuditLog::Event(
            "AUDIT_SELFTEST",
            "{\"provider\":\"ms_graph\",\"kind\":\"selftest\"}");
        return 0;
    }

    if (parser.isSet(oauthHttpsSelftestOpt)) {
        ngks::providers::yahoo::YahooProfile yahoo;
        ngks::core::oauth::OAuthConfig cfg;
        cfg.provider = yahoo.ProviderId();
        cfg.redirectScheme = "https";
        cfg.redirectHost = "localhost";
        cfg.listenUseHttps = true;
        cfg.listenPort = 53682;
        cfg.timeoutSeconds = 10;
        cfg.certPath = "artifacts/certs/localhost.crt.pem";
        cfg.keyPath = "artifacts/certs/localhost.key.pem";

        QString redirectUri;
        QString selftestError;
        if (!ngks::core::oauth::OAuthBroker::OAuthHttpsSelftest(cfg, redirectUri, selftestError)) {
            std::cout << "OAUTH_HTTPS_SELFTEST_FAIL " << selftestError.toStdString() << std::endl;
            return 75;
        }

        std::cout << "OAUTH_HTTPS_SELFTEST_OK redirectUri=" << redirectUri.toStdString() << std::endl;
        return 0;
    }

    ngks::providers::core::ProviderRegistry providerRegistry;
    providerRegistry.RegisterBuiltins(db);

    if (parser.isSet(oauthConnectOpt)
        || parser.isSet(gmailOauthConnectOpt)
        || parser.isSet(msOauthConnectOpt)
        || parser.isSet(icloudConnectOpt)
        || parser.isSet(yahooConnectOpt)
        || parser.isSet(genericImapConnectOpt)
        || parser.isSet(genericImapConnectInteractiveOpt)) {
        const QString email = parser.value(emailOpt).trimmed();
        if (email.isEmpty()
            && !parser.isSet(genericImapConnectOpt)
            && !parser.isSet(genericImapConnectInteractiveOpt)
            && !parser.isSet(icloudConnectOpt)
            && !parser.isSet(yahooConnectOpt)) {
            ngks::core::logging::AuditLog::Event("OAUTH_CONNECT_FAIL", "{\"reason\":\"missing-email\"}");
            WriteOAuthConnectProof("FAIL", "", "missing-email", "", CountOAuthRows(db));
            return 70;
        }

        QString providerId;
        if (parser.isSet(gmailOauthConnectOpt)) {
            providerId = "gmail";
        } else if (parser.isSet(msOauthConnectOpt)) {
            providerId = "ms_graph";
        } else if (parser.isSet(icloudConnectOpt)) {
            providerId = "icloud";
        } else if (parser.isSet(yahooConnectOpt)) {
            providerId = "yahoo_app_password";
        } else if (parser.isSet(genericImapConnectOpt) || parser.isSet(genericImapConnectInteractiveOpt)) {
            providerId = "generic_imap";
        } else {
            const auto* discovered = providerRegistry.DetectProviderByEmail(email);
            if (!discovered) {
                ngks::core::logging::AuditLog::Event(
                    "OAUTH_CONNECT_FAIL",
                    QString("{\"reason\":\"provider-not-detected\",\"email\":\"%1\"}")
                        .arg(JsonEscape(RedactUsername(email)))
                        .toStdString());
                WriteOAuthConnectProof("FAIL", RedactUsername(email), "provider-not-detected", "", CountOAuthRows(db));
                return 73;
            }
            providerId = discovered->ProviderId();
        }

        auto* driver = providerRegistry.GetAuthDriverById(providerId);
        if (!driver) {
            ngks::core::logging::AuditLog::Event(
                "OAUTH_CONNECT_FAIL",
                QString("{\"reason\":\"driver-not-found\",\"provider\":\"%1\"}")
                    .arg(JsonEscape(providerId))
                    .toStdString());
            WriteOAuthConnectProof("FAIL", RedactUsername(email), "driver-not-found", "", CountOAuthRows(db));
            return 74;
        }

        ngks::core::logging::AuditLog::Event(
            "OAUTH_CONNECT_START",
            QString("{\"provider\":\"%1\",\"email\":\"%2\"}")
                .arg(JsonEscape(providerId), JsonEscape(RedactUsername(email)))
                .toStdString());

        const bool interactiveGenericImap = parser.isSet(genericImapConnectInteractiveOpt) && providerId.compare("generic_imap", Qt::CaseInsensitive) == 0;
        const ngks::auth::AuthResult result = interactiveGenericImap
            ? driver->BeginConnectInteractive(email)
            : driver->BeginConnect(email);
        if (!result.ok) {
            ngks::core::logging::AuditLog::Event(
                "OAUTH_CONNECT_FAIL",
                QString("{\"provider\":\"%1\",\"reason\":\"%2\",\"email\":\"%3\"}")
                    .arg(JsonEscape(providerId), JsonEscape(result.detail), JsonEscape(RedactUsername(email)))
                    .toStdString());
            WriteOAuthConnectProof("FAIL", RedactUsername(email), result.detail, result.brokerProofPath, CountOAuthRows(db));
            return result.exitCode;
        }

        ngks::core::logging::AuditLog::Event(
            "OAUTH_CONNECT_OK",
            QString("{\"provider\":\"%1\",\"email\":\"%2\"}")
                .arg(JsonEscape(providerId), JsonEscape(RedactUsername(email)))
                .toStdString());
        WriteOAuthConnectProof("OK", RedactUsername(email), result.detail, result.brokerProofPath, CountOAuthRows(db));
        return 0;
    }

    if (parser.isSet(resolveOpt)) {
        const QString email = parser.value(emailOpt).trimmed();
        const QString host = parser.value(hostOpt).trimmed();
        const QString username = parser.value(userOpt).trimmed();
        const QString password = parser.value(passOpt);
        const int port = parser.value(portOpt).toInt();
        const bool tls = parser.value(tlsOpt).compare("false", Qt::CaseInsensitive) != 0;
        const bool allowLocalhost = parser.isSet(allowLocalhostOpt);
        const QString effectiveEmail = email.isEmpty() ? username : email;

        if (effectiveEmail.isEmpty() || host.isEmpty()) {
            ngks::core::logging::AuditLog::Event("RESOLVE_FAIL", "{\"reason\":\"missing-args\"}");
            return 20;
        }

        if (!allowLocalhost && IsLocalhostHost(host)) {
            ngks::core::logging::AuditLog::Event(
                "RESOLVE_FAIL",
                QString("{\"reason\":\"localhost-forbidden\",\"host\":\"%1\"}")
                    .arg(JsonEscape(host))
                    .toStdString());
            return 23;
        }

        if (password.isEmpty()) {
            ngks::core::logging::AuditLog::Event("RESOLVE_FAIL", "{\"reason\":\"missing-password\"}");
            return 20;
        }

        ngks::core::logging::AuditLog::Event(
            "RESOLVE_START",
            QString("{\"component\":\"imap\",\"host\":\"%1\",\"port\":%2,\"tls\":%3,\"username\":\"%4\"}")
                .arg(JsonEscape(host))
                .arg(port)
                .arg(tls ? "true" : "false")
                .arg(JsonEscape(RedactUsername(username)))
                .toStdString());

        ngks::core::mail::providers::imap::ResolveRequest request;
        request.email = effectiveEmail;
        request.host = host;
        request.port = port;
        request.tls = tls;
        request.username = username;
        request.password = password;
        request.useXoauth2 = false;
        request.oauthAccessToken.clear();

        ngks::core::mail::providers::imap::ImapProvider provider;
        QVector<ngks::core::mail::providers::imap::ResolvedFolder> folders;
        QString resolveError;
        QString transcriptPath;
        if (!provider.ResolveAccount(request, folders, resolveError, transcriptPath)) {
            const Xoauth2FailMeta xoauth2Meta = ParseXoauth2FailMeta(resolveError);
            if (xoauth2Meta.isXoauth2) {
                ngks::core::logging::AuditLog::Event(
                    "RESOLVE_FAIL",
                    QString("{\"reason\":\"XOAUTH2 failed\",\"imap_phase\":\"%1\",\"imap_tag\":\"%2\",\"imap_last_reply\":\"%3\",\"imap_last_tagged\":\"%4\",\"imap_last_untagged\":\"%5\",\"saw_plus_continuation\":%6,\"xoauth2_shape_ok\":%7,\"xoauth2_shape_reason\":\"%8\",\"auth_bearer_count\":%9,\"first_auth_bearer_pos\":%10,\"second_auth_bearer_pos\":%11,\"raw_len\":%12}")
                        .arg(JsonEscape(xoauth2Meta.imapPhase))
                        .arg(JsonEscape(xoauth2Meta.imapTag))
                        .arg(JsonEscape(xoauth2Meta.imapLastReply))
                        .arg(JsonEscape(xoauth2Meta.imapLastTagged))
                        .arg(JsonEscape(xoauth2Meta.imapLastUntagged))
                        .arg(xoauth2Meta.sawPlusContinuation ? "true" : "false")
                        .arg(xoauth2Meta.shapeOk ? "true" : "false")
                        .arg(JsonEscape(xoauth2Meta.shapeReason))
                        .arg(xoauth2Meta.authBearerCount)
                        .arg(xoauth2Meta.firstAuthBearerPos)
                        .arg(xoauth2Meta.secondAuthBearerPos)
                        .arg(xoauth2Meta.rawLen)
                        .toStdString());
            } else {
                ngks::core::logging::AuditLog::Event("RESOLVE_FAIL", QString("{\"reason\":\"%1\"}").arg(resolveError).toStdString());
            }
            return 21;
        }

        ngks::core::mail::providers::imap::FolderMirrorService mirror;
        int accountId = -1;
        QString mirrorError;
        const QString credentialRef = "DEV_PLAINTEXT";
        if (!mirror.MirrorResolvedAccount(db, request, credentialRef, folders, accountId, mirrorError)) {
            ngks::core::logging::AuditLog::Event("RESOLVE_FAIL", QString("{\"reason\":\"%1\"}").arg(mirrorError).toStdString());
            return 22;
        }

        ngks::core::logging::AuditLog::Event("RESOLVE_WARNING", "{\"credential_ref\":\"DEV_PLAINTEXT\"}");
        ngks::core::logging::AuditLog::Event(
            "RESOLVE_OK",
            QString("{\"account_id\":%1,\"folder_count\":%2,\"transcript\":\"%3\"}")
                .arg(accountId)
                .arg(folders.size())
                .arg(transcriptPath)
                .toStdString());
        return 0;
    }

    mainWindow_.reset(new ngks::ui::MainWindow());
    mainWindow_->show();
    mainWindow_->raise();
    mainWindow_->activateWindow();

    QTimer::singleShot(1000, &qtApp, []() {
    });

    return qtApp.exec();
}

} // namespace ngks::app
#include "providers/generic_imap/GenericImapAuthDriver.hpp"

#include <iostream>
#include <string>

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#endif

#include "core/mail/providers/imap/FolderMirrorService.h"
#include "core/mail/providers/imap/ImapProvider.h"
#include "db/BasicCredentialStore.hpp"
#include "providers/generic_imap/generic_imap_env.hpp"

namespace ngks::providers::generic_imap {

namespace {

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

QString PromptLine(const QString& label)
{
    std::cout << label.toStdString() << " " << std::flush;
    std::string line;
    std::getline(std::cin, line);
    return QString::fromStdString(line).trimmed();
}

QString PromptSecret(const QString& label)
{
    std::cout << label.toStdString() << " " << std::flush;

#if defined(_WIN32)
    if (_isatty(_fileno(stdin))) {
        std::string secret;
        while (true) {
            const int ch = _getch();
            if (ch == '\r' || ch == '\n') {
                std::cout << std::endl;
                break;
            }
            if (ch == 8) {
                if (!secret.empty()) {
                    secret.pop_back();
                }
                continue;
            }
            if (ch == 3) {
                std::cout << std::endl;
                return QString();
            }
            if (ch >= 32 && ch <= 126) {
                secret.push_back(static_cast<char>(ch));
            }
        }
        return QString::fromStdString(secret).trimmed();
    }
#endif

    std::string line;
    std::getline(std::cin, line);
    return QString::fromStdString(line).trimmed();
}

QString PromptWithDefault(const QString& label, const QString& defaultValue, bool isSecret)
{
    const QString entered = isSecret ? PromptSecret(label) : PromptLine(label);
    if (entered.isEmpty()) {
        return defaultValue.trimmed();
    }
    return entered;
}

void PrintConnectFail(const QString& shortReason)
{
    std::cout << "CONNECT_FAIL provider=generic_imap reason=" << shortReason.toStdString() << std::endl;
}

QString ParseOrDefaultBool(const QString& value, bool defaultValue)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return defaultValue ? "true" : "false";
    }
    return trimmed;
}

ngks::auth::AuthResult BeginConnectImpl(
    ngks::core::storage::Db& db,
    const ngks::providers::generic_imap::GenericImapProfile& profile,
    const QString& email,
    bool interactive)
{
    ngks::auth::AuthResult result;
    result.providerId = profile.ProviderId();

    const QString envEmail = env::ReadOptional(env::kEmail).value_or(QString());
    const QString envUsername = env::ReadOptional(env::kUsername).value_or(QString());
    const QString envPassword = env::ReadOptional(env::kPassword).value_or(QString());
    const QString envImapHost = env::ReadOptional(env::kImapHost).value_or(QString());
    const QString envImapPort = env::ReadOptional(env::kImapPort).value_or(QString());
    const QString envImapTls = env::ReadOptional(env::kImapTls).value_or(QString());
    const QString envSmtpHost = env::ReadOptional(env::kSmtpHost).value_or(QString());
    const QString envSmtpPort = env::ReadOptional(env::kSmtpPort).value_or(QString());
    const QString envSmtpTls = env::ReadOptional(env::kSmtpTls).value_or(QString());

    QString effectiveEmail = email.trimmed();
    if (interactive) {
        const QString emailDefault = effectiveEmail.isEmpty() ? envEmail : effectiveEmail;
        effectiveEmail = PromptWithDefault("Email (e.g. user@example.com):", emailDefault, false);
    } else if (effectiveEmail.isEmpty()) {
        effectiveEmail = envEmail;
    }

    const QString username = interactive
        ? PromptWithDefault("Username (e.g. user@example.com):", envUsername.isEmpty() ? effectiveEmail : envUsername, false)
        : envUsername;
    const QString password = interactive
        ? PromptWithDefault("Password:", envPassword, true)
        : envPassword;
    const QString imapHost = interactive
        ? PromptWithDefault("IMAP Host (e.g. imap.example.com):", envImapHost, false)
        : envImapHost;
    const QString imapPortRaw = interactive
        ? PromptWithDefault("IMAP Port (default 993):", envImapPort.isEmpty() ? "993" : envImapPort, false)
        : envImapPort;
    const QString imapTlsRaw = interactive
        ? ParseOrDefaultBool(PromptWithDefault("IMAP TLS (true/false, default true):", envImapTls, false), true)
        : envImapTls;
    const QString smtpHost = interactive
        ? PromptWithDefault("SMTP Host (e.g. smtp.example.com):", envSmtpHost, false)
        : envSmtpHost;
    const QString smtpPortRaw = interactive
        ? PromptWithDefault("SMTP Port (default 587):", envSmtpPort.isEmpty() ? "587" : envSmtpPort, false)
        : envSmtpPort;
    const QString smtpTlsRaw = interactive
        ? ParseOrDefaultBool(PromptWithDefault("SMTP TLS (true/false, default true):", envSmtpTls, false), true)
        : envSmtpTls;

    if (effectiveEmail.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-email");
        }
        result.detail = QString("%1 missing-email").arg(env::LOG_PREFIX);
        result.exitCode = 71;
        return result;
    }
    if (username.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-username");
        }
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kUsername);
        result.exitCode = 71;
        return result;
    }
    if (password.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-password");
        }
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kPassword);
        result.exitCode = 71;
        return result;
    }
    if (imapHost.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-imap-host");
        }
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kImapHost);
        result.exitCode = 71;
        return result;
    }
    if (imapPortRaw.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-imap-port");
        }
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kImapPort);
        result.exitCode = 71;
        return result;
    }
    if (imapTlsRaw.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-imap-tls");
        }
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kImapTls);
        result.exitCode = 71;
        return result;
    }
    if (smtpHost.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-smtp-host");
        }
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kSmtpHost);
        result.exitCode = 71;
        return result;
    }
    if (smtpPortRaw.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-smtp-port");
        }
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kSmtpPort);
        result.exitCode = 71;
        return result;
    }
    if (smtpTlsRaw.isEmpty()) {
        if (interactive) {
            PrintConnectFail("missing-smtp-tls");
        }
        result.detail = QString("%1 missing-env %2").arg(env::LOG_PREFIX, env::kSmtpTls);
        result.exitCode = 71;
        return result;
    }

    int imapPort = 993;
    if (!ParsePort(imapPortRaw, imapPort)) {
        if (interactive) {
            PrintConnectFail("invalid-imap-port");
        }
        result.detail = QString("%1 invalid-port %2").arg(env::LOG_PREFIX, env::kImapPort);
        result.exitCode = 71;
        return result;
    }

    int smtpPort = 587;
    if (!ParsePort(smtpPortRaw, smtpPort)) {
        if (interactive) {
            PrintConnectFail("invalid-smtp-port");
        }
        result.detail = QString("%1 invalid-port %2").arg(env::LOG_PREFIX, env::kSmtpPort);
        result.exitCode = 71;
        return result;
    }

    bool imapTls = true;
    if (!env::ParseBoolText(imapTlsRaw, imapTls)) {
        if (interactive) {
            PrintConnectFail("invalid-imap-tls");
        }
        result.detail = QString("%1 invalid-bool %2").arg(env::LOG_PREFIX, env::kImapTls);
        result.exitCode = 71;
        return result;
    }

    bool smtpTls = true;
    if (!env::ParseBoolText(smtpTlsRaw, smtpTls)) {
        if (interactive) {
            PrintConnectFail("invalid-smtp-tls");
        }
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
    request.password = password;
    request.useXoauth2 = false;
    request.oauthAccessToken.clear();

    ngks::core::mail::providers::imap::ImapProvider imap;
    QVector<ngks::core::mail::providers::imap::ResolvedFolder> folders;
    QString resolveError;
    QString transcriptPath;
    if (!imap.ResolveAccount(request, folders, resolveError, transcriptPath)) {
        if (interactive) {
            PrintConnectFail("imap-check-failed");
        }
        result.detail = QString("%1 imap-check-failed: %2").arg(env::LOG_PREFIX, resolveError);
        result.exitCode = 71;
        return result;
    }

    ngks::db::BasicCredentialRecord cred;
    cred.providerId = profile.ProviderId();
    cred.email = effectiveEmail;
    cred.username = username;
    cred.secret = password;

    QString credErr;
    if (!ngks::db::BasicCredentialStore::Upsert(db, cred, credErr)) {
        if (interactive) {
            PrintConnectFail("credential-store-failed");
        }
        result.detail = QString("%1 credential-store-failed: %2").arg(env::LOG_PREFIX, credErr);
        result.exitCode = 72;
        return result;
    }

    ngks::core::mail::providers::imap::FolderMirrorService mirror;
    int accountId = -1;
    QString mirrorError;
    const QString credentialRef = QString("basic_credentials:%1:%2").arg(profile.ProviderId(), effectiveEmail);
    if (!mirror.MirrorResolvedAccount(
            db,
            request,
            credentialRef,
            folders,
            accountId,
            mirrorError,
            profile.ProviderId())) {
        if (interactive) {
            PrintConnectFail("db-mirror-failed");
        }
        result.detail = QString("%1 db-mirror-failed: %2").arg(env::LOG_PREFIX, mirrorError);
        result.exitCode = 72;
        return result;
    }

    if (interactive) {
        std::cout << "CONNECT_OK provider=generic_imap email="
                  << effectiveEmail.toStdString()
                  << " imap=" << imapHost.toStdString() << ":" << imapPort
                  << " tls=" << (imapTls ? "true" : "false")
                  << std::endl;
    }

    result.ok = true;
    result.exitCode = 0;
    result.detail = QString("%1 connect-ok").arg(env::LOG_PREFIX);
    return result;
}

} // namespace

GenericImapAuthDriver::GenericImapAuthDriver(ngks::core::storage::Db& db)
    : db_(db)
{
}

ngks::auth::AuthResult GenericImapAuthDriver::BeginConnect(const QString& email)
{
    return BeginConnectImpl(db_, profile_, email, false);
}

ngks::auth::AuthResult GenericImapAuthDriver::BeginConnectInteractive(const QString& email)
{
    return BeginConnectImpl(db_, profile_, email, true);
}

} // namespace ngks::providers::generic_imap

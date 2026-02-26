#pragma once

#include <optional>

#include <QByteArray>
#include <QString>

namespace ngks::providers::yahoo_app_password::env {

inline constexpr const char* kPrefix = "NGKS_YAHOO_";
inline constexpr const char* kEmail = "NGKS_YAHOO_EMAIL";
inline constexpr const char* kUsername = "NGKS_YAHOO_USERNAME";
inline constexpr const char* kAppPassword = "NGKS_YAHOO_APP_PASSWORD";
inline constexpr const char* kImapHost = "NGKS_YAHOO_IMAP_HOST";
inline constexpr const char* kImapPort = "NGKS_YAHOO_IMAP_PORT";
inline constexpr const char* kImapTls = "NGKS_YAHOO_IMAP_TLS";
inline constexpr const char* kSmtpHost = "NGKS_YAHOO_SMTP_HOST";
inline constexpr const char* kSmtpPort = "NGKS_YAHOO_SMTP_PORT";
inline constexpr const char* kSmtpTls = "NGKS_YAHOO_SMTP_TLS";

inline constexpr const char* LOG_PREFIX = "[YAHOO_APP_PASSWORD]";

inline std::optional<QString> ReadOptional(const char* key)
{
    const QString value = QString::fromUtf8(qgetenv(key)).trimmed();
    if (value.isEmpty()) {
        return std::nullopt;
    }
    return value;
}

inline bool ParseBoolText(const QString& value, bool& outValue)
{
    const QString lowered = value.trimmed().toLower();
    if (lowered == "true" || lowered == "1" || lowered == "yes") {
        outValue = true;
        return true;
    }
    if (lowered == "false" || lowered == "0" || lowered == "no") {
        outValue = false;
        return true;
    }
    return false;
}

} // namespace ngks::providers::yahoo_app_password::env

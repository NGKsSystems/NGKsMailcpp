#pragma once

#include <optional>

#include <QByteArray>
#include <QString>

namespace ngks::providers::generic_imap::env {

inline constexpr const char* kPrefix = "NGKS_GENERIC_IMAP_";
inline constexpr const char* kEmail = "NGKS_GENERIC_IMAP_EMAIL";
inline constexpr const char* kUsername = "NGKS_GENERIC_IMAP_USERNAME";
inline constexpr const char* kPassword = "NGKS_GENERIC_IMAP_PASSWORD";
inline constexpr const char* kImapHost = "NGKS_GENERIC_IMAP_IMAP_HOST";
inline constexpr const char* kImapPort = "NGKS_GENERIC_IMAP_IMAP_PORT";
inline constexpr const char* kImapTls = "NGKS_GENERIC_IMAP_IMAP_TLS";
inline constexpr const char* kSmtpHost = "NGKS_GENERIC_IMAP_SMTP_HOST";
inline constexpr const char* kSmtpPort = "NGKS_GENERIC_IMAP_SMTP_PORT";
inline constexpr const char* kSmtpTls = "NGKS_GENERIC_IMAP_SMTP_TLS";

inline constexpr const char* LOG_PREFIX = "[GENERIC_IMAP]";

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

} // namespace ngks::providers::generic_imap::env

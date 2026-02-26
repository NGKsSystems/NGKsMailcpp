#pragma once

#include <optional>

#include <QString>

namespace ngks::providers::icloud::env {

inline constexpr const char* kPrefix = "NGKS_ICLOUD_";
inline constexpr const char* kEmail = "NGKS_ICLOUD_EMAIL";
inline constexpr const char* kUsername = "NGKS_ICLOUD_USERNAME";
inline constexpr const char* kAppPassword = "NGKS_ICLOUD_APP_PASSWORD";
inline constexpr const char* kImapHost = "NGKS_ICLOUD_IMAP_HOST";
inline constexpr const char* kImapPort = "NGKS_ICLOUD_IMAP_PORT";
inline constexpr const char* kImapTls = "NGKS_ICLOUD_IMAP_TLS";
inline constexpr const char* kSmtpHost = "NGKS_ICLOUD_SMTP_HOST";
inline constexpr const char* kSmtpPort = "NGKS_ICLOUD_SMTP_PORT";
inline constexpr const char* kSmtpTls = "NGKS_ICLOUD_SMTP_TLS";

inline constexpr const char* LOG_PREFIX = "[ICLOUD]";

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

} // namespace ngks::providers::icloud::env

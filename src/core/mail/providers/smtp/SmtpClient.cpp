#include "core/mail/providers/smtp/SmtpClient.h"

namespace ngks::core::mail::providers::smtp {
bool SmtpClient::Connect(const std::string& endpoint) {
    return !endpoint.empty();
}
}

#include "core/mail/providers/smtp/SmtpProvider.h"

namespace ngks::core::mail::providers::smtp {
std::string SmtpProvider::Name() const {
    return "smtp";
}
}

#pragma once

#include "core/mail/providers/Provider.h"

namespace ngks::core::mail::providers::smtp {
class SmtpProvider : public Provider {
public:
    std::string Name() const override;
};
}

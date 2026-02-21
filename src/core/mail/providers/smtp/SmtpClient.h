#pragma once

#include <string>

namespace ngks::core::mail::providers::smtp {
class SmtpClient {
public:
    bool Connect(const std::string& endpoint);
};
}

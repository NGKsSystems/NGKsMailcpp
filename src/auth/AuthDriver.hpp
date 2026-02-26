#pragma once

#include <QString>

#include "auth/AuthResult.hpp"

namespace ngks::auth {

class AuthDriver {
public:
    virtual ~AuthDriver() = default;
    virtual AuthResult BeginConnect(const QString& email) = 0;
    virtual AuthResult BeginConnectInteractive(const QString& email)
    {
        return BeginConnect(email);
    }
};

} // namespace ngks::auth

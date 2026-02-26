#pragma once

#include "auth/AuthDriver.hpp"
#include "core/storage/Db.h"
#include "providers/yahoo/YahooProfile.hpp"

namespace ngks::providers::yahoo {

class YahooAuthDriver final : public ngks::auth::AuthDriver {
public:
    explicit YahooAuthDriver(ngks::core::storage::Db& db);
    ngks::auth::AuthResult BeginConnect(const QString& email) override;

private:
    ngks::core::storage::Db& db_;
    YahooProfile profile_{};
};

} // namespace ngks::providers::yahoo

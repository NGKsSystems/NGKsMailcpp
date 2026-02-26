#pragma once

#include "auth/AuthDriver.hpp"
#include "core/storage/Db.h"
#include "providers/yahoo_app_password/YahooAppPasswordProfile.hpp"

namespace ngks::providers::yahoo_app_password {

class YahooAppPasswordAuthDriver final : public ngks::auth::AuthDriver {
public:
    explicit YahooAppPasswordAuthDriver(ngks::core::storage::Db& db);
    ngks::auth::AuthResult BeginConnect(const QString& email) override;

private:
    ngks::core::storage::Db& db_;
    YahooAppPasswordProfile profile_{};
};

} // namespace ngks::providers::yahoo_app_password

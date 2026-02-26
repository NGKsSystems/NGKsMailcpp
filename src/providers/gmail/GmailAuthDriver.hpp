#pragma once

#include "auth/AuthDriver.hpp"
#include "core/storage/Db.h"
#include "providers/gmail/GmailProfile.hpp"

namespace ngks::providers::gmail {

class GmailAuthDriver final : public ngks::auth::AuthDriver {
public:
    explicit GmailAuthDriver(ngks::core::storage::Db& db);
    ngks::auth::AuthResult BeginConnect(const QString& email) override;

private:
    ngks::core::storage::Db& db_;
    GmailProfile profile_{};
};

} // namespace ngks::providers::gmail

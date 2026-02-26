#pragma once

#include "auth/AuthDriver.hpp"
#include "core/storage/Db.h"
#include "providers/generic_imap/GenericImapProfile.hpp"

namespace ngks::providers::generic_imap {

class GenericImapAuthDriver final : public ngks::auth::AuthDriver {
public:
    explicit GenericImapAuthDriver(ngks::core::storage::Db& db);
    ngks::auth::AuthResult BeginConnect(const QString& email) override;
    ngks::auth::AuthResult BeginConnectInteractive(const QString& email) override;

private:
    ngks::core::storage::Db& db_;
    GenericImapProfile profile_{};
};

} // namespace ngks::providers::generic_imap

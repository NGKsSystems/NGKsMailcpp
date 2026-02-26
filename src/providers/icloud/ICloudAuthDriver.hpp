#pragma once

#include "auth/AuthDriver.hpp"
#include "core/storage/Db.h"
#include "providers/icloud/ICloudProfile.hpp"

namespace ngks::providers::icloud {

class ICloudAuthDriver final : public ngks::auth::AuthDriver {
public:
    explicit ICloudAuthDriver(ngks::core::storage::Db& db);
    ngks::auth::AuthResult BeginConnect(const QString& email) override;

private:
    ngks::core::storage::Db& db_;
    ICloudProfile profile_{};
};

} // namespace ngks::providers::icloud

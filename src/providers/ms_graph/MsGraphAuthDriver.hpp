#pragma once

#include "auth/AuthDriver.hpp"
#include "core/storage/Db.h"
#include "providers/ms_graph/MsGraphProfile.hpp"

namespace ngks::providers::ms_graph {

class MsGraphAuthDriver final : public ngks::auth::AuthDriver {
public:
    explicit MsGraphAuthDriver(ngks::core::storage::Db& db);
    ngks::auth::AuthResult BeginConnect(const QString& email) override;

private:
    ngks::core::storage::Db& db_;
    MsGraphProfile profile_{};
};

} // namespace ngks::providers::ms_graph

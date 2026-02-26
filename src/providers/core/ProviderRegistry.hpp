#pragma once

#include <memory>
#include <vector>

#include "auth/AuthDriver.hpp"
#include "core/storage/Db.h"
#include "providers/core/ProviderProfile.hpp"

namespace ngks::providers::core {

class ProviderRegistry {
public:
    struct Entry {
        std::unique_ptr<ProviderProfile> profile;
        std::unique_ptr<ngks::auth::AuthDriver> authDriver;
    };

    void Register(Entry entry);
    void RegisterBuiltins(ngks::core::storage::Db& db);

    const ProviderProfile* GetProviderById(const QString& providerId) const;
    ngks::auth::AuthDriver* GetAuthDriverById(const QString& providerId) const;
    const ProviderProfile* DetectProviderByEmail(const QString& email) const;

    const std::vector<Entry>& Entries() const;

private:
    std::vector<Entry> entries_{};
};

} // namespace ngks::providers::core

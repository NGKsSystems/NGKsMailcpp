#pragma once

#include <memory>
#include <vector>

#include "core/mail/providers/Provider.h"

namespace ngks::core::mail::providers {
class ProviderRegistry {
public:
    void Register(std::shared_ptr<Provider> provider);
    std::size_t Count() const;
private:
    std::vector<std::shared_ptr<Provider>> providers_{};
};
}

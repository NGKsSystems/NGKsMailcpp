#include "core/mail/providers/ProviderRegistry.h"

namespace ngks::core::mail::providers {
void ProviderRegistry::Register(std::shared_ptr<Provider> provider) {
    if (provider) {
        providers_.push_back(std::move(provider));
    }
}

std::size_t ProviderRegistry::Count() const {
    return providers_.size();
}
}

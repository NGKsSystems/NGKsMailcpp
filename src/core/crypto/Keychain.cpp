#include "core/crypto/Keychain.h"

namespace ngks::core::crypto {
bool Keychain::StoreSecret(const std::string& key, const std::string& value) {
    return !key.empty() && !value.empty();
}

std::string Keychain::LoadSecret(const std::string& key) const {
    return key.empty() ? std::string{} : std::string{"stub-secret"};
}
}

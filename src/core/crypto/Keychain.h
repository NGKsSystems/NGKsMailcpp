#pragma once

#include <string>

namespace ngks::core::crypto {
class Keychain {
public:
    bool StoreSecret(const std::string& key, const std::string& value);
    std::string LoadSecret(const std::string& key) const;
};
}

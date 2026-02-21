#pragma once

#include <string>

namespace ngks::core::config {
class SettingsStore {
public:
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
};
}

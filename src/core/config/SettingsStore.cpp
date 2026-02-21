#include "core/config/SettingsStore.h"

namespace ngks::core::config {
bool SettingsStore::Load(const std::string& path) {
    return !path.empty();
}

bool SettingsStore::Save(const std::string& path) const {
    return !path.empty();
}
}

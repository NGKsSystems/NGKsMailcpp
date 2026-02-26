#pragma once

#include <QString>

#include "providers/core/ProviderTypes.hpp"

namespace ngks::resolve {

class ProviderDiscovery {
public:
    static bool MatchesEmail(const QString& email, const ngks::providers::core::DiscoveryRules& rules);
};

} // namespace ngks::resolve

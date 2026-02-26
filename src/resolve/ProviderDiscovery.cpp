#include "resolve/ProviderDiscovery.hpp"

namespace ngks::resolve {

bool ProviderDiscovery::MatchesEmail(const QString& email, const ngks::providers::core::DiscoveryRules& rules)
{
    const QString trimmed = email.trimmed().toLower();
    const int at = trimmed.indexOf('@');
    if (at < 0 || at >= trimmed.size() - 1) {
        return false;
    }

    const QString domain = trimmed.mid(at + 1);

    for (const QString& exact : rules.explicitDomains) {
        if (domain == exact.trimmed().toLower()) {
            return true;
        }
    }

    for (const QString& suffix : rules.domainSuffixes) {
        const QString s = suffix.trimmed().toLower();
        if (!s.isEmpty() && domain.endsWith(s)) {
            return true;
        }
    }

    return false;
}

} // namespace ngks::resolve

#include "providers/core/ProviderRegistry.hpp"

#include "providers/gmail/GmailAuthDriver.hpp"
#include "providers/gmail/GmailProfile.hpp"
#include "providers/generic_imap/GenericImapAuthDriver.hpp"
#include "providers/generic_imap/GenericImapProfile.hpp"
#include "providers/icloud/ICloudAuthDriver.hpp"
#include "providers/icloud/ICloudProfile.hpp"
#include "providers/ms_graph/MsGraphAuthDriver.hpp"
#include "providers/ms_graph/MsGraphProfile.hpp"
#include "providers/yahoo_app_password/YahooAppPasswordAuthDriver.hpp"
#include "providers/yahoo_app_password/YahooAppPasswordProfile.hpp"
#include "providers/yahoo/YahooAuthDriver.hpp"
#include "providers/yahoo/YahooProfile.hpp"
#include "resolve/ProviderDiscovery.hpp"

namespace ngks::providers::core {

void ProviderRegistry::Register(Entry entry)
{
    if (!entry.profile || !entry.authDriver) {
        return;
    }
    entries_.push_back(std::move(entry));
}

void ProviderRegistry::RegisterBuiltins(ngks::core::storage::Db& db)
{
    entries_.clear();

    Register(Entry{
        std::make_unique<ngks::providers::gmail::GmailProfile>(),
        std::make_unique<ngks::providers::gmail::GmailAuthDriver>(db),
    });

    Register(Entry{
        std::make_unique<ngks::providers::generic_imap::GenericImapProfile>(),
        std::make_unique<ngks::providers::generic_imap::GenericImapAuthDriver>(db),
    });

    Register(Entry{
        std::make_unique<ngks::providers::icloud::ICloudProfile>(),
        std::make_unique<ngks::providers::icloud::ICloudAuthDriver>(db),
    });

    Register(Entry{
        std::make_unique<ngks::providers::ms_graph::MsGraphProfile>(),
        std::make_unique<ngks::providers::ms_graph::MsGraphAuthDriver>(db),
    });

    Register(Entry{
        std::make_unique<ngks::providers::yahoo_app_password::YahooAppPasswordProfile>(),
        std::make_unique<ngks::providers::yahoo_app_password::YahooAppPasswordAuthDriver>(db),
    });

    Register(Entry{
        std::make_unique<ngks::providers::yahoo::YahooProfile>(),
        std::make_unique<ngks::providers::yahoo::YahooAuthDriver>(db),
    });
}

const ProviderProfile* ProviderRegistry::GetProviderById(const QString& providerId) const
{
    const QString wanted = providerId.trimmed();
    for (const Entry& entry : entries_) {
        if (entry.profile->ProviderId().compare(wanted, Qt::CaseInsensitive) == 0) {
            return entry.profile.get();
        }
    }
    return nullptr;
}

ngks::auth::AuthDriver* ProviderRegistry::GetAuthDriverById(const QString& providerId) const
{
    const QString wanted = providerId.trimmed();
    for (const Entry& entry : entries_) {
        if (entry.profile->ProviderId().compare(wanted, Qt::CaseInsensitive) == 0) {
            return entry.authDriver.get();
        }
    }
    return nullptr;
}

const ProviderProfile* ProviderRegistry::DetectProviderByEmail(const QString& email) const
{
    for (const Entry& entry : entries_) {
        if (ngks::resolve::ProviderDiscovery::MatchesEmail(email, entry.profile->Discovery())) {
            return entry.profile.get();
        }
    }
    return nullptr;
}

const std::vector<ProviderRegistry::Entry>& ProviderRegistry::Entries() const
{
    return entries_;
}

} // namespace ngks::providers::core

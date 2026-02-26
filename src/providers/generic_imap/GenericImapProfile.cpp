#include "providers/generic_imap/GenericImapProfile.hpp"

#include "providers/generic_imap/generic_imap_env.hpp"

namespace ngks::providers::generic_imap {

QString GenericImapProfile::ProviderId() const { return "generic_imap"; }
QString GenericImapProfile::DisplayName() const { return "Generic IMAP"; }

ngks::providers::core::DiscoveryRules GenericImapProfile::Discovery() const
{
    return {};
}

ngks::providers::core::AuthType GenericImapProfile::GetAuthType() const
{
    return ngks::providers::core::AuthType::Basic;
}

ngks::providers::core::OAuthEndpoints GenericImapProfile::OAuth() const
{
    return {};
}

QStringList GenericImapProfile::Scopes() const
{
    return {};
}

QString GenericImapProfile::ImapMechanism() const { return "LOGIN"; }
QString GenericImapProfile::SmtpMechanism() const { return "LOGIN"; }
QString GenericImapProfile::EnvNamespace() const { return ngks::providers::generic_imap::env::kPrefix; }

} // namespace ngks::providers::generic_imap

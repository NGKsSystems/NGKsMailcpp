#include "providers/icloud/ICloudProfile.hpp"

#include "providers/icloud/icloud_env.hpp"

namespace ngks::providers::icloud {

QString ICloudProfile::ProviderId() const { return "icloud"; }
QString ICloudProfile::DisplayName() const { return "iCloud"; }

ngks::providers::core::DiscoveryRules ICloudProfile::Discovery() const
{
    ngks::providers::core::DiscoveryRules rules;
    rules.explicitDomains = {"icloud.com", "me.com", "mac.com"};
    rules.domainSuffixes = {"icloud.com", "me.com", "mac.com"};
    rules.mxHints = {"icloud", "me.com", "mac.com"};
    return rules;
}

ngks::providers::core::AuthType ICloudProfile::GetAuthType() const
{
    return ngks::providers::core::AuthType::Basic;
}

ngks::providers::core::OAuthEndpoints ICloudProfile::OAuth() const
{
    return {};
}

QStringList ICloudProfile::Scopes() const
{
    return {};
}

QString ICloudProfile::ImapMechanism() const { return "LOGIN"; }
QString ICloudProfile::SmtpMechanism() const { return "LOGIN"; }
QString ICloudProfile::EnvNamespace() const { return ngks::providers::icloud::env::kPrefix; }

} // namespace ngks::providers::icloud

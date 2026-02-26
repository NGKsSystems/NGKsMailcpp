#include "providers/yahoo_app_password/YahooAppPasswordProfile.hpp"

#include "providers/yahoo_app_password/yahoo_env.hpp"

namespace ngks::providers::yahoo_app_password {

QString YahooAppPasswordProfile::ProviderId() const { return "yahoo_app_password"; }
QString YahooAppPasswordProfile::DisplayName() const { return "Yahoo App Password"; }

ngks::providers::core::DiscoveryRules YahooAppPasswordProfile::Discovery() const
{
    ngks::providers::core::DiscoveryRules rules;
    rules.explicitDomains = {"yahoo.com", "ymail.com", "rocketmail.com"};
    rules.domainSuffixes = {"yahoo.com", "ymail.com", "rocketmail.com"};
    rules.mxHints = {"yahoodns", "yahoodns.net", "yahoo"};
    return rules;
}

ngks::providers::core::AuthType YahooAppPasswordProfile::GetAuthType() const
{
    return ngks::providers::core::AuthType::Basic;
}

ngks::providers::core::OAuthEndpoints YahooAppPasswordProfile::OAuth() const
{
    return {};
}

QStringList YahooAppPasswordProfile::Scopes() const
{
    return {};
}

QString YahooAppPasswordProfile::ImapMechanism() const { return "LOGIN"; }
QString YahooAppPasswordProfile::SmtpMechanism() const { return "LOGIN"; }
QString YahooAppPasswordProfile::EnvNamespace() const { return ngks::providers::yahoo_app_password::env::kPrefix; }

} // namespace ngks::providers::yahoo_app_password

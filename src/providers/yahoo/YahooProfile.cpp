#include "providers/yahoo/YahooProfile.hpp"

#include "providers/yahoo/yahoo_env.hpp"

namespace ngks::providers::yahoo {

QString YahooProfile::ProviderId() const { return "yahoo"; }
QString YahooProfile::DisplayName() const { return "Yahoo"; }

ngks::providers::core::DiscoveryRules YahooProfile::Discovery() const
{
    ngks::providers::core::DiscoveryRules rules;
    rules.explicitDomains = {"yahoo.com", "ymail.com", "rocketmail.com"};
    rules.domainSuffixes = {"yahoo.com", "ymail.com", "rocketmail.com"};
    rules.mxHints = {"yahoodns", "yahoodns.net", "yahoo"};
    return rules;
}

ngks::providers::core::AuthType YahooProfile::GetAuthType() const
{
    return ngks::providers::core::AuthType::OAuthPKCE;
}

ngks::providers::core::OAuthEndpoints YahooProfile::OAuth() const
{
    ngks::providers::core::OAuthEndpoints endpoints;
    endpoints.authorizeUrl = "https://api.login.yahoo.com/oauth2/request_auth";
    endpoints.tokenUrl = "https://api.login.yahoo.com/oauth2/get_token";
    return endpoints;
}

QStringList YahooProfile::Scopes() const
{
    return {"mail-r", "mail-w", "openid", "offline_access"};
}

QString YahooProfile::ImapMechanism() const { return "XOAUTH2"; }
QString YahooProfile::SmtpMechanism() const { return "XOAUTH2"; }
QString YahooProfile::EnvNamespace() const { return ngks::providers::yahoo::env::kPrefix; }

} // namespace ngks::providers::yahoo

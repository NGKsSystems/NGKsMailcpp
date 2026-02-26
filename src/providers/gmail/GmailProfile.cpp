#include "providers/gmail/GmailProfile.hpp"

#include "providers/gmail/gmail_env.hpp"

namespace ngks::providers::gmail {

QString GmailProfile::ProviderId() const { return "gmail"; }
QString GmailProfile::DisplayName() const { return "Gmail"; }

ngks::providers::core::DiscoveryRules GmailProfile::Discovery() const
{
    ngks::providers::core::DiscoveryRules rules;
    rules.explicitDomains = {"gmail.com", "googlemail.com"};
    rules.domainSuffixes = {"gmail.com", "googlemail.com"};
    rules.mxHints = {"google"};
    return rules;
}

ngks::providers::core::AuthType GmailProfile::GetAuthType() const
{
    return ngks::providers::core::AuthType::OAuthPKCE;
}

ngks::providers::core::OAuthEndpoints GmailProfile::OAuth() const
{
    ngks::providers::core::OAuthEndpoints endpoints;
    endpoints.authorizeUrl = "https://accounts.google.com/o/oauth2/v2/auth";
    endpoints.tokenUrl = "https://oauth2.googleapis.com/token";
    return endpoints;
}

QStringList GmailProfile::Scopes() const
{
    return {"https://mail.google.com/"};
}

QString GmailProfile::ImapMechanism() const { return "XOAUTH2"; }
QString GmailProfile::SmtpMechanism() const { return "XOAUTH2"; }
QString GmailProfile::EnvNamespace() const { return ngks::providers::gmail::env::kPrefix; }

} // namespace ngks::providers::gmail

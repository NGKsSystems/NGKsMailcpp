#include "providers/ms_graph/MsGraphProfile.hpp"

#include "providers/ms_graph/ms_graph_env.hpp"

namespace ngks::providers::ms_graph {

QString MsGraphProfile::ProviderId() const { return "ms_graph"; }
QString MsGraphProfile::DisplayName() const { return "Microsoft Graph"; }

ngks::providers::core::DiscoveryRules MsGraphProfile::Discovery() const
{
    ngks::providers::core::DiscoveryRules rules;
    rules.explicitDomains = {"outlook.com", "hotmail.com", "live.com", "office365.com", "microsoft.com"};
    rules.domainSuffixes = {"outlook.com", "hotmail.com", "live.com", "office365.com", "microsoft.com"};
    rules.mxHints = {"outlook", "office365", "microsoft"};
    return rules;
}

ngks::providers::core::AuthType MsGraphProfile::GetAuthType() const
{
    return ngks::providers::core::AuthType::OAuthPKCE;
}

ngks::providers::core::OAuthEndpoints MsGraphProfile::OAuth() const
{
    ngks::providers::core::OAuthEndpoints endpoints;
    endpoints.authorizeUrl = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize";
    endpoints.tokenUrl = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
    endpoints.audience = "https://graph.microsoft.com";
    endpoints.resource = "https://graph.microsoft.com";
    return endpoints;
}

QStringList MsGraphProfile::Scopes() const
{
    return {"offline_access", "openid", "email", "IMAP.AccessAsUser.All", "SMTP.Send"};
}

QString MsGraphProfile::ImapMechanism() const { return "XOAUTH2"; }
QString MsGraphProfile::SmtpMechanism() const { return "XOAUTH2"; }
QString MsGraphProfile::EnvNamespace() const { return ngks::providers::ms_graph::env::kPrefix; }

} // namespace ngks::providers::ms_graph

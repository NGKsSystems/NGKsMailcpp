#pragma once

#include <QString>
#include <QStringList>

namespace ngks::providers::core {

enum class AuthType {
    Basic,
    OAuthPKCE,
    OAuthDeviceCode,
};

struct DiscoveryRules {
    QStringList domainSuffixes;
    QStringList explicitDomains;
    QStringList mxHints;
};

struct OAuthEndpoints {
    QString authorizeUrl;
    QString tokenUrl;
    QString deviceUrl;
    QString audience;
    QString resource;
};

} // namespace ngks::providers::core

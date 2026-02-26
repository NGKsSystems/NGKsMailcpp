#pragma once

#include <QString>
#include <QStringList>

#include "providers/core/ProviderTypes.hpp"

namespace ngks::providers::core {

class ProviderProfile {
public:
    virtual ~ProviderProfile() = default;

    virtual QString ProviderId() const = 0;
    virtual QString DisplayName() const = 0;
    virtual DiscoveryRules Discovery() const = 0;
    virtual AuthType GetAuthType() const = 0;
    virtual OAuthEndpoints OAuth() const = 0;
    virtual QStringList Scopes() const = 0;
    virtual QString ImapMechanism() const = 0;
    virtual QString SmtpMechanism() const = 0;
    virtual QString EnvNamespace() const = 0;
};

} // namespace ngks::providers::core

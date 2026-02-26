#pragma once

#include "providers/core/ProviderProfile.hpp"

namespace ngks::providers::icloud {

class ICloudProfile final : public ngks::providers::core::ProviderProfile {
public:
    QString ProviderId() const override;
    QString DisplayName() const override;
    ngks::providers::core::DiscoveryRules Discovery() const override;
    ngks::providers::core::AuthType GetAuthType() const override;
    ngks::providers::core::OAuthEndpoints OAuth() const override;
    QStringList Scopes() const override;
    QString ImapMechanism() const override;
    QString SmtpMechanism() const override;
    QString EnvNamespace() const override;
};

} // namespace ngks::providers::icloud

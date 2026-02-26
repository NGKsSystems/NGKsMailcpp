#include "providers/yahoo/YahooAuthDriver.hpp"

namespace ngks::providers::yahoo {

YahooAuthDriver::YahooAuthDriver(ngks::core::storage::Db& db)
    : db_(db)
{
    (void)db_;
}

ngks::auth::AuthResult YahooAuthDriver::BeginConnect(const QString& email)
{
    ngks::auth::AuthResult result;
    result.providerId = profile_.ProviderId();
    result.detail = QString("%1 connect-not-implemented email=%2")
                        .arg(kLogPrefix, email.trimmed());
    result.exitCode = 71;
    return result;
}

} // namespace ngks::providers::yahoo

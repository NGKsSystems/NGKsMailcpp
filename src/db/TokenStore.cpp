#include "db/TokenStore.hpp"

#include "core/auth/OAuthStore.h"

namespace ngks::db {

bool TokenStore::Store(ngks::core::storage::Db& db, const TokenRecord& rec, QString& outError)
{
    ngks::core::auth::OAuthTokenRecord row;
    row.provider = rec.providerId.trimmed();
    row.email = rec.email.trimmed();
    row.refreshToken = rec.refreshToken;
    row.accessToken = rec.accessToken;
    row.expiresAtUtc = rec.expiresAtUtc;
    row.clientId = rec.clientId;
    row.clientSecret = rec.clientSecret;
    return ngks::core::auth::OAuthStore::UpsertToken(db, row, outError);
}

bool TokenStore::Load(ngks::core::storage::Db& db, const QString& providerId, const QString& email, TokenRecord& outRec, QString& outError)
{
    outError.clear();
    outRec = TokenRecord{};

    QString refreshToken;
    if (!ngks::core::auth::OAuthStore::GetRefreshToken(db, providerId, email, refreshToken, outError)) {
        return false;
    }

    outRec.providerId = providerId.trimmed();
    outRec.email = email.trimmed();
    outRec.refreshToken = refreshToken;
    return true;
}

} // namespace ngks::db

#pragma once

#include <QString>

#include "core/storage/Db.h"

namespace ngks::db {

struct TokenRecord {
    QString providerId;
    QString email;
    QString refreshToken;
    QString accessToken;
    qint64 expiresAtUtc = 0;
    QString clientId;
    QString clientSecret;
};

class TokenStore {
public:
    static bool Store(ngks::core::storage::Db& db, const TokenRecord& rec, QString& outError);
    static bool Load(ngks::core::storage::Db& db, const QString& providerId, const QString& email, TokenRecord& outRec, QString& outError);
};

} // namespace ngks::db

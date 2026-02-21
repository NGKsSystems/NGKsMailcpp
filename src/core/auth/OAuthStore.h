// src/core/auth/OAuthStore.h
#pragma once

#include <QString>

#include "core/storage/Db.h"

namespace ngks::core::auth {

struct OAuthTokenRecord {
    QString provider;        // e.g. "gmail"
    QString email;           // full email
    QString refreshToken;    // stored in DB
    QString accessToken;     // optional
    qint64  expiresAtUtc = 0; // unix seconds; 0 = unknown/none
};

class OAuthStore {
public:
    // Creates oauth_tokens table if missing.
    // Returns true on success; false sets outError.
    static bool EnsureTables(ngks::core::storage::Db& db, QString& outError);

    // Insert or update token row for (provider,email).
    // Returns true on success; false sets outError.
    static bool UpsertToken(ngks::core::storage::Db& db, const OAuthTokenRecord& rec, QString& outError);

    // Fetch refresh token for (provider,email).
    // Returns true if found; false if not found or error (check outError to distinguish).
    static bool GetRefreshToken(ngks::core::storage::Db& db,
                               const QString& provider,
                               const QString& email,
                               QString& outRefreshToken,
                               QString& outError);
};

} // namespace ngks::core::auth
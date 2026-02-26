#pragma once

#include <QString>

#include "core/storage/Db.h"

namespace ngks::db {

struct BasicCredentialRecord {
    QString providerId;
    QString email;
    QString username;
    QString secret;
};

class BasicCredentialStore {
public:
    static bool Upsert(ngks::core::storage::Db& db, const BasicCredentialRecord& rec, QString& outError);
};

} // namespace ngks::db

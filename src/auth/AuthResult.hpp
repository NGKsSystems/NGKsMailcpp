#pragma once

#include <QString>

namespace ngks::auth {

struct AuthResult {
    bool ok = false;
    int exitCode = 1;
    QString providerId;
    QString detail;
    QString brokerProofPath;
    QString accessToken;
    QString refreshToken;
    qint64 expiresAtUtc = 0;
};

} // namespace ngks::auth

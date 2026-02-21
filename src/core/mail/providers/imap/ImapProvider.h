#pragma once

#include <QVector>
#include <QString>

#include "core/mail/providers/Provider.h"

namespace ngks::core::mail::providers::imap {

struct ResolveRequest {
    QString email;
    QString host;
    int port = 993;
    bool tls = true;
    QString username;
    QString password;
    bool useXoauth2 = false;
    QString oauthAccessToken;
};

struct ResolvedFolder {
    QString remoteName;
    QString displayName;
    QString delimiter;
    QString attrsJson;
    QString specialUse;
};

class ImapProvider : public Provider {
public:
    std::string Name() const override;
    bool ResolveAccount(const ResolveRequest& request, QVector<ResolvedFolder>& outFolders, QString& outError, QString& transcriptPath);
};

}

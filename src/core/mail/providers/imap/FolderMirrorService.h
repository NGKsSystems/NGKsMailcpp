#pragma once

#include <QString>
#include <QVector>

#include "core/mail/providers/imap/ImapProvider.h"

namespace ngks::core::storage {
class Db;
}

namespace ngks::core::mail::providers::imap {

class FolderMirrorService {
public:
    bool MirrorResolvedAccount(
        ngks::core::storage::Db& db,
        const ResolveRequest& request,
        const QString& credentialRef,
        const QVector<ResolvedFolder>& folders,
        int& outAccountId,
        QString& outError);
};

}

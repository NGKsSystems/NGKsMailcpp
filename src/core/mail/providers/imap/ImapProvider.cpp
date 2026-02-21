// src/core/mail/providers/imap/ImapProvider.cpp
#include "core/mail/providers/imap/ImapProvider.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "core/mail/providers/imap/ImapClient.h"
#include "platform/common/Paths.h"

namespace ngks::core::mail::providers::imap {

namespace {

QString MakeTag(int index)
{
    return QString("A%1").arg(index, 3, 10, QChar('0'));
}

QString SanitizeForPath(const QString& value)
{
    QString out = value;
    out.replace('@', '_');
    out.replace('.', '_');
    out.replace(QRegularExpression("[^A-Za-z0-9_\\-]"), "_");
    return out;
}

QString EscapeQuoted(const QString& value)
{
    QString out = value;
    out.replace("\\", "\\\\");
    out.replace("\"", "\\\"");
    return out;
}

QVector<ResolvedFolder> ParseListLines(const QStringList& lines)
{
    QVector<ResolvedFolder> folders;
    const QRegularExpression re(
        "^\\*\\s+(?:LIST|XLIST)\\s+\\(([^)]*)\\)\\s+\"([^\"]*)\"\\s+(.+)$",
        QRegularExpression::CaseInsensitiveOption);

    for (const QString& line : lines) {
        const QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch()) continue;

        QString attrs = m.captured(1).trimmed();
        QString delimiter = m.captured(2);
        QString mailbox = m.captured(3).trimmed();
        if (mailbox.startsWith('"') && mailbox.endsWith('"') && mailbox.size() >= 2) {
            mailbox = mailbox.mid(1, mailbox.size() - 2);
        }
        if (mailbox.isEmpty()) continue;

        QString specialUse;
        if (attrs.contains("\\Inbox", Qt::CaseInsensitive)) specialUse = "\\Inbox";
        else if (attrs.contains("\\Sent", Qt::CaseInsensitive)) specialUse = "\\Sent";
        else if (attrs.contains("\\Drafts", Qt::CaseInsensitive)) specialUse = "\\Drafts";
        else if (attrs.contains("\\Archive", Qt::CaseInsensitive)) specialUse = "\\Archive";
        else if (attrs.contains("\\Trash", Qt::CaseInsensitive)) specialUse = "\\Trash";
        else if (attrs.contains("\\Junk", Qt::CaseInsensitive)) specialUse = "\\Junk";

        QJsonObject attrsObj;
        QJsonArray attrsArray;
        for (const QString& attr : attrs.split(' ', Qt::SkipEmptyParts)) {
            attrsArray.push_back(attr);
        }
        attrsObj.insert("attrs", attrsArray);

        ResolvedFolder f;
        f.remoteName = mailbox;
        const QStringList parts = mailbox.split(delimiter.isEmpty() ? '/' : delimiter[0], Qt::SkipEmptyParts);
        f.displayName = parts.isEmpty() ? mailbox : parts.last();
        f.delimiter = delimiter;
        f.attrsJson = QString::fromUtf8(QJsonDocument(attrsObj).toJson(QJsonDocument::Compact));
        f.specialUse = specialUse;
        folders.push_back(f);
    }

    return folders;
}

bool IsTaggedOk(const QStringList& lines, const QString& tag)
{
    for (const QString& line : lines) {
        if (line.startsWith(tag + " OK", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

// Gmail often sends: "+ <base64(json)>"
QString TryDecodeB64JsonFromPlusLine(const QString& line)
{
    // Accept "+ <b64>" or "+<space><b64>"
    QString trimmed = line.trimmed();
    if (!trimmed.startsWith("+")) return QString();
    QString b64 = trimmed.mid(1).trimmed();
    if (b64.isEmpty()) return QString();

    QByteArray raw = QByteArray::fromBase64(b64.toLatin1());
    if (raw.isEmpty()) return QString();
    return QString::fromUtf8(raw);
}

QString SanitizeReplyForAudit(const QString& value)
{
    QString out = value;
    out.replace(QRegularExpression("[\\r\\n\\t]"), " ");
    out.replace(QRegularExpression("Bearer\\s+[A-Za-z0-9\\-\\._~\\+/=]+", QRegularExpression::CaseInsensitiveOption),
                "Bearer <REDACTED>");
    out = out.simplified();
    if (out.size() > 200) {
        out = out.left(200);
    }
    return out;
}

QByteArray BuildXoauth2Raw(const QString& user, const QString& accessToken)
{
    QString token = accessToken.trimmed();
    const QString bearerPrefix = "Bearer ";
    while (token.startsWith(bearerPrefix, Qt::CaseInsensitive)) {
        token = token.mid(bearerPrefix.size()).trimmed();
    }
    token.replace("auth=Bearer ", "", Qt::CaseInsensitive);

    QByteArray raw;
    raw.reserve(user.toUtf8().size() + token.toUtf8().size() + 32);
    raw += "user=";
    raw += user.toUtf8();
    raw.append(char(0x01));
    raw += "auth=Bearer ";
    raw += token.toUtf8();
    raw.append(char(0x01));
    raw.append(char(0x01));
    return raw;
}

int CountSubstr(const QByteArray& s, const QByteArray& sub)
{
    if (sub.isEmpty()) {
        return 0;
    }

    int count = 0;
    int pos = 0;
    while (true) {
        pos = s.indexOf(sub, pos);
        if (pos < 0) {
            break;
        }
        ++count;
        pos += sub.size();
    }
    return count;
}

void AnalyzeAuthBearerMarker(const QByteArray& raw, int& count, int& firstPos, int& secondPos)
{
    const QByteArray marker("auth=Bearer ");
    count = CountSubstr(raw, marker);
    firstPos = raw.indexOf(marker);
    secondPos = (firstPos >= 0) ? raw.indexOf(marker, firstPos + marker.size()) : -1;
}

bool ValidateXoauth2Shape(const QByteArray& raw,
                         const QString& user,
                         int authBearerCount,
                         QString& outReason)
{
    outReason.clear();

    if (authBearerCount == 0) {
        outReason = "missing-auth-bearer";
        return false;
    }
    if (authBearerCount != 1) {
        outReason = "invalid-bearer-count";
        return false;
    }

    QByteArray expectedPrefix;
    expectedPrefix += "user=";
    expectedPrefix += user.toUtf8();
    expectedPrefix.append(char(0x01));
    expectedPrefix += "auth=Bearer ";
    if (!raw.startsWith(expectedPrefix)) {
        outReason = "missing-prefix";
        return false;
    }

    if (raw.size() < 2 || raw.at(raw.size() - 2) != char(0x01) || raw.at(raw.size() - 1) != char(0x01)) {
        outReason = "missing-terminator";
        return false;
    }

    return true;
}

QString BuildXoauth2FailError(const QString& lastReply,
                              bool shapeOk,
                              const QString& shapeReason,
                              int authBearerCount,
                              int firstAuthBearerPos,
                              int secondAuthBearerPos,
                              int rawLen,
                              const QString& imapPhase,
                              const QString& imapTag,
                              const QString& imapLastTagged,
                              const QString& imapLastUntagged,
                              bool sawPlusContinuation)
{
    return QString("XOAUTH2 failed|imap_last_reply=%1|xoauth2_shape_ok=%2|xoauth2_shape_reason=%3|auth_bearer_count=%4|first_auth_bearer_pos=%5|second_auth_bearer_pos=%6|raw_len=%7|imap_phase=%8|imap_tag=%9|imap_last_tagged=%10|imap_last_untagged=%11|saw_plus_continuation=%12")
        .arg(SanitizeReplyForAudit(lastReply),
             shapeOk ? "true" : "false",
             shapeReason.isEmpty() ? "ok" : shapeReason)
        .arg(authBearerCount)
        .arg(firstAuthBearerPos)
        .arg(secondAuthBearerPos)
        .arg(rawLen)
        .arg(SanitizeReplyForAudit(imapPhase))
        .arg(SanitizeReplyForAudit(imapTag))
        .arg(SanitizeReplyForAudit(imapLastTagged))
        .arg(SanitizeReplyForAudit(imapLastUntagged))
        .arg(sawPlusContinuation ? "true" : "false");
}

void ExtractLastTaggedAndUntagged(const QStringList& lines,
                                  const QString& tag,
                                  QString& outTagged,
                                  QString& outUntagged)
{
    outTagged.clear();
    outUntagged.clear();
    for (const QString& line : lines) {
        if (line.startsWith(tag, Qt::CaseInsensitive)) {
            outTagged = line;
        } else if (line.startsWith("*")) {
            outUntagged = line;
        }
    }
}

}

std::string ImapProvider::Name() const
{
    return "imap";
}

bool ImapProvider::ResolveAccount(const ResolveRequest& request,
                                  QVector<ResolvedFolder>& outFolders,
                                  QString& outError,
                                  QString& transcriptPath)
{
    outFolders.clear();
    outError.clear();

    const auto imapLogRoot =
        QString::fromStdString((ngks::platform::common::ArtifactsDir() / "logs" / "imap").string());
    QDir().mkpath(imapLogRoot);

    transcriptPath = QString("%1/%2_%3.txt")
        .arg(imapLogRoot,
             SanitizeForPath(request.email),
             QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz"));

    ImapClient client;
    if (!client.Connect(request.host, request.port, request.tls, transcriptPath)) {
        outError = QString("%1; socket_error=%2; socket_error_string=%3; encrypted=%4")
                       .arg(client.LastError())
                       .arg(client.LastSocketErrorCode())
                       .arg(client.LastSocketErrorString())
                       .arg(client.WasEncrypted() ? "true" : "false");
        return false;
    }

    client.ReadGreeting();

    int commandIndex = 1;

    const QString capabilityTag = MakeTag(commandIndex++);
    if (!client.SendCommand(capabilityTag + " CAPABILITY")) {
        outError = client.LastError();
        client.Disconnect();
        return false;
    }

    const QStringList capabilityLines = client.ReadResponseUntilTag(capabilityTag);
    if (!IsTaggedOk(capabilityLines, capabilityTag)) {
        outError = "CAPABILITY failed";
        client.Disconnect();
        return false;
    }

    QString capabilityLine;
    for (const QString& line : capabilityLines) {
        if (line.startsWith("* CAPABILITY", Qt::CaseInsensitive)) {
            capabilityLine = line;
            break;
        }
    }

    const bool hasNamespace = capabilityLine.contains("NAMESPACE", Qt::CaseInsensitive);
    const bool hasSpecialUse = capabilityLine.contains("SPECIAL-USE", Qt::CaseInsensitive);

    // --- AUTH ---
    const QString authTag = MakeTag(commandIndex++);

    if (request.useXoauth2) {
        // IMPORTANT: Gmail IMAP ties XOAUTH2 to the authenticated user.
        // Use request.username for user=... (not request.email).
        const QString xoauth2User = request.username.isEmpty() ? request.email : request.username;
        const QByteArray xoauth2Raw = BuildXoauth2Raw(xoauth2User, request.oauthAccessToken);
        int authBearerCount = 0;
        int firstAuthBearerPos = -1;
        int secondAuthBearerPos = -1;
        AnalyzeAuthBearerMarker(xoauth2Raw, authBearerCount, firstAuthBearerPos, secondAuthBearerPos);
        const int xoauth2RawLen = xoauth2Raw.size();
        QString xoauth2ShapeReason;
        const bool xoauth2ShapeOk = ValidateXoauth2Shape(xoauth2Raw, xoauth2User, authBearerCount, xoauth2ShapeReason);
        const QString xoauth2B64 = QString::fromLatin1(xoauth2Raw.toBase64());

        QString imapPhase = "post-capability";
        const QString imapTag = authTag;
        QString imapLastTagged;
        QString imapLastUntagged;
        bool sawPlusContinuation = false;

        // Safer flow: do NOT use SASL-IR here. Start AUTHENTICATE XOAUTH2, wait for '+', then send response.
        const QString beginCmd = QString("%1 AUTHENTICATE XOAUTH2").arg(authTag);
        const QString beginCmdRedacted = beginCmd;

        if (!client.SendCommand(beginCmd, beginCmdRedacted)) {
            outError = client.LastError();
            client.Disconnect();
            return false;
        }
        imapPhase = "post-authenticate-command";

        // Expect '+' continuation
        const QString first = client.ReadLine();
        if (first.isEmpty()) {
            outError = client.LastError().isEmpty() ? "XOAUTH2: missing server continuation" : client.LastError();
            client.Disconnect();
            return false;
        }

        if (!first.trimmed().startsWith("+")) {
            // Server already tagged response (OK/NO/BAD)
            QStringList rest = client.ReadResponseUntilTag(authTag);
            rest.prepend(first);
            ExtractLastTaggedAndUntagged(rest, authTag, imapLastTagged, imapLastUntagged);
            QString lastReply = first;
            for (const QString& line : rest) {
                if (line.startsWith(authTag, Qt::CaseInsensitive)) {
                    lastReply = line;
                    break;
                }
            }
            outError = BuildXoauth2FailError(lastReply,
                                            xoauth2ShapeOk,
                                            xoauth2ShapeReason,
                                            authBearerCount,
                                            firstAuthBearerPos,
                                            secondAuthBearerPos,
                                            xoauth2RawLen,
                                            imapPhase,
                                            imapTag,
                                            imapLastTagged,
                                            imapLastUntagged,
                                            sawPlusContinuation);
            client.Disconnect();
            return false;
        }
        sawPlusContinuation = true;
        imapPhase = "post-continuation";

        // Send SASL response (base64) as raw line (no tag), redacted in transcript by ImapClient.
        if (!client.SendRawLine(xoauth2B64, "<REDACTED>")) {
            outError = client.LastError();
            client.Disconnect();
            return false;
        }
        imapPhase = "post-auth-response";

        bool sentEmptyAfterChallenge = false;
        QString taggedCompletion;
        for (int i = 0; i < 120; ++i) {
            const QString line = client.ReadLine();
            if (line.isEmpty()) {
                break;
            }

            if (line.startsWith("*")) {
                imapLastUntagged = line;
                continue;
            }

            if (line.startsWith(authTag, Qt::CaseInsensitive)) {
                taggedCompletion = line;
                imapLastTagged = line;
                break;
            }

            if (line.trimmed().startsWith("+")) {
                sawPlusContinuation = true;
                imapPhase = "post-continuation";
                if (!sentEmptyAfterChallenge) {
                    if (!client.SendRawLine("", "")) {
                        outError = client.LastError();
                        client.Disconnect();
                        return false;
                    }
                    sentEmptyAfterChallenge = true;
                }
            }
        }

        if (taggedCompletion.isEmpty()) {
            const QString lastReply = !imapLastTagged.isEmpty() ? imapLastTagged : (!imapLastUntagged.isEmpty() ? imapLastUntagged : "no-tagged-completion");
            outError = BuildXoauth2FailError(lastReply,
                                            xoauth2ShapeOk,
                                            xoauth2ShapeReason,
                                            authBearerCount,
                                            firstAuthBearerPos,
                                            secondAuthBearerPos,
                                            xoauth2RawLen,
                                            imapPhase,
                                            imapTag,
                                            imapLastTagged,
                                            imapLastUntagged,
                                            sawPlusContinuation);
            client.Disconnect();
            return false;
        }

        if (!taggedCompletion.startsWith(authTag + " OK", Qt::CaseInsensitive)) {
            outError = BuildXoauth2FailError(taggedCompletion,
                                            xoauth2ShapeOk,
                                            xoauth2ShapeReason,
                                            authBearerCount,
                                            firstAuthBearerPos,
                                            secondAuthBearerPos,
                                            xoauth2RawLen,
                                            imapPhase,
                                            imapTag,
                                            imapLastTagged,
                                            imapLastUntagged,
                                            sawPlusContinuation);
            client.Disconnect();
            return false;
        }
    } else {
        const QString loginCommand = QString("%1 LOGIN \"%2\" \"%3\"")
            .arg(authTag, EscapeQuoted(request.username), EscapeQuoted(request.password));
        const QString loginCommandRedacted = QString("%1 LOGIN \"%2\" \"<REDACTED>\"")
            .arg(authTag, EscapeQuoted(request.username));

        if (!client.SendCommand(loginCommand, loginCommandRedacted)) {
            outError = client.LastError();
            client.Disconnect();
            return false;
        }
        const QStringList loginLines = client.ReadResponseUntilTag(authTag);
        if (!IsTaggedOk(loginLines, authTag)) {
            outError = "LOGIN failed";
            client.Disconnect();
            return false;
        }
    }

    // --- NAMESPACE delimiter ---
    QString delimiter = "/";

    if (hasNamespace) {
        const QString namespaceTag = MakeTag(commandIndex++);
        if (!client.SendCommand(namespaceTag + " NAMESPACE")) {
            outError = client.LastError();
            client.Disconnect();
            return false;
        }
        const QStringList nsLines = client.ReadResponseUntilTag(namespaceTag);
        if (!IsTaggedOk(nsLines, namespaceTag)) {
            outError = "NAMESPACE failed";
            client.Disconnect();
            return false;
        }
        const QRegularExpression delimRe("\"\"\\s+\"([^\"]*)\"");
        for (const QString& line : nsLines) {
            const QRegularExpressionMatch m = delimRe.match(line);
            if (m.hasMatch()) {
                delimiter = m.captured(1);
                break;
            }
        }
    }

    // --- LIST folders ---
    const QString listTag = MakeTag(commandIndex++);
    if (!client.SendCommand(listTag + " LIST \"\" \"*\"")) {
        outError = client.LastError();
        client.Disconnect();
        return false;
    }
    const QStringList listLines = client.ReadResponseUntilTag(listTag);
    if (!IsTaggedOk(listLines, listTag)) {
        outError = "LIST failed";
        client.Disconnect();
        return false;
    }
    outFolders = ParseListLines(listLines);

    // --- Special-use mapping ---
    if (hasSpecialUse) {
        const QString suTag = MakeTag(commandIndex++);
        if (client.SendCommand(suTag + " LIST (SPECIAL-USE) \"\" \"*\"")) {
            const QStringList suLines = client.ReadResponseUntilTag(suTag);
            const QVector<ResolvedFolder> suFolders = ParseListLines(suLines);
            for (const auto& folder : suFolders) {
                for (auto& existing : outFolders) {
                    if (existing.remoteName.compare(folder.remoteName, Qt::CaseInsensitive) == 0 &&
                        !folder.specialUse.isEmpty()) {
                        existing.specialUse = folder.specialUse;
                    }
                }
            }
        }
    } else {
        const QString xlistTag = MakeTag(commandIndex++);
        if (client.SendCommand(xlistTag + " XLIST \"\" \"*\"")) {
            const QStringList xlLines = client.ReadResponseUntilTag(xlistTag);
            const QVector<ResolvedFolder> xlFolders = ParseListLines(xlLines);
            for (const auto& folder : xlFolders) {
                for (auto& existing : outFolders) {
                    if (existing.remoteName.compare(folder.remoteName, Qt::CaseInsensitive) == 0 &&
                        !folder.specialUse.isEmpty()) {
                        existing.specialUse = folder.specialUse;
                    }
                }
            }
        }
    }

    for (auto& folder : outFolders) {
        if (folder.delimiter.isEmpty()) folder.delimiter = delimiter;
    }

    const QString logoutTag = MakeTag(commandIndex++);
    client.SendCommand(logoutTag + " LOGOUT");
    client.ReadResponseUntilTag(logoutTag);
    client.Disconnect();

    if (outFolders.isEmpty()) {
        outError = "No folders returned by server";
        return false;
    }

    return true;
}

} // namespace ngks::core::mail::providers::imap
#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QByteArray>
#include <memory>

namespace ngks::core::mail::providers::imap {

class ImapClient {
public:
    ImapClient();
    ~ImapClient();

    bool Connect(const QString& host, int port, bool tls, const QString& transcriptPath);
    void Disconnect();

    // Sends a tagged IMAP command line (appends CRLF if missing). Logs "C " with redaction if provided.
    bool SendCommand(const QString& taggedCommand, const QString& redactedCommand = QString());

    // Sends an untagged IMAP line (continuation response, etc). Appends CRLF if missing.
    // Use redactedLine to avoid secrets in transcript.
    bool SendRawLine(const QString& line, const QString& redactedLine = QString());

    // Reads a single IMAP line (trimmed) and logs "S ". Returns empty on timeout/error.
    QString ReadLine(int timeoutMs = 10000);

    // Reads until a tagged response line begins with "<tag> " (inclusive). Logs "S ".
    QStringList ReadResponseUntilTag(const QString& tag);

    // Raw variant that preserves bytes/newlines (required for FETCH BODY[] literals).
    QList<QByteArray> ReadResponseUntilTagRaw(const QString& tag);

    QString ReadGreeting();

    QString LastError() const;
    int LastSocketErrorCode() const;
    QString LastSocketErrorString() const;
    bool WasEncrypted() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}

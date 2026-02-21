#include "core/mail/providers/imap/ImapClient.h"

#include <QFile>
#include <QIODevice>
#include <QSslSocket>
#include <QTextStream>

namespace ngks::core::mail::providers::imap {

class ImapClient::Impl {
public:
    QSslSocket socket;
    QFile transcript;
    QString lastError;
    int lastSocketErrorCode = static_cast<int>(QAbstractSocket::UnknownSocketError);
    QString lastSocketErrorString;
    bool encryptedReached = false;

    void LogLine(const QString& direction, const QString& text)
    {
        if (!transcript.isOpen()) {
            return;
        }
        QTextStream ts(&transcript);
        ts << direction << text << '\n';
        ts.flush();
    }

    void SetSocketFailure(const QString& context)
    {
        lastSocketErrorCode = static_cast<int>(socket.error());
        lastSocketErrorString = socket.errorString();
        encryptedReached = socket.isEncrypted();
        lastError = QString("%1; socket_error=%2; socket_error_string=%3; encrypted=%4")
                        .arg(context)
                        .arg(lastSocketErrorCode)
                        .arg(lastSocketErrorString)
                        .arg(encryptedReached ? "true" : "false");
        LogLine("! ", lastError);
    }

    void SetTimeoutFailure(const QString& context)
    {
        lastSocketErrorCode = static_cast<int>(socket.error());
        lastSocketErrorString = socket.errorString();
        encryptedReached = socket.isEncrypted();
        lastError = QString("%1; socket_error=%2; socket_error_string=%3; encrypted=%4")
                        .arg(context)
                        .arg(lastSocketErrorCode)
                        .arg(lastSocketErrorString)
                        .arg(encryptedReached ? "true" : "false");
        LogLine("! ", lastError);
    }
};

ImapClient::ImapClient()
    : impl_(std::make_unique<Impl>())
{
}

ImapClient::~ImapClient() = default;

bool ImapClient::Connect(const QString& host, int port, bool tls, const QString& transcriptPath)
{
    impl_->lastError.clear();
    impl_->lastSocketErrorCode = static_cast<int>(QAbstractSocket::UnknownSocketError);
    impl_->lastSocketErrorString.clear();
    impl_->encryptedReached = false;
    impl_->transcript.setFileName(transcriptPath);
    if (!impl_->transcript.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        impl_->lastError = QStringLiteral("failed to open transcript");
        return false;
    }

    if (tls) {
        impl_->socket.connectToHostEncrypted(host, static_cast<quint16>(port));
        if (!impl_->socket.waitForEncrypted(10000)) {
            impl_->SetSocketFailure(QStringLiteral("connectToHostEncrypted failed"));
            return false;
        }
        impl_->encryptedReached = impl_->socket.isEncrypted();
    } else {
        impl_->socket.connectToHost(host, static_cast<quint16>(port));
        if (!impl_->socket.waitForConnected(10000)) {
            impl_->SetSocketFailure(QStringLiteral("connectToHost failed"));
            return false;
        }
    }

    impl_->LogLine("I ", QString("CONNECTED %1:%2 tls=%3").arg(host).arg(port).arg(tls ? "true" : "false"));
    return true;
}

void ImapClient::Disconnect()
{
    if (impl_->socket.state() != QAbstractSocket::UnconnectedState) {
        impl_->socket.disconnectFromHost();
        impl_->socket.waitForDisconnected(2000);
    }
    if (impl_->transcript.isOpen()) {
        impl_->transcript.close();
    }
}

static bool WriteLineWithCrlf(QSslSocket& sock, const QString& line, int timeoutMs)
{
    QString out = line;
    if (!out.endsWith("\r\n")) {
        out += "\r\n";
    }
    const QByteArray bytes = out.toUtf8();
    const qint64 wrote = sock.write(bytes);
    if (wrote != bytes.size()) {
        return false;
    }
    return sock.waitForBytesWritten(timeoutMs);
}

bool ImapClient::SendCommand(const QString& taggedCommand, const QString& redactedCommand)
{
    if (!WriteLineWithCrlf(impl_->socket, taggedCommand, 5000)) {
        impl_->SetSocketFailure(QStringLiteral("write command failed"));
        return false;
    }

    const QString logged = redactedCommand.isEmpty() ? taggedCommand : redactedCommand;
    impl_->LogLine("C ", logged);
    return true;
}

bool ImapClient::SendRawLine(const QString& line, const QString& redactedLine)
{
    // This is used for SASL continuation responses (untagged).
    if (!WriteLineWithCrlf(impl_->socket, line, 5000)) {
        impl_->SetSocketFailure(QStringLiteral("write raw line failed"));
        return false;
    }

    // Log something visible even if the line is empty.
    QString logged;
    if (!redactedLine.isEmpty()) {
        logged = redactedLine;
    } else if (line.isEmpty()) {
        logged = QStringLiteral("<EMPTY>");
    } else {
        logged = line;
    }

    impl_->LogLine("C ", logged);
    return true;
}

QString ImapClient::ReadLine(int timeoutMs)
{
    if (!impl_->socket.canReadLine() && !impl_->socket.waitForReadyRead(timeoutMs)) {
        impl_->SetTimeoutFailure(QStringLiteral("timeout waiting for IMAP line"));
        return QString();
    }

    const QString line = QString::fromUtf8(impl_->socket.readLine()).trimmed();
    if (!line.isEmpty()) {
        impl_->LogLine("S ", line);
    }
    return line;
}

QString ImapClient::ReadGreeting()
{
    if (!impl_->socket.waitForReadyRead(10000)) {
        impl_->lastSocketErrorCode = static_cast<int>(impl_->socket.error());
        impl_->lastSocketErrorString = impl_->socket.errorString();
        impl_->encryptedReached = impl_->socket.isEncrypted();
        impl_->lastError = QStringLiteral("timeout waiting for greeting; socket_error=%1; socket_error_string=%2; encrypted=%3")
                              .arg(impl_->lastSocketErrorCode)
                              .arg(impl_->lastSocketErrorString)
                              .arg(impl_->encryptedReached ? "true" : "false");
        impl_->LogLine("! ", impl_->lastError);
        return QString();
    }

    const QString line = QString::fromUtf8(impl_->socket.readLine()).trimmed();
    if (!line.isEmpty()) {
        impl_->LogLine("S ", line);
    }
    return line;
}

QStringList ImapClient::ReadResponseUntilTag(const QString& tag)
{
    QStringList lines;
    const QString tagPrefix = tag + ' ';

    while (true) {
        if (!impl_->socket.waitForReadyRead(10000)) {
            impl_->lastSocketErrorCode = static_cast<int>(impl_->socket.error());
            impl_->lastSocketErrorString = impl_->socket.errorString();
            impl_->encryptedReached = impl_->socket.isEncrypted();
            impl_->lastError = QStringLiteral("timeout waiting for IMAP response; socket_error=%1; socket_error_string=%2; encrypted=%3")
                                  .arg(impl_->lastSocketErrorCode)
                                  .arg(impl_->lastSocketErrorString)
                                  .arg(impl_->encryptedReached ? "true" : "false");
            impl_->LogLine("! ", impl_->lastError);
            break;
        }

        while (impl_->socket.canReadLine()) {
            const QString line = QString::fromUtf8(impl_->socket.readLine()).trimmed();
            if (line.isEmpty()) {
                continue;
            }
            impl_->LogLine("S ", line);
            lines.push_back(line);

            if (line.startsWith(tagPrefix, Qt::CaseInsensitive)) {
                return lines;
            }
        }
    }

    return lines;
}

QString ImapClient::LastError() const
{
    return impl_->lastError;
}

int ImapClient::LastSocketErrorCode() const
{
    return impl_->lastSocketErrorCode;
}

QString ImapClient::LastSocketErrorString() const
{
    return impl_->lastSocketErrorString;
}

bool ImapClient::WasEncrypted() const
{
    return impl_->encryptedReached;
}

}

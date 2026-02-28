#include "core/mail/mime/MimeParser.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cctype>

namespace ngks::core::mail::mime {

namespace {

std::string Trim(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

QString TrimCrLf(const QString& line)
{
    QString out = line;
    while (out.endsWith('\n') || out.endsWith('\r')) {
        out.chop(1);
    }
    return out;
}

void SplitHeadersBody(const QString& raw, QString& outHeaders, QString& outBody)
{
    const int p1 = raw.indexOf("\r\n\r\n");
    if (p1 >= 0) {
        outHeaders = raw.left(p1);
        outBody = raw.mid(p1 + 4);
        return;
    }
    const int p2 = raw.indexOf("\n\n");
    if (p2 >= 0) {
        outHeaders = raw.left(p2);
        outBody = raw.mid(p2 + 2);
        return;
    }
    outHeaders = raw;
    outBody.clear();
}

QMap<QString, QString> ParseHeaders(const QString& headerBlock)
{
    QMap<QString, QString> headers;
    QStringList lines = headerBlock.split(QRegularExpression("\\r?\\n"));
    QString currentKey;

    for (const auto& lineRaw : lines) {
        const QString line = TrimCrLf(lineRaw);
        if (line.isEmpty()) {
            continue;
        }

        if ((line.startsWith(' ') || line.startsWith('\t')) && !currentKey.isEmpty()) {
            headers[currentKey] += " " + line.trimmed();
            continue;
        }

        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }

        currentKey = line.left(colon).trimmed().toLower();
        headers[currentKey] = line.mid(colon + 1).trimmed();
    }
    return headers;
}

QString HeaderParam(const QString& headerValue, const QString& key)
{
    const QRegularExpression re(QString("(?:^|;)\\s*%1\\s*=\\s*\\\"?([^;\\\"]+)\\\"?").arg(QRegularExpression::escape(key)),
                                QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(headerValue);
    if (!m.hasMatch()) {
        return {};
    }
    return m.captured(1).trimmed();
}

QByteArray DecodeQuotedPrintable(const QString& encoded)
{
    QByteArray in = encoded.toUtf8();
    QByteArray out;
    out.reserve(in.size());

    for (int i = 0; i < in.size(); ++i) {
        const char c = in.at(i);
        if (c == '=' && i + 2 < in.size()) {
            const char c1 = in.at(i + 1);
            const char c2 = in.at(i + 2);
            if (c1 == '\r' && c2 == '\n') {
                i += 2;
                continue;
            }
            const QByteArray hex = QByteArray() + c1 + c2;
            bool ok = false;
            const int value = hex.toInt(&ok, 16);
            if (ok) {
                out.append(static_cast<char>(value));
                i += 2;
                continue;
            }
        }
        out.append(c);
    }

    return out;
}

QByteArray DecodeBody(const QString& body, const QString& transferEncoding)
{
    const QString enc = transferEncoding.trimmed().toLower();
    if (enc == "base64") {
        QByteArray compact = body.toUtf8();
        compact.replace("\r", "");
        compact.replace("\n", "");
        return QByteArray::fromBase64(compact);
    }
    if (enc == "quoted-printable") {
        return DecodeQuotedPrintable(body);
    }
    return body.toUtf8();
}

QString ToUtf8Text(const QByteArray& data)
{
    return QString::fromUtf8(data.constData(), data.size());
}

QString EscapeHtml(const QString& value)
{
    return value.toHtmlEscaped().replace("\n", "<br/>");
}

void ParsePartRecursive(const QString& rawPart, MimeParseResult& out)
{
    QString headerBlock;
    QString body;
    SplitHeadersBody(rawPart, headerBlock, body);
    const auto headers = ParseHeaders(headerBlock);

    const QString contentType = headers.value("content-type", "text/plain");
    const QString transferEncoding = headers.value("content-transfer-encoding");
    const QString disposition = headers.value("content-disposition");

    if (contentType.startsWith("multipart/", Qt::CaseInsensitive)) {
        const QString boundary = HeaderParam(contentType, "boundary");
        if (boundary.isEmpty()) {
            return;
        }

        const QString marker = "--" + boundary;
        const QString endMarker = marker + "--";
        const QStringList lines = body.split(QRegularExpression("\\r?\\n"));
        QString current;
        bool inPart = false;

        for (const auto& line : lines) {
            if (line == marker) {
                if (inPart && !current.trimmed().isEmpty()) {
                    ParsePartRecursive(current, out);
                    current.clear();
                }
                inPart = true;
                continue;
            }
            if (line == endMarker) {
                if (inPart && !current.trimmed().isEmpty()) {
                    ParsePartRecursive(current, out);
                }
                break;
            }
            if (inPart) {
                current += line;
                current += "\r\n";
            }
        }
        return;
    }

    const QByteArray decoded = DecodeBody(body, transferEncoding);
    const QString contentTypeLower = contentType.toLower();

    if (contentTypeLower.startsWith("text/html")) {
        if (out.html.empty()) {
            out.html = decoded.toStdString();
        }
        return;
    }

    if (contentTypeLower.startsWith("text/plain")) {
        if (out.text.empty()) {
            out.text = decoded.toStdString();
        }
        return;
    }

    Attachment a;
    const QString fileFromDisp = HeaderParam(disposition, "filename");
    const QString fileFromType = HeaderParam(contentType, "name");
    const QString contentId = headers.value("content-id").trimmed();

    a.name = (fileFromDisp.isEmpty() ? fileFromType : fileFromDisp).toStdString();
    a.contentType = contentType.toStdString();
    a.contentId = contentId.toStdString();
    a.isInline = disposition.contains("inline", Qt::CaseInsensitive) || contentTypeLower.startsWith("image/");
    a.dataBase64 = QString::fromLatin1(decoded.toBase64()).toStdString();
    out.attachments.push_back(std::move(a));
}

void InlineCidImages(MimeParseResult& out)
{
    if (out.html.empty()) {
        return;
    }

    QString html = QString::fromStdString(out.html);
    for (const auto& attachment : out.attachments) {
        if (!attachment.isInline || attachment.contentId.empty() || attachment.dataBase64.empty()) {
            continue;
        }

        QString cid = QString::fromStdString(attachment.contentId).trimmed();
        if (cid.startsWith('<') && cid.endsWith('>') && cid.size() > 2) {
            cid = cid.mid(1, cid.size() - 2);
        }
        if (cid.isEmpty()) {
            continue;
        }

        const QString mime = QString::fromStdString(attachment.contentType).trimmed();
        const QString dataUri = QString("data:%1;base64,%2").arg(mime, QString::fromStdString(attachment.dataBase64));
        html.replace(QString("cid:%1").arg(cid), dataUri, Qt::CaseInsensitive);
        html.replace(QString("cid:<%1>").arg(cid), dataUri, Qt::CaseInsensitive);
    }

    out.html = html.toStdString();
}

} // namespace

bool MimeParser::Parse(const std::string& rawMime, MimeParseResult& out)
{
    out = MimeParseResult{};
    if (rawMime.empty()) {
        return false;
    }

    const QString raw = QString::fromUtf8(rawMime.data(), static_cast<int>(rawMime.size()));
    QString headerBlock;
    QString body;
    SplitHeadersBody(raw, headerBlock, body);

    const auto headers = ParseHeaders(headerBlock);
    out.subject = headers.value("subject").toStdString();
    out.from = headers.value("from").toStdString();
    out.date = headers.value("date").toStdString();
    out.messageId = headers.value("message-id").toStdString();

    ParsePartRecursive(raw, out);

    if (out.html.empty() && !out.text.empty()) {
        out.html = QString("<pre style='font-family:Segoe UI,Arial,sans-serif; white-space:pre-wrap'>%1</pre>")
                       .arg(EscapeHtml(QString::fromStdString(out.text)))
                       .toStdString();
    }

    InlineCidImages(out);

    return !out.subject.empty() || !out.from.empty() || !out.text.empty() || !out.html.empty() || !out.attachments.empty();
}
}

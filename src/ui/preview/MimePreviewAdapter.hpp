#pragma once

#include <QLabel>
#include <QRegularExpression>
#include <QString>

#include "ui/models/MailUiService.hpp"

namespace ngks::ui::preview {

struct PreviewDocument {
    QString html;
    QStringList attachments;
};

inline PreviewDocument BuildPreviewDocument(const ngks::ui::models::MessageItem& item)
{
    PreviewDocument doc;
    doc.attachments = item.attachments;

    if (!item.bodyHtml.trimmed().isEmpty()) {
        doc.html = item.bodyHtml;
    } else if (!item.bodyText.trimmed().isEmpty()) {
        const QString escaped = item.bodyText.toHtmlEscaped().replace("\n", "<br/>");
        doc.html = QString("<pre style='font-family:Segoe UI,Arial,sans-serif; white-space:pre-wrap'>%1</pre>").arg(escaped);
    } else {
        doc.html = "<p>No message body available.</p>";
    }

    return doc;
}

inline QString BuildAttachmentHtmlList(const QStringList& attachments)
{
    if (attachments.isEmpty()) {
        return "<p><b>Attachments:</b> none</p>";
    }

    QString html = "<p><b>Attachments:</b></p><ul>";
    for (const auto& name : attachments) {
        html += QString("<li>%1</li>").arg(name.toHtmlEscaped());
    }
    html += "</ul>";
    return html;
}

} // namespace ngks::ui::preview

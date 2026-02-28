#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace ngks::ui::compose {

struct ComposeRequest {
    QString to;
    QString cc;
    QString bcc;
    QString subject;
    QString htmlBody;
    QStringList attachments;
};

class IComposeDispatcher {
public:
    virtual ~IComposeDispatcher() = default;
    virtual bool Send(const ComposeRequest& request, QString& outError) = 0;
    virtual bool SaveDraft(const ComposeRequest& request, QString& outError) = 0;
};

class StubComposeDispatcher final : public IComposeDispatcher {
public:
    bool Send(const ComposeRequest& request, QString& outError) override
    {
        Q_UNUSED(request);
        outError.clear();
        return true;
    }

    bool SaveDraft(const ComposeRequest& request, QString& outError) override
    {
        Q_UNUSED(request);
        outError.clear();
        return true;
    }
};

} // namespace ngks::ui::compose

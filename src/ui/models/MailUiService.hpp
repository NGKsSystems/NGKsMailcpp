#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

namespace ngks::ui::models {

struct AccountItem {
    int accountId = 0;
    QString email;
    QString provider;
};

struct FolderItem {
    int folderId = 0;
    int accountId = 0;
    QString displayName;
    QString specialUse;
};

struct MessageItem {
    int messageId = 0;
    int accountId = 0;
    int folderId = 0;
    QString from;
    QString subject;
    QDateTime dateUtc;
    QString provider;
    bool unread = true;
    QString previewHtml;
    QString bodyHtml;
    QString bodyText;
    QStringList attachments;
};

class IMailUiService {
public:
    virtual ~IMailUiService() = default;

    virtual QVector<AccountItem> ListAccounts() = 0;
    virtual QVector<FolderItem> ListFolders(int accountId) = 0;
    virtual QVector<MessageItem> ListMessages(int accountId, int folderId) = 0;
    virtual bool MarkRead(int messageId, bool read)
    {
        Q_UNUSED(messageId);
        Q_UNUSED(read);
        return false;
    }
    virtual bool DeleteMessage(int messageId)
    {
        Q_UNUSED(messageId);
        return false;
    }
};

class StubMailUiService final : public IMailUiService {
public:
    QVector<AccountItem> ListAccounts() override
    {
        return {
            {1, "support@ngkssystems.com", "gmail"},
            {2, "ops@ngkssystems.com", "ms_graph"},
            {3, "hello@ngkssystems.com", "icloud"},
        };
    }

    QVector<FolderItem> ListFolders(int accountId) override
    {
        return {
            {100 + accountId, accountId, "Inbox", "\\inbox"},
            {200 + accountId, accountId, "Sent", "\\sent"},
            {300 + accountId, accountId, "Archive", ""},
        };
    }

    QVector<MessageItem> ListMessages(int accountId, int folderId) override
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        return {
            {1000 + folderId, accountId, folderId, "alerts@service.local", "Daily health report", now.addSecs(-900), "aggregated", true,
                "<h3>Daily health report</h3><p>All monitored providers are online.</p>",
                "<h3>Daily health report</h3><p>All monitored providers are online.</p>",
                "Daily health report\nAll monitored providers are online.",
                {"health.csv"}},
            {2000 + folderId, accountId, folderId, "team@ngkssystems.com", "Build pipeline completed", now.addSecs(-3600), "aggregated", false,
                "<h3>Build pipeline completed</h3><p>Release validation finished successfully.</p>",
                "<h3>Build pipeline completed</h3><p>Release validation finished successfully.</p>",
                "Build pipeline completed\nRelease validation finished successfully.",
                {}},
            {3000 + folderId, accountId, folderId, "noreply@example.com", "Welcome", now.addDays(-1), "aggregated", false,
                "<h3>Welcome</h3><p>This is placeholder content from the stub service adapter.</p>",
                "<h3>Welcome</h3><p>This is placeholder content from the stub service adapter.</p>",
                "Welcome\nThis is placeholder content from the stub service adapter.",
                {}},
        };
    }
};

inline std::shared_ptr<IMailUiService> CreateStubMailUiService()
{
    return std::make_shared<StubMailUiService>();
}

} // namespace ngks::ui::models

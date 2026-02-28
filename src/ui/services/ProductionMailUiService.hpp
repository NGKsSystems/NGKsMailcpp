#pragma once

#include "ui/models/MailUiService.hpp"

namespace ngks::ui::services {

class ProductionMailUiService final : public ngks::ui::models::IMailUiService {
public:
    QVector<ngks::ui::models::AccountItem> ListAccounts() override;
    QVector<ngks::ui::models::FolderItem> ListFolders(int accountId) override;
    QVector<ngks::ui::models::MessageItem> ListMessages(int accountId, int folderId) override;
    bool MarkRead(int messageId, bool read) override;
    bool DeleteMessage(int messageId) override;
};

} // namespace ngks::ui::services

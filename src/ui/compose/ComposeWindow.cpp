#include "ui/compose/ComposeWindow.hpp"

#include <QAction>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include "core/logging/AuditLog.h"
#include "ui/compose/ComposeDispatcher.hpp"

namespace ngks::ui::compose {

class ComposeWindow::Impl {
public:
    std::shared_ptr<IComposeDispatcher> dispatcher;
    QWidget* central = nullptr;
    QLineEdit* to = nullptr;
    QLineEdit* cc = nullptr;
    QWidget* bccRow = nullptr;
    QLineEdit* bcc = nullptr;
    QLineEdit* subject = nullptr;
    QTextEdit* editor = nullptr;
    QListWidget* attachments = nullptr;
    QPushButton* sendBtn = nullptr;
    QPushButton* cancelBtn = nullptr;
    QPushButton* saveDraftBtn = nullptr;
    QPushButton* toggleBccBtn = nullptr;
};

static void LogCompose(const QString& action)
{
    ngks::core::logging::AuditLog::Event(
        "UI_ACTION",
        QString("{\"area\":\"compose\",\"action\":\"%1\"}").arg(action).toStdString());
}

ComposeWindow::ComposeWindow(std::shared_ptr<IComposeDispatcher> dispatcher, QWidget* parent)
    : QMainWindow(parent)
{
    impl_ = std::make_unique<Impl>();
    impl_->dispatcher = dispatcher;

    setWindowTitle("Compose");
    resize(820, 620);

    impl_->central = new QWidget(this);
    auto* root = new QVBoxLayout(impl_->central);

    auto* formWidget = new QWidget(impl_->central);
    auto* form = new QFormLayout(formWidget);

    impl_->to = new QLineEdit(formWidget);
    impl_->cc = new QLineEdit(formWidget);
    impl_->bcc = new QLineEdit(formWidget);
    impl_->subject = new QLineEdit(formWidget);

    form->addRow("To", impl_->to);
    form->addRow("CC", impl_->cc);

    impl_->bccRow = new QWidget(formWidget);
    auto* bccLayout = new QHBoxLayout(impl_->bccRow);
    bccLayout->setContentsMargins(0, 0, 0, 0);
    bccLayout->addWidget(impl_->bcc);
    impl_->bccRow->setVisible(false);
    form->addRow("BCC", impl_->bccRow);

    form->addRow("Subject", impl_->subject);

    impl_->toggleBccBtn = new QPushButton("Toggle BCC", formWidget);
    form->addRow("", impl_->toggleBccBtn);

    impl_->editor = new QTextEdit(impl_->central);
    impl_->editor->setAcceptRichText(true);

    auto* attachmentsRow = new QWidget(impl_->central);
    auto* attachmentsLayout = new QVBoxLayout(attachmentsRow);
    auto* attachmentsLabel = new QLabel("Attachments", attachmentsRow);
    impl_->attachments = new QListWidget(attachmentsRow);

    auto* attachButtons = new QHBoxLayout();
    auto* addAttachment = new QPushButton("Add", attachmentsRow);
    auto* removeAttachment = new QPushButton("Remove", attachmentsRow);
    attachButtons->addWidget(addAttachment);
    attachButtons->addWidget(removeAttachment);
    attachButtons->addStretch();

    attachmentsLayout->addWidget(attachmentsLabel);
    attachmentsLayout->addWidget(impl_->attachments);
    attachmentsLayout->addLayout(attachButtons);

    auto* bottomButtons = new QHBoxLayout();
    impl_->sendBtn = new QPushButton("Send", impl_->central);
    impl_->cancelBtn = new QPushButton("Cancel", impl_->central);
    impl_->saveDraftBtn = new QPushButton("Save Draft", impl_->central);
    bottomButtons->addStretch();
    bottomButtons->addWidget(impl_->saveDraftBtn);
    bottomButtons->addWidget(impl_->cancelBtn);
    bottomButtons->addWidget(impl_->sendBtn);

    root->addWidget(formWidget);
    root->addWidget(impl_->editor, 1);
    root->addWidget(attachmentsRow);
    root->addLayout(bottomButtons);

    setCentralWidget(impl_->central);

    connect(impl_->toggleBccBtn, &QPushButton::clicked, this, &ComposeWindow::OnToggleBcc);
    connect(addAttachment, &QPushButton::clicked, this, &ComposeWindow::OnAddAttachment);
    connect(removeAttachment, &QPushButton::clicked, this, &ComposeWindow::OnRemoveAttachment);
    connect(impl_->sendBtn, &QPushButton::clicked, this, &ComposeWindow::OnSend);
    connect(impl_->cancelBtn, &QPushButton::clicked, this, &ComposeWindow::OnCancel);
    connect(impl_->saveDraftBtn, &QPushButton::clicked, this, &ComposeWindow::OnSaveDraft);
}

ComposeWindow::~ComposeWindow() = default;

void ComposeWindow::OnToggleBcc()
{
    impl_->bccRow->setVisible(!impl_->bccRow->isVisible());
    LogCompose("toggle_bcc");
}

void ComposeWindow::OnAddAttachment()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, "Attach files");
    for (const auto& f : files) {
        impl_->attachments->addItem(f);
    }
    if (!files.isEmpty()) {
        LogCompose("add_attachment");
    }
}

void ComposeWindow::OnRemoveAttachment()
{
    qDeleteAll(impl_->attachments->selectedItems());
    LogCompose("remove_attachment");
}

void ComposeWindow::OnSend()
{
    if (!impl_->dispatcher) {
        QMessageBox::warning(this, "Send", "No dispatcher is configured.");
        return;
    }

    ComposeRequest req;
    req.to = impl_->to->text().trimmed();
    req.cc = impl_->cc->text().trimmed();
    req.bcc = impl_->bcc->text().trimmed();
    req.subject = impl_->subject->text().trimmed();
    req.htmlBody = impl_->editor->toHtml();
    for (int i = 0; i < impl_->attachments->count(); ++i) {
        req.attachments.push_back(impl_->attachments->item(i)->text());
    }

    QString err;
    if (!impl_->dispatcher->Send(req, err)) {
        QMessageBox::warning(this, "Send failed", err.isEmpty() ? "Unknown send failure" : err);
        return;
    }

    LogCompose("send");
    QMessageBox::information(this, "Send", "Message queued for delivery.");
}

void ComposeWindow::OnCancel()
{
    LogCompose("cancel");
    close();
}

void ComposeWindow::OnSaveDraft()
{
    if (!impl_->dispatcher) {
        QMessageBox::warning(this, "Save Draft", "No dispatcher is configured.");
        return;
    }

    ComposeRequest req;
    req.to = impl_->to->text().trimmed();
    req.cc = impl_->cc->text().trimmed();
    req.bcc = impl_->bcc->text().trimmed();
    req.subject = impl_->subject->text().trimmed();
    req.htmlBody = impl_->editor->toHtml();
    for (int i = 0; i < impl_->attachments->count(); ++i) {
        req.attachments.push_back(impl_->attachments->item(i)->text());
    }

    QString err;
    if (!impl_->dispatcher->SaveDraft(req, err)) {
        QMessageBox::warning(this, "Save Draft failed", err.isEmpty() ? "Unknown save failure" : err);
        return;
    }

    LogCompose("save_draft");
    QMessageBox::information(this, "Save Draft", "Draft saved.");
}

} // namespace ngks::ui::compose

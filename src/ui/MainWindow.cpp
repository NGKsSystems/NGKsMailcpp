#include "ui/MainWindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>
#include <QSplitter>
#include <QSpinBox>
#include <QPushButton>
#include <QTableView>
#include <QTextBrowser>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include "core/mail/providers/imap/FolderMirrorService.h"
#include "core/mail/providers/imap/ImapProvider.h"
#include "core/logging/AuditLog.h"
#include "core/storage/Db.h"
#include "db/BasicCredentialStore.hpp"
#include "platform/common/Paths.h"
#include "providers/core/ProviderRegistry.hpp"
#include "ui/compose/ComposeDispatcher.hpp"
#include "ui/compose/ComposeWindow.hpp"
#include "ui/models/AccountTreeModel.hpp"
#include "ui/models/FolderTreeModel.hpp"
#include "ui/models/MailUiService.hpp"
#include "ui/models/MessageListModel.hpp"
#include "ui/preview/MimePreviewAdapter.hpp"
#include "ui/services/ProductionMailUiService.hpp"

namespace ngks::ui {

namespace {

void LogUiAction(const QString& action)
{
    ngks::core::logging::AuditLog::Event(
        "UI_ACTION",
        QString("{\"area\":\"mail_shell\",\"action\":\"%1\"}").arg(action).toStdString());
}

QString BaseStyleSheet()
{
    return QString(
        "QMainWindow { background: palette(window); }"
        "QToolBar { spacing: 6px; border-bottom: 1px solid palette(mid); padding: 4px; }"
        "QTreeView, QTableView, QTextBrowser {"
        "  background: palette(base);"
        "  alternate-background-color: palette(alternate-base);"
        "  border: 1px solid palette(mid);"
        "}"
        "QTreeView::item:hover, QTableView::item:hover {"
        "  background: palette(light);"
        "}"
        "QTreeView::item:selected, QTableView::item:selected {"
        "  background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}");
}

bool LooksLikeEmail(const QString& value)
{
    const QString trimmed = value.trimmed();
    return !trimmed.isEmpty() && trimmed.contains('@') && !trimmed.startsWith('@') && !trimmed.endsWith('@');
}

} // namespace

class MainWindow::Impl {
public:
    std::shared_ptr<ngks::ui::models::IMailUiService> service;
    std::shared_ptr<ngks::ui::compose::IComposeDispatcher> composeDispatcher;
    ngks::ui::compose::ComposeWindow* composeWindow = nullptr;

    ngks::ui::models::AccountTreeModel* accountModel = nullptr;
    ngks::ui::models::FolderTreeModel* folderModel = nullptr;
    ngks::ui::models::MessageListModel* messageModel = nullptr;
    QSortFilterProxyModel* messageProxyModel = nullptr;

    QSplitter* rootSplitter = nullptr;
    QSplitter* leftSplitter = nullptr;
    QTreeView* accountTree = nullptr;
    QTreeView* folderTree = nullptr;
    QTableView* messageTable = nullptr;
    QTextBrowser* preview = nullptr;
    QDockWidget* attachmentDock = nullptr;
    QListWidget* attachmentList = nullptr;
    QLineEdit* searchEdit = nullptr;
    QLabel* statusLabel = nullptr;
};

MainWindow::MainWindow()
{
    impl_ = std::make_unique<Impl>();

    setWindowTitle("NGKsMailcpp");
    resize(1440, 860);
    setStyleSheet(BaseStyleSheet());

    BuildUi();
    LoadInitialData();
    RestoreUiState();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event)
{
    SaveUiState();
    QMainWindow::closeEvent(event);
}

void MainWindow::BuildUi()
{
    auto* toolbar = addToolBar("MainToolbar");
    toolbar->setMovable(false);

    auto* refreshAction = toolbar->addAction("Refresh");
    auto* composeAction = toolbar->addAction("Compose");
    auto* deleteAction = toolbar->addAction("Delete");
    auto* markReadAction = toolbar->addAction("Mark Read");
    auto* settingsAction = toolbar->addAction("Settings");
    toolbar->addSeparator();
    impl_->searchEdit = new QLineEdit(toolbar);
    impl_->searchEdit->setPlaceholderText("Search subject, sender, provider...");
    impl_->searchEdit->setClearButtonEnabled(true);
    impl_->searchEdit->setMinimumWidth(260);
    toolbar->addWidget(impl_->searchEdit);
    impl_->statusLabel = new QLabel("Ready", toolbar);
    toolbar->addWidget(impl_->statusLabel);

    connect(refreshAction, &QAction::triggered, this, &MainWindow::OnRefresh, Qt::QueuedConnection);
    connect(composeAction, &QAction::triggered, this, &MainWindow::OnCompose);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::OnDelete);
    connect(markReadAction, &QAction::triggered, this, &MainWindow::OnMarkRead);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::OnSettings);

    impl_->service = std::make_shared<ngks::ui::services::ProductionMailUiService>();
    if (impl_->service->ListAccounts().isEmpty()) {
        impl_->service = ngks::ui::models::CreateStubMailUiService();
    }
    impl_->composeDispatcher = std::make_shared<ngks::ui::compose::StubComposeDispatcher>();

    impl_->accountModel = new ngks::ui::models::AccountTreeModel(this);
    impl_->accountModel->SetService(impl_->service);

    impl_->folderModel = new ngks::ui::models::FolderTreeModel(this);
    impl_->folderModel->SetService(impl_->service);

    impl_->messageModel = new ngks::ui::models::MessageListModel(this);
    impl_->messageModel->SetService(impl_->service);
    impl_->messageProxyModel = new QSortFilterProxyModel(this);
    impl_->messageProxyModel->setSourceModel(impl_->messageModel);
    impl_->messageProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    impl_->messageProxyModel->setFilterKeyColumn(-1);

    impl_->rootSplitter = new QSplitter(Qt::Horizontal, this);
    impl_->leftSplitter = new QSplitter(Qt::Vertical, impl_->rootSplitter);

    impl_->accountTree = new QTreeView(impl_->leftSplitter);
    impl_->accountTree->setModel(impl_->accountModel);
    impl_->accountTree->setHeaderHidden(false);
    impl_->accountTree->setAlternatingRowColors(true);
    impl_->accountTree->setMinimumWidth(220);

    impl_->folderTree = new QTreeView(impl_->leftSplitter);
    impl_->folderTree->setModel(impl_->folderModel);
    impl_->folderTree->setHeaderHidden(false);
    impl_->folderTree->setAlternatingRowColors(true);
    impl_->folderTree->setMinimumWidth(220);

    impl_->leftSplitter->addWidget(impl_->accountTree);
    impl_->leftSplitter->addWidget(impl_->folderTree);
    impl_->leftSplitter->setStretchFactor(0, 1);
    impl_->leftSplitter->setStretchFactor(1, 2);

    impl_->messageTable = new QTableView(impl_->rootSplitter);
    impl_->messageTable->setModel(impl_->messageProxyModel);
    impl_->messageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    impl_->messageTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    impl_->messageTable->setAlternatingRowColors(true);
    impl_->messageTable->setSortingEnabled(true);
    impl_->messageTable->verticalHeader()->setVisible(false);
    impl_->messageTable->horizontalHeader()->setStretchLastSection(true);
    impl_->messageTable->horizontalHeader()->setSectionResizeMode(ngks::ui::models::MessageListModel::FromColumn, QHeaderView::ResizeToContents);
    impl_->messageTable->horizontalHeader()->setSectionResizeMode(ngks::ui::models::MessageListModel::DateColumn, QHeaderView::ResizeToContents);
    impl_->messageTable->horizontalHeader()->setSectionResizeMode(ngks::ui::models::MessageListModel::ProviderColumn, QHeaderView::ResizeToContents);
    impl_->messageTable->horizontalHeader()->setSectionResizeMode(ngks::ui::models::MessageListModel::ReadStateColumn, QHeaderView::ResizeToContents);
    impl_->messageTable->setMinimumWidth(420);
    impl_->messageTable->sortByColumn(ngks::ui::models::MessageListModel::DateColumn, Qt::DescendingOrder);
    impl_->messageTable->setContextMenuPolicy(Qt::CustomContextMenu);

    impl_->preview = new QTextBrowser(impl_->rootSplitter);
    impl_->preview->setOpenExternalLinks(true);
    impl_->preview->setMinimumWidth(360);
    impl_->preview->setHtml("<h3>Preview</h3><p>Select a message to load content.</p>");

    impl_->rootSplitter->addWidget(impl_->leftSplitter);
    impl_->rootSplitter->addWidget(impl_->messageTable);
    impl_->rootSplitter->addWidget(impl_->preview);
    impl_->rootSplitter->setStretchFactor(0, 2);
    impl_->rootSplitter->setStretchFactor(1, 4);
    impl_->rootSplitter->setStretchFactor(2, 4);
    impl_->rootSplitter->setCollapsible(0, false);
    impl_->rootSplitter->setCollapsible(1, false);
    impl_->rootSplitter->setCollapsible(2, false);

    setCentralWidget(impl_->rootSplitter);

    impl_->attachmentDock = new QDockWidget("Attachments", this);
    impl_->attachmentList = new QListWidget(impl_->attachmentDock);
    impl_->attachmentDock->setWidget(impl_->attachmentList);
    addDockWidget(Qt::RightDockWidgetArea, impl_->attachmentDock);

    connect(impl_->accountTree->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::OnAccountChanged);
    connect(impl_->folderTree->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::OnFolderChanged);
    connect(impl_->messageTable->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::OnMessageChanged);
    connect(impl_->searchEdit, &QLineEdit::textChanged, this, &MainWindow::OnSearchTextChanged);
    connect(impl_->messageTable, &QTableView::doubleClicked, this, &MainWindow::OnMessageDoubleClicked);
    connect(impl_->messageTable, &QTableView::customContextMenuRequested, this, &MainWindow::OnMessageContextMenuRequested);
}

void MainWindow::LoadInitialData()
{
    impl_->accountModel->Reload();
    if (impl_->accountModel->rowCount({}) > 0) {
        const QModelIndex firstAccount = impl_->accountModel->index(0, 0, {});
        impl_->accountTree->setCurrentIndex(firstAccount);
    }
}

void MainWindow::RestoreUiState()
{
    QSettings settings("NGKsSystems", "NGKsMailcpp");
    restoreGeometry(settings.value("ui/main_window_geometry").toByteArray());
    if (impl_->rootSplitter) {
        impl_->rootSplitter->restoreState(settings.value("ui/root_splitter").toByteArray());
    }
    if (impl_->leftSplitter) {
        impl_->leftSplitter->restoreState(settings.value("ui/left_splitter").toByteArray());
    }
}

void MainWindow::SaveUiState()
{
    QSettings settings("NGKsSystems", "NGKsMailcpp");
    settings.setValue("ui/main_window_geometry", saveGeometry());
    if (impl_->rootSplitter) {
        settings.setValue("ui/root_splitter", impl_->rootSplitter->saveState());
    }
    if (impl_->leftSplitter) {
        settings.setValue("ui/left_splitter", impl_->leftSplitter->saveState());
    }
}

void MainWindow::OnRefresh()
{
    LogUiAction("refresh");
    impl_->accountModel->Reload();
    impl_->folderModel->Reload();
    impl_->messageModel->Reload();
    impl_->statusLabel->setText("Synced");
}

void MainWindow::OnCompose()
{
    LogUiAction("compose");
    if (!impl_->composeWindow) {
        impl_->composeWindow = new ngks::ui::compose::ComposeWindow(impl_->composeDispatcher, this);
    }
    impl_->composeWindow->show();
    impl_->composeWindow->raise();
    impl_->composeWindow->activateWindow();
}

void MainWindow::OnDelete()
{
    const auto selectedRows = impl_->messageTable->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        return;
    }

    int deleted = 0;
    for (int i = selectedRows.size() - 1; i >= 0; --i) {
        const QModelIndex source = impl_->messageProxyModel->mapToSource(selectedRows[i]);
        if (impl_->messageModel->DeleteAt(source.row())) {
            ++deleted;
        }
    }

    if (deleted > 0) {
        LogUiAction("delete");
        impl_->statusLabel->setText(QString("Deleted %1 message(s)").arg(deleted));
    }
}

void MainWindow::OnMarkRead()
{
    const auto selectedRows = impl_->messageTable->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        return;
    }

    int changed = 0;
    for (const auto& proxyIndex : selectedRows) {
        const QModelIndex source = impl_->messageProxyModel->mapToSource(proxyIndex);
        if (impl_->messageModel->MarkReadAt(source.row(), true)) {
            ++changed;
        }
    }

    if (changed > 0) {
        LogUiAction("mark_read");
        impl_->statusLabel->setText(QString("Marked read: %1").arg(changed));
    }
}

void MainWindow::OnSettings()
{
    LogUiAction("settings");

    QDialog dialog(this);
    dialog.setWindowTitle("Connect Account");

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* methodCombo = new QComboBox(&dialog);
    methodCombo->addItem("Gmail (OAuth)", "gmail");
    methodCombo->addItem("Microsoft 365 (OAuth)", "ms_graph");
    methodCombo->addItem("Generic IMAP (Password)", "generic_imap");
    methodCombo->addItem("iCloud (App Password)", "icloud");
    methodCombo->addItem("Yahoo (App Password)", "yahoo_app_password");
    methodCombo->addItem("Yahoo (OAuth Legacy)", "yahoo");

    auto* emailEdit = new QLineEdit(&dialog);
    emailEdit->setPlaceholderText("user@example.com");
    auto* usernameEdit = new QLineEdit(&dialog);
    usernameEdit->setPlaceholderText("user@example.com");
    auto* passwordEdit = new QLineEdit(&dialog);
    passwordEdit->setEchoMode(QLineEdit::Password);
    auto* imapHostEdit = new QLineEdit(&dialog);
    imapHostEdit->setPlaceholderText("imap.example.com");
    auto* imapPortSpin = new QSpinBox(&dialog);
    imapPortSpin->setRange(1, 65535);
    imapPortSpin->setValue(993);
    auto* tlsCheck = new QCheckBox("Use TLS", &dialog);
    tlsCheck->setChecked(true);

    auto* hintLabel = new QLabel(&dialog);
    hintLabel->setWordWrap(true);

    form->addRow("Method", methodCombo);
    form->addRow("Email", emailEdit);
    form->addRow("Username", usernameEdit);
    form->addRow("Password", passwordEdit);
    form->addRow("IMAP Host", imapHostEdit);
    form->addRow("IMAP Port", imapPortSpin);
    form->addRow("", tlsCheck);
    form->addRow("", hintLabel);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Connect");
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    const auto updateMethodUi = [&]() {
        const QString method = methodCombo->currentData().toString();
        const bool passwordMethod = (method == "generic_imap" || method == "icloud" || method == "yahoo_app_password");

        usernameEdit->setVisible(passwordMethod);
        passwordEdit->setVisible(passwordMethod);
        imapHostEdit->setVisible(passwordMethod);
        imapPortSpin->setVisible(passwordMethod);
        tlsCheck->setVisible(passwordMethod);

        form->labelForField(usernameEdit)->setVisible(passwordMethod);
        form->labelForField(passwordEdit)->setVisible(passwordMethod);
        form->labelForField(imapHostEdit)->setVisible(passwordMethod);
        form->labelForField(imapPortSpin)->setVisible(passwordMethod);

        if (method == "generic_imap") {
            imapHostEdit->setText("imap.example.com");
            imapPortSpin->setValue(993);
            tlsCheck->setChecked(true);
            hintLabel->setText("Enter your provider's IMAP credentials.");
        } else if (method == "icloud") {
            imapHostEdit->setText("imap.mail.me.com");
            imapPortSpin->setValue(993);
            tlsCheck->setChecked(true);
            hintLabel->setText("Use your Apple ID email, account username, and an app-specific password.");
        } else if (method == "yahoo_app_password") {
            imapHostEdit->setText("imap.mail.yahoo.com");
            imapPortSpin->setValue(993);
            tlsCheck->setChecked(true);
            hintLabel->setText("Use your Yahoo email, account username, and an app-specific password.");
        } else if (method == "gmail") {
            hintLabel->setText("OAuth browser flow will open for Google sign-in.");
        } else if (method == "ms_graph") {
            hintLabel->setText("OAuth browser flow will open for Microsoft sign-in.");
        } else if (method == "yahoo") {
            hintLabel->setText("Legacy only: uses existing Yahoo OAuth tokens if available; otherwise use Yahoo App Password.");
        } else {
            hintLabel->clear();
        }
    };

    QObject::connect(methodCombo, &QComboBox::currentIndexChanged, &dialog, [&](int) { updateMethodUi(); });
    updateMethodUi();

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString method = methodCombo->currentData().toString();
    const QString email = emailEdit->text().trimmed();
    const QString username = usernameEdit->text().trimmed();
    const QString password = passwordEdit->text();
    const QString imapHost = imapHostEdit->text().trimmed();
    const int imapPort = imapPortSpin->value();
    const bool useTls = tlsCheck->isChecked();

    if (!LooksLikeEmail(email)) {
        QMessageBox::warning(this, "Connect Account", "Please enter a valid email address.");
        return;
    }

    ngks::core::storage::Db db;
    if (!db.Open(ngks::platform::common::DbFilePath())) {
        QMessageBox::warning(this, "Connect Account", "Failed to open local database.");
        return;
    }

    if (method == "gmail" || method == "ms_graph" || method == "yahoo") {
        ngks::providers::core::ProviderRegistry registry;
        registry.RegisterBuiltins(db);
        auto* driver = registry.GetAuthDriverById(method);
        if (!driver) {
            QMessageBox::warning(this, "Connect Account", "Auth driver not found for selected method.");
            return;
        }

        const ngks::auth::AuthResult result = driver->BeginConnect(email);
        if (!result.ok) {
            QMessageBox::warning(this, "Connect Account", QString("Connect failed: %1").arg(result.detail));
            return;
        }
    } else {
        if (username.isEmpty() || password.isEmpty() || imapHost.isEmpty()) {
            QMessageBox::warning(this, "Connect Account", "Username, password, and IMAP host are required.");
            return;
        }

        ngks::core::mail::providers::imap::ResolveRequest request;
        request.email = email;
        request.host = imapHost;
        request.port = imapPort;
        request.tls = useTls;
        request.username = username;
        request.password = password;
        request.useXoauth2 = false;
        request.oauthAccessToken.clear();

        ngks::core::mail::providers::imap::ImapProvider imap;
        QVector<ngks::core::mail::providers::imap::ResolvedFolder> folders;
        QString resolveError;
        QString transcriptPath;
        if (!imap.ResolveAccount(request, folders, resolveError, transcriptPath)) {
            QMessageBox::warning(this, "Connect Account", QString("Connection failed: %1").arg(resolveError));
            return;
        }

        ngks::db::BasicCredentialRecord cred;
        cred.providerId = method;
        cred.email = email;
        cred.username = username;
        cred.secret = password;

        QString credErr;
        if (!ngks::db::BasicCredentialStore::Upsert(db, cred, credErr)) {
            QMessageBox::warning(this, "Connect Account", QString("Failed to save credentials: %1").arg(credErr));
            return;
        }

        ngks::core::mail::providers::imap::FolderMirrorService mirror;
        int accountId = -1;
        QString mirrorError;
        const QString credentialRef = QString("basic_credentials:%1:%2").arg(method, email);
        if (!mirror.MirrorResolvedAccount(db, request, credentialRef, folders, accountId, mirrorError, method)) {
            QMessageBox::warning(this, "Connect Account", QString("Failed to mirror folders: %1").arg(mirrorError));
            return;
        }
    }

    impl_->service = std::make_shared<ngks::ui::services::ProductionMailUiService>();
    impl_->accountModel->SetService(impl_->service);
    impl_->folderModel->SetService(impl_->service);
    impl_->messageModel->SetService(impl_->service);

    impl_->accountModel->Reload();
    impl_->folderModel->Reload();
    impl_->messageModel->Reload();

    impl_->statusLabel->setText(QString("Connected: %1").arg(email));
    QMessageBox::information(this, "Connect Account", "Account connected and folders mirrored.");
}

void MainWindow::OnSearchTextChanged(const QString& text)
{
    impl_->messageProxyModel->setFilterFixedString(text.trimmed());
    impl_->statusLabel->setText(text.trimmed().isEmpty() ? "Ready" : QString("Filter: %1").arg(text.trimmed()));
}

void MainWindow::OnAccountChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    const int accountId = current.data(ngks::ui::models::AccountTreeModel::AccountIdRole).toInt();
    impl_->folderModel->SetAccountId(accountId);
    impl_->folderModel->Reload();
    impl_->messageModel->SetContext(accountId, 0);
    impl_->messageModel->Reload();
    impl_->preview->setHtml("<h3>Preview</h3><p>Select a folder and message.</p>");

    if (impl_->folderModel->rowCount({}) > 0) {
        impl_->folderTree->setCurrentIndex(impl_->folderModel->index(0, 0, {}));
    }
}

void MainWindow::OnFolderChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    const int accountId = current.data(ngks::ui::models::FolderTreeModel::AccountIdRole).toInt();
    const int folderId = current.data(ngks::ui::models::FolderTreeModel::FolderIdRole).toInt();
    impl_->messageModel->SetContext(accountId, folderId);
    impl_->messageModel->Reload();
    impl_->preview->setHtml("<h3>Preview</h3><p>Select a message to load content.</p>");
    impl_->attachmentList->clear();
    impl_->statusLabel->setText(QString("Account %1 / Folder %2").arg(accountId).arg(folderId));

    if (impl_->messageModel->rowCount({}) > 0) {
        impl_->messageTable->setCurrentIndex(impl_->messageProxyModel->index(0, 0));
    }
}

void MainWindow::OnMessageChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    const QModelIndex source = impl_->messageProxyModel->mapToSource(current);
    const auto item = impl_->messageModel->ItemAt(source.row());
    const auto previewDoc = ngks::ui::preview::BuildPreviewDocument(item);

    impl_->preview->setHtml(previewDoc.html + ngks::ui::preview::BuildAttachmentHtmlList(previewDoc.attachments));
    impl_->attachmentList->clear();
    impl_->attachmentList->addItems(previewDoc.attachments);

    if (source.row() >= 0 && impl_->messageModel->IsUnreadAt(source.row())) {
        impl_->messageModel->MarkReadAt(source.row(), true);
    }
}

void MainWindow::OnMessageDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    const QModelIndex source = impl_->messageProxyModel->mapToSource(index);
    if (source.isValid()) {
        impl_->messageModel->MarkReadAt(source.row(), true);
    }
}

void MainWindow::OnMessageContextMenuRequested(const QPoint& pos)
{
    QMenu menu(this);
    QAction* markReadAction = menu.addAction("Mark as Read");
    QAction* deleteAction = menu.addAction("Delete");
    QAction* selected = menu.exec(impl_->messageTable->viewport()->mapToGlobal(pos));

    if (selected == markReadAction) {
        OnMarkRead();
    } else if (selected == deleteAction) {
        const auto answer = QMessageBox::question(this, "Delete", "Delete selected message(s)?");
        if (answer == QMessageBox::Yes) {
            OnDelete();
        }
    }
}

} // namespace ngks::ui
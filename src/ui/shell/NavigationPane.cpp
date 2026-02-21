#include "ui/shell/NavigationPane.h"

#include <QItemSelectionModel>
#include <QLabel>
#include <QStackedLayout>
#include <QTreeView>
#include <QVBoxLayout>

#include "ui/models/FolderTreeModel.h"

namespace ngks::ui::shell {

NavigationPane::NavigationPane(QWidget* parent)
	: QWidget(parent)
{
	auto* rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);

	stack_ = new QStackedLayout();
	rootLayout->addLayout(stack_);

	emptyState_ = new QLabel("Add account", this);
	emptyState_->setAlignment(Qt::AlignCenter);
	emptyState_->setObjectName("emptyStateAddAccount");

	tree_ = new QTreeView(this);
	tree_->setHeaderHidden(true);
	tree_->setUniformRowHeights(true);

	model_ = new ngks::ui::models::FolderTreeModel(this);
	tree_->setModel(model_);

	stack_->addWidget(emptyState_);
	stack_->addWidget(tree_);

	WireSignals();
	Refresh();
}

void NavigationPane::Refresh()
{
	if (!model_) {
		return;
	}

	model_->Reload();

	if (!model_->HasResolvedAccounts()) {
		stack_->setCurrentWidget(emptyState_);
		return;
	}

	stack_->setCurrentWidget(tree_);
	tree_->expandAll();

	const QModelIndex inbox = model_->FirstInboxIndex();
	if (inbox.isValid()) {
		tree_->setCurrentIndex(inbox);
		tree_->scrollTo(inbox);
	}
}

void NavigationPane::WireSignals()
{
	auto* selection = tree_->selectionModel();
	if (!selection) {
		return;
	}

	connect(selection, &QItemSelectionModel::currentChanged, this,
		[this](const QModelIndex& current, const QModelIndex&) {
			if (!current.isValid() || !model_) {
				return;
			}

			const bool isAccount = current.data(ngks::ui::models::FolderTreeModel::IsAccountNodeRole).toBool();
			if (isAccount) {
				return;
			}

			const int accountId = current.data(ngks::ui::models::FolderTreeModel::AccountIdRole).toInt();
			const int folderId = current.data(ngks::ui::models::FolderTreeModel::FolderIdRole).toInt();
			const QString role = current.data(ngks::ui::models::FolderTreeModel::FolderRoleRole).toString();
			const QString name = current.data(Qt::DisplayRole).toString();

			emit FolderSelected(accountId, folderId, role, name);
		});
}

} // namespace ngks::ui::shell

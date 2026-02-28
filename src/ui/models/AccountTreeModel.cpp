#include "ui/models/AccountTreeModel.hpp"

#include "ui/models/MailUiService.hpp"

namespace ngks::ui::models {

AccountTreeModel::AccountTreeModel(QObject* parent)
	: QAbstractItemModel(parent)
{
}

QModelIndex AccountTreeModel::index(int row, int column, const QModelIndex& parentIndex) const
{
	if (parentIndex.isValid() || row < 0 || row >= rows_.size() || column != 0) {
		return QModelIndex();
	}
	return createIndex(row, column);
}

QModelIndex AccountTreeModel::parent(const QModelIndex& child) const
{
	Q_UNUSED(child);
	return QModelIndex();
}

int AccountTreeModel::rowCount(const QModelIndex& parentIndex) const
{
	if (parentIndex.isValid()) {
		return 0;
	}
	return rows_.size();
}

int AccountTreeModel::columnCount(const QModelIndex& parentIndex) const
{
	Q_UNUSED(parentIndex);
	return 1;
}

QVariant AccountTreeModel::data(const QModelIndex& modelIndex, int role) const
{
	if (!modelIndex.isValid() || modelIndex.row() < 0 || modelIndex.row() >= rows_.size()) {
		return {};
	}

	const auto& row = rows_[modelIndex.row()];
	switch (role) {
	case Qt::DisplayRole:
		return QString("%1 (%2)").arg(row.email, row.provider);
	case AccountIdRole:
		return row.accountId;
	case ProviderRole:
		return row.provider;
	case EmailRole:
		return row.email;
	default:
		return {};
	}
}

QVariant AccountTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0) {
		return "Accounts";
	}
	return {};
}

void AccountTreeModel::SetService(const std::shared_ptr<IMailUiService>& service)
{
	service_ = service;
}

void AccountTreeModel::Reload()
{
	beginResetModel();
	rows_.clear();
	if (service_) {
		rows_ = service_->ListAccounts();
	}
	endResetModel();
}

} // namespace ngks::ui::models

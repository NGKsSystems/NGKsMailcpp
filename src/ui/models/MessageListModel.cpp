#include "ui/models/MessageListModel.hpp"

#include <QFont>

#include "ui/models/MailUiService.hpp"

namespace ngks::ui::models {

MessageListModel::MessageListModel(QObject* parent)
	: QAbstractTableModel(parent)
{
}

int MessageListModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid()) {
		return 0;
	}
	return rows_.size();
}

int MessageListModel::columnCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	return ColumnCount;
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
		return {};
	}

	const auto& row = rows_[index.row()];

	if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case FromColumn:
			return row.from;
		case SubjectColumn:
			return row.subject;
		case DateColumn:
			return row.dateUtc.toLocalTime().toString("yyyy-MM-dd HH:mm");
		case ProviderColumn:
			return row.provider;
		case ReadStateColumn:
			return row.unread ? "●" : "";
		default:
			return {};
		}
	}

	if (role == Qt::FontRole && row.unread) {
		QFont font;
		font.setBold(true);
		return font;
	}

	if (role == Qt::TextAlignmentRole && index.column() == ReadStateColumn) {
		return Qt::AlignCenter;
	}

	return {};
}

QVariant MessageListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return {};
	}

	switch (section) {
	case FromColumn:
		return "From";
	case SubjectColumn:
		return "Subject";
	case DateColumn:
		return "Date";
	case ProviderColumn:
		return "Provider";
	case ReadStateColumn:
		return "Unread";
	default:
		return {};
	}
}

void MessageListModel::SetService(const std::shared_ptr<IMailUiService>& service)
{
	service_ = service;
}

void MessageListModel::SetContext(int accountId, int folderId)
{
	accountId_ = accountId;
	folderId_ = folderId;
}

void MessageListModel::Reload()
{
	beginResetModel();
	rows_.clear();
	if (service_ && accountId_ > 0 && folderId_ > 0) {
		rows_ = service_->ListMessages(accountId_, folderId_);
	}
	endResetModel();
}

int MessageListModel::MessageIdAt(int row) const
{
	if (row < 0 || row >= rows_.size()) {
		return 0;
	}
	return rows_[row].messageId;
}

bool MessageListModel::IsUnreadAt(int row) const
{
	if (row < 0 || row >= rows_.size()) {
		return false;
	}
	return rows_[row].unread;
}

MessageItem MessageListModel::ItemAt(int row) const
{
	if (row < 0 || row >= rows_.size()) {
		return {};
	}
	return rows_[row];
}

bool MessageListModel::MarkReadAt(int row, bool read)
{
	if (!service_ || row < 0 || row >= rows_.size()) {
		return false;
	}
	const int messageId = rows_[row].messageId;
	if (messageId <= 0 || !service_->MarkRead(messageId, read)) {
		return false;
	}
	rows_[row].unread = !read;
	emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
	return true;
}

bool MessageListModel::DeleteAt(int row)
{
	if (!service_ || row < 0 || row >= rows_.size()) {
		return false;
	}
	const int messageId = rows_[row].messageId;
	if (messageId <= 0 || !service_->DeleteMessage(messageId)) {
		return false;
	}
	beginRemoveRows({}, row, row);
	rows_.removeAt(row);
	endRemoveRows();
	return true;
}

QString MessageListModel::PreviewHtmlAt(int row) const
{
	if (row < 0 || row >= rows_.size()) {
		return "<p>Select a message to preview.</p>";
	}
	const auto& item = rows_[row];
	if (!item.previewHtml.trimmed().isEmpty()) {
		return item.previewHtml;
	}
	if (!item.bodyHtml.trimmed().isEmpty()) {
		return item.bodyHtml;
	}
	if (!item.bodyText.trimmed().isEmpty()) {
		return QString("<pre style='font-family:Segoe UI,Arial,sans-serif; white-space:pre-wrap'>%1</pre>")
			.arg(item.bodyText.toHtmlEscaped());
	}
	return "<p>No message body available.</p>";
}

} // namespace ngks::ui::models

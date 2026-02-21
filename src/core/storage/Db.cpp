#include "core/storage/Db.h"

#include <QSqlDatabase>

namespace ngks::core::storage {

Db::Db() {
    if (QSqlDatabase::contains()) {
        QSqlDatabase existing = QSqlDatabase::database();
        db_ = new QSqlDatabase(existing);
    } else {
        QSqlDatabase created = QSqlDatabase::addDatabase("QSQLITE");
        db_ = new QSqlDatabase(created);
    }
}

Db::~Db() {
    if (db_ != nullptr) {
        if (db_->isOpen()) {
            db_->close();
        }
        delete db_;
        db_ = nullptr;
    }
}

bool Db::Open(const std::filesystem::path& path) {
    if (db_ == nullptr) {
        return false;
    }

    db_->setDatabaseName(QString::fromStdString(path.string()));
    open_ = db_->open();
    return open_;
}

bool Db::IsOpen() const {
    return db_ != nullptr && db_->isOpen() && open_;
}

QSqlDatabase& Db::Handle() {
    return *db_;
}

}

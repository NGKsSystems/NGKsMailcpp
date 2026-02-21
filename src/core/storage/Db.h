#pragma once

#include <filesystem>

class QSqlDatabase;

namespace ngks::core::storage {

class Db {
public:
    Db();
    ~Db();

    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    bool Open(const std::filesystem::path& path);
    bool IsOpen() const;
    QSqlDatabase& Handle();

private:
    QSqlDatabase* db_ = nullptr;
    bool open_ = false;
};

}

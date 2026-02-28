#pragma once

namespace ngks::core::storage {

class Db;

class Schema {
public:
    explicit Schema(Db& db);
    bool Ensure();

private:
    bool EnsureMeta();
    bool EnsureTables();
    int CurrentVersion() const;
    bool SetVersion(int version);
    bool MigrateToV2();
    bool MigrateToV3();
    bool MigrateToV4();

    Db& db_;
};

} // namespace ngks::core::storage
#include "platform/common/Paths.h"

#include <QCoreApplication>

namespace ngks::platform::common {

static bool LooksLikeRepoRoot(const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") || std::filesystem::exists(path / ".git");
}

std::filesystem::path RepoRoot() {
    std::filesystem::path cursor = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString());

    while (!cursor.empty()) {
        if (LooksLikeRepoRoot(cursor)) {
            return cursor;
        }
        if (cursor == cursor.root_path()) {
            break;
        }
        cursor = cursor.parent_path();
    }

    return std::filesystem::current_path();
}

std::filesystem::path ArtifactsDir() {
    return RepoRoot() / "artifacts";
}

std::filesystem::path AuditLogFilePath() {
    return ArtifactsDir() / "audit" / "audit.jsonl";
}

std::filesystem::path DbFilePath() {
    return ArtifactsDir() / "db" / "ngksmail.db";
}

std::filesystem::path SettingsFilePath() {
    return ArtifactsDir() / "config" / "settings.json";
}

bool EnsureAppDirectories() {
    std::error_code ec;
    std::filesystem::create_directories(ArtifactsDir() / "_proof", ec);
    if (ec) {
        return false;
    }

    std::filesystem::create_directories(ArtifactsDir() / "audit", ec);
    if (ec) {
        return false;
    }

    std::filesystem::create_directories(ArtifactsDir() / "audit" / "providers", ec);
    if (ec) {
        return false;
    }

    std::filesystem::create_directories(ArtifactsDir() / "db", ec);
    if (ec) {
        return false;
    }

    std::filesystem::create_directories(ArtifactsDir() / "config", ec);
    return !ec;
}

}

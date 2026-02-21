#pragma once

#include <filesystem>

namespace ngks::platform::common {

std::filesystem::path RepoRoot();
std::filesystem::path ArtifactsDir();
std::filesystem::path AuditLogFilePath();
std::filesystem::path DbFilePath();
std::filesystem::path SettingsFilePath();
bool EnsureAppDirectories();

}

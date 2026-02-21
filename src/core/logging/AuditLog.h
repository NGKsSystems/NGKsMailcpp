#pragma once

#include <atomic>
#include <mutex>
#include <string>

namespace ngks::core::logging {

class AuditLog {
public:
    static void Init(const std::string& auditPath);
    static void Event(const std::string& event, const std::string& payloadJson);

    // Convenience helpers
    static void AppStart(const std::string& dbPath, int phase);
    static void AppExit(int phase);

private:
    static std::string s_path;
    static std::mutex s_mu;

    // Ensures APP_EXIT is written once per process.
    static std::atomic<bool> s_appExitWritten;

    static void WriteLineLocked(const std::string& line);
    static std::string NowIsoUtc();
    static std::string HashLine(const std::string& prevHash, const std::string& jsonBody);
    static std::string EscapeJson(const std::string& s);
    static std::string LoadPrevHash();
};

} // namespace ngks::core::logging

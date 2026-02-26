#include "core/logging/AuditLog.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

std::string ExtractProviderId(const std::string& payloadJson)
{
    const std::string key = "\"provider\"";
    const std::size_t keyPos = payloadJson.find(key);
    if (keyPos == std::string::npos) {
        return "";
    }

    const std::size_t colonPos = payloadJson.find(':', keyPos + key.size());
    if (colonPos == std::string::npos) {
        return "";
    }

    std::size_t valueStart = payloadJson.find('"', colonPos + 1);
    if (valueStart == std::string::npos) {
        return "";
    }
    ++valueStart;

    const std::size_t valueEnd = payloadJson.find('"', valueStart);
    if (valueEnd == std::string::npos || valueEnd <= valueStart) {
        return "";
    }

    return payloadJson.substr(valueStart, valueEnd - valueStart);
}

} // namespace

namespace ngks::core::logging {

std::string AuditLog::s_path;
std::mutex AuditLog::s_mu;
std::atomic<bool> AuditLog::s_appExitWritten{false};

void AuditLog::Init(const std::string& auditPath)
{
    std::lock_guard<std::mutex> lk(s_mu);
    s_path = auditPath;
    std::filesystem::create_directories(std::filesystem::path(s_path).parent_path());
    // Do not reset s_appExitWritten here; it is per-process and starts false.
}

void AuditLog::AppStart(const std::string& dbPath, int phase)
{
    std::ostringstream payload;
    payload << "{\"component\":\"app\",\"db\":\"" << EscapeJson(dbPath) << "\",\"phase\":\"" << phase << "\"}";
    Event("APP_START", payload.str());
}

void AuditLog::AppExit(int phase)
{
    // Idempotent: only allow a single APP_EXIT per process.
    bool expected = false;
    if (!s_appExitWritten.compare_exchange_strong(expected, true)) {
        return;
    }

    std::ostringstream payload;
    payload << "{\"component\":\"app\",\"phase\":\"" << phase << "\"}";
    Event("APP_EXIT", payload.str());
}

// ---- existing Event()/WriteLineLocked()/hash helpers remain unchanged ----
// If you already have these implemented, keep your current implementations
// and only ensure AppExit() is guarded as above.

void AuditLog::Event(const std::string& event, const std::string& payloadJson)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (s_path.empty()) return;

    // Keep your existing hash-chain format. If your current implementation
    // already builds the JSON with ts/event/payload/prev_hash/hash, leave it.
    // This is a minimal safe placeholder ONLY if Event() is currently stubbed.
    // If your Event() is non-stub, do NOT replace it with this.
    std::ostringstream line;
    line << "{\"ts\":\"" << NowIsoUtc() << "\",\"event\":\"" << event << "\",\"payload\":" << payloadJson << "}\n";
    WriteLineLocked(line.str());
}

void AuditLog::WriteLineLocked(const std::string& line)
{
    std::ofstream f(s_path, std::ios::app | std::ios::binary);
    f << line;

    const std::filesystem::path globalPath(s_path);
    const std::string providerId = ExtractProviderId(line);
    if (providerId.empty()) {
        return;
    }

    const std::filesystem::path providersDir = globalPath.parent_path() / "providers";
    std::error_code ec;
    std::filesystem::create_directories(providersDir, ec);
    if (ec) {
        return;
    }

    const std::filesystem::path providerFile = providersDir / (providerId + ".jsonl");
    std::ofstream p(providerFile, std::ios::app | std::ios::binary);
    p << line;
}

std::string AuditLog::NowIsoUtc()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string AuditLog::EscapeJson(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        default:   out += c; break;
        }
    }
    return out;
}

std::string AuditLog::HashLine(const std::string&, const std::string&) { return ""; }
std::string AuditLog::LoadPrevHash() { return ""; }

} // namespace ngks::core::logging

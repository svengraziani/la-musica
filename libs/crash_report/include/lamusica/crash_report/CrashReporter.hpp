#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::crash_report {

struct CrashReporterOptions {
    std::string_view applicationName;
    std::filesystem::path directory;
};

struct CrashReportRecord {
    std::filesystem::path path;
    std::string contents;
};

std::filesystem::path defaultCrashReportDirectory();
void installCrashReporter(const CrashReporterOptions& options);
[[nodiscard]] std::vector<CrashReportRecord>
collectCrashReports(const std::filesystem::path& directory = defaultCrashReportDirectory(),
                    std::uintmax_t maxBytesPerReport = 65536);

} // namespace lamusica::crash_report

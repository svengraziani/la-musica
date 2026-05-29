#include "lamusica/crash_report/CrashReporter.hpp"

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace lamusica::crash_report {
namespace {

#if defined(__unix__) || defined(__APPLE__)
int crashFd = -1;

void writeBytes(const char* text, std::size_t size) noexcept {
    if (crashFd >= 0) {
        static_cast<void>(::write(crashFd, text, size));
    }
}

template <std::size_t Size>
void writeLiteral(const char (&text)[Size]) noexcept {
    writeBytes(text, Size - 1U);
}

void writeNumber(long value) noexcept {
    std::array<char, 32> buffer{};
    auto index = buffer.size();
    const bool negative = value < 0;
    unsigned long magnitude = negative ? static_cast<unsigned long>(-(value + 1L)) + 1UL
                                       : static_cast<unsigned long>(value);
    do {
        buffer[--index] = static_cast<char>('0' + (magnitude % 10UL));
        magnitude /= 10UL;
    } while (magnitude > 0UL && index > 0U);
    if (negative && index > 0U) {
        buffer[--index] = '-';
    }
    writeBytes(buffer.data() + index, buffer.size() - index);
}

void writeField(std::string_view key, std::string_view value) noexcept {
    writeBytes(key.data(), key.size());
    writeLiteral("=");
    writeBytes(value.data(), value.size());
    writeLiteral("\n");
}

void signalHandler(int signalNumber) {
    writeLiteral("signal=");
    writeNumber(signalNumber);
    writeLiteral("\npid=");
    writeNumber(static_cast<long>(::getpid()));
    writeLiteral("\nbacktrace=deferred\n");
    ::_exit(128 + signalNumber);
}
#endif

std::string sanitizeName(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (const char character : name) {
        result.push_back((character >= 'A' && character <= 'Z') ||
                                 (character >= 'a' && character <= 'z') ||
                                 (character >= '0' && character <= '9') || character == '-' ||
                                 character == '_'
                             ? character
                             : '-');
    }
    return result.empty() ? std::string{"lamusica"} : result;
}

} // namespace

std::filesystem::path defaultCrashReportDirectory() {
    return std::filesystem::temp_directory_path() / "LaMusica Crash Reports";
}

void installCrashReporter(const CrashReporterOptions& options) {
#if defined(__unix__) || defined(__APPLE__)
    const auto directory =
        options.directory.empty() ? defaultCrashReportDirectory() : options.directory;
    std::filesystem::create_directories(directory);

    const auto timestamp = static_cast<long long>(std::time(nullptr));
    const auto path = directory / (sanitizeName(options.applicationName) + "-" +
                                  std::to_string(timestamp) + ".crashlog");

    const int fd = ::open(path.string().c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        return;
    }

    if (crashFd >= 0) {
        ::close(crashFd);
    }
    crashFd = fd;
    writeField("application", options.applicationName);
    writeField("version", options.version);
    writeField("commit", options.gitCommit);
    writeField("buildDate", options.buildDate);
    writeLiteral("reportFormat=lamusica-crashlog-v1\n");

    struct sigaction action {};
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = static_cast<decltype(action.sa_flags)>(SA_RESETHAND);
    sigaction(SIGABRT, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
#else
    static_cast<void>(options);
#endif
}

std::vector<CrashReportRecord> collectCrashReports(const std::filesystem::path& directory,
                                                   std::uintmax_t maxBytesPerReport) {
    std::vector<CrashReportRecord> reports;
    if (directory.empty() || !std::filesystem::is_directory(directory)) {
        return reports;
    }

    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator{directory}) {
        if (entry.is_regular_file() && entry.path().extension() == ".crashlog") {
            paths.push_back(entry.path());
        }
    }
    std::ranges::sort(paths);

    for (const auto& path : paths) {
        std::error_code sizeError;
        const auto fileSize = std::filesystem::file_size(path, sizeError);
        if (sizeError || fileSize > maxBytesPerReport) {
            continue;
        }
        std::ifstream input{path, std::ios::binary};
        if (!input) {
            continue;
        }
        std::string contents;
        contents.resize(static_cast<std::size_t>(fileSize));
        input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (input || input.gcount() == static_cast<std::streamsize>(contents.size())) {
            reports.push_back({.path = path, .contents = std::move(contents)});
        }
    }
    return reports;
}

} // namespace lamusica::crash_report

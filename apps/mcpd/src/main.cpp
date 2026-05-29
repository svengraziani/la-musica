#include "lamusica/mcp_bridge/Capability.hpp"
#include "lamusica/version.hpp"
#include "lamusica/crash_report/CrashReporter.hpp"
#include "lamusica/mcp_bridge/DaemonSession.hpp"
#include "lamusica/mcp_bridge/Protocol.hpp"
#include "lamusica/session/ProjectDocument.hpp"

#include <exception>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

int diagnosticsCrashSmoke() {
#if defined(__unix__) || defined(__APPLE__)
    const auto crashDirectory =
        std::filesystem::temp_directory_path() / "lamusica-mcpd-diagnostics-smoke";
    std::filesystem::remove_all(crashDirectory);
    std::filesystem::create_directories(crashDirectory);

    const auto pid = ::fork();
    if (pid == 0) {
        lamusica::crash_report::installCrashReporter(
            {.applicationName = "lamusica-mcpd-smoke",
             .version = lamusica::build::version,
             .gitCommit = lamusica::build::gitCommit,
             .buildDate = lamusica::build::buildDate,
             .directory = crashDirectory});
        std::raise(SIGABRT);
        return 127;
    }
    if (pid < 0) {
        std::cerr << "lamusica-mcpd diagnostics smoke fork failed\n";
        return 2;
    }

    int status = 0;
    static_cast<void>(::waitpid(pid, &status, 0));
    const auto reports = lamusica::crash_report::collectCrashReports(crashDirectory);
    const bool crashHandlerExited =
        WIFEXITED(status) && WEXITSTATUS(status) == 128 + SIGABRT;
    const bool wroteLocalReport =
        !reports.empty() &&
        reports.front().contents.find("application=lamusica-mcpd-smoke") != std::string::npos &&
        reports.front().contents.find("version=") != std::string::npos &&
        reports.front().contents.find("commit=") != std::string::npos &&
        reports.front().contents.find("buildDate=") != std::string::npos &&
        reports.front().contents.find("reportFormat=lamusica-crashlog-v1") != std::string::npos &&
        reports.front().contents.find("signal=6") != std::string::npos &&
        reports.front().contents.find("backtrace=deferred") != std::string::npos;
    std::filesystem::remove_all(crashDirectory);

    std::cout << "lamusica-mcpd diagnostics crash smoke: localReport="
              << (wroteLocalReport ? "true" : "false")
              << " crashHandlerExited=" << (crashHandlerExited ? "true" : "false")
              << " uploadAttempted=false childStatus=" << status << '\n';
    return wroteLocalReport && crashHandlerExited ? 0 : 3;
#else
    std::cout << "lamusica-mcpd diagnostics crash smoke: skipped unsupported platform\n";
    return 0;
#endif
}

} // namespace

int main(int argc, char** argv) {
    lamusica::crash_report::installCrashReporter(
        {.applicationName = "lamusica-mcpd",
         .version = lamusica::build::version,
         .gitCommit = lamusica::build::gitCommit,
         .buildDate = lamusica::build::buildDate,
         .directory = {}});
    lamusica::mcp_bridge::DaemonSession session;
    if (argc > 1 && std::string{argv[1]} == "--version") {
        std::cout << "lamusica-mcpd " << lamusica::build::version
                  << " commit=" << lamusica::build::gitCommit
                  << " dirty=" << (lamusica::build::gitDirty ? "true" : "false")
                  << " buildDate=" << lamusica::build::buildDate << '\n';
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--diagnostics-crash-smoke") {
        return diagnosticsCrashSmoke();
    }
    if (argc > 1 && std::string{argv[1]} == "--install") {
        const auto label = argc > 2 ? std::string{argv[2]} : std::string{"com.lamusica.mcpd"};
        session.install(label);
        std::cout << "lamusica-mcpd installed label=" << session.launchLabel() << '\n';
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--launch") {
        session.launch();
        std::cout << "lamusica-mcpd launched label=" << session.launchLabel() << '\n';
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--stop") {
        session.stop();
        std::cout << "lamusica-mcpd stopped\n";
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--logs") {
        session.install("com.lamusica.mcpd");
        session.launch();
        for (const auto& entry : session.daemonLog()) {
            std::cout << entry.id << ' ' << entry.event << ' ' << entry.detail << '\n';
        }
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--health") {
        const auto health = session.health();
        std::cout << "lamusica-mcpd health=" << (health.ok ? "ok" : "error")
                  << " state=" << health.message
                  << " lifecycle=" << lamusica::mcp_bridge::toString(session.lifecycleStatus())
                  << '\n';
        return health.ok ? 0 : 1;
    }
    if (argc > 1 && std::string{argv[1]} == "--stdio") {
        std::optional<lamusica::session::ProjectManifest> manifest;
        std::string line;
        while (std::getline(std::cin, line)) {
            const lamusica::mcp_bridge::ProtocolProjectState state{
                .manifest = manifest.has_value() ? &*manifest : nullptr};
            auto response = lamusica::mcp_bridge::handleProtocolLine(session, state, line);
            if (response.ok && session.attached() && line.starts_with("attach ")) {
                try {
                    manifest =
                        lamusica::session::ProjectDocument::open(std::string{session.projectPath()})
                            .manifest();
                } catch (const std::exception& exception) {
                    session.detachProject();
                    manifest.reset();
                    response = {.ok = false, .body = exception.what()};
                }
            } else if (response.ok && !session.attached()) {
                manifest.reset();
            }
            std::cout << lamusica::mcp_bridge::serializeProtocolResponse(response) << '\n';
            std::cout.flush();
        }
        return 0;
    }

    const auto health = session.health();
    std::cout << "lamusica-mcpd health=" << (health.ok ? "ok" : "error")
              << " state=" << health.message
              << " lifecycle=" << lamusica::mcp_bridge::toString(session.lifecycleStatus())
              << " capability="
              << lamusica::mcp_bridge::toString(lamusica::mcp_bridge::Capability::ReadOnly) << '\n';
    return 0;
}

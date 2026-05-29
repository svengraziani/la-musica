#include "lamusica/plugin_host/PluginScanWorker.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lamusica::plugin_host {
namespace {

session::PluginScanOutcome outcomeFromExitCode(int exitCode) noexcept {
    return exitCode == 0 ? session::PluginScanOutcome::Valid : session::PluginScanOutcome::Invalid;
}

std::string failureReasonForExit(int exitCode) {
    return exitCode == 0 ? std::string{} : "worker_exit_" + std::to_string(exitCode);
}

} // namespace

int runMockPluginScanWorkerMode(std::string_view mode) {
    if (mode == "valid") {
        return 0;
    }
    if (mode == "invalid") {
        return 2;
    }
    if (mode == "crash") {
#if defined(__unix__) || defined(__APPLE__)
        (void)kill(getpid(), SIGSEGV);
#endif
        return 101;
    }
    if (mode == "hang") {
        std::this_thread::sleep_for(std::chrono::seconds{10});
        return 102;
    }
    return 103;
}

WorkerProbeResult probePluginWithWorker(const std::filesystem::path& workerPath,
                                        WorkerProbeRequest request,
                                        session::PluginScanPolicy policy) {
    if (workerPath.empty()) {
        throw std::invalid_argument("Plugin scan worker path is required");
    }

    const auto timeout = std::chrono::milliseconds{policy.timeoutMilliseconds};
    const auto started = std::chrono::steady_clock::now();
    WorkerProbeResult result{.candidate = {.description = std::move(request.description)},
                             .processIsolated = true};

#if defined(__unix__) || defined(__APPLE__)
    const auto child = fork();
    if (child < 0) {
        throw std::runtime_error("Failed to fork plugin scan worker");
    }
    if (child == 0) {
        execl(workerPath.c_str(), workerPath.c_str(), "--mock-probe", request.mode.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    const auto deadline = started + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto waited = waitpid(child, &status, WNOHANG);
        if (waited == child) {
            result.elapsedMilliseconds = static_cast<std::uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
            if (WIFSIGNALED(status)) {
                result.signalNumber = WTERMSIG(status);
                result.candidate.outcome = session::PluginScanOutcome::Crashed;
                result.candidate.failureReason =
                    "worker_signal_" + std::to_string(result.signalNumber);
                return result;
            }
            if (WIFEXITED(status)) {
                result.exitCode = WEXITSTATUS(status);
                result.candidate.outcome = outcomeFromExitCode(result.exitCode);
                result.candidate.failureReason = failureReasonForExit(result.exitCode);
                return result;
            }
            result.candidate.outcome = session::PluginScanOutcome::Invalid;
            result.candidate.failureReason = "worker_unknown_status";
            return result;
        }
        if (waited < 0) {
            throw std::runtime_error("Failed waiting for plugin scan worker");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }

    (void)kill(child, SIGKILL);
    (void)waitpid(child, &status, 0);
    result.timedOut = true;
    result.elapsedMilliseconds = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
    result.candidate.outcome = session::PluginScanOutcome::TimedOut;
    result.candidate.failureReason =
        "worker_timed_out_after_" + std::to_string(policy.timeoutMilliseconds) + "ms";
    return result;
#else
    result.processIsolated = false;
    const auto exitCode = runMockPluginScanWorkerMode(request.mode);
    result.exitCode = exitCode;
    result.elapsedMilliseconds = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
    result.candidate.outcome = outcomeFromExitCode(exitCode);
    result.candidate.failureReason = failureReasonForExit(exitCode);
    return result;
#endif
}

} // namespace lamusica::plugin_host

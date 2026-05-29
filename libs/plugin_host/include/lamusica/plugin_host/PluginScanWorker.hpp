#pragma once

#include "lamusica/session/Plugin.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace lamusica::plugin_host {

struct WorkerProbeRequest {
    session::PluginDescription description;
    std::string mode;
};

struct WorkerProbeResult {
    session::PluginScanCandidate candidate;
    bool processIsolated{false};
    bool timedOut{false};
    int exitCode{-1};
    int signalNumber{0};
    std::uint32_t elapsedMilliseconds{0};
};

[[nodiscard]] int runMockPluginScanWorkerMode(std::string_view mode);
[[nodiscard]] WorkerProbeResult probePluginWithWorker(const std::filesystem::path& workerPath,
                                                      WorkerProbeRequest request,
                                                      session::PluginScanPolicy policy = {});

} // namespace lamusica::plugin_host

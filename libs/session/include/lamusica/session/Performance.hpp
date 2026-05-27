#pragma once

#include "lamusica/session/Midi.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace lamusica::session {

struct StressProjectSpec {
    std::size_t tracks{128};
    std::size_t clipsPerTrack{64};
    std::size_t markers{128};
    std::size_t pluginsPerTrack{2};
    std::size_t automationLanesPerTrack{4};
    std::size_t automationPointsPerLane{16};
    std::size_t midiNotesPerMidiClip{32};
    std::size_t assets{256};
    std::size_t mcpAuditEntries{128};
};

struct BenchmarkThresholds {
    double maxStartupMilliseconds{250.0};
    double maxPluginScanMilliseconds{250.0};
    double maxCpuWorkMilliseconds{100.0};
    double maxSaveLoadMilliseconds{500.0};
    double maxQueryMilliseconds{100.0};
    double maxRenderRealtimeFactor{1.0};
    std::size_t maxEstimatedMemoryBytes{256U * 1024U * 1024U};
    std::size_t maxEstimatedDiskBytes{64U * 1024U * 1024U};
};

struct RealtimeSafetyReport {
    bool allocationFree{true};
    bool lockFree{true};
    bool noFileIo{true};
    bool noLogging{true};
    bool noJsonParsing{true};
    bool noMcpWork{true};
    std::vector<std::string> violations;
};

struct StressProjectFixture {
    ProjectManifest manifest;
    std::vector<MidiClipData> midiClips;
};

struct MachineContext {
    std::string cpuModel{"unknown"};
    std::size_t logicalCores{0};
    std::size_t memoryMegabytes{0};
    std::string operatingSystem{"unknown"};
    std::string compiler{"unknown"};
};

struct BenchmarkResult {
    double startupMilliseconds{0.0};
    double pluginScanMilliseconds{0.0};
    double cpuWorkMilliseconds{0.0};
    double saveLoadMilliseconds{0.0};
    double queryMilliseconds{0.0};
    double renderRealtimeFactor{0.0};
    std::size_t estimatedMemoryBytes{0};
    std::size_t estimatedDiskBytes{0};
};

struct BenchmarkReport {
    BenchmarkResult result;
    BenchmarkThresholds thresholds;
    MachineContext machine;
    bool passed{false};
    std::vector<std::string> regressions;
};

struct StressBenchmarkOptions {
    StressProjectSpec stressSpec;
    BenchmarkThresholds thresholds;
    std::uint32_t renderFrames{256};
    double sampleRate{48000.0};
};

[[nodiscard]] ProjectManifest makeStressProject(StressProjectSpec spec);
[[nodiscard]] StressProjectFixture makeStressProjectFixture(StressProjectSpec spec);
[[nodiscard]] RealtimeSafetyReport
validateRealtimeCallbackPolicy(std::span<const std::string> operations);
[[nodiscard]] bool thresholdsArePositive(BenchmarkThresholds thresholds) noexcept;
[[nodiscard]] MachineContext currentMachineContext();
[[nodiscard]] std::size_t estimateStressProjectMemoryBytes(const StressProjectFixture& fixture);
[[nodiscard]] std::size_t estimateStressProjectDiskBytes(const StressProjectFixture& fixture);
[[nodiscard]] BenchmarkReport evaluateBenchmarkResult(BenchmarkResult result,
                                                      BenchmarkThresholds thresholds,
                                                      MachineContext machine = {});
[[nodiscard]] BenchmarkReport runStressBenchmark(StressBenchmarkOptions options = {});

} // namespace lamusica::session

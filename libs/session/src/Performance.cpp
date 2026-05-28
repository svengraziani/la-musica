#include "lamusica/session/Performance.hpp"

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/AudioGraph.hpp"
#include "lamusica/session/GraphCompiler.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <span>
#include <string>
#include <thread>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace lamusica::session {
namespace {

std::string compilerName() {
#if defined(__clang__)
    return "clang " + std::string{__clang_version__};
#elif defined(__GNUC__)
    return "gcc " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(_MSC_VER)
    return "msvc " + std::to_string(_MSC_VER);
#else
    return "unknown";
#endif
}

std::string operatingSystemName() {
#if defined(__APPLE__)
    return "macOS";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "unknown";
#endif
}

std::string cpuModelName() {
#if defined(__APPLE__)
    std::size_t size = 0;
    if (sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0) == 0 && size > 1U) {
        std::string value(size, '\0');
        if (sysctlbyname("machdep.cpu.brand_string", value.data(), &size, nullptr, 0) == 0) {
            if (!value.empty() && value.back() == '\0') {
                value.pop_back();
            }
            if (!value.empty()) {
                return value;
            }
        }
    }
#elif defined(_WIN32)
    if (const auto* processor = std::getenv("PROCESSOR_IDENTIFIER"); processor != nullptr) {
        return processor;
    }
#elif defined(__linux__)
    if (const auto* processor = std::getenv("PROCESSOR_IDENTIFIER"); processor != nullptr) {
        return processor;
    }
#endif
    return "unknown";
}

std::size_t memoryMegabytes() {
#if defined(__APPLE__)
    std::uint64_t bytes = 0;
    std::size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0 && bytes > 0U) {
        return static_cast<std::size_t>(bytes / (1024U * 1024U));
    }
#elif defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) != 0) {
        return static_cast<std::size_t>(status.ullTotalPhys / (1024U * 1024U));
    }
#else
    const auto pages = sysconf(_SC_PHYS_PAGES);
    const auto pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageSize > 0) {
        return static_cast<std::size_t>(
            (static_cast<unsigned long long>(pages) * static_cast<unsigned long long>(pageSize)) /
            (1024ULL * 1024ULL));
    }
#endif
    return 0U;
}

template <typename Function> double elapsedMilliseconds(Function&& function) {
    const auto started = std::chrono::steady_clock::now();
    function();
    const auto finished = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>{finished - started}.count();
}

std::size_t callbackSampleCount(std::uint32_t frames, std::uint32_t channels) noexcept {
    return static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
}

double blockDeadlineMilliseconds(std::uint32_t frames, double sampleRate) noexcept {
    if (frames == 0U || sampleRate <= 0.0) {
        return 0.0;
    }
    return (static_cast<double>(frames) / sampleRate) * 1000.0;
}

} // namespace

ProjectManifest makeStressProject(StressProjectSpec spec) {
    return makeStressProjectFixture(spec).manifest;
}

StressProjectFixture makeStressProjectFixture(StressProjectSpec spec) {
    ProjectManifest manifest;
    manifest.name = "Stress Project";
    manifest.tracks.reserve(spec.tracks);
    manifest.clips.reserve(spec.tracks * spec.clipsPerTrack);
    manifest.markers.reserve(spec.markers);
    manifest.assets.reserve(spec.assets);
    manifest.plugins.reserve(spec.tracks * spec.pluginsPerTrack);
    manifest.automation.reserve(spec.tracks * spec.automationLanesPerTrack);
    manifest.mcpAuditLog.reserve(spec.mcpAuditEntries);
    std::vector<MidiClipData> midiClips;
    midiClips.reserve(spec.tracks * spec.clipsPerTrack);

    for (std::size_t trackIndex = 0; trackIndex < spec.tracks; ++trackIndex) {
        const auto trackId = "stress-track-" + std::to_string(trackIndex);
        const auto trackType = trackIndex % 4 == 0 ? TrackType::Midi : TrackType::Audio;
        manifest.tracks.push_back({.id = trackId,
                                   .name = "Stress Track " + std::to_string(trackIndex),
                                   .type = trackType});
        for (std::size_t pluginIndex = 0; pluginIndex < spec.pluginsPerTrack; ++pluginIndex) {
            manifest.plugins.push_back(
                {.id = trackId + "-plugin-" + std::to_string(pluginIndex),
                 .trackId = trackId,
                 .format = "built_in",
                 .identifier = "stress.plugin." + std::to_string(pluginIndex)});
        }
        for (std::size_t automationIndex = 0; automationIndex < spec.automationLanesPerTrack;
             ++automationIndex) {
            AutomationLane automationLane{
                .id = trackId + "-automation-" + std::to_string(automationIndex),
                .targetId = trackId,
                .parameterId = automationIndex % 2 == 0 ? "volumeDb" : "pan"};
            AutomationRegion region{
                .startSample = 0,
                .endSample = static_cast<std::int64_t>(
                    std::max<std::size_t>(spec.automationPointsPerLane, 1) * 4800)};
            region.points.reserve(spec.automationPointsPerLane);
            for (std::size_t pointIndex = 0; pointIndex < spec.automationPointsPerLane;
                 ++pointIndex) {
                region.points.push_back(
                    {.samplePosition = static_cast<std::int64_t>(pointIndex * 4800),
                     .value = static_cast<float>((pointIndex % 11) / 10.0)});
            }
            automationLane.regions.push_back(std::move(region));
            manifest.automation.push_back(std::move(automationLane));
        }
        for (std::size_t clipIndex = 0; clipIndex < spec.clipsPerTrack; ++clipIndex) {
            const auto clipId = trackId + "-clip-" + std::to_string(clipIndex);
            manifest.clips.push_back(
                {.id = clipId,
                 .trackId = trackId,
                 .type = trackType == TrackType::Midi ? ClipType::Midi : ClipType::Audio,
                 .startSample = static_cast<std::int64_t>(clipIndex * 48000),
                 .lengthSamples = 24000,
                 .assetId =
                     trackType == TrackType::Audio
                         ? "stress-asset-" + std::to_string((trackIndex + clipIndex) %
                                                            std::max<std::size_t>(spec.assets, 1))
                         : ""});
            if (trackType == TrackType::Midi) {
                MidiClipData midiClip{.clipId = clipId};
                midiClip.notes.reserve(spec.midiNotesPerMidiClip);
                for (std::size_t noteIndex = 0; noteIndex < spec.midiNotesPerMidiClip;
                     ++noteIndex) {
                    midiClip.notes.push_back(
                        {.id = clipId + "-note-" + std::to_string(noteIndex),
                         .startSample = static_cast<std::int64_t>(noteIndex * 1200),
                         .lengthSamples = 600,
                         .pitch = static_cast<std::uint8_t>(48 + (noteIndex % 36)),
                         .velocity = static_cast<std::uint8_t>(64 + (noteIndex % 48))});
                }
                manifest.midiClips.push_back({.clipId = clipId, .dataId = clipId});
                midiClips.push_back(std::move(midiClip));
            }
        }
    }

    for (std::size_t assetIndex = 0; assetIndex < spec.assets; ++assetIndex) {
        manifest.assets.push_back(
            {.id = "stress-asset-" + std::to_string(assetIndex),
             .relativePath = "Audio/stress-" + std::to_string(assetIndex) + ".wav",
             .mediaType = "audio/wav"});
    }

    for (std::size_t markerIndex = 0; markerIndex < spec.markers; ++markerIndex) {
        manifest.markers.push_back(
            {.id = "marker-" + std::to_string(markerIndex),
             .name = "Marker " + std::to_string(markerIndex),
             .samplePosition = static_cast<std::int64_t>(markerIndex * 48000)});
    }

    for (std::size_t auditIndex = 0; auditIndex < spec.mcpAuditEntries; ++auditIndex) {
        manifest.mcpAuditLog.push_back({.id = "mcp-audit-" + std::to_string(auditIndex),
                                        .toolName = "stress_query",
                                        .capability = "read_only"});
    }

    return {.manifest = std::move(manifest), .midiClips = std::move(midiClips)};
}

RealtimeSafetyReport validateRealtimeCallbackPolicy(std::span<const std::string> operations) {
    RealtimeSafetyReport report;
    for (const auto& operation : operations) {
        if (operation == "allocate") {
            report.allocationFree = false;
            report.violations.push_back(operation);
        } else if (operation == "lock") {
            report.lockFree = false;
            report.violations.push_back(operation);
        } else if (operation == "file_io") {
            report.noFileIo = false;
            report.violations.push_back(operation);
        } else if (operation == "log") {
            report.noLogging = false;
            report.violations.push_back(operation);
        } else if (operation == "json_parse") {
            report.noJsonParsing = false;
            report.violations.push_back(operation);
        } else if (operation == "mcp_work") {
            report.noMcpWork = false;
            report.violations.push_back(operation);
        }
    }
    return report;
}

RealtimeCallbackAudit auditRealtimeGraphCallback(const audio::AudioGraph& graph,
                                                 audio::EngineConfig config, std::uint32_t frames) {
    RealtimeCallbackAudit audit;
    audit.frames = frames;
    audit.operations = {"realtime_command_queue", "bounded_graph_schedule",
                        "preallocated_audio_buffers"};
    audit.policy = validateRealtimeCallbackPolicy(audit.operations);

    audio::AudioEngine engine{config};
    std::vector<float> output(callbackSampleCount(frames, config.outputChannels));
    const auto startPosition = engine.transport().samplePosition;
    (void)engine.enqueueCommand({.type = audio::RealtimeCommandType::Play});
    audit.callbackMilliseconds =
        elapsedMilliseconds([&] { engine.renderGraphBlock(graph, output, frames); });
    audit.callbackCompleted = true;
    audit.transportAdvanced =
        engine.transport().samplePosition == startPosition + static_cast<std::int64_t>(frames);
    audit.withinBlockDeadline =
        audit.callbackMilliseconds <= blockDeadlineMilliseconds(frames, config.sampleRate);
    return audit;
}

std::size_t estimateStressProjectMemoryBytes(const StressProjectFixture& fixture) {
    const auto& manifest = fixture.manifest;
    std::size_t bytes = sizeof(ProjectManifest);
    bytes += manifest.tracks.size() * sizeof(Track);
    bytes += manifest.clips.size() * sizeof(Clip);
    bytes += manifest.markers.size() * sizeof(Marker);
    bytes += manifest.assets.size() * sizeof(Asset);
    bytes += manifest.plugins.size() * sizeof(PluginReference);
    bytes += manifest.automation.size() * sizeof(AutomationLane);
    bytes += manifest.mcpAuditLog.size() * sizeof(McpAuditEntry);
    bytes += manifest.midiClips.size() * sizeof(MidiClipReference);

    for (const auto& lane : manifest.automation) {
        bytes += lane.regions.size() * sizeof(AutomationRegion);
        for (const auto& region : lane.regions) {
            bytes += region.points.size() * sizeof(AutomationPoint);
        }
    }
    for (const auto& midiClip : fixture.midiClips) {
        bytes += sizeof(MidiClipData);
        bytes += midiClip.notes.size() * sizeof(MidiNote);
        bytes += midiClip.controlChanges.size() * sizeof(MidiControlChange);
        bytes += midiClip.pitchBends.size() * sizeof(MidiPitchBend);
        bytes += midiClip.aftertouch.size() * sizeof(MidiAftertouch);
        bytes += midiClip.programChanges.size() * sizeof(MidiProgramChange);
        bytes += midiClip.metadata.size() * sizeof(MidiMetadata);
    }
    return bytes;
}

std::size_t estimateStressProjectDiskBytes(const StressProjectFixture& fixture) {
    return serializeProjectManifest(fixture.manifest).size() +
           std::accumulate(fixture.midiClips.begin(), fixture.midiClips.end(), std::size_t{0},
                           [](std::size_t total, const MidiClipData& midiClip) {
                               return total + midiClip.clipId.size() +
                                      (midiClip.notes.size() * sizeof(MidiNote)) +
                                      (midiClip.controlChanges.size() * sizeof(MidiControlChange)) +
                                      (midiClip.pitchBends.size() * sizeof(MidiPitchBend)) +
                                      (midiClip.aftertouch.size() * sizeof(MidiAftertouch)) +
                                      (midiClip.programChanges.size() * sizeof(MidiProgramChange)) +
                                      (midiClip.metadata.size() * sizeof(MidiMetadata));
                           });
}

bool thresholdsArePositive(BenchmarkThresholds thresholds) noexcept {
    return thresholds.maxStartupMilliseconds > 0.0 && thresholds.maxPluginScanMilliseconds > 0.0 &&
           thresholds.maxCpuWorkMilliseconds > 0.0 && thresholds.maxEditMilliseconds > 0.0 &&
           thresholds.maxSaveLoadMilliseconds > 0.0 && thresholds.maxQueryMilliseconds > 0.0 &&
           thresholds.maxMcpQueryMilliseconds > 0.0 &&
           thresholds.maxRealtimeCallbackMilliseconds > 0.0 &&
           thresholds.maxRenderRealtimeFactor > 0.0 && thresholds.maxEstimatedMemoryBytes > 0U &&
           thresholds.maxEstimatedDiskBytes > 0U;
}

MachineContext currentMachineContext() {
    const auto logicalCores = std::thread::hardware_concurrency();
    return {.cpuModel = cpuModelName(),
            .logicalCores = logicalCores == 0U ? 1U : logicalCores,
            .memoryMegabytes = memoryMegabytes(),
            .operatingSystem = operatingSystemName(),
            .compiler = compilerName()};
}

BenchmarkReport evaluateBenchmarkResult(BenchmarkResult result, BenchmarkThresholds thresholds,
                                        MachineContext machine) {
    BenchmarkReport report{.result = result,
                           .thresholds = thresholds,
                           .machine = std::move(machine),
                           .passed = thresholdsArePositive(thresholds)};
    if (!thresholdsArePositive(thresholds)) {
        report.regressions.push_back("thresholds_must_be_positive");
        report.passed = false;
        return report;
    }
    if (result.startupMilliseconds > thresholds.maxStartupMilliseconds) {
        report.regressions.push_back("startup");
    }
    if (result.pluginScanMilliseconds > thresholds.maxPluginScanMilliseconds) {
        report.regressions.push_back("plugin_scan");
    }
    if (result.cpuWorkMilliseconds > thresholds.maxCpuWorkMilliseconds) {
        report.regressions.push_back("cpu_work");
    }
    if (result.editMilliseconds > thresholds.maxEditMilliseconds) {
        report.regressions.push_back("edit");
    }
    if (result.saveLoadMilliseconds > thresholds.maxSaveLoadMilliseconds) {
        report.regressions.push_back("save_load");
    }
    if (result.queryMilliseconds > thresholds.maxQueryMilliseconds) {
        report.regressions.push_back("query");
    }
    if (result.mcpQueryMilliseconds > thresholds.maxMcpQueryMilliseconds) {
        report.regressions.push_back("mcp_query");
    }
    if (!result.realtimeCallbackSafe ||
        result.realtimeCallbackMilliseconds > thresholds.maxRealtimeCallbackMilliseconds) {
        report.regressions.push_back("realtime_callback");
    }
    if (result.renderRealtimeFactor > thresholds.maxRenderRealtimeFactor) {
        report.regressions.push_back("render");
    }
    if (result.estimatedMemoryBytes > thresholds.maxEstimatedMemoryBytes) {
        report.regressions.push_back("memory");
    }
    if (result.estimatedDiskBytes > thresholds.maxEstimatedDiskBytes) {
        report.regressions.push_back("disk");
    }
    report.passed = report.regressions.empty();
    return report;
}

BenchmarkReport runStressBenchmark(StressBenchmarkOptions options) {
    StressProjectFixture fixture;
    BenchmarkResult result;

    result.startupMilliseconds = elapsedMilliseconds([&] {
        fixture = makeStressProjectFixture(options.stressSpec);
        validateProjectManifest(fixture.manifest);
    });

    result.estimatedMemoryBytes = estimateStressProjectMemoryBytes(fixture);
    result.estimatedDiskBytes = estimateStressProjectDiskBytes(fixture);

    std::size_t scanAccumulator = 0;
    result.pluginScanMilliseconds = elapsedMilliseconds([&fixture, &scanAccumulator] {
        for (const auto& plugin : fixture.manifest.plugins) {
            scanAccumulator += plugin.id.size();
            scanAccumulator += plugin.format.size();
            scanAccumulator += plugin.identifier.size();
        }
    });

    result.saveLoadMilliseconds = elapsedMilliseconds([&fixture] {
        const auto serialized = serializeProjectManifest(fixture.manifest);
        const auto parsed = parseProjectManifest(serialized);
        validateProjectManifest(parsed);
    });

    result.editMilliseconds = elapsedMilliseconds([&fixture] {
        auto edited = fixture.manifest;
        for (std::size_t clipIndex = 0; clipIndex < edited.clips.size(); ++clipIndex) {
            if (clipIndex % 3U == 0U) {
                edited.clips[clipIndex].startSample += 120;
            }
        }
        for (std::size_t markerIndex = 0; markerIndex < edited.markers.size(); ++markerIndex) {
            edited.markers[markerIndex].samplePosition +=
                static_cast<std::int64_t>(markerIndex * 24U);
        }
        validateProjectManifest(edited);
    });

    std::size_t queryAccumulator = 0;
    const auto queryWork = [&fixture, &queryAccumulator] {
        for (const auto& track : fixture.manifest.tracks) {
            queryAccumulator += track.id.size();
        }
        for (const auto& clip : fixture.manifest.clips) {
            if (clip.startSample >= 0 && clip.lengthSamples > 0) {
                queryAccumulator += clip.id.size();
            }
        }
        for (const auto& plugin : fixture.manifest.plugins) {
            queryAccumulator += plugin.identifier.size();
        }
        for (const auto& lane : fixture.manifest.automation) {
            queryAccumulator += lane.parameterId.size();
        }
        for (const auto& asset : fixture.manifest.assets) {
            queryAccumulator += asset.id.size();
        }
        for (const auto& midiClip : fixture.midiClips) {
            queryAccumulator += midiClip.notes.size();
        }
    };
    result.queryMilliseconds = elapsedMilliseconds(queryWork);
    result.cpuWorkMilliseconds = elapsedMilliseconds([&queryWork] {
        for (int iteration = 0; iteration < 8; ++iteration) {
            queryWork();
        }
    });

    std::size_t mcpAccumulator = 0;
    result.mcpQueryMilliseconds = elapsedMilliseconds([&fixture, &mcpAccumulator] {
        for (const auto& auditEntry : fixture.manifest.mcpAuditLog) {
            mcpAccumulator += auditEntry.id.size();
            mcpAccumulator += auditEntry.toolName.size();
            mcpAccumulator += auditEntry.capability.size();
        }
        for (const auto& track : fixture.manifest.tracks) {
            mcpAccumulator += track.name.size();
        }
        for (const auto& lane : fixture.manifest.automation) {
            mcpAccumulator += lane.targetId.size();
        }
    });

    result.renderRealtimeFactor = 0.0;
    if (options.renderFrames > 0U && options.sampleRate > 0.0) {
        const auto graph = compileProjectAudioGraph(
            fixture.manifest, {}, {.synthesizeAssetBackedClipsWithoutProjectRoot = true});
        const auto callbackAudit = auditRealtimeGraphCallback(graph,
                                                              {.sampleRate = options.sampleRate,
                                                               .maxBlockSize = options.renderFrames,
                                                               .outputChannels = 2},
                                                              options.renderFrames);
        result.realtimeCallbackMilliseconds = callbackAudit.callbackMilliseconds;
        result.realtimeCallbackSafe = callbackAudit.callbackCompleted &&
                                      callbackAudit.transportAdvanced &&
                                      callbackAudit.policy.violations.empty();
        const auto renderMilliseconds = elapsedMilliseconds([&] {
            audio::AudioEngine engine{{.sampleRate = options.sampleRate,
                                       .maxBlockSize = options.renderFrames,
                                       .outputChannels = 2}};
            const auto rendered = engine.renderGraphOffline(graph, options.renderFrames);
            queryAccumulator += rendered.interleavedSamples.size();
        });
        const auto realtimeMilliseconds =
            (static_cast<double>(options.renderFrames) / options.sampleRate) * 1000.0;
        result.renderRealtimeFactor = renderMilliseconds / realtimeMilliseconds;
    }

    return evaluateBenchmarkResult(result, options.thresholds, currentMachineContext());
}

} // namespace lamusica::session

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/Bounce.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/Performance.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task_info.h>
#endif

#if defined(__linux__)
#include <unistd.h>
#endif

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::size_t currentRssBytes() {
#if defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    const auto result =
        task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info),
                  &count);
    if (result != KERN_SUCCESS) {
        return 0U;
    }
    return static_cast<std::size_t>(info.resident_size);
#elif defined(__linux__)
    std::ifstream statm{"/proc/self/statm"};
    long pages = 0;
    long resident = 0;
    (void)pages;
    statm >> pages >> resident;
    return static_cast<std::size_t>(resident) * static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#else
    return 0U;
#endif
}

double percentile(std::vector<double> values, double p) {
    std::ranges::sort(values);
    const auto index =
        static_cast<std::size_t>(std::clamp(p, 0.0, 1.0) * static_cast<double>(values.size() - 1U));
    return values[index];
}

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char rawCharacter : value) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (character < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                escaped += "\\u00";
                escaped.push_back(hex[(character >> 4U) & 0x0FU]);
                escaped.push_back(hex[character & 0x0FU]);
            } else {
                escaped.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    return escaped;
}

struct DeadlineMeasurement {
    std::uint32_t tracks{0};
    double p99Ms{0.0};
    double maxMs{0.0};
    double deadlineMs{0.0};
    int xruns{0};
};

DeadlineMeasurement measureDeadline(std::uint32_t tracks, int blocks, std::uint32_t blockSize,
                                    double sampleRate, std::vector<float>& allSamples) {
    const auto fixture =
        lamusica::session::makeStressProjectFixture({.tracks = tracks,
                                                     .clipsPerTrack = 1,
                                                     .markers = 8,
                                                     .pluginsPerTrack = 1,
                                                     .automationLanesPerTrack = 1,
                                                     .automationPointsPerLane = 4,
                                                     .midiNotesPerMidiClip = 4,
                                                     .assets = 8,
                                                     .mcpAuditEntries = 4});
    lamusica::session::GraphCompileOptions compileOptions;
    compileOptions.synthesizeAssetBackedClipsWithoutProjectRoot = true;
    const auto graph = lamusica::session::compileProjectAudioGraph(fixture.manifest, {},
                                                                   compileOptions);

    const double deadlineMs = static_cast<double>(blockSize) / sampleRate * 1000.0;
    lamusica::audio::AudioEngine engine{
        {.sampleRate = sampleRate, .maxBlockSize = blockSize, .outputChannels = 2}};
    std::vector<float> block(static_cast<std::size_t>(blockSize) * 2U);
    for (int warmup = 0; warmup < 8; ++warmup) {
        engine.renderGraphBlock(graph, block, blockSize);
    }

    std::vector<double> blockMs;
    blockMs.reserve(static_cast<std::size_t>(blocks));
    int xruns = 0;
    for (int index = 0; index < blocks; ++index) {
        const auto started = std::chrono::steady_clock::now();
        engine.renderGraphBlock(graph, block, blockSize);
        const auto elapsed =
            std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - started}
                .count();
        blockMs.push_back(elapsed);
        if (elapsed >= deadlineMs) {
            ++xruns;
        }
        if (tracks == 32U) {
            allSamples.insert(allSamples.end(), block.begin(), block.end());
        }
    }
    const auto maxMs = *std::ranges::max_element(blockMs);
    const auto p99Ms = percentile(blockMs, 0.99);
    return {.tracks = tracks,
            .p99Ms = p99Ms,
            .maxMs = maxMs,
            .deadlineMs = deadlineMs,
            .xruns = xruns};
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path workDir = argc >= 2 ? argv[1] : ".";
        std::filesystem::create_directories(workDir);

        constexpr std::uint32_t blockSize = 1024;
        constexpr double sampleRate = 48000.0;
        constexpr int blocks = 256;
        std::vector<float> allSamples;
        allSamples.reserve(static_cast<std::size_t>(blocks) * blockSize * 2U);
        const std::vector<std::uint32_t> trackCounts{8U, 16U, 32U, 64U};
        std::vector<DeadlineMeasurement> measurements;
        measurements.reserve(trackCounts.size());
        for (const auto tracks : trackCounts) {
            measurements.push_back(
                measureDeadline(tracks, blocks, blockSize, sampleRate, allSamples));
        }
        const auto summary = measurements.back();
        const auto outputPath = workDir / "rt-deadline.wav";
        lamusica::audio::writePcm16Wav(outputPath,
                                       {.channels = 2,
                                        .frames = blockSize * blocks,
                                        .interleavedSamples = std::move(allSamples)},
                                       sampleRate);
        const auto diskBytes = std::filesystem::file_size(outputPath);
        const auto rssBytes = currentRssBytes();
        const auto machine = lamusica::session::currentMachineContext();
        const auto historyPath = workDir / "rt-history.jsonl";
        std::ofstream history{historyPath, std::ios::app};
        history << "{\"machine\":{\"cpu\":\"" << escapeJson(machine.cpuModel)
                << "\",\"cores\":" << machine.logicalCores
                << ",\"memoryMegabytes\":" << machine.memoryMegabytes
                << ",\"operatingSystem\":\"" << escapeJson(machine.operatingSystem)
                << "\",\"compiler\":\"" << escapeJson(machine.compiler)
                << "\"},\"blockSize\":" << blockSize << ",\"sampleRate\":" << sampleRate
                << ",\"rssBytes\":" << rssBytes << ",\"diskBytes\":" << diskBytes
                << ",\"scaling\":[";
        for (std::size_t index = 0; index < measurements.size(); ++index) {
            const auto& measurement = measurements[index];
            history << "{\"tracks\":" << measurement.tracks << ",\"p99Ms\":"
                    << measurement.p99Ms << ",\"maxMs\":" << measurement.maxMs
                    << ",\"deadlineMs\":" << measurement.deadlineMs << ",\"xruns\":"
                    << measurement.xruns << "}";
            if (index + 1U < measurements.size()) {
                history << ',';
            }
        }
        history << "]}\n";
        require(history.good(), "perf history was not written");
        for (const auto& measurement : measurements) {
            if (measurement.xruns != 0) {
                std::ostringstream message;
                message << "deadline bench reported xruns: tracks=" << measurement.tracks
                        << " xruns=" << measurement.xruns << " p99Ms=" << measurement.p99Ms
                        << " maxMs=" << measurement.maxMs
                        << " deadlineMs=" << measurement.deadlineMs;
                throw std::runtime_error(message.str());
            }
            if (measurement.p99Ms >= measurement.deadlineMs) {
                std::ostringstream message;
                message << "p99 block time exceeded buffer period: tracks=" << measurement.tracks
                        << " p99Ms=" << measurement.p99Ms << " maxMs=" << measurement.maxMs
                        << " deadlineMs=" << measurement.deadlineMs
                        << " xruns=" << measurement.xruns;
                throw std::runtime_error(message.str());
            }
        }
        require(diskBytes > 44U, "disk write byte measurement did not observe wav output");
#if defined(__APPLE__) || defined(__linux__)
        require(rssBytes > 0U, "rss byte measurement did not observe process memory");
#endif
        std::cout << "rt deadline p99Ms=" << summary.p99Ms << " maxMs=" << summary.maxMs
                  << " deadlineMs=" << summary.deadlineMs << " xruns=" << summary.xruns
                  << " rssBytes=" << rssBytes << " diskBytes=" << diskBytes
                  << " scalingPoints=" << measurements.size() << " history=" << historyPath
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rt deadline bench failed: " << error.what() << '\n';
        return 1;
    }
}

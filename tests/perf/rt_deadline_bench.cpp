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
#if defined(__linux__)
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
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

struct DeadlineMeasurement {
    std::uint32_t tracks{0};
    double p99Ms{0.0};
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
    const auto graph = lamusica::session::compileProjectAudioGraph(
        fixture.manifest, {}, {.synthesizeAssetBackedClipsWithoutProjectRoot = true});

    const double deadlineMs = static_cast<double>(blockSize) / sampleRate * 1000.0;
    lamusica::audio::AudioEngine engine{
        {.sampleRate = sampleRate, .maxBlockSize = blockSize, .outputChannels = 2}};
    std::vector<float> block(static_cast<std::size_t>(blockSize) * 2U);
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
    return {.tracks = tracks,
            .p99Ms = percentile(std::move(blockMs), 0.99),
            .deadlineMs = deadlineMs,
            .xruns = xruns};
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path workDir = argc >= 2 ? argv[1] : ".";
        std::filesystem::create_directories(workDir);

        constexpr std::uint32_t blockSize = 512;
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
                    << measurement.p99Ms << ",\"deadlineMs\":" << measurement.deadlineMs
                    << ",\"xruns\":" << measurement.xruns << "}";
            if (index + 1U < measurements.size()) {
                history << ',';
            }
        }
        history << "]}\n";
        require(history.good(), "perf history was not written");
        for (const auto& measurement : measurements) {
            require(measurement.xruns == 0, "deadline bench reported xruns");
            require(measurement.p99Ms < measurement.deadlineMs,
                    "p99 block time exceeded buffer period");
        }
        require(diskBytes > 44U, "disk write byte measurement did not observe wav output");
#if defined(__linux__)
        require(rssBytes > 0U, "rss byte measurement did not observe process memory");
#endif
        std::cout << "rt deadline p99Ms=" << summary.p99Ms
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

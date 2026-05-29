#include "lamusica/commands/Command.hpp"
#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/Bounce.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/StarterProject.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::uint64_t fnv1aBytes(std::span<const std::byte> bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto byte : bytes) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string digestFloats(const std::vector<float>& samples) {
    const auto bytes = std::as_bytes(std::span{samples});
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << fnv1aBytes(bytes);
    return out.str();
}

std::string digestFile(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    require(input.good(), "golden wav was not written: " + path.string());
    std::vector<char> data((std::istreambuf_iterator<char>(input)), {});
    const auto bytes = std::as_bytes(std::span{data.data(), data.size()});
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << fnv1aBytes(bytes);
    return out.str();
}

lamusica::audio::RenderedAudio renderInBlocks(const lamusica::audio::AudioGraph& graph,
                                              std::uint32_t totalFrames, std::uint32_t blockSize) {
    lamusica::audio::AudioEngine engine{
        {.sampleRate = 48000.0, .maxBlockSize = blockSize, .outputChannels = 2}};
    lamusica::audio::RenderedAudio rendered{
        .channels = 2,
        .frames = totalFrames,
        .interleavedSamples = std::vector<float>(static_cast<std::size_t>(totalFrames) * 2U)};
    std::vector<float> block(static_cast<std::size_t>(blockSize) * 2U);
    for (std::uint32_t offset = 0; offset < totalFrames; offset += blockSize) {
        const auto frames = std::min(blockSize, totalFrames - offset);
        std::fill(block.begin(), block.end(), 0.0F);
        engine.renderGraphBlock(graph, block, frames);
        std::copy_n(block.begin(), static_cast<std::size_t>(frames) * 2U,
                    rendered.interleavedSamples.begin() + static_cast<std::size_t>(offset) * 2U);
    }
    return rendered;
}

std::string readText(const std::filesystem::path& path) {
    std::ifstream input{path};
    require(input.good(), "missing committed golden digest: " + path.string());
    std::string value;
    input >> value;
    return value;
}

std::string firstDifferingSampleReport(const std::vector<float>& left,
                                       const std::vector<float>& right, std::uint32_t channels) {
    const auto samples = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < samples; ++index) {
        if (left[index] != right[index]) {
            const auto frame = index / channels;
            const auto channel = index % channels;
            return "firstDifferingFrame=" + std::to_string(frame) +
                   " firstDifferingChannel=" + std::to_string(channel) +
                   " left=" + std::to_string(left[index]) +
                   " right=" + std::to_string(right[index]);
        }
    }
    return "firstDifferingFrame=size leftSamples=" + std::to_string(left.size()) +
           " rightSamples=" + std::to_string(right.size());
}

void requireMatchesGoldenWav(const lamusica::audio::RenderedAudio& actual,
                             const std::filesystem::path& goldenPath) {
    if (!std::filesystem::exists(goldenPath)) {
        return;
    }

    const auto golden = lamusica::audio::readPcm16Wav(goldenPath);
    require(golden.sampleRate == 48000.0, "golden WAV sample rate changed");
    require(golden.audio.channels == actual.channels, "golden WAV channel count changed");
    require(golden.audio.frames == actual.frames, "golden WAV frame count changed");

    lamusica::audio::RenderedAudio quantizedActual = actual;
    for (auto& sample : quantizedActual.interleavedSamples) {
        const auto clamped = std::clamp(sample, -1.0F, 1.0F);
        const auto pcm = static_cast<std::int16_t>(std::lrint(clamped * 32767.0F));
        sample = pcm < 0 ? static_cast<float>(pcm) / 32768.0F
                         : static_cast<float>(pcm) / 32767.0F;
    }
    require(quantizedActual.interleavedSamples == golden.audio.interleavedSamples,
            "golden WAV mismatch " +
                firstDifferingSampleReport(quantizedActual.interleavedSamples,
                                           golden.audio.interleavedSamples, actual.channels));
}

void executeAndRecord(lamusica::commands::CommandHistory& history,
                      lamusica::session::ProjectManifest& manifest,
                      lamusica::commands::CommandPtr command,
                      std::vector<std::string>& serializedCommands) {
    serializedCommands.push_back(command->serialize());
    const auto result = history.execute(manifest, std::move(command));
    require(result.ok, "command failed during live edit: " + result.message);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path workDir = argc >= 2 ? argv[1] : ".";
        const std::filesystem::path sourceDir = argc >= 3 ? argv[2] : ".";
        std::filesystem::create_directories(workDir);

        auto baseManifest =
            lamusica::session::makeFirstTrackStarterManifest("Determinism Fixture");
        baseManifest.trackMix.push_back(
            {.trackId = "drums", .volumeDb = -3.0F, .pan = -0.2F, .muted = false});
        baseManifest.trackMix.push_back(
            {.trackId = "bass", .volumeDb = -6.0F, .pan = 0.25F, .muted = false});

        auto manifest = baseManifest;
        lamusica::commands::CommandHistory history;
        std::vector<std::string> serializedCommands;
        executeAndRecord(history, manifest,
                         lamusica::commands::makeSetClipFadeCommand(
                             "det-fade", "audit-det-fade", "drum-loop", 128, 256),
                         serializedCommands);
        executeAndRecord(history, manifest,
                         lamusica::commands::makeSetClipRenderPropertiesCommand(
                             "det-render-props", "audit-det-render-props", "drum-loop", -4.0F,
                             false, false),
                         serializedCommands);
        executeAndRecord(history, manifest,
                         lamusica::commands::makeDuplicateClipCommand(
                             "det-duplicate", "audit-det-duplicate", "drum-loop",
                             "drum-loop-overlap", 48000),
                         serializedCommands);

        const auto graph = lamusica::session::compileProjectAudioGraph(
            manifest, {}, {.synthesizeAssetBackedClipsWithoutProjectRoot = true});

        constexpr std::uint32_t frames = 96000;
        const auto renderA = renderInBlocks(graph, frames, 128);
        const auto renderB = renderInBlocks(graph, frames, 128);
        const auto renderC = renderInBlocks(graph, frames, 512);
        require(renderA.interleavedSamples == renderB.interleavedSamples,
                "two independent renders diverged " +
                    firstDifferingSampleReport(renderA.interleavedSamples,
                                               renderB.interleavedSamples, renderA.channels));
        require(renderA.interleavedSamples == renderC.interleavedSamples,
                "128-frame and 512-frame renders diverged " +
                    firstDifferingSampleReport(renderA.interleavedSamples,
                                               renderC.interleavedSamples, renderA.channels));

        auto replayedManifest = baseManifest;
        const auto replayReport =
            lamusica::commands::replaySerializedCommands(replayedManifest, serializedCommands);
        require(replayReport.appliedCount == serializedCommands.size(),
                "serialized command replay applied " + std::to_string(replayReport.appliedCount) +
                    " of " + std::to_string(serializedCommands.size()) + " commands");
        require(lamusica::session::serializeProjectManifest(replayedManifest) ==
                    lamusica::session::serializeProjectManifest(manifest),
                "serialized command replay manifest differs from live-edited manifest");
        const auto replayedGraph = lamusica::session::compileProjectAudioGraph(
            replayedManifest, {}, {.synthesizeAssetBackedClipsWithoutProjectRoot = true});
        const auto replayed = renderInBlocks(replayedGraph, frames, 128);
        require(renderA.interleavedSamples == replayed.interleavedSamples,
                "serialized command replay render diverged " +
                    firstDifferingSampleReport(renderA.interleavedSamples,
                                               replayed.interleavedSamples, renderA.channels));

        const auto floatDigest = digestFloats(renderA.interleavedSamples);
        const auto wavPath = workDir / "determinism-golden.wav";
        lamusica::audio::writePcm16Wav(wavPath, renderA, 48000.0);
        const auto pcmDigest = digestFile(wavPath);
        const auto combined = floatDigest + ":" + pcmDigest;
        const auto expected = readText(sourceDir / "tests/determinism/golden/render-golden.sha256");
        requireMatchesGoldenWav(renderA, sourceDir / "tests/determinism/golden/render-golden.wav");
        require(combined == expected, "golden mismatch firstDifferingFrame=unknown expected=" +
                                          expected + " actual=" + combined);

        std::cout << "determinism golden=" << combined << " frames=" << frames << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "determinism test failed: " << error.what() << '\n';
        return 1;
    }
}

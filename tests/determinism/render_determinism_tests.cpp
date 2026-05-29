#include "lamusica/commands/Command.hpp"
#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/Bounce.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/StarterProject.hpp"

#include <algorithm>
#include <array>
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

std::uint32_t rotateRight(std::uint32_t value, std::uint32_t bits) noexcept {
    return (value >> bits) | (value << (32U - bits));
}

std::string sha256Bytes(std::span<const std::byte> bytes) {
    constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
        0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
        0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
        0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

    std::vector<std::uint8_t> padded;
    padded.reserve(bytes.size() + 72U);
    for (const auto byte : bytes) {
        padded.push_back(static_cast<std::uint8_t>(byte));
    }
    const auto bitLength = static_cast<std::uint64_t>(bytes.size()) * 8ULL;
    padded.push_back(0x80U);
    while ((padded.size() % 64U) != 56U) {
        padded.push_back(0U);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded.push_back(static_cast<std::uint8_t>((bitLength >> shift) & 0xffU));
    }

    std::array<std::uint32_t, 8> hash{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

    for (std::size_t offset = 0; offset < padded.size(); offset += 64U) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16U; ++index) {
            const auto base = offset + index * 4U;
            words[index] = (static_cast<std::uint32_t>(padded[base]) << 24U) |
                           (static_cast<std::uint32_t>(padded[base + 1U]) << 16U) |
                           (static_cast<std::uint32_t>(padded[base + 2U]) << 8U) |
                           static_cast<std::uint32_t>(padded[base + 3U]);
        }
        for (std::size_t index = 16U; index < 64U; ++index) {
            const auto s0 = rotateRight(words[index - 15U], 7U) ^
                            rotateRight(words[index - 15U], 18U) ^
                            (words[index - 15U] >> 3U);
            const auto s1 = rotateRight(words[index - 2U], 17U) ^
                            rotateRight(words[index - 2U], 19U) ^
                            (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
        }

        auto a = hash[0];
        auto b = hash[1];
        auto c = hash[2];
        auto d = hash[3];
        auto e = hash[4];
        auto f = hash[5];
        auto g = hash[6];
        auto h = hash[7];
        for (std::size_t index = 0; index < 64U; ++index) {
            const auto s1 = rotateRight(e, 6U) ^ rotateRight(e, 11U) ^ rotateRight(e, 25U);
            const auto choice = (e & f) ^ ((~e) & g);
            const auto temp1 = h + s1 + choice + constants[index] + words[index];
            const auto s0 = rotateRight(a, 2U) ^ rotateRight(a, 13U) ^ rotateRight(a, 22U);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = s0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto word : hash) {
        out << std::setw(8) << word;
    }
    return out.str();
}

std::string digestFloats(const std::vector<float>& samples) {
    const auto bytes = std::as_bytes(std::span{samples});
    return sha256Bytes(bytes);
}

std::string digestFile(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    require(input.good(), "golden wav was not written: " + path.string());
    std::vector<char> data((std::istreambuf_iterator<char>(input)), {});
    const auto bytes = std::as_bytes(std::span{data.data(), data.size()});
    return sha256Bytes(bytes);
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

void writeFloatGolden(const std::filesystem::path& path, const std::vector<float>& samples) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    require(output.good(), "could not write float golden: " + path.string());
    const auto bytes = std::as_bytes(std::span{samples});
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    require(output.good(), "failed while writing float golden: " + path.string());
}

void writeTextFile(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output{path, std::ios::trunc};
    require(output.good(), "could not write text file: " + path.string());
    output << text << '\n';
    require(output.good(), "failed while writing text file: " + path.string());
}

std::vector<float> readFloatGolden(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    require(input.good(), "missing committed float golden: " + path.string());
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
    require(bytes.size() % sizeof(float) == 0U,
            "committed float golden has invalid size: " + path.string());
    std::vector<float> samples(bytes.size() / sizeof(float));
    std::memcpy(samples.data(), bytes.data(), bytes.size());
    return samples;
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

void requireMatchesGoldenFloats(const lamusica::audio::RenderedAudio& actual,
                                const std::filesystem::path& goldenPath) {
    const auto golden = readFloatGolden(goldenPath);
    require(actual.interleavedSamples == golden,
            "golden float mismatch " +
                firstDifferingSampleReport(actual.interleavedSamples, golden, actual.channels));
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
        constexpr std::string_view shaVector{"abc"};
        require(sha256Bytes(std::as_bytes(std::span{shaVector.data(), shaVector.size()})) ==
                    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "SHA-256 helper failed known vector");

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

        lamusica::session::GraphCompileOptions compileOptions;
        compileOptions.synthesizeAssetBackedClipsWithoutProjectRoot = true;
        const auto graph =
            lamusica::session::compileProjectAudioGraph(manifest, {}, compileOptions);

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
        const auto replayedGraph =
            lamusica::session::compileProjectAudioGraph(replayedManifest, {}, compileOptions);
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
        const auto digestPath = sourceDir / "tests/determinism/golden/render-golden.sha256";
        const auto wavGoldenPath = sourceDir / "tests/determinism/golden/render-golden.wav";
        auto expected = readText(digestPath);
        const auto floatGoldenPath =
            sourceDir / "tests/determinism/golden/render-golden.float32";
        if (argc >= 4 && std::string_view{argv[3]} == "--update-golden") {
            writeFloatGolden(floatGoldenPath, renderA.interleavedSamples);
            lamusica::audio::writePcm16Wav(wavGoldenPath, renderA, 48000.0);
            writeTextFile(digestPath, combined);
            expected = combined;
        }
        requireMatchesGoldenFloats(renderA, floatGoldenPath);
        requireMatchesGoldenWav(renderA, wavGoldenPath);
        require(combined == expected, "golden digest mismatch expected=" +
                                          expected + " actual=" + combined);

        std::cout << "determinism golden=" << combined << " frames=" << frames << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "determinism test failed: " << error.what() << '\n';
        return 1;
    }
}

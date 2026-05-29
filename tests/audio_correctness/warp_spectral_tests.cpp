#include "lamusica/session/Warp.hpp"

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/AudioGraph.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/Performance.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

lamusica::audio::RenderedAudio sine(double frequency, std::uint32_t frames) {
    lamusica::audio::RenderedAudio audio{
        .channels = 1, .frames = frames, .interleavedSamples = std::vector<float>(frames)};
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        audio.interleavedSamples[frame] = static_cast<float>(
            std::sin(2.0 * 3.14159265358979323846 * frequency * frame / 48000.0));
    }
    return audio;
}

lamusica::audio::RenderedAudio rampedSine(double frequency, std::uint32_t frames) {
    auto audio = sine(frequency, frames);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const auto envelope =
            0.25F + (0.75F * static_cast<float>(frame) / static_cast<float>(frames - 1U));
        audio.interleavedSamples[frame] *= envelope;
    }
    return audio;
}

lamusica::audio::RenderedAudio impulse(std::uint32_t frames, std::uint32_t impulseFrame) {
    lamusica::audio::RenderedAudio audio{
        .channels = 1, .frames = frames, .interleavedSamples = std::vector<float>(frames)};
    if (impulseFrame < frames) {
        audio.interleavedSamples[impulseFrame] = 1.0F;
    }
    return audio;
}

std::uint32_t peakFrame(const lamusica::audio::RenderedAudio& audio) {
    std::uint32_t frame = 0;
    float peak = 0.0F;
    for (std::uint32_t index = 0; index < audio.frames; ++index) {
        const auto value = std::abs(audio.interleavedSamples[index]);
        if (value > peak) {
            peak = value;
            frame = index;
        }
    }
    return frame;
}

std::vector<float> sineSamples(double frequency, std::uint32_t frames, double sampleRate) {
    std::vector<float> samples(frames);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        samples[frame] = static_cast<float>(
            std::sin(2.0 * 3.14159265358979323846 * frequency * frame / sampleRate));
    }
    return samples;
}

double dominantFrequencyHz(const lamusica::audio::RenderedAudio& audio, double minHz,
                           double maxHz) {
    require(audio.channels == 1U, "spectral fixture must be mono");
    double bestFrequency = 0.0;
    double bestPower = -1.0;
    constexpr double sampleRate = 48000.0;
    const auto frames = static_cast<double>(audio.frames);
    for (int frequency = static_cast<int>(std::ceil(minHz));
         frequency <= static_cast<int>(std::floor(maxHz)); ++frequency) {
        const double omega = 2.0 * 3.14159265358979323846 *
                             (static_cast<double>(frequency) * frames / sampleRate) / frames;
        const double coefficient = 2.0 * std::cos(omega);
        double previous = 0.0;
        double previous2 = 0.0;
        for (const auto sample : audio.interleavedSamples) {
            const double current = static_cast<double>(sample) + coefficient * previous - previous2;
            previous2 = previous;
            previous = current;
        }
        const double power = previous2 * previous2 + previous * previous -
                             coefficient * previous * previous2;
        if (power > bestPower) {
            bestPower = power;
            bestFrequency = static_cast<double>(frequency);
        }
    }
    return bestFrequency;
}

lamusica::audio::RenderedAudio firstFrames(const lamusica::audio::RenderedAudio& audio,
                                           std::uint32_t frames) {
    lamusica::audio::RenderedAudio subset{.channels = audio.channels,
                                          .frames = frames,
                                          .interleavedSamples = std::vector<float>(
                                              static_cast<std::size_t>(frames) * audio.channels)};
    std::copy_n(audio.interleavedSamples.begin(), subset.interleavedSamples.size(),
                subset.interleavedSamples.begin());
    return subset;
}

void requireSameSamples(const lamusica::audio::RenderedAudio& left,
                        const lamusica::audio::RenderedAudio& right,
                        const std::string& message) {
    require(left.channels == right.channels && left.frames == right.frames &&
                left.interleavedSamples.size() == right.interleavedSamples.size(),
            message + " layout mismatch");
    for (std::size_t index = 0; index < left.interleavedSamples.size(); ++index) {
        require(std::abs(left.interleavedSamples[index] - right.interleavedSamples[index]) <
                    0.000001F,
                message + " sample mismatch");
    }
}

void requireHalvesDiffer(const lamusica::audio::RenderedAudio& audio, const std::string& message) {
    require(audio.channels == 1U && audio.frames % 2U == 0U, message + " layout mismatch");
    float maxDelta = 0.0F;
    const auto halfFrames = audio.frames / 2U;
    for (std::uint32_t frame = 0; frame < halfFrames; ++frame) {
        maxDelta = std::max(
            maxDelta,
            std::abs(audio.interleavedSamples[frame] - audio.interleavedSamples[halfFrames + frame]));
    }
    require(maxDelta > 0.01F, message);
}

} // namespace

int main() {
    try {
        const auto source = sine(440.0, 96000);
        const auto stretchSource = rampedSine(440.0, 96000);
        const lamusica::session::WarpState stretchWarp{
            .clipId = "warp-fixture",
            .enabled = true,
            .sourceTempoBpm = 120.0,
            .targetTempoBpm = 60.0,
            .quality = lamusica::session::StretchQuality::High,
            .markers = {{"start", 0, 0}, {"middle", 24000, 48000}, {"end", 48000, 96000}}};
        const auto stretchPlan =
            lamusica::session::makeWarpRenderPlan(stretchWarp, {}, 0, 48000, "stretch-cache.wav");
        const auto stretched = lamusica::session::renderWarpedAudio(stretchSource, stretchPlan);
        const auto stretchedAgain = lamusica::session::renderWarpedAudio(stretchSource, stretchPlan);
        const auto stretchedPreview = lamusica::session::renderWarpPreview(stretchSource, stretchPlan);
        requireSameSamples(stretched, stretchedAgain,
                           "repeated stretch render is not deterministic");
        requireSameSamples(stretched, stretchedPreview,
                           "offline stretch render does not match preview render");
        std::vector<lamusica::session::RenderCacheEntry> renderCache{
            {.clipId = stretchWarp.clipId,
             .cacheKey = stretchPlan.cacheKey,
             .relativePath = "stretch-cache.wav",
             .valid = true}};
        const auto cachedStretchPlan = lamusica::session::makeWarpRenderPlan(
            stretchWarp, renderCache, 0, 48000, "unused-cache.wav");
        require(cachedStretchPlan.cacheHit &&
                    cachedStretchPlan.relativePath == "stretch-cache.wav",
                "identical warp cache key did not reuse the valid render cache entry");
        require(stretched.frames == 96000U, "stretch duration did not match requested ratio");
        const auto stretchedFundamental = dominantFrequencyHz(stretched, 380.0, 500.0);
        require(std::abs(stretchedFundamental - 440.0) < 5.0,
                "time-stretch changed the dominant pitch");
        requireHalvesDiffer(stretched, "time-stretch fell back to repeated source tiling");
        require(lamusica::session::mapSourceToTimeline(stretchWarp, 24000) == 48000,
                "transient did not land at warp marker");

        const auto transientSource = impulse(48000, 24000);
        const auto transientRendered =
            lamusica::session::renderWarpedAudio(transientSource, stretchPlan);
        require(std::abs(static_cast<int>(peakFrame(transientRendered)) - 48000) <= 1,
                "rendered transient peak did not land at warp marker");

        lamusica::audio::AudioGraph warpedPlaybackGraph;
        warpedPlaybackGraph.outputNodeId = "master";
        warpedPlaybackGraph.nodes.push_back({.id = "clip:warped",
                                             .kind = lamusica::audio::GraphNodeKind::Sample,
                                             .startSample = 0,
                                             .lengthSamples = stretched.frames,
                                             .sampleChannels = stretched.channels,
                                             .sampleFrames = stretched.frames,
                                             .sampleRate = 48000.0,
                                             .samples = stretched.interleavedSamples});
        warpedPlaybackGraph.nodes.push_back(
            {.id = "master", .kind = lamusica::audio::GraphNodeKind::Output});
        warpedPlaybackGraph.connections.push_back(
            {.sourceNodeId = "clip:warped", .destinationNodeId = "master", .gain = 1.0F});
        std::vector<float> graphWarpedPlayback(stretched.interleavedSamples.size());
        lamusica::audio::renderGraph(
            warpedPlaybackGraph,
            {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = stretched.channels}, 0,
            stretched.frames, graphWarpedPlayback);
        requireSameSamples(stretched,
                           {.channels = stretched.channels,
                            .frames = stretched.frames,
                            .interleavedSamples = graphWarpedPlayback},
                           "precomputed warped buffer does not match graph playback path");
        lamusica::audio::AudioEngine liveWarpEngine{
            {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = stretched.channels}};
        std::vector<float> liveWarpedPlayback(stretched.interleavedSamples.size());
        for (std::uint32_t offset = 0; offset < stretched.frames; offset += 512U) {
            const auto blockFrames = std::min<std::uint32_t>(512U, stretched.frames - offset);
            auto block = std::span<float>{liveWarpedPlayback}.subspan(
                static_cast<std::size_t>(offset) * stretched.channels,
                static_cast<std::size_t>(blockFrames) * stretched.channels);
            liveWarpEngine.renderGraphBlock(warpedPlaybackGraph, block, blockFrames);
        }
        requireSameSamples(stretched,
                           {.channels = stretched.channels,
                            .frames = stretched.frames,
                            .interleavedSamples = liveWarpedPlayback},
                           "precomputed warped buffer does not match bounded live graph path");
        const auto realtimeWarpAudit = lamusica::session::auditRealtimeGraphCallback(
            warpedPlaybackGraph,
            {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = stretched.channels},
            512);
        require(realtimeWarpAudit.callbackCompleted && realtimeWarpAudit.policy.allocationFree &&
                    realtimeWarpAudit.policy.lockFree && realtimeWarpAudit.policy.noFileIo &&
                    realtimeWarpAudit.policy.noLogging && realtimeWarpAudit.policy.noJsonParsing &&
                    realtimeWarpAudit.policy.noMcpWork,
                "precomputed warped buffer playback failed realtime callback policy audit");

        const auto projectRoot =
            std::filesystem::temp_directory_path() / "lamusica-warp-graphcompiler.Project.lamusica";
        std::filesystem::remove_all(projectRoot);
        std::filesystem::create_directories(projectRoot / "Audio");
        const auto sourcePath = projectRoot / "Audio" / "warp-source.wav";
        lamusica::audio::writePcm16Wav(sourcePath, source, 48000.0);
        lamusica::session::ProjectManifest manifest;
        manifest.name = "Warp GraphCompiler";
        manifest.projectSampleRate = 48000.0;
        manifest.assets = {{.id = "warp-source",
                            .relativePath = "Audio/warp-source.wav",
                            .mediaType = "audio/wav"}};
        manifest.tracks = {{.id = "track-a", .name = "Track A", .type = lamusica::session::TrackType::Audio},
                           {.id = "master", .name = "Master", .type = lamusica::session::TrackType::Master}};
        manifest.clips = {{.id = "warp-clip",
                           .trackId = "track-a",
                           .type = lamusica::session::ClipType::Audio,
                           .startSample = 0,
                           .lengthSamples = 48000,
                           .assetId = "warp-source"}};
        const lamusica::session::WarpState compileWarp{
            .clipId = "warp-clip",
            .enabled = true,
            .sourceTempoBpm = 120.0,
            .targetTempoBpm = 60.0,
            .quality = lamusica::session::StretchQuality::High,
            .markers = {{"start", 0, 0}, {"end", 48000, 96000}}};
        lamusica::session::GraphCompileOptions compileOptions;
        compileOptions.projectRoot = projectRoot;
        compileOptions.warpStates = {compileWarp};
        const auto compiledWarpGraph =
            lamusica::session::compileProjectAudioGraph(manifest, {}, compileOptions);
        std::vector<float> compiledWarpRender(96000);
        lamusica::audio::renderGraph(
            compiledWarpGraph, {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = 1},
            0, 96000, compiledWarpRender);
        const auto decodedSource = lamusica::audio::readPcm16Wav(sourcePath);
        const auto compiledExpectedPlan = lamusica::session::makeWarpRenderPlan(
            compileWarp, {}, 0, 48000, "compile-warp-cache.wav");
        const auto compiledExpected =
            lamusica::session::renderWarpedAudio(decodedSource.audio, compiledExpectedPlan);
        requireSameSamples(compiledExpected,
                           {.channels = 1,
                            .frames = 96000,
                            .interleavedSamples = compiledWarpRender},
                           "GraphCompiler warped clip render does not match precomputed DSP");

        std::filesystem::create_directories(projectRoot / "Cache");
        const auto cachedWarpPath = projectRoot / "Cache" / "cached-warp.wav";
        const lamusica::audio::RenderedAudio cachedWarpAudio{
            .channels = 1,
            .frames = 96000,
            .interleavedSamples = std::vector<float>(96000, 0.25F)};
        lamusica::audio::writePcm16Wav(cachedWarpPath, cachedWarpAudio, 48000.0);
        const auto decodedCachedWarp = lamusica::audio::readPcm16Wav(cachedWarpPath);
        auto cachedCompileOptions = compileOptions;
        cachedCompileOptions.warpRenderCache = {
            {.clipId = compileWarp.clipId,
             .cacheKey = compiledExpectedPlan.cacheKey,
             .relativePath = "Cache/cached-warp.wav",
             .valid = true}};
        const auto cachedCompiledWarpGraph =
            lamusica::session::compileProjectAudioGraph(manifest, {}, cachedCompileOptions);
        std::vector<float> cachedCompiledWarpRender(96000);
        lamusica::audio::renderGraph(
            cachedCompiledWarpGraph,
            {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = 1}, 0, 96000,
            cachedCompiledWarpRender);
        requireSameSamples(decodedCachedWarp.audio,
                           {.channels = 1,
                            .frames = 96000,
                            .interleavedSamples = cachedCompiledWarpRender},
                           "GraphCompiler ignored a valid warped render cache entry");
        require(cachedCompiledWarpRender != compiledWarpRender,
                "warped render cache test did not distinguish cached and recomputed audio");

        const auto staleCachedWarpPath = projectRoot / "Cache" / "stale-warp.wav";
        const lamusica::audio::RenderedAudio staleCachedWarpAudio{
            .channels = 1,
            .frames = 48000,
            .interleavedSamples = std::vector<float>(48000, 0.5F)};
        lamusica::audio::writePcm16Wav(staleCachedWarpPath, staleCachedWarpAudio, 44100.0);
        auto staleCachedCompileOptions = compileOptions;
        staleCachedCompileOptions.warpRenderCache = {
            {.clipId = compileWarp.clipId,
             .cacheKey = compiledExpectedPlan.cacheKey,
             .relativePath = "Cache/stale-warp.wav",
             .valid = true}};
        const auto staleCachedCompiledWarpGraph =
            lamusica::session::compileProjectAudioGraph(manifest, {}, staleCachedCompileOptions);
        std::vector<float> staleCachedCompiledWarpRender(96000);
        lamusica::audio::renderGraph(
            staleCachedCompiledWarpGraph,
            {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = 1}, 0, 96000,
            staleCachedCompiledWarpRender);
        requireSameSamples(compiledExpected,
                           {.channels = 1,
                            .frames = 96000,
                            .interleavedSamples = staleCachedCompiledWarpRender},
                           "GraphCompiler used a stale warped render cache artifact");

        const auto wrongChannelCachedWarpPath = projectRoot / "Cache" / "wrong-channel-warp.wav";
        const lamusica::audio::RenderedAudio wrongChannelCachedWarpAudio{
            .channels = 2,
            .frames = 96000,
            .interleavedSamples = std::vector<float>(96000 * 2U, 0.75F)};
        lamusica::audio::writePcm16Wav(wrongChannelCachedWarpPath, wrongChannelCachedWarpAudio,
                                       48000.0);
        auto wrongChannelCachedCompileOptions = compileOptions;
        wrongChannelCachedCompileOptions.warpRenderCache = {
            {.clipId = compileWarp.clipId,
             .cacheKey = compiledExpectedPlan.cacheKey,
             .relativePath = "Cache/wrong-channel-warp.wav",
             .valid = true}};
        const auto wrongChannelCachedCompiledWarpGraph = lamusica::session::compileProjectAudioGraph(
            manifest, {}, wrongChannelCachedCompileOptions);
        std::vector<float> wrongChannelCachedCompiledWarpRender(96000);
        lamusica::audio::renderGraph(
            wrongChannelCachedCompiledWarpGraph,
            {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = 1}, 0, 96000,
            wrongChannelCachedCompiledWarpRender);
        requireSameSamples(compiledExpected,
                           {.channels = 1,
                            .frames = 96000,
                            .interleavedSamples = wrongChannelCachedCompiledWarpRender},
                           "GraphCompiler used a channel-mismatched warped cache artifact");

        const auto escapedCachedWarpPath = projectRoot.parent_path() / "escaped-warp-cache.wav";
        lamusica::audio::writePcm16Wav(escapedCachedWarpPath, cachedWarpAudio, 48000.0);
        auto escapedCachedCompileOptions = compileOptions;
        escapedCachedCompileOptions.warpRenderCache = {
            {.clipId = compileWarp.clipId,
             .cacheKey = compiledExpectedPlan.cacheKey,
             .relativePath = "../escaped-warp-cache.wav",
             .valid = true}};
        const auto escapedCachedCompiledWarpGraph =
            lamusica::session::compileProjectAudioGraph(manifest, {}, escapedCachedCompileOptions);
        std::vector<float> escapedCachedCompiledWarpRender(96000);
        lamusica::audio::renderGraph(
            escapedCachedCompiledWarpGraph,
            {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = 1}, 0, 96000,
            escapedCachedCompiledWarpRender);
        requireSameSamples(compiledExpected,
                           {.channels = 1,
                            .frames = 96000,
                            .interleavedSamples = escapedCachedCompiledWarpRender},
                           "GraphCompiler used a warped cache path outside the project root");
        std::filesystem::remove(escapedCachedWarpPath);
        std::filesystem::remove_all(projectRoot);

        const lamusica::session::WarpState pitchWarp{
            .clipId = "pitch-fixture",
            .enabled = true,
            .sourceTempoBpm = 120.0,
            .targetTempoBpm = 120.0,
            .pitchShiftSemitones = 12.0F,
            .quality = lamusica::session::StretchQuality::High,
            .markers = {{"start", 0, 0}, {"end", 96000, 96000}}};
        const auto pitchPlan =
            lamusica::session::makeWarpRenderPlan(pitchWarp, {}, 0, 96000, "pitch-cache.wav");
        const auto pitched = lamusica::session::renderWarpedAudio(source, pitchPlan);
        const auto pitchedPreview = lamusica::session::renderWarpPreview(source, pitchPlan);
        requireSameSamples(pitched, pitchedPreview,
                           "offline pitch render does not match preview render");
        require(std::abs(pitchPlan.pitchRatio - 2.0) < 0.0001, "pitch ratio is not one octave up");
        const auto activePitchWindow = firstFrames(pitched, 48000);
        const auto pitchFundamental = dominantFrequencyHz(activePitchWindow, 820.0, 940.0);
        require(std::abs(pitchFundamental - 880.0) < 5.0,
                "rendered pitch fundamental is outside tolerance");
        require(pitched.frames == source.frames, "pitch shift changed rendered duration");

        auto downPitchWarp = pitchWarp;
        downPitchWarp.clipId = "pitch-down-fixture";
        downPitchWarp.pitchShiftSemitones = -12.0F;
        const auto downPitchPlan = lamusica::session::makeWarpRenderPlan(
            downPitchWarp, {}, 0, 96000, "pitch-down-cache.wav");
        const auto downPitched = lamusica::session::renderWarpedAudio(source, downPitchPlan);
        require(downPitchPlan.cacheKey != pitchPlan.cacheKey,
                "pitch edit did not change warp cache key");
        const auto downPitchFundamental = dominantFrequencyHz(downPitched, 190.0, 250.0);
        require(std::abs(downPitchFundamental - 220.0) < 5.0,
                "rendered down-pitch fundamental is outside tolerance");
        require(downPitched.frames == source.frames, "down-pitch shift changed rendered duration");

        lamusica::audio::AudioGraph graph;
        graph.outputNodeId = "master";
        graph.nodes.push_back({.id = "clip:441",
                               .kind = lamusica::audio::GraphNodeKind::Sample,
                               .startSample = 0,
                               .lengthSamples = 48000,
                               .sampleChannels = 1,
                               .sampleFrames = 44100,
                               .sampleRate = 44100.0,
                               .samples = sineSamples(440.0, 44100, 44100.0)});
        graph.nodes.push_back(
            {.id = "master", .kind = lamusica::audio::GraphNodeKind::Output});
        graph.connections.push_back(
            {.sourceNodeId = "clip:441", .destinationNodeId = "master", .gain = 1.0F});
        std::vector<float> rendered441(static_cast<std::size_t>(48000) * 1U);
        lamusica::audio::renderGraph(
            graph, {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = 1}, 0, 48000,
            rendered441);
        const lamusica::audio::RenderedAudio srConverted{.channels = 1,
                                                         .frames = 48000,
                                                         .interleavedSamples = rendered441};
        const auto sampleRateFundamental = dominantFrequencyHz(srConverted, 380.0, 500.0);
        require(std::abs(sampleRateFundamental - 440.0) < 5.0,
                "44.1k sample node did not render at correct pitch through 48k graph");
        std::cout << "warp spectral pitchHz=" << pitchFundamental
                  << " downPitchHz=" << downPitchFundamental
                  << " stretchHz=" << stretchedFundamental
                  << " stretchedFrames=" << stretched.frames << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "warp spectral test failed: " << error.what() << '\n';
        return 1;
    }
}

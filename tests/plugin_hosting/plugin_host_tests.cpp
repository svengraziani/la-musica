#include "lamusica/plugin_host/PluginHostCapabilities.hpp"
#include "lamusica/plugin_host/PluginScanWorker.hpp"
#include "lamusica/plugin_host/PluginScanCacheFile.hpp"
#include "lamusica/audio/AudioGraph.hpp"
#include "lamusica/audio/Bounce.hpp"
#include "lamusica/session/Performance.hpp"
#include "lamusica/session/Plugin.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

lamusica::audio::AudioGraph mockPluginTransformGraph(float gain) {
    lamusica::audio::AudioGraph graph;
    graph.outputNodeId = "out";
    graph.nodes = {{.id = "source",
                    .kind = lamusica::audio::GraphNodeKind::Sine,
                    .frequencyHz = 220.0,
                    .gain = gain,
                    .lengthSamples = 512},
                   {.id = "out", .kind = lamusica::audio::GraphNodeKind::Output}};
    graph.connections = {{.sourceNodeId = "source", .destinationNodeId = "out", .gain = 1.0F}};
    return graph;
}

float mockPluginGain(const lamusica::session::PluginInsertChain& chain,
                     std::string_view insertId) {
    const auto* insert = lamusica::session::findInsert(chain, insertId);
    require(insert != nullptr, "mock plugin insert missing from parsed state");
    const auto gain = lamusica::session::findParameterValue(*insert, "gain");
    require(gain.has_value(), "mock plugin gain parameter missing from parsed state");
    return *gain;
}

lamusica::audio::RenderedAudio renderMockPluginInsert(
    const lamusica::session::PluginInsertChain& chain) {
    return lamusica::audio::renderGraphRange(
        mockPluginTransformGraph(mockPluginGain(chain, "insert-a")),
        {.outputPath = "unused.wav", .frames = 512, .channels = 2});
}

void requireByteIdenticalAudio(const lamusica::audio::RenderedAudio& left,
                               const lamusica::audio::RenderedAudio& right,
                               std::string_view label) {
    require(left.channels == right.channels, std::string{label} + " channel-count mismatch");
    require(left.frames == right.frames, std::string{label} + " frame-count mismatch");
    require(left.interleavedSamples == right.interleavedSamples,
            std::string{label} + " rendered samples were not byte-identical");
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(argc >= 2, "plugin host tests require <plugin-scan-worker>");
        const std::filesystem::path workerPath{argv[1]};
        const auto environment = lamusica::plugin_host::probePluginHostEnvironment(
            {.vst3SdkPresent = true, .vst3LicenseAccepted = false, .workerPath = workerPath});
        const auto support = lamusica::session::pluginFormatSupport(environment);
        require(environment.outOfProcessHostingAvailable,
                "plugin host capability probe did not find scan worker");
        require(std::ranges::any_of(support, [](const auto& item) {
                    return item.format == lamusica::session::PluginFormat::BuiltIn &&
                           item.available;
                }),
                "plugin host capability probe did not report built-in support");
        require(std::ranges::any_of(support, [](const auto& item) {
                    return item.format == lamusica::session::PluginFormat::Vst3 &&
                           !item.available && item.reason == "vst3_license_not_accepted";
                }),
                "plugin host capability probe did not preserve VST3 license gate");

        const lamusica::session::PluginDescription mockPlugin{
            .identifier = "lamusica.test.gain",
            .name = "LaMusica Test Gain",
            .vendor = "LaMusica",
            .format = lamusica::session::PluginFormat::BuiltIn,
            .parameters = {{.id = "gain", .name = "Gain", .defaultValue = 0.5F}}};
        constexpr auto probeTimeout = std::chrono::milliseconds{1000};
        const lamusica::session::PluginScanPolicy probePolicy{
            .timeoutMilliseconds = static_cast<std::uint32_t>(probeTimeout.count())};
        const auto validProbe = lamusica::plugin_host::probePluginWithWorker(
            workerPath, {.description = mockPlugin, .mode = "valid"}, probePolicy);
        const auto crashProbe = lamusica::plugin_host::probePluginWithWorker(
            workerPath,
            {.description = {.identifier = "lamusica.test.crash",
                             .name = "Crashing Test Plugin",
                             .vendor = "LaMusica"},
             .mode = "crash"},
            probePolicy);
        const auto timeoutProbe = lamusica::plugin_host::probePluginWithWorker(
            workerPath,
            {.description = {.identifier = "lamusica.test.timeout",
                             .name = "Hanging Test Plugin",
                             .vendor = "LaMusica"},
             .mode = "hang"},
            probePolicy);
        require(validProbe.processIsolated, "valid plugin probe was not process isolated");
        require(crashProbe.processIsolated, "crashing plugin probe was not process isolated");
        require(timeoutProbe.processIsolated, "hanging plugin probe was not process isolated");
        require(validProbe.candidate.outcome == lamusica::session::PluginScanOutcome::Valid,
                "valid plugin probe did not exit cleanly");
        require(crashProbe.candidate.outcome == lamusica::session::PluginScanOutcome::Crashed,
                "crashing plugin probe was not observed as a child-process crash");
        require(timeoutProbe.candidate.outcome == lamusica::session::PluginScanOutcome::TimedOut,
                "hung plugin probe was not killed on timeout");
        require(timeoutProbe.timedOut, "hung plugin probe did not report timeout kill");

        lamusica::session::PluginScanCache cache;
        const std::vector<lamusica::session::PluginScanCandidate> candidates{
            validProbe.candidate, crashProbe.candidate, timeoutProbe.candidate};
        const auto scan = lamusica::session::scanPluginCandidates(cache, candidates, probePolicy);
        require(scan.scanned.size() == 3U, "scanner did not report all plugin candidates");
        require(lamusica::session::findPlugin(cache, "lamusica.test.gain").has_value(),
                "valid mock plugin was not discoverable");
        require(lamusica::session::isBlacklisted(cache, "lamusica.test.crash"),
                "crashing plugin was not isolated in blacklist");
        require(lamusica::session::isBlacklisted(cache, "lamusica.test.timeout"),
                "hung plugin was not isolated in blacklist");
        require(scan.appLaunchSafe, "isolated plugin worker crash should not make app launch unsafe");
        const auto pluginFile = std::filesystem::temp_directory_path() /
                                "lamusica-plugin-host-source-key.test";
        {
            std::ofstream pluginBytes{pluginFile, std::ios::binary | std::ios::trunc};
            pluginBytes << "plugin-binary-v1";
        }
        const auto sourceKey =
            lamusica::plugin_host::scanSourceKeyForFile("lamusica.test.gain", pluginFile);
        lamusica::plugin_host::upsertScanSourceKey(cache, sourceKey);
        require(lamusica::plugin_host::cacheKeyMatches(cache, sourceKey),
                "plugin scan cache did not match current source key");
        const auto cacheText = lamusica::plugin_host::serializeScanCache(cache);
        const auto parsedCache = lamusica::plugin_host::parseScanCache(cacheText);
        require(lamusica::plugin_host::serializeScanCache(parsedCache) == cacheText,
                "plugin scan cache did not round-trip deterministically");
        require(lamusica::plugin_host::cacheKeyMatches(parsedCache, sourceKey),
                "plugin scan cache source key did not round-trip");
        {
            std::ofstream pluginBytes{pluginFile, std::ios::binary | std::ios::app};
            pluginBytes << "-changed";
        }
        const auto changedSourceKey =
            lamusica::plugin_host::scanSourceKeyForFile("lamusica.test.gain", pluginFile);
        require(!lamusica::plugin_host::cacheKeyMatches(parsedCache, changedSourceKey),
                "plugin scan cache failed to invalidate changed plugin source key");
        const auto cachePath = std::filesystem::temp_directory_path() /
                               "lamusica-plugin-host-scan-cache.test";
        lamusica::plugin_host::writeScanCacheFile(cachePath, cache);
        const auto fileCache = lamusica::plugin_host::readScanCacheFile(cachePath);
        require(lamusica::plugin_host::serializeScanCache(fileCache) == cacheText,
                "plugin scan cache file did not round-trip deterministically");
        std::filesystem::remove(cachePath);
        std::filesystem::remove(pluginFile);

        lamusica::session::PluginInsertChain chain{.trackId = "track-a"};
        lamusica::session::addInsert(chain, {.id = "insert-a",
                                             .pluginIdentifier = "lamusica.test.gain",
                                             .parameterValues = {{"gain", 0.25F}}});
        const auto serialized = lamusica::session::serializePluginInsertChain(chain);
        const auto renderBeforeStateRoundTrip = renderMockPluginInsert(chain);
        const auto parsed = lamusica::session::parsePluginInsertChain(serialized);
        require(lamusica::session::serializePluginInsertChain(parsed) == serialized,
                "opaque plugin state did not round-trip byte-identically");
        requireByteIdenticalAudio(renderBeforeStateRoundTrip, renderMockPluginInsert(parsed),
                                  "plugin state round-trip");

        const auto dry = lamusica::audio::renderGraphRange(
            mockPluginTransformGraph(0.25F),
            {.outputPath = "unused.wav", .frames = 512, .channels = 2});
        const auto wet = lamusica::audio::renderGraphRange(
            mockPluginTransformGraph(0.75F),
            {.outputPath = "unused.wav", .frames = 512, .channels = 2});
        require(lamusica::audio::peakAbsoluteSample(wet) >
                    lamusica::audio::peakAbsoluteSample(dry) * 2.0F,
                "mock plugin transform did not affect rendered audio");

        const auto rtAudit = lamusica::session::auditRealtimeGraphCallback(
            mockPluginTransformGraph(0.5F), {.maxBlockSize = 128}, 128);
        require(rtAudit.policy.violations.empty(), "realtime plugin path reported violations");
        require(rtAudit.callbackCompleted && rtAudit.transportAdvanced,
                "realtime plugin callback did not complete and advance");

        std::cout << "plugin-hosting mockProcessed=true stateRoundTrip=true crashIsolated=true "
                     "timeoutIsolated=true rtViolations=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "plugin hosting test failed: " << error.what() << '\n';
        return 1;
    }
}

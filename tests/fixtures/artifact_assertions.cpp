#include "lamusica/audio/Bounce.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/ApplicationSession.hpp"
#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/ProjectDocument.hpp"
#include "lamusica/session/StarterProject.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <fstream>
#include <filesystem>
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

std::uint32_t parseUint32(std::string_view value, std::string_view label) {
    std::size_t consumed = 0;
    const auto parsed = std::stoul(std::string{value}, &consumed);
    require(consumed == value.size(), "invalid " + std::string{label});
    return static_cast<std::uint32_t>(parsed);
}

std::int64_t parseInt64(std::string_view value, std::string_view label) {
    std::size_t consumed = 0;
    const auto parsed = std::stoll(std::string{value}, &consumed);
    require(consumed == value.size(), "invalid " + std::string{label});
    return static_cast<std::int64_t>(parsed);
}

float parseFloat(std::string_view value, std::string_view label) {
    std::size_t consumed = 0;
    const auto parsed = std::stof(std::string{value}, &consumed);
    require(consumed == value.size(), "invalid " + std::string{label});
    return parsed;
}

bool parseBool(std::string_view value, std::string_view label) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw std::runtime_error("invalid " + std::string{label});
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    require(input.good(), "unable to open text file: " + path.string());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void requireContains(std::string_view text, std::string_view needle,
                     std::string_view message) {
    require(text.find(needle) != std::string_view::npos, std::string{message});
}

const lamusica::session::Clip& requireClip(const lamusica::session::ProjectDocument& document,
                                           std::string_view clipId) {
    for (const auto& clip : document.manifest().clips) {
        if (clip.id == clipId) {
            return clip;
        }
    }
    throw std::runtime_error("clip not found: " + std::string{clipId});
}

const lamusica::session::TrackMixState&
requireTrackMix(const lamusica::session::ProjectDocument& document, std::string_view trackId) {
    for (const auto& mix : document.manifest().trackMix) {
        if (mix.trackId == trackId) {
            return mix;
        }
    }
    throw std::runtime_error("track mix not found: " + std::string{trackId});
}

void assertWav(int argc, char** argv) {
    require(argc == 6, "assert-wav requires <path> <frames> <bits> <min-peak>");
    const auto wav = lamusica::audio::readPcm16Wav(argv[2]);
    const auto expectedFrames = parseUint32(argv[3], "frames");
    const auto expectedBits = parseUint32(argv[4], "bits");
    const auto minPeak = parseFloat(argv[5], "minimum peak");
    require(wav.audio.frames == expectedFrames, "wav frame count mismatch");
    require(wav.bitsPerSample == expectedBits, "wav bit depth mismatch");
    require(lamusica::audio::peakAbsoluteSample(wav.audio) >= minPeak, "wav is silent");
    std::cout << "artifact wav ok path=" << std::filesystem::path{argv[2]}
              << " frames=" << wav.audio.frames << " bits=" << wav.bitsPerSample
              << " peak=" << lamusica::audio::peakAbsoluteSample(wav.audio) << '\n';
}

void assertStems(int argc, char** argv) {
    require(argc == 7, "assert-stems requires <dir> <count> <frames> <bits> <min-peak>");
    const std::filesystem::path directory{argv[2]};
    require(std::filesystem::is_directory(directory), "stem directory is missing");
    const auto expectedCount = parseUint32(argv[3], "stem count");
    const auto expectedFrames = parseUint32(argv[4], "frames");
    const auto expectedBits = parseUint32(argv[5], "bits");
    const auto minPeak = parseFloat(argv[6], "minimum peak");
    std::uint32_t count = 0;
    float peak = 0.0F;
    for (const auto& entry : std::filesystem::directory_iterator{directory}) {
        if (!entry.is_regular_file() || entry.path().extension() != ".wav") {
            continue;
        }
        const auto wav = lamusica::audio::readPcm16Wav(entry.path());
        require(wav.audio.frames == expectedFrames, "stem frame count mismatch");
        require(wav.bitsPerSample == expectedBits, "stem bit depth mismatch");
        peak = std::max(peak, lamusica::audio::peakAbsoluteSample(wav.audio));
        ++count;
    }
    require(count == expectedCount, "stem count mismatch");
    require(peak >= minPeak, "stems are silent");
    std::cout << "artifact stems ok path=" << directory << " count=" << count
              << " frames=" << expectedFrames << " bits=" << expectedBits << " peak=" << peak
              << '\n';
}

void assertProject(int argc, char** argv) {
    require(argc == 5, "assert-project requires <path> <tracks> <clips>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    require(document.manifest().tracks.size() == parseUint32(argv[3], "tracks"),
            "project track count mismatch");
    require(document.manifest().clips.size() == parseUint32(argv[4], "clips"),
            "project clip count mismatch");
    std::cout << "artifact project ok path=" << document.path()
              << " tracks=" << document.manifest().tracks.size()
              << " clips=" << document.manifest().clips.size() << '\n';
}

void assertFirstTrackReady(int argc, char** argv) {
    require(argc == 8,
            "assert-first-track-ready requires <path> <render-frames> <tracks> <clips> <plugins> "
            "<automation>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const auto readiness = lamusica::session::inspectFirstTrackReadiness(document.manifest());
    require(readiness.starterStructureReady, "first-track starter structure is not ready");
    require(readiness.firstTrackEditable, "first-track project is not editable");
    require(readiness.renderable, "first-track project is not renderable");
    require(readiness.loopReady, "first-track loop is not ready");
    require(readiness.renderFrames == parseUint32(argv[3], "render frames"),
            "first-track render frame count mismatch");
    require(readiness.trackCount == parseUint32(argv[4], "tracks"),
            "first-track track count mismatch");
    require(readiness.clipCount == parseUint32(argv[5], "clips"),
            "first-track clip count mismatch");
    require(readiness.pluginCount == parseUint32(argv[6], "plugins"),
            "first-track plugin count mismatch");
    require(readiness.automationLaneCount == parseUint32(argv[7], "automation"),
            "first-track automation count mismatch");
    require(readiness.starterMidiNoteCount == 8U, "first-track MIDI note count mismatch");
    lamusica::session::GraphCompileOptions compileOptions;
    compileOptions.projectRoot = document.path();
    (void)lamusica::session::compileProjectAudioGraph(document.manifest(), {}, compileOptions);
    std::cout << "artifact first-track ready path=" << document.path()
              << " renderFrames=" << readiness.renderFrames << " tracks=" << readiness.trackCount
              << " clips=" << readiness.clipCount << " plugins=" << readiness.pluginCount
              << " automation=" << readiness.automationLaneCount << '\n';
}

void assertFirstTrackState(int argc, char** argv) {
    require(argc == 10,
            "assert-first-track-state requires <path> <render-frames> <bass-transpose> "
            "<loop-enabled> <loop-start> <loop-end> <tracks> <clips>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const auto readiness = lamusica::session::inspectFirstTrackReadiness(document.manifest());
    require(readiness.firstTrackEditable, "first-track project is not editable");
    require(readiness.renderable, "first-track project is not renderable");
    require(readiness.renderFrames == parseUint32(argv[3], "render frames"),
            "first-track render frame count mismatch");
    require(readiness.starterBassTransposeSemitones ==
                static_cast<int>(parseInt64(argv[4], "bass transpose")),
            "first-track bass transpose mismatch");
    require(document.manifest().loopEnabled == parseBool(argv[5], "loop enabled"),
            "first-track loop enabled mismatch");
    require(document.manifest().loopStartSample == parseInt64(argv[6], "loop start"),
            "first-track loop start mismatch");
    require(document.manifest().loopEndSample == parseInt64(argv[7], "loop end"),
            "first-track loop end mismatch");
    require(readiness.trackCount == parseUint32(argv[8], "tracks"),
            "first-track track count mismatch");
    require(readiness.clipCount == parseUint32(argv[9], "clips"),
            "first-track clip count mismatch");
    std::cout << "artifact first-track state ok path=" << document.path()
              << " renderFrames=" << readiness.renderFrames
              << " bassTranspose=" << readiness.starterBassTransposeSemitones
              << " loopEnabled=" << (document.manifest().loopEnabled ? "true" : "false")
              << " loopStart=" << document.manifest().loopStartSample
              << " loopEnd=" << document.manifest().loopEndSample
              << " tracks=" << readiness.trackCount << " clips=" << readiness.clipCount << '\n';
}

void assertClipGain(int argc, char** argv) {
    require(argc == 5, "assert-clip-gain requires <path> <clip-id> <gain-db>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const std::string_view clipId{argv[3]};
    const auto expectedGain = parseFloat(argv[4], "gain dB");
    const auto& clip = requireClip(document, clipId);
    require(std::abs(clip.gainDb - expectedGain) < 0.0001F, "clip gain mismatch");
    std::cout << "artifact clip gain ok path=" << document.path() << " clip=" << clip.id
              << " gainDb=" << clip.gainDb << '\n';
}

void assertClipTiming(int argc, char** argv) {
    require(argc == 7,
            "assert-clip-timing requires <path> <clip-id> <start> <length> <source-offset>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const auto& clip = requireClip(document, argv[3]);
    require(clip.startSample == parseInt64(argv[4], "start sample"), "clip start mismatch");
    require(clip.lengthSamples == parseInt64(argv[5], "length samples"), "clip length mismatch");
    require(clip.sourceOffsetSamples == parseInt64(argv[6], "source offset samples"),
            "clip source offset mismatch");
    std::cout << "artifact clip timing ok path=" << document.path() << " clip=" << clip.id
              << " startSample=" << clip.startSample << " lengthSamples=" << clip.lengthSamples
              << " sourceOffsetSamples=" << clip.sourceOffsetSamples << '\n';
}

void assertClipFades(int argc, char** argv) {
    require(argc == 6, "assert-clip-fades requires <path> <clip-id> <fade-in> <fade-out>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const auto& clip = requireClip(document, argv[3]);
    require(clip.fadeInSamples == parseInt64(argv[4], "fade-in samples"),
            "clip fade-in mismatch");
    require(clip.fadeOutSamples == parseInt64(argv[5], "fade-out samples"),
            "clip fade-out mismatch");
    std::cout << "artifact clip fades ok path=" << document.path() << " clip=" << clip.id
              << " fadeInSamples=" << clip.fadeInSamples
              << " fadeOutSamples=" << clip.fadeOutSamples << '\n';
}

void assertClipMuted(int argc, char** argv) {
    require(argc == 5, "assert-clip-muted requires <path> <clip-id> <true|false>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const auto& clip = requireClip(document, argv[3]);
    require(clip.muted == parseBool(argv[4], "muted"), "clip muted state mismatch");
    std::cout << "artifact clip muted ok path=" << document.path() << " clip=" << clip.id
              << " muted=" << (clip.muted ? "true" : "false") << '\n';
}

void assertClipReversed(int argc, char** argv) {
    require(argc == 5, "assert-clip-reversed requires <path> <clip-id> <true|false>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const auto& clip = requireClip(document, argv[3]);
    require(clip.reversed == parseBool(argv[4], "reversed"), "clip reversed state mismatch");
    std::cout << "artifact clip reversed ok path=" << document.path() << " clip=" << clip.id
              << " reversed=" << (clip.reversed ? "true" : "false") << '\n';
}

void assertClipMissing(int argc, char** argv) {
    require(argc == 4, "assert-clip-missing requires <path> <clip-id>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const std::string_view clipId{argv[3]};
    for (const auto& clip : document.manifest().clips) {
        require(clip.id != clipId, "clip should be absent: " + std::string{clipId});
    }
    std::cout << "artifact clip missing ok path=" << document.path() << " clip=" << clipId
              << '\n';
}

void assertTrackMix(int argc, char** argv) {
    require(argc == 8,
            "assert-track-mix requires <path> <track-id> <volume-db> <pan> <muted> <solo>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    const auto& mix = requireTrackMix(document, argv[3]);
    require(std::abs(mix.volumeDb - parseFloat(argv[4], "volume dB")) < 0.0001F,
            "track mix volume mismatch");
    require(std::abs(mix.pan - parseFloat(argv[5], "pan")) < 0.0001F, "track mix pan mismatch");
    require(mix.muted == parseBool(argv[6], "muted"), "track mix muted state mismatch");
    require(mix.solo == parseBool(argv[7], "solo"), "track mix solo state mismatch");
    std::cout << "artifact track mix ok path=" << document.path() << " track=" << mix.trackId
              << " volumeDb=" << mix.volumeDb << " pan=" << mix.pan
              << " muted=" << (mix.muted ? "true" : "false")
              << " solo=" << (mix.solo ? "true" : "false") << '\n';
}

void assertAssets(int argc, char** argv) {
    require(argc == 5, "assert-assets requires <path> <available-count> <missing-count>");
    const auto document = lamusica::session::ProjectDocument::open(argv[2]);
    std::uint32_t available = 0;
    std::uint32_t missing = 0;
    for (const auto& asset : document.manifest().assets) {
        require(!asset.relativePath.empty() && !asset.relativePath.is_absolute(),
                "project asset path is unsafe");
        if (std::filesystem::exists(document.path() / asset.relativePath)) {
            ++available;
        } else {
            ++missing;
        }
    }
    require(available == parseUint32(argv[3], "available assets"),
            "available asset count mismatch");
    require(missing == parseUint32(argv[4], "missing assets"), "missing asset count mismatch");
    std::cout << "artifact assets ok path=" << document.path() << " available=" << available
              << " missing=" << missing << '\n';
}

void assertPackage(int argc, char** argv) {
    require(argc == 8,
            "assert-package requires <dir> <render-frames> <stems> <tracks> <clips> <assets>");
    lamusica::session::ApplicationSession session;
    const auto result = session.verifyFirstTrackPackage(argv[2]);
    require(result.renderFrames == parseUint32(argv[3], "render frames"),
            "package render frame count mismatch");
    require(result.stemCount == parseUint32(argv[4], "stems"), "package stem count mismatch");
    require(result.trackCount == parseUint32(argv[5], "tracks"), "package track count mismatch");
    require(result.clipCount == parseUint32(argv[6], "clips"), "package clip count mismatch");
    require(result.projectAssetCount == parseUint32(argv[7], "assets"),
            "package asset count mismatch");
    require(result.projectSnapshotVerified, "package project snapshot was not verified");
    require(std::filesystem::exists(result.manifestPath), "package manifest is missing");
    require(std::filesystem::exists(result.projectSnapshotPath), "package project snapshot missing");
    std::cout << "artifact package ok path=" << result.packageDirectory
              << " renderFrames=" << result.renderFrames << " stems=" << result.stemCount
              << " tracks=" << result.trackCount << " clips=" << result.clipCount
              << " assets=" << result.projectAssetCount << '\n';
}

bool isProjectBundlePath(const std::filesystem::path& path) {
    return path.extension() == ".lamusica" &&
           std::filesystem::exists(path / lamusica::session::ProjectDocument::manifestFileName);
}

std::vector<std::filesystem::path> findProjectBundles(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> projects;
    if (isProjectBundlePath(root)) {
        projects.push_back(root);
        return projects;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator{root}) {
        if (entry.is_directory() && isProjectBundlePath(entry.path())) {
            projects.push_back(entry.path());
        }
    }
    return projects;
}

void assertExamples(int argc, char** argv) {
    require(argc == 4, "assert-examples requires <examples-dir> <count>");
    const auto projects = findProjectBundles(argv[2]);
    require(projects.size() == parseUint32(argv[3], "example count"),
            "example project count mismatch");
    for (const auto& projectPath : projects) {
        const auto document = lamusica::session::ProjectDocument::open(projectPath);
        lamusica::session::validateProjectManifest(document.manifest());
        for (const auto& asset : document.manifest().assets) {
            require(!asset.relativePath.empty() && !asset.relativePath.is_absolute(),
                    "example asset path is unsafe");
            require(std::filesystem::exists(document.path() / asset.relativePath),
                    "example asset file is missing");
        }
    }
    std::cout << "artifact examples ok root=" << std::filesystem::path{argv[2]}
              << " count=" << projects.size() << '\n';
}

void assertCliJson(int argc, char** argv) {
    require(argc == 4,
            "assert-cli-json requires <json-file> "
            "<query|query-arbitrary|query-escaped|preview|preview-arbitrary|render|schema|migrate>");
    const auto json = readTextFile(argv[2]);
    const std::string_view kind{argv[3]};
    requireContains(json, "\"schemaVersion\":1", "cli json schemaVersion mismatch");
    for (const auto character : json) {
        const auto byte = static_cast<unsigned char>(character);
        require(character == '\n' || byte >= 0x20U, "cli json contains a raw control byte");
    }
    if (kind == "query" || kind == "query-arbitrary" || kind == "query-escaped") {
        requireContains(json, "\"project\":{", "cli query json missing project object");
        requireContains(json, "\"name\":", "cli query json missing project name");
        requireContains(json, "\"tracks\":", "cli query json missing track count");
        requireContains(json, "\"clips\":", "cli query json missing clip count");
        requireContains(json, "\"plugins\":", "cli query json missing plugin count");
        requireContains(json, "\"automation\":", "cli query json missing automation count");
        requireContains(json, "\"trackIds\":[", "cli query json missing track ids");
        requireContains(json, "\"clipIds\":[", "cli query json missing clip ids");
        if (kind == "query-arbitrary") {
            requireContains(json, "\"track-custom\"", "cli arbitrary query json missing custom track id");
            requireContains(json, "\"custom-clip-alpha\"",
                            "cli arbitrary query json missing custom clip id");
        }
        if (kind == "query-escaped") {
            requireContains(json, "\\\"Escaped\\\"", "cli query json did not escape quotes");
            requireContains(json, "\\\\ Path", "cli query json did not escape backslash");
            requireContains(json, "\\t", "cli query json did not escape tab");
            requireContains(json, "\\u0001", "cli query json did not escape control byte");
        }
    } else if (kind == "preview" || kind == "preview-arbitrary") {
        requireContains(json, "\"preview\":true", "cli preview json missing preview=true");
        requireContains(json, "\"mutated\":false", "cli preview json missing mutated=false");
        requireContains(json, "\"command\":\"set-clip-gain\"",
                        "cli preview json missing command");
        if (kind == "preview-arbitrary") {
            requireContains(json, "\"clipId\":\"custom-clip-alpha\"",
                            "cli arbitrary preview json missing custom clip id");
            requireContains(
                json, "\"confirmationToken\":\"set-clip-gain:clip-custom-clip-alpha:confirm\"",
                "cli arbitrary preview json missing confirmation token");
        } else {
            requireContains(json, "\"confirmationToken\":\"set-clip-gain:clip-drum-loop:confirm\"",
                            "cli preview json missing confirmation token");
        }
    } else if (kind == "render") {
        requireContains(json, "\"render\":{", "cli render json missing render object");
        requireContains(json, "\"format\":\"wav\"", "cli render json missing wav format");
        requireContains(json, "\"bitDepth\":24", "cli render json missing 24-bit depth");
        requireContains(json, "\"startSample\":0", "cli render json missing start sample");
        requireContains(json, "\"frames\":48000", "cli render json missing frame count");
        requireContains(json, "\"postDitherPeak\":", "cli render json missing post-dither peak");
    } else if (kind == "schema") {
        requireContains(json, "\"projectSchemaVersion\":", "cli schema json missing project schema");
        requireContains(json, "\"cliSchemaVersion\":1", "cli schema json missing cli schema");
    } else if (kind == "migrate") {
        requireContains(json, "\"migrated\":true", "cli migrate json missing migrated=true");
        requireContains(json, "\"from\":0", "cli migrate json missing legacy source schema");
        requireContains(json,
                        "\"to\":" +
                            std::to_string(lamusica::session::currentProjectSchemaVersion),
                        "cli migrate json missing current target schema");
    } else {
        throw std::runtime_error("unknown cli json kind: " + std::string{kind});
    }
    std::cout << "artifact cli json ok path=" << std::filesystem::path{argv[2]}
              << " kind=" << kind << '\n';
}

void assertCliText(int argc, char** argv) {
    require(argc == 4,
            "assert-cli-text requires <text-file> "
            "<first-track-clips|first-track-takes|first-track-mix|inspect-ready|inspect-missing-media>");
    const auto text = readTextFile(argv[2]);
    const std::string_view kind{argv[3]};
    if (kind == "first-track-clips") {
        requireContains(text, "first-track clips:", "clip listing missing header");
        requireContains(text, "count=2", "clip listing missing initial clip count");
        requireContains(text, "firstTrackEditable=true", "clip listing missing editable status");
        requireContains(text, "mediaReady=true", "clip listing missing media readiness");
        requireContains(text, "clip clipId=drum-loop", "clip listing missing drum loop");
        requireContains(text, "clip clipId=bass-pattern", "clip listing missing bass pattern");
    } else if (kind == "first-track-clips-edited") {
        requireContains(text, "first-track clips:", "edited clip listing missing header");
        requireContains(text, "count=5", "edited clip listing missing clip count");
        requireContains(text, "firstTrackEditable=true",
                        "edited clip listing missing editable status");
        requireContains(text, "mediaReady=true", "edited clip listing missing media readiness");
        requireContains(text, "clip clipId=imported-audio-1",
                        "edited clip listing missing imported audio");
        requireContains(text, "clip clipId=imported-audio-copy",
                        "edited clip listing missing duplicated import");
        requireContains(text, "startSample=108000", "edited clip listing missing duplicate start");
        requireContains(text, "sourceOffsetSamples=12000",
                        "edited clip listing missing source offset");
        requireContains(text, "muted=false", "edited clip listing missing unmuted state");
    } else if (kind == "first-track-takes") {
        requireContains(text, "first-track takes:", "take listing missing header");
        requireContains(text, "count=1", "take listing missing take count");
        requireContains(text, "take clipId=recorded-take-1", "take listing missing recorded take");
        requireContains(text, "frames=24000", "take listing missing frame count");
        requireContains(text, "muted=false", "take listing should show unmuted take");
        requireContains(text, "mediaAvailable=true", "take listing missing media status");
    } else if (kind == "first-track-takes-punch") {
        requireContains(text, "first-track takes:", "punch take listing missing header");
        requireContains(text, "count=1", "punch take listing missing take count");
        requireContains(text, "muted=0", "punch take listing missing muted summary");
        requireContains(text, "take clipId=recorded-take-1",
                        "punch take listing missing recorded take");
        requireContains(text, "startSample=48000", "punch take listing missing punch start");
        requireContains(text, "frames=48000", "punch take listing missing punch frame count");
        requireContains(text, "muted=false", "punch take listing should show unmuted take");
        requireContains(text, "mediaAvailable=true", "punch take listing missing media status");
    } else if (kind == "first-track-takes-muted") {
        requireContains(text, "first-track takes:", "muted take listing missing header");
        requireContains(text, "count=1", "muted take listing missing take count");
        requireContains(text, "muted=1", "muted take listing missing muted summary");
        requireContains(text, "take clipId=recorded-take-1",
                        "muted take listing missing recorded take");
        requireContains(text, "muted=true", "muted take listing should show muted take");
        requireContains(text, "mediaAvailable=true", "muted take listing missing media status");
    } else if (kind == "first-track-mix") {
        requireContains(text, "first-track track mix:", "track mix listing missing header");
        requireContains(text, "count=3", "track mix listing missing track count");
        requireContains(text, "track trackId=drums", "track mix listing missing drums");
        requireContains(text, "volumeDb=-12", "track mix listing missing volume");
        requireContains(text, "pan=0.25", "track mix listing missing pan");
        requireContains(text, "muted=false", "track mix listing missing muted flag");
        requireContains(text, "solo=false", "track mix listing missing solo flag");
    } else if (kind == "inspect-ready") {
        requireContains(text, "project name=\"CLI Edited First Track\"",
                        "inspect output missing project name");
        requireContains(text, "schemaVersion=", "inspect output missing schema version");
        requireContains(text, "tracks=5", "inspect output missing track count");
        requireContains(text, "clips=5", "inspect output missing clip count");
        requireContains(text, "firstTrackReady=true", "inspect output missing ready status");
        requireContains(text, "firstTrackEditable=true", "inspect output missing editable status");
        requireContains(text, "mediaReady=true", "inspect output missing media readiness");
        requireContains(text, "renderFrames=192000", "inspect output missing render frames");
    } else if (kind == "inspect-missing-media") {
        requireContains(text, "project name=\"CLI Edited First Track\"",
                        "missing-media inspect output missing project name");
        requireContains(text, "firstTrackReady=false",
                        "missing-media inspect output should not be ready");
        requireContains(text, "firstTrackEditable=false",
                        "missing-media inspect output should not be editable");
        requireContains(text, "mediaReady=false",
                        "missing-media inspect output missing media failure");
        requireContains(text, "media error:", "missing-media inspect output missing error detail");
    } else {
        throw std::runtime_error("unknown cli text kind: " + std::string{kind});
    }
    std::cout << "artifact cli text ok path=" << std::filesystem::path{argv[2]}
              << " kind=" << kind << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(argc >= 2, "missing artifact assertion verb");
        const std::string_view verb{argv[1]};
        if (verb == "assert-wav") {
            assertWav(argc, argv);
        } else if (verb == "assert-stems") {
            assertStems(argc, argv);
        } else if (verb == "assert-project") {
            assertProject(argc, argv);
        } else if (verb == "assert-first-track-ready") {
            assertFirstTrackReady(argc, argv);
        } else if (verb == "assert-first-track-state") {
            assertFirstTrackState(argc, argv);
        } else if (verb == "assert-clip-gain") {
            assertClipGain(argc, argv);
        } else if (verb == "assert-clip-timing") {
            assertClipTiming(argc, argv);
        } else if (verb == "assert-clip-fades") {
            assertClipFades(argc, argv);
        } else if (verb == "assert-clip-muted") {
            assertClipMuted(argc, argv);
        } else if (verb == "assert-clip-reversed") {
            assertClipReversed(argc, argv);
        } else if (verb == "assert-clip-missing") {
            assertClipMissing(argc, argv);
        } else if (verb == "assert-track-mix") {
            assertTrackMix(argc, argv);
        } else if (verb == "assert-assets") {
            assertAssets(argc, argv);
        } else if (verb == "assert-package") {
            assertPackage(argc, argv);
        } else if (verb == "assert-examples") {
            assertExamples(argc, argv);
        } else if (verb == "assert-cli-json") {
            assertCliJson(argc, argv);
        } else if (verb == "assert-cli-text") {
            assertCliText(argc, argv);
        } else {
            throw std::runtime_error("unknown artifact assertion verb: " + std::string{verb});
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "artifact assertion failed: " << error.what() << '\n';
        return 1;
    }
}

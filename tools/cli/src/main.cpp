#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/version.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/ApplicationSession.hpp"
#include "lamusica/session/Export.hpp"
#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/Performance.hpp"
#include "lamusica/session/Project.hpp"
#include "lamusica/session/ProjectDocument.hpp"
#include "lamusica/session/StarterProject.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void printUsage() {
    std::cout
        << "usage: lamusica_cli --help\n"
        << "       lamusica_cli query <Project.lamusica> [--json]\n"
        << "       lamusica_cli edit <Project.lamusica> set-clip-gain <clip-id> <gain-db> "
           "[--dry-run] [--confirm <token>]\n"
        << "       lamusica_cli render <Project.lamusica> <output.wav|stems-dir> "
           "[--format wav] [--bit-depth 16|24] [--range start:end] [--stems]\n"
        << "       lamusica_cli schema [--json]\n"
        << "       lamusica_cli migrate <Project.lamusica> [--json]\n"
        << "usage: lamusica_cli create-project <Project.lamusica> <name>\n"
        << "       lamusica_cli create-first-track <Project.lamusica> <name>\n"
        << "       lamusica_cli validate <Project.lamusica>\n"
        << "       lamusica_cli verify-first-track-project <Project.lamusica>\n"
        << "       lamusica_cli transpose-first-track-bass <Project.lamusica> <semitones>\n"
        << "       lamusica_cli set-first-track-clip-gain <Project.lamusica> <clip-id> <gain-db>\n"
        << "       lamusica_cli set-first-track-clip-fades <Project.lamusica> <clip-id> "
           "<fade-in-samples> <fade-out-samples>\n"
        << "       lamusica_cli set-first-track-clip-muted <Project.lamusica> <clip-id> "
           "<true|false>\n"
        << "       lamusica_cli set-first-track-clip-reversed <Project.lamusica> <clip-id> "
           "<true|false>\n"
        << "       lamusica_cli set-first-track-clip-timing <Project.lamusica> <clip-id> "
           "<start-sample> <length-samples> <source-offset-samples>\n"
        << "       lamusica_cli set-first-track-track-mix <Project.lamusica> <track-id> "
           "<volume-db> <pan> <muted> <solo>\n"
        << "       lamusica_cli list-first-track-track-mix <Project.lamusica>\n"
        << "       lamusica_cli duplicate-first-track-clip <Project.lamusica> <clip-id> "
           "<new-clip-id> <start-sample>\n"
        << "       lamusica_cli remove-first-track-clip <Project.lamusica> <clip-id>\n"
        << "       lamusica_cli set-first-track-loop-intro <Project.lamusica>\n"
        << "       lamusica_cli extend-first-track-verse <Project.lamusica>\n"
        << "       lamusica_cli plan-first-track-recording <Project.lamusica> <frames> "
           "[start-sample] [count-in-bars] [punch-in-sample] [punch-out-sample]\n"
        << "       lamusica_cli record-first-track-take <Project.lamusica> <frames> "
           "[start-sample] [count-in-bars] [punch-in-sample] [punch-out-sample]\n"
        << "       lamusica_cli list-first-track-clips <Project.lamusica>\n"
        << "       lamusica_cli list-first-track-takes <Project.lamusica>\n"
        << "       lamusica_cli mute-first-track-take <Project.lamusica> <clip-id> <true|false>\n"
        << "       lamusica_cli import-first-track-audio <Project.lamusica> <source.wav> "
           "<start-sample>\n"
        << "       lamusica_cli relink-first-track-audio <Project.lamusica> <asset-id> "
           "<source.wav>\n"
        << "       lamusica_cli verify-examples <examples-dir>\n"
        << "       lamusica_cli benchmark-smoke\n"
        << "       lamusica_cli inspect-project <Project.lamusica>\n"
        << "       lamusica_cli render-project <Project.lamusica> <output.wav>\n"
        << "       lamusica_cli export-first-track-package <Project.lamusica> <output-dir>\n"
        << "       lamusica_cli verify-first-track-package <output-dir>\n"
        << "       lamusica_cli render-test-tone <output.wav>\n"
        << "       lamusica_cli inspect-wav <input.wav>\n";
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

std::optional<std::string> missingAssetReport(const lamusica::session::ProjectDocument& document) {
    for (const auto& asset : document.manifest().assets) {
        if (asset.relativePath.is_absolute() || asset.relativePath.empty() ||
            asset.relativePath.string().find("..") != std::string::npos) {
            return "unsafe asset path in " + document.path().string() + ": " +
                   asset.relativePath.string();
        }
        const auto assetPath = document.path() / asset.relativePath;
        if (!std::filesystem::exists(assetPath)) {
            return "missing asset in " + document.path().string() + ": " + asset.id + " -> " +
                   asset.relativePath.string();
        }
    }
    return std::nullopt;
}

int parseInt(std::string_view value, std::string_view label) {
    int parsed{};
    std::istringstream input{std::string{value}};
    if (!(input >> parsed) || !input.eof()) {
        throw std::runtime_error("invalid " + std::string{label} + ": " + std::string{value});
    }
    return parsed;
}

std::uint32_t parseUint32(std::string_view value, std::string_view label) {
    const auto parsed = parseInt(value, label);
    if (parsed < 0) {
        throw std::runtime_error(std::string{label} + " must be non-negative");
    }
    return static_cast<std::uint32_t>(parsed);
}

std::int64_t parseInt64(std::string_view value, std::string_view label) {
    std::int64_t parsed{};
    std::istringstream input{std::string{value}};
    if (!(input >> parsed) || !input.eof()) {
        throw std::runtime_error("invalid " + std::string{label} + ": " + std::string{value});
    }
    return parsed;
}

float parseFloat(std::string_view value, std::string_view label) {
    float parsed{};
    std::istringstream input{std::string{value}};
    if (!(input >> parsed) || !input.eof()) {
        throw std::runtime_error("invalid " + std::string{label} + ": " + std::string{value});
    }
    return parsed;
}

bool parseBool(std::string_view value, std::string_view label) {
    if (value == "true" || value == "1" || value == "yes") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no") {
        return false;
    }
    throw std::runtime_error("invalid " + std::string{label} + ": " + std::string{value});
}

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '"':
        case '\\':
            escaped.push_back('\\');
            escaped.push_back(character);
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
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

bool hasFlag(int argc, char** argv, std::string_view flag) {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == flag) {
            return true;
        }
    }
    return false;
}

std::optional<std::string_view> optionValue(int argc, char** argv, std::string_view option) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::string_view{argv[index]} == option) {
            return std::string_view{argv[index + 1]};
        }
    }
    return std::nullopt;
}

lamusica::session::Clip* findClip(lamusica::session::ProjectManifest& manifest,
                                  std::string_view clipId) {
    const auto found =
        std::ranges::find_if(manifest.clips, [clipId](const lamusica::session::Clip& clip) {
            return clip.id == clipId;
        });
    return found == manifest.clips.end() ? nullptr : &*found;
}

std::string confirmationToken(std::string_view commandId, std::string_view auditId) {
    return std::string{commandId} + ":" + std::string{auditId} + ":confirm";
}

std::pair<std::int64_t, std::uint32_t> parseRange(std::string_view value) {
    const auto separator = value.find(':');
    if (separator == std::string_view::npos) {
        throw std::runtime_error("range must be start:end samples");
    }
    const auto start = parseInt64(value.substr(0, separator), "range start");
    const auto end = parseInt64(value.substr(separator + 1), "range end");
    if (start < 0 || end <= start) {
        throw std::runtime_error("range must be non-negative and non-empty");
    }
    if (end - start > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("range is too large");
    }
    return {start, static_cast<std::uint32_t>(end - start)};
}

std::vector<std::string> audioTrackIds(const lamusica::session::ProjectManifest& manifest) {
    std::vector<std::string> ids;
    for (const auto& track : manifest.tracks) {
        if (track.type != lamusica::session::TrackType::Master) {
            ids.push_back(track.id);
        }
    }
    return ids;
}

lamusica::audio::ExportBitDepth parseExportBitDepth(std::string_view value) {
    if (value == "16") {
        return lamusica::audio::ExportBitDepth::Pcm16;
    }
    if (value == "24") {
        return lamusica::audio::ExportBitDepth::Pcm24;
    }
    throw std::runtime_error("unsupported bit depth: " + std::string{value});
}

int bitDepthValue(lamusica::audio::ExportBitDepth bitDepth) noexcept {
    switch (bitDepth) {
    case lamusica::audio::ExportBitDepth::Pcm16:
        return 16;
    case lamusica::audio::ExportBitDepth::Pcm24:
        return 24;
    }
    return 16;
}

void validateRenderFormat(std::string_view value) {
    if (value != "wav") {
        throw std::runtime_error("unsupported render format: " + std::string{value});
    }
}

lamusica::session::FirstTrackRecordingOptions parseRecordingOptions(int argc, char** argv) {
    lamusica::session::FirstTrackRecordingOptions options;
    options.frames = parseUint32(argv[3], "frames");
    if (argc >= 5) {
        options.startSample = parseInt64(argv[4], "start sample");
    }
    if (argc >= 6) {
        options.countInBars = parseUint32(argv[5], "count-in bars");
    }
    if (argc >= 7) {
        options.punchInSample = parseInt64(argv[6], "punch-in sample");
    }
    if (argc >= 8) {
        options.punchOutSample = parseInt64(argv[7], "punch-out sample");
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view{argv[1]} == "--version") {
        std::cout << "lamusica-cli " << lamusica::build::version
                  << " commit=" << lamusica::build::gitCommit
                  << " dirty=" << (lamusica::build::gitDirty ? "true" : "false")
                  << " buildDate=" << lamusica::build::buildDate << '\n';
        return 0;
    }

    if (argc > 1 &&
        (std::string_view{argv[1]} == "--help" || std::string_view{argv[1]} == "help")) {
        printUsage();
        return 0;
    }

    if ((argc == 2 || argc == 3) && std::string_view{argv[1]} == "schema") {
        const bool json = hasFlag(argc, argv, "--json");
        if (json) {
            std::cout << "{\"schemaVersion\":1,\"projectSchemaVersion\":"
                      << lamusica::session::currentProjectSchemaVersion
                      << ",\"cliSchemaVersion\":1}\n";
        } else {
            std::cout << "schema projectVersion=" << lamusica::session::currentProjectSchemaVersion
                      << " cliSchemaVersion=1\n";
        }
        return 0;
    }

    if ((argc == 3 || argc == 4) && std::string_view{argv[1]} == "query") {
        try {
            const bool json = hasFlag(argc, argv, "--json");
            const auto document = lamusica::session::ProjectDocument::open(argv[2]);
            const auto& manifest = document.manifest();
            if (json) {
                std::cout << "{\"schemaVersion\":1,\"project\":{\"name\":\""
                          << escapeJson(document.project().name())
                          << "\",\"schemaVersion\":" << manifest.schemaVersion
                          << ",\"tracks\":" << manifest.tracks.size()
                          << ",\"clips\":" << manifest.clips.size()
                          << ",\"plugins\":" << manifest.plugins.size()
                          << ",\"automation\":" << manifest.automation.size() << "}}\n";
            } else {
                std::cout << "query project=\"" << document.project().name()
                          << "\" schemaVersion=" << manifest.schemaVersion
                          << " tracks=" << manifest.tracks.size()
                          << " clips=" << manifest.clips.size()
                          << " plugins=" << manifest.plugins.size()
                          << " automation=" << manifest.automation.size() << '\n';
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "query failed: " << error.what() << '\n';
            return 33;
        }
    }

    if ((argc == 3 || argc == 4) && std::string_view{argv[1]} == "migrate") {
        try {
            const bool json = hasFlag(argc, argv, "--json");
            auto document = lamusica::session::ProjectDocument::open(argv[2]);
            const auto before = document.manifest().schemaVersion;
            document.mutableManifest() =
                lamusica::session::migrateProjectManifest(document.manifest());
            lamusica::session::validateProjectManifest(document.manifest());
            document.save();
            if (json) {
                std::cout << "{\"schemaVersion\":1,\"migrated\":true,\"from\":" << before
                          << ",\"to\":" << document.manifest().schemaVersion << "}\n";
            } else {
                std::cout << "migrated project: from=" << before
                          << " to=" << document.manifest().schemaVersion << '\n';
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "migrate failed: " << error.what() << '\n';
            return 34;
        }
    }

    if (argc >= 6 && std::string_view{argv[1]} == "edit" &&
        std::string_view{argv[3]} == "set-clip-gain") {
        try {
            const bool json = hasFlag(argc, argv, "--json");
            const bool dryRun = hasFlag(argc, argv, "--dry-run");
            const auto confirm = optionValue(argc, argv, "--confirm");
            const std::string commandId = "set-clip-gain";
            const std::string auditId = std::string{"clip-"} + argv[4];
            const auto expectedToken = confirmationToken(commandId, auditId);
            auto document = lamusica::session::ProjectDocument::open(argv[2]);
            auto* clip = findClip(document.mutableManifest(), argv[4]);
            if (clip == nullptr) {
                throw std::runtime_error("clip was not found: " + std::string{argv[4]});
            }
            const auto newGain = parseFloat(argv[5], "clip gain dB");
            if (dryRun || !confirm.has_value()) {
                if (json) {
                    std::cout << "{\"schemaVersion\":1,\"preview\":true,\"mutated\":false,"
                              << "\"command\":\"" << commandId << "\",\"project\":\""
                              << escapeJson(document.project().name()) << "\",\"clipId\":\""
                              << escapeJson(argv[4]) << "\",\"fromGainDb\":" << clip->gainDb
                              << ",\"toGainDb\":" << newGain << ",\"confirmationToken\":\""
                              << escapeJson(expectedToken) << "\"}\n";
                } else {
                    std::cout << "preview command=set-clip-gain project=\""
                              << document.project().name() << "\" clip=\"" << argv[4]
                              << "\" fromGainDb=" << clip->gainDb << " toGainDb=" << newGain
                              << " confirmationToken=" << expectedToken << '\n';
                }
                return 0;
            }
            if (*confirm != std::string_view{expectedToken}) {
                throw std::runtime_error("confirmation token did not match preview");
            }
            clip->gainDb = newGain;
            document.save();
            if (json) {
                std::cout << "{\"schemaVersion\":1,\"preview\":false,\"mutated\":true,"
                          << "\"command\":\"" << commandId << "\",\"project\":\""
                          << escapeJson(document.project().name()) << "\",\"clipId\":\""
                          << escapeJson(argv[4]) << "\",\"gainDb\":" << newGain << "}\n";
            } else {
                std::cout << "edited project=\"" << document.project().name()
                          << "\" command=" << commandId << " clip=\"" << argv[4]
                          << "\" gainDb=" << newGain << '\n';
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "edit failed: " << error.what() << '\n';
            return 35;
        }
    }

    if (argc >= 4 && std::string_view{argv[1]} == "render") {
        try {
            const auto document = lamusica::session::ProjectDocument::open(argv[2]);
            const bool json = hasFlag(argc, argv, "--json");
            const bool stems = hasFlag(argc, argv, "--stems");
            if (const auto format = optionValue(argc, argv, "--format")) {
                validateRenderFormat(*format);
            }
            const auto bitDepthText = optionValue(argc, argv, "--bit-depth").value_or("16");
            const auto bitDepth = parseExportBitDepth(bitDepthText);
            std::int64_t startSample = 0;
            auto frames = lamusica::session::renderableArrangementFrames(document.manifest());
            if (const auto range = optionValue(argc, argv, "--range")) {
                const auto parsed = parseRange(*range);
                startSample = parsed.first;
                frames = parsed.second;
            }
            if (stems) {
                const auto results = lamusica::session::exportProjectStemsToWav(
                    document.manifest(), {},
                    {.outputDirectory = argv[3],
                     .trackIds = audioTrackIds(document.manifest()),
                     .startSample = startSample,
                     .frames = frames,
                     .channels = 2,
                     .projectRoot = document.path(),
                     .bitDepth = bitDepth,
                     .ditherMode = lamusica::audio::DitherMode::Triangular,
                     .normalizePeak = true,
                     .normalizeTargetPeak = 0.98F});
                float peak = 0.0F;
                for (const auto& result : results) {
                    peak = std::max(peak, result.bounce.peakAfterDither);
                }
                if (json) {
                    std::cout << "{\"schemaVersion\":1,\"render\":{\"project\":\""
                              << escapeJson(document.project().name()) << "\",\"output\":\""
                              << escapeJson(argv[3]) << "\",\"format\":\"wav\",\"bitDepth\":"
                              << bitDepthValue(bitDepth) << ",\"startSample\":" << startSample
                              << ",\"frames\":" << frames << ",\"stems\":true,\"stemCount\":"
                              << results.size() << ",\"postDitherPeak\":" << peak << "}}\n";
                } else {
                    std::cout << "rendered stems: project=\"" << document.project().name()
                              << "\" directory=" << argv[3] << " stems=" << results.size()
                              << " frames=" << frames << " postDitherPeak=" << peak << '\n';
                }
            } else {
                const auto result = lamusica::session::exportProjectMixToWav(
                    document.manifest(), {},
                    {.outputPath = argv[3],
                     .startSample = startSample,
                     .frames = frames,
                     .channels = 2,
                     .projectRoot = document.path(),
                     .bitDepth = bitDepth,
                     .ditherMode = lamusica::audio::DitherMode::Triangular,
                     .normalizePeak = true,
                     .normalizeTargetPeak = 0.98F});
                if (json) {
                    std::cout << "{\"schemaVersion\":1,\"render\":{\"project\":\""
                              << escapeJson(document.project().name()) << "\",\"output\":\""
                              << escapeJson(result.outputPath.string())
                              << "\",\"format\":\"wav\",\"bitDepth\":" << bitDepthValue(bitDepth)
                              << ",\"startSample\":" << startSample
                              << ",\"frames\":" << result.frames
                              << ",\"stems\":false,\"postDitherPeak\":"
                              << result.peakAfterDither << "}}\n";
                } else {
                    std::cout << "rendered project: " << document.project().name()
                              << " path=" << result.outputPath << " frames=" << result.frames
                              << " postDitherPeak=" << result.peakAfterDither << '\n';
                }
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "render failed: " << error.what() << '\n';
            return 36;
        }
    }

    if (argc == 4 && std::string_view{argv[1]} == "create-project") {
        try {
            const auto document = lamusica::session::ProjectDocument::createEmpty(argv[2], argv[3]);
            std::cout << "created project: " << document.project().name()
                      << " path=" << document.path() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "create failed: " << error.what() << '\n';
            return 5;
        }
    }

    if (argc == 4 && std::string_view{argv[1]} == "create-first-track") {
        try {
            const auto document = lamusica::session::ProjectDocument::create(
                argv[2], lamusica::session::makeFirstTrackStarterManifest(argv[3]));
            std::cout << "created first-track project: " << document.project().name()
                      << " path=" << document.path()
                      << " tracks=" << document.manifest().tracks.size()
                      << " clips=" << document.manifest().clips.size() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "create first-track failed: " << error.what() << '\n';
            return 10;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "validate") {
        try {
            const auto document = lamusica::session::ProjectDocument::open(argv[2]);
            std::cout << "valid project: " << document.project().name() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "invalid project: " << error.what() << '\n';
            return 2;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "verify-first-track-project") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto verification = session.verifyFirstTrackProject();
            std::cout << "verified first-track project: project=\"" << verification.projectName
                      << "\""
                      << " firstTrackReady=" << (verification.firstTrackReady ? "true" : "false")
                      << " firstTrackEditable="
                      << (verification.firstTrackEditable ? "true" : "false")
                      << " mediaReady=" << (verification.mediaReady ? "true" : "false")
                      << " starterStructure="
                      << (verification.starterStructureReady ? "true" : "false")
                      << " renderable=" << (verification.renderable ? "true" : "false")
                      << " loopEnabled=" << (verification.loopEnabled ? "true" : "false")
                      << " loopFrames=" << verification.loopFrames
                      << " renderFrames=" << verification.renderFrames
                      << " tracks=" << verification.trackCount
                      << " clips=" << verification.clipCount
                      << " plugins=" << verification.pluginCount
                      << " automation=" << verification.automationLaneCount
                      << " starterMidiNotes=" << verification.starterMidiNoteCount
                      << " missingRequirements=" << verification.missingRequirements.size() << '\n';
            if (!verification.firstTrackReady) {
                std::cerr << "first-track project is not ready\n";
                if (!verification.missingRequirements.empty()) {
                    std::cerr << "missing requirements:";
                    for (const auto& requirement : verification.missingRequirements) {
                        std::cerr << ' ' << requirement;
                    }
                    std::cerr << '\n';
                }
                if (!verification.mediaReady && !verification.mediaError.empty()) {
                    std::cerr << "media error: " << verification.mediaError << '\n';
                }
                return 13;
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track project verification failed: " << error.what() << '\n';
            return 13;
        }
    }

    if (argc == 4 && std::string_view{argv[1]} == "transpose-first-track-bass") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            session.transposeFirstTrackBass(parseInt(argv[3], "transpose semitones"));
            std::cout << "transposed first-track bass: project=\"" << session.status().projectName
                      << "\" semitones=" << session.status().starterBassTransposeSemitones
                      << " firstTrackReady="
                      << (session.status().firstTrackReady ? "true" : "false") << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track bass transpose failed: " << error.what() << '\n';
            return 14;
        }
    }

    if (argc == 5 && std::string_view{argv[1]} == "set-first-track-clip-gain") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            session.setClipGain(argv[3], parseFloat(argv[4], "clip gain dB"));
            std::cout << "set first-track clip gain: project=\"" << session.status().projectName
                      << "\" clip=\"" << argv[3] << "\" gainDb=" << argv[4]
                      << " drumGainDb=" << session.status().drumClipGainDb
                      << " bassGainDb=" << session.status().bassClipGainDb << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track clip gain failed: " << error.what() << '\n';
            return 15;
        }
    }

    if (argc == 6 && std::string_view{argv[1]} == "set-first-track-clip-fades") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto fadeInSamples = parseInt64(argv[4], "fade-in samples");
            const auto fadeOutSamples = parseInt64(argv[5], "fade-out samples");
            session.setClipFades(argv[3], fadeInSamples, fadeOutSamples);
            std::cout << "set first-track clip fades: project=\"" << session.status().projectName
                      << "\" clip=\"" << argv[3] << "\" fadeInSamples=" << fadeInSamples
                      << " fadeOutSamples=" << fadeOutSamples << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track clip fade failed: " << error.what() << '\n';
            return 23;
        }
    }

    if (argc == 5 && std::string_view{argv[1]} == "set-first-track-clip-muted") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const bool muted = parseBool(argv[4], "clip muted flag");
            session.setClipMuted(argv[3], muted);
            std::cout << "set first-track clip muted: project=\"" << session.status().projectName
                      << "\" clip=\"" << argv[3] << "\" muted=" << (muted ? "true" : "false")
                      << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track clip mute failed: " << error.what() << '\n';
            return 28;
        }
    }

    if (argc == 5 && std::string_view{argv[1]} == "set-first-track-clip-reversed") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const bool reversed = parseBool(argv[4], "clip reversed flag");
            session.setClipReversed(argv[3], reversed);
            std::cout << "set first-track clip reversed: project=\"" << session.status().projectName
                      << "\" clip=\"" << argv[3] << "\" reversed=" << (reversed ? "true" : "false")
                      << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track clip reverse failed: " << error.what() << '\n';
            return 27;
        }
    }

    if (argc == 7 && std::string_view{argv[1]} == "set-first-track-clip-timing") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto startSample = parseInt64(argv[4], "start sample");
            const auto lengthSamples = parseInt64(argv[5], "length samples");
            const auto sourceOffsetSamples = parseInt64(argv[6], "source offset samples");
            session.setClipTiming(argv[3], startSample, lengthSamples, sourceOffsetSamples);
            std::cout << "set first-track clip timing: project=\"" << session.status().projectName
                      << "\" clip=\"" << argv[3] << "\" startSample=" << startSample
                      << " lengthSamples=" << lengthSamples
                      << " sourceOffsetSamples=" << sourceOffsetSamples << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track clip timing failed: " << error.what() << '\n';
            return 24;
        }
    }

    if (argc == 8 && std::string_view{argv[1]} == "set-first-track-track-mix") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto volumeDb = parseFloat(argv[4], "track volume dB");
            const auto pan = parseFloat(argv[5], "track pan");
            const bool muted = parseBool(argv[6], "track muted flag");
            const bool solo = parseBool(argv[7], "track solo flag");
            session.setFirstTrackTrackMix(argv[3], volumeDb, pan, muted, solo);
            std::cout << "set first-track track mix: project=\"" << session.status().projectName
                      << "\" track=\"" << argv[3] << "\" volumeDb=" << volumeDb << " pan=" << pan
                      << " muted=" << (muted ? "true" : "false")
                      << " solo=" << (solo ? "true" : "false") << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track track mix failed: " << error.what() << '\n';
            return 31;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "list-first-track-track-mix") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto mixes = session.firstTrackTrackMix();
            std::cout << "first-track track mix: project=\"" << session.status().projectName
                      << "\" count=" << mixes.size() << '\n';
            for (const auto& mix : mixes) {
                std::cout << "track trackId=" << mix.trackId << " trackName=\"" << mix.trackName
                          << "\" type=" << lamusica::session::toString(mix.type)
                          << " volumeDb=" << mix.volumeDb << " pan=" << mix.pan
                          << " muted=" << (mix.muted ? "true" : "false")
                          << " solo=" << (mix.solo ? "true" : "false") << '\n';
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track track mix listing failed: " << error.what() << '\n';
            return 32;
        }
    }

    if (argc == 6 && std::string_view{argv[1]} == "duplicate-first-track-clip") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto duplicated =
                session.duplicateClip(argv[3], argv[4], parseInt64(argv[5], "start sample"));
            std::cout << "duplicated first-track clip: project=\"" << session.status().projectName
                      << "\" sourceClip=\"" << duplicated.sourceClipId << "\" clip=\""
                      << duplicated.clipId << "\" track=\"" << duplicated.trackId
                      << "\" startSample=" << duplicated.startSample
                      << " lengthSamples=" << duplicated.lengthSamples
                      << " clips=" << session.status().clipCount << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track clip duplicate failed: " << error.what() << '\n';
            return 25;
        }
    }

    if (argc == 4 && std::string_view{argv[1]} == "remove-first-track-clip") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto removed = session.removeClip(argv[3]);
            std::cout << "removed first-track clip: project=\"" << session.status().projectName
                      << "\" clip=\"" << removed.clipId << "\" track=\"" << removed.trackId
                      << "\" startSample=" << removed.startSample
                      << " lengthSamples=" << removed.lengthSamples
                      << " removedMidiReferences=" << removed.removedMidiReferenceCount
                      << " clips=" << session.status().clipCount << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track clip removal failed: " << error.what() << '\n';
            return 26;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "set-first-track-loop-intro") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            session.setFirstTrackLoopToIntro();
            std::cout << "set first-track intro loop: project=\"" << session.status().projectName
                      << "\" loopEnabled=" << (session.status().loopEnabled ? "true" : "false")
                      << " loopStart=" << session.status().loopStartSample
                      << " loopEnd=" << session.status().loopEndSample
                      << " loopFrames=" << session.status().loopFrames << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track loop failed: " << error.what() << '\n';
            return 16;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "extend-first-track-verse") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            session.extendFirstTrackArrangementToVerse();
            std::cout << "extended first-track verse: project=\"" << session.status().projectName
                      << "\" renderFrames=" << session.status().renderFrames
                      << " loopFrames=" << session.status().loopFrames << " firstTrackReady="
                      << (session.status().firstTrackReady ? "true" : "false") << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track verse extension failed: " << error.what() << '\n';
            return 17;
        }
    }

    if ((argc >= 4 && argc <= 8) && std::string_view{argv[1]} == "record-first-track-take") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto recording = session.recordFirstTrackTake(parseRecordingOptions(argc, argv));
            std::cout << "recorded first-track take: project=\"" << session.status().projectName
                      << "\" assetId=" << recording.assetId << " clipId=" << recording.clipId
                      << " path=" << recording.committed.path
                      << " frames=" << recording.committed.frames
                      << " startSample=" << recording.timelineStartSample
                      << " countInSamples=" << recording.countInSamples
                      << " prerollStart=" << recording.prerollStartSample
                      << " punchEnabled=" << (recording.punchEnabled ? "true" : "false")
                      << " punchIn=" << recording.punchInSample
                      << " punchOut=" << recording.punchOutSample
                      << " recordedTakeCount=" << session.status().recordedTakeCount
                      << " tracks=" << session.status().trackCount
                      << " clips=" << session.status().clipCount << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track recording failed: " << error.what() << '\n';
            return 18;
        }
    }

    if ((argc >= 4 && argc <= 8) && std::string_view{argv[1]} == "plan-first-track-recording") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto options = parseRecordingOptions(argc, argv);
            const auto plan = session.prepareFirstTrackRecording(options);
            std::cout << "planned first-track recording: project=\"" << session.status().projectName
                      << "\"" << " startSample=" << plan.timelineStartSample
                      << " recordFrames=" << plan.recordFrames
                      << " countInBars=" << plan.countInBars
                      << " countInSamples=" << plan.countInSamples
                      << " prerollStart=" << plan.prerollStartSample
                      << " punchEnabled=" << (plan.punchEnabled ? "true" : "false")
                      << " punchIn=" << plan.punchInSample << " punchOut=" << plan.punchOutSample
                      << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track recording plan failed: " << error.what() << '\n';
            return 20;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "list-first-track-takes") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto takes = session.recordedFirstTrackTakes();
            std::cout << "first-track takes: project=\"" << session.status().projectName
                      << "\" count=" << takes.size()
                      << " muted=" << session.status().mutedRecordedTakeCount << '\n';
            for (const auto& take : takes) {
                std::cout << "take clipId=" << take.clipId << " assetId=" << take.assetId
                          << " startSample=" << take.startSample << " frames=" << take.frames
                          << " fadeInSamples=" << take.fadeInSamples
                          << " fadeOutSamples=" << take.fadeOutSamples
                          << " reversed=" << (take.reversed ? "true" : "false")
                          << " muted=" << (take.muted ? "true" : "false")
                          << " mediaAvailable=" << (take.mediaAvailable ? "true" : "false")
                          << " path=" << take.path << '\n';
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track take listing failed: " << error.what() << '\n';
            return 21;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "list-first-track-clips") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto clips = session.firstTrackClips();
            std::cout << "first-track clips: project=\"" << session.status().projectName
                      << "\" count=" << clips.size() << " firstTrackEditable="
                      << (session.status().firstTrackEditable ? "true" : "false")
                      << " mediaReady=" << (session.status().mediaReady ? "true" : "false") << '\n';
            for (const auto& clip : clips) {
                std::cout << "clip clipId=" << clip.clipId << " trackId=" << clip.trackId
                          << " trackName=\"" << clip.trackName
                          << "\" type=" << lamusica::session::toString(clip.type)
                          << " startSample=" << clip.startSample
                          << " lengthSamples=" << clip.lengthSamples
                          << " sourceOffsetSamples=" << clip.sourceOffsetSamples
                          << " fadeInSamples=" << clip.fadeInSamples
                          << " fadeOutSamples=" << clip.fadeOutSamples << " gainDb=" << clip.gainDb
                          << " reversed=" << (clip.reversed ? "true" : "false")
                          << " muted=" << (clip.muted ? "true" : "false")
                          << " assetId=" << clip.assetId
                          << " assetBacked=" << (clip.assetBacked ? "true" : "false")
                          << " mediaAvailable=" << (clip.mediaAvailable ? "true" : "false")
                          << " path=" << clip.path << '\n';
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track clip listing failed: " << error.what() << '\n';
            return 29;
        }
    }

    if (argc == 5 && std::string_view{argv[1]} == "mute-first-track-take") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const bool muted = parseBool(argv[4], "take muted flag");
            session.setFirstTrackTakeMuted(argv[3], muted);
            const auto takes = session.recordedFirstTrackTakes();
            const auto found =
                std::ranges::find_if(takes, [clipId = std::string_view{argv[3]}](const auto& take) {
                    return take.clipId == clipId;
                });
            std::cout << "muted first-track take: project=\"" << session.status().projectName
                      << "\" clipId=" << argv[3]
                      << " muted=" << (found != takes.end() && found->muted ? "true" : "false")
                      << " mutedRecordedTakeCount=" << session.status().mutedRecordedTakeCount
                      << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track take mute failed: " << error.what() << '\n';
            return 22;
        }
    }

    if (argc == 5 && std::string_view{argv[1]} == "import-first-track-audio") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto imported =
                session.importAudioFileToFirstTrack(argv[3], parseInt64(argv[4], "start sample"));
            std::cout << "imported first-track audio: project=\"" << session.status().projectName
                      << "\" assetId=" << imported.assetId << " clipId=" << imported.clipId
                      << " path=" << imported.copiedPath << " frames=" << imported.frames
                      << " channels=" << imported.channels
                      << " importedAudioClipCount=" << session.status().importedAudioClipCount
                      << " tracks=" << session.status().trackCount
                      << " clips=" << session.status().clipCount << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track audio import failed: " << error.what() << '\n';
            return 19;
        }
    }

    if (argc == 5 && std::string_view{argv[1]} == "relink-first-track-audio") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto relinked = session.relinkFirstTrackAudioAsset(argv[3], argv[4]);
            std::cout << "relinked first-track audio: project=\"" << session.status().projectName
                      << "\" assetId=" << relinked.assetId << " path=" << relinked.copiedPath
                      << " frames=" << relinked.frames << " channels=" << relinked.channels
                      << " mediaReady=" << (relinked.mediaReady ? "true" : "false")
                      << " firstTrackReady="
                      << (session.status().firstTrackReady ? "true" : "false")
                      << " firstTrackEditable="
                      << (session.status().firstTrackEditable ? "true" : "false") << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track audio relink failed: " << error.what() << '\n';
            return 30;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "verify-examples") {
        try {
            const std::filesystem::path examplesRoot{argv[2]};
            if (!std::filesystem::exists(examplesRoot)) {
                std::cerr << "examples directory missing: " << examplesRoot << '\n';
                return 7;
            }
            const auto projects = findProjectBundles(examplesRoot);
            if (projects.empty()) {
                std::cerr << "no example projects found: " << examplesRoot << '\n';
                return 7;
            }
            for (const auto& projectPath : projects) {
                const auto document = lamusica::session::ProjectDocument::open(projectPath);
                if (const auto missingAsset = missingAssetReport(document);
                    missingAsset.has_value()) {
                    std::cerr << *missingAsset << '\n';
                    return 7;
                }
            }
            std::cout << "verified example projects: " << projects.size() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "example verification failed: " << error.what() << '\n';
            return 7;
        }
    }

    if (argc == 2 && std::string_view{argv[1]} == "benchmark-smoke") {
        try {
            const auto report = lamusica::session::runStressBenchmark(
                {.stressSpec = {.tracks = 3,
                                .clipsPerTrack = 8,
                                .markers = 4,
                                .pluginsPerTrack = 1,
                                .automationLanesPerTrack = 1,
                                .automationPointsPerLane = 4,
                                .midiNotesPerMidiClip = 4,
                                .assets = 8,
                                .mcpAuditEntries = 4},
                 .thresholds = {.maxStartupMilliseconds = 5000.0,
                                .maxPluginScanMilliseconds = 5000.0,
                                .maxCpuWorkMilliseconds = 5000.0,
                                .maxEditMilliseconds = 5000.0,
                                .maxSaveLoadMilliseconds = 5000.0,
                                .maxQueryMilliseconds = 5000.0,
                                .maxMcpQueryMilliseconds = 5000.0,
                                .maxRealtimeCallbackMilliseconds = 5000.0,
                                .maxRenderRealtimeFactor = 1000.0,
                                .maxEstimatedMemoryBytes = 512U * 1024U * 1024U,
                                .maxEstimatedDiskBytes = 128U * 1024U * 1024U},
                 .renderFrames = 64});
            std::cout << "benchmark smoke passed=" << (report.passed ? "true" : "false")
                      << " startupMs=" << report.result.startupMilliseconds
                      << " pluginScanMs=" << report.result.pluginScanMilliseconds
                      << " cpuWorkMs=" << report.result.cpuWorkMilliseconds
                      << " editMs=" << report.result.editMilliseconds
                      << " saveLoadMs=" << report.result.saveLoadMilliseconds
                      << " queryMs=" << report.result.queryMilliseconds
                      << " mcpQueryMs=" << report.result.mcpQueryMilliseconds
                      << " realtimeCallbackMs=" << report.result.realtimeCallbackMilliseconds
                      << " realtimeSafe=" << (report.result.realtimeCallbackSafe ? "true" : "false")
                      << " renderRealtimeFactor=" << report.result.renderRealtimeFactor
                      << " memoryBytes=" << report.result.estimatedMemoryBytes
                      << " diskBytes=" << report.result.estimatedDiskBytes << " cpu=\""
                      << report.machine.cpuModel << "\"" << " cores=" << report.machine.logicalCores
                      << " memoryMb=" << report.machine.memoryMegabytes << " os=\""
                      << report.machine.operatingSystem << "\"" << " compiler=\""
                      << report.machine.compiler << "\"\n";
            if (!report.passed) {
                std::cerr << "benchmark regressions:";
                for (const auto& regression : report.regressions) {
                    std::cerr << ' ' << regression;
                }
                std::cerr << '\n';
                return 8;
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "benchmark smoke failed: " << error.what() << '\n';
            return 8;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "inspect-project") {
        try {
            const auto document = lamusica::session::ProjectDocument::open(argv[2]);
            const auto& manifest = document.manifest();
            const auto readiness = lamusica::session::inspectFirstTrackReadiness(manifest);
            const auto arrangement = lamusica::session::summarizeFirstTrackArrangement(manifest);
            bool mediaReady = true;
            std::string mediaError;
            try {
                (void)lamusica::session::compileProjectAudioGraph(manifest, {},
                                                                  {.projectRoot = document.path()});
            } catch (const std::exception& error) {
                mediaReady = false;
                mediaError = error.what();
            }
            const bool firstTrackReady = readiness.starterStructureReady && readiness.renderable &&
                                         readiness.loopReady && mediaReady;
            const bool firstTrackEditable = readiness.firstTrackEditable && mediaReady;
            std::cout << "project name=\"" << document.project().name() << "\""
                      << " schemaVersion=" << manifest.schemaVersion
                      << " tempoBpm=" << arrangement.tempoBpm
                      << " timeSignature=" << arrangement.timeSignatureNumerator << '/'
                      << arrangement.timeSignatureDenominator
                      << " sections=" << arrangement.sectionCount << " firstSection=\""
                      << arrangement.firstSectionName << "\""
                      << " firstSectionSample=" << arrangement.firstSectionSample
                      << " finalSection=\"" << arrangement.finalSectionName << "\""
                      << " finalSectionSample=" << arrangement.finalSectionSample
                      << " audioTracks=" << arrangement.audioTrackCount
                      << " midiTracks=" << arrangement.midiTrackCount
                      << " masterTracks=" << arrangement.masterTrackCount
                      << " tracks=" << manifest.tracks.size() << " clips=" << manifest.clips.size()
                      << " assets=" << manifest.assets.size()
                      << " routing=" << manifest.routing.size()
                      << " plugins=" << manifest.plugins.size()
                      << " automation=" << manifest.automation.size()
                      << " mcpAuditLog=" << manifest.mcpAuditLog.size()
                      << " firstTrackReady=" << (firstTrackReady ? "true" : "false")
                      << " firstTrackEditable=" << (firstTrackEditable ? "true" : "false")
                      << " mediaReady=" << (mediaReady ? "true" : "false")
                      << " midiRefs=" << readiness.midiClipReferenceCount
                      << " starterMidiNotes=" << readiness.starterMidiNoteCount
                      << " bassTranspose=" << readiness.starterBassTransposeSemitones
                      << " loopEnabled=" << (readiness.loopReady ? "true" : "false")
                      << " loopStart=" << readiness.loopStartSample
                      << " loopEnd=" << readiness.loopEndSample
                      << " readinessPlugins=" << readiness.pluginCount
                      << " readinessAutomation=" << readiness.automationLaneCount
                      << " renderFrames=" << readiness.renderFrames;
            if (!mediaReady && !mediaError.empty()) {
                std::cout << " media error: " << mediaError;
            }
            std::cout << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "inspect project failed: " << error.what() << '\n';
            return 6;
        }
    }

    if (argc == 4 && std::string_view{argv[1]} == "render-project") {
        try {
            const auto document = lamusica::session::ProjectDocument::open(argv[2]);
            const auto frames = lamusica::session::renderableArrangementFrames(document.manifest());
            const auto result = lamusica::session::exportProjectMixToWav(
                document.manifest(), {},
                {.outputPath = argv[3],
                 .startSample = 0,
                 .frames = frames,
                 .channels = 2,
                 .projectRoot = document.path(),
                 .bitDepth = lamusica::audio::ExportBitDepth::Pcm16,
                 .ditherMode = lamusica::audio::DitherMode::Triangular,
                 .normalizePeak = true,
                 .normalizeTargetPeak = 0.98F});
            std::cout << "rendered project: " << document.project().name()
                      << " path=" << result.outputPath << " frames=" << result.frames
                      << " peak=" << result.peakAfterNormalization << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "project render failed: " << error.what() << '\n';
            return 9;
        }
    }

    if (argc == 4 && std::string_view{argv[1]} == "export-first-track-package") {
        try {
            lamusica::session::ApplicationSession session;
            session.openProject(argv[2]);
            const auto package = session.exportFirstTrackPackage(argv[3]);
            std::cout << "exported first-track package: project=" << session.status().projectName
                      << " directory=" << package.outputDirectory
                      << " manifest=" << package.manifestPath << " mixFrames=" << package.mix.frames
                      << " loopFrames=" << package.loop.frames
                      << " stemCount=" << package.stems.size() << " packageManifest="
                      << (std::filesystem::exists(package.manifestPath) ? "true" : "false") << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track package export failed: " << error.what() << '\n';
            return 11;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "verify-first-track-package") {
        try {
            lamusica::session::ApplicationSession session;
            const auto result = session.verifyFirstTrackPackage(argv[2]);
            std::cout << "verified first-track package: directory=" << result.packageDirectory
                      << " project=\"" << result.projectName << "\""
                      << " renderFrames=" << result.renderFrames
                      << " loopFrames=" << result.loopFrames << " stemCount=" << result.stemCount
                      << " trackCount=" << result.trackCount << " clipCount=" << result.clipCount
                      << " projectSnapshot=" << result.projectSnapshotPath
                      << " projectAssets=" << result.projectAssetCount
                      << " recordedTakeCount=" << result.recordedTakeCount
                      << " importedAudioClipCount=" << result.importedAudioClipCount
                      << " projectSnapshotVerified="
                      << (result.projectSnapshotVerified ? "true" : "false") << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "first-track package verification failed: " << error.what() << '\n';
            return 12;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "render-test-tone") {
        try {
            lamusica::audio::AudioEngine engine{{.sampleRate = 48000.0, .maxBlockSize = 512}};
            const auto rendered = engine.renderSineOffline(48000, 440.0, 0.25F);
            lamusica::audio::writePcm16Wav(argv[2], rendered, engine.config().sampleRate);
            std::cout << "rendered test tone: " << std::filesystem::path{argv[2]} << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "render failed: " << error.what() << '\n';
            return 3;
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "inspect-wav") {
        try {
            const auto wav = lamusica::audio::readPcm16Wav(argv[2]);
            std::cout << "wav sampleRate=" << wav.sampleRate << " channels=" << wav.audio.channels
                      << " frames=" << wav.audio.frames << " bitsPerSample=" << wav.bitsPerSample
                      << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "inspect failed: " << error.what() << '\n';
            return 4;
        }
    }

    const lamusica::session::Project project{"Untitled"};
    std::cout << "LaMusica CLI project=" << project.name() << '\n';
    printUsage();
    return 0;
}

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/Performance.hpp"
#include "lamusica/session/Project.hpp"
#include "lamusica/session/ProjectDocument.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void printUsage() {
    std::cout << "usage: lamusica_cli create-project <Project.lamusica> <name>\n"
              << "       lamusica_cli validate <Project.lamusica>\n"
              << "       lamusica_cli verify-examples <examples-dir>\n"
              << "       lamusica_cli benchmark-smoke\n"
              << "       lamusica_cli inspect-project <Project.lamusica>\n"
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

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view{argv[1]} == "--version") {
        std::cout << "lamusica-cli 0.1.0\n";
        return 0;
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
                                .maxSaveLoadMilliseconds = 5000.0,
                                .maxQueryMilliseconds = 5000.0,
                                .maxRenderRealtimeFactor = 1000.0,
                                .maxEstimatedMemoryBytes = 512U * 1024U * 1024U,
                                .maxEstimatedDiskBytes = 128U * 1024U * 1024U},
                 .renderFrames = 64});
            std::cout << "benchmark smoke passed=" << (report.passed ? "true" : "false")
                      << " startupMs=" << report.result.startupMilliseconds
                      << " pluginScanMs=" << report.result.pluginScanMilliseconds
                      << " cpuWorkMs=" << report.result.cpuWorkMilliseconds
                      << " saveLoadMs=" << report.result.saveLoadMilliseconds
                      << " queryMs=" << report.result.queryMilliseconds
                      << " renderRealtimeFactor=" << report.result.renderRealtimeFactor
                      << " memoryBytes=" << report.result.estimatedMemoryBytes
                      << " diskBytes=" << report.result.estimatedDiskBytes << " cpu=\""
                      << report.machine.cpuModel << "\""
                      << " cores=" << report.machine.logicalCores
                      << " memoryMb=" << report.machine.memoryMegabytes << " os=\""
                      << report.machine.operatingSystem << "\""
                      << " compiler=\"" << report.machine.compiler << "\"\n";
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
            std::cout << "project name=\"" << document.project().name() << "\""
                      << " schemaVersion=" << manifest.schemaVersion
                      << " tracks=" << manifest.tracks.size() << " clips=" << manifest.clips.size()
                      << " assets=" << manifest.assets.size()
                      << " routing=" << manifest.routing.size()
                      << " plugins=" << manifest.plugins.size()
                      << " automation=" << manifest.automation.size()
                      << " mcpAuditLog=" << manifest.mcpAuditLog.size() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "inspect project failed: " << error.what() << '\n';
            return 6;
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

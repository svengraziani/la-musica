#include "lamusica/session/ProjectDocument.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace lamusica::session {
namespace {

std::filesystem::path manifestPath(const std::filesystem::path& projectPath) {
    return projectPath / ProjectDocument::manifestFileName;
}

std::filesystem::path temporaryManifestPath(const std::filesystem::path& projectPath) {
    return projectPath / (std::string{ProjectDocument::manifestFileName} + ".tmp");
}

} // namespace

ProjectDocument ProjectDocument::createEmpty(std::filesystem::path path, std::string name) {
    ProjectManifest manifest;
    manifest.name = std::move(name);
    return create(std::move(path), std::move(manifest));
}

ProjectDocument ProjectDocument::create(std::filesystem::path path, ProjectManifest manifest) {
    if (path.empty()) {
        throw std::runtime_error("Project path must not be empty");
    }
    if (std::filesystem::exists(path)) {
        throw std::runtime_error("Project path already exists: " + path.string());
    }
    ProjectDocument document{std::move(path), std::move(manifest), true};
    document.save();
    return document;
}

ProjectDocument ProjectDocument::open(std::filesystem::path path) {
    std::ifstream input{manifestPath(path)};
    if (!input) {
        throw std::runtime_error("Project manifest could not be opened");
    }

    const std::string manifestJson{std::istreambuf_iterator<char>{input},
                                   std::istreambuf_iterator<char>{}};
    return ProjectDocument{std::move(path), parseProjectManifest(manifestJson), true};
}

void ProjectDocument::save() const {
    if (!open_) {
        throw std::runtime_error("Cannot save a closed project document");
    }

    std::filesystem::create_directories(path_);

    const auto finalPath = manifestPath(path_);
    const auto temporaryPath = temporaryManifestPath(path_);
    {
        std::ofstream output{temporaryPath, std::ios::trunc};
        if (!output) {
            throw std::runtime_error("Project manifest temporary file could not be written");
        }

        output << serializeProjectManifest(manifest_);
        output.flush();
        if (!output) {
            throw std::runtime_error("Project manifest temporary file could not be flushed");
        }
    }

    std::error_code renameError;
    std::filesystem::rename(temporaryPath, finalPath, renameError);
    if (renameError) {
        std::filesystem::remove(temporaryPath);
        throw std::runtime_error("Project manifest could not be written");
    }
}

void ProjectDocument::close() noexcept {
    open_ = false;
}

const Project& ProjectDocument::project() const noexcept {
    return project_;
}

const ProjectManifest& ProjectDocument::manifest() const noexcept {
    return manifest_;
}

ProjectManifest& ProjectDocument::mutableManifest() noexcept {
    return manifest_;
}

const std::filesystem::path& ProjectDocument::path() const noexcept {
    return path_;
}

bool ProjectDocument::isOpen() const noexcept {
    return open_;
}

ProjectDocument::ProjectDocument(std::filesystem::path path, ProjectManifest manifest, bool open)
    : path_(std::move(path)), manifest_(std::move(manifest)), project_(manifest_.name),
      open_(open) {}

} // namespace lamusica::session

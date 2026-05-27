#include "lamusica/session/ProjectDocument.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace lamusica::session {
namespace {

std::filesystem::path manifestPath(const std::filesystem::path& projectPath) {
    return projectPath / ProjectDocument::manifestFileName;
}

} // namespace

ProjectDocument ProjectDocument::createEmpty(std::filesystem::path path, std::string name) {
    ProjectManifest manifest;
    manifest.name = std::move(name);
    return create(std::move(path), std::move(manifest));
}

ProjectDocument ProjectDocument::create(std::filesystem::path path, ProjectManifest manifest) {
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

    std::ofstream output{manifestPath(path_)};
    if (!output) {
        throw std::runtime_error("Project manifest could not be written");
    }

    output << serializeProjectManifest(manifest_);
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

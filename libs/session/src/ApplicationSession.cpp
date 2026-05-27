#include "lamusica/session/ApplicationSession.hpp"

#include <algorithm>
#include <utility>

namespace lamusica::session {

const ApplicationSessionStatus& ApplicationSession::status() const noexcept {
    return status_;
}

const ProjectDocument* ApplicationSession::currentDocument() const noexcept {
    return document_.has_value() ? &*document_ : nullptr;
}

const std::vector<std::filesystem::path>& ApplicationSession::recentProjects() const noexcept {
    return recentProjects_;
}

void ApplicationSession::createProject(std::filesystem::path path, std::string name) {
    document_.emplace(ProjectDocument::createEmpty(std::move(path), std::move(name)));
    rememberRecentProject(document_->path());
    updateStatus();
}

void ApplicationSession::openProject(std::filesystem::path path) {
    document_.emplace(ProjectDocument::open(std::move(path)));
    rememberRecentProject(document_->path());
    updateStatus();
}

void ApplicationSession::saveProject() {
    if (document_.has_value()) {
        document_->save();
    }
    updateStatus();
}

void ApplicationSession::closeProject() noexcept {
    if (document_.has_value()) {
        document_->close();
        document_.reset();
    }
    updateStatus();
}

bool ApplicationSession::recoverLastProject(const std::filesystem::path& path) {
    if (path.empty() || !std::filesystem::exists(path / ProjectDocument::manifestFileName)) {
        closeProject();
        return false;
    }
    openProject(path);
    return true;
}

void ApplicationSession::updateStatus() {
    if (!document_.has_value() || !document_->isOpen()) {
        status_ = {};
        return;
    }
    status_ = ApplicationSessionStatus{.hasOpenProject = true,
                                       .dirty = false,
                                       .projectPath = document_->path(),
                                       .projectName = std::string{document_->project().name()}};
}

void ApplicationSession::rememberRecentProject(const std::filesystem::path& path) {
    std::erase(recentProjects_, path);
    recentProjects_.insert(recentProjects_.begin(), path);
    if (recentProjects_.size() > 10) {
        recentProjects_.resize(10);
    }
}

} // namespace lamusica::session

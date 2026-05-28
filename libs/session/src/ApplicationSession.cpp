#include "lamusica/session/ApplicationSession.hpp"

#include <algorithm>
#include <stdexcept>
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

const ApplicationPreferences& ApplicationSession::preferences() const noexcept {
    return preferences_;
}

ApplicationPanel ApplicationSession::focusedPanel() const noexcept {
    return focusedPanel_;
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

void ApplicationSession::setPreferences(ApplicationPreferences preferences) {
    validatePreferences(preferences);
    preferences_ = std::move(preferences);
}

void ApplicationSession::setKeyboardShortcut(std::string command, std::string keyEquivalent) {
    if (command.empty() || keyEquivalent.empty()) {
        throw std::runtime_error("Keyboard shortcut command and key equivalent are required");
    }

    const auto found = std::ranges::find_if(preferences_.keyboardShortcuts,
                                            [&command](const KeyboardShortcutPreference& shortcut) {
                                                return shortcut.command == command;
                                            });
    if (found == preferences_.keyboardShortcuts.end()) {
        preferences_.keyboardShortcuts.push_back(
            {.command = std::move(command), .keyEquivalent = std::move(keyEquivalent)});
    } else {
        found->keyEquivalent = std::move(keyEquivalent);
    }
}

void ApplicationSession::focusPanel(ApplicationPanel panel) noexcept {
    focusedPanel_ = panel;
}

MenuCommandRoute ApplicationSession::routeMenuCommand(std::string_view command) {
    if (command.empty()) {
        throw std::runtime_error("Menu command must not be empty");
    }

    MenuCommandRoute route{
        .command = std::string{command}, .panel = focusedPanel_, .handled = true, .enabled = true};
    if (command == "view.browser") {
        focusPanel(ApplicationPanel::Browser);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "view.timeline") {
        focusPanel(ApplicationPanel::Timeline);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "view.inspector") {
        focusPanel(ApplicationPanel::Inspector);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "view.mixer") {
        focusPanel(ApplicationPanel::Mixer);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "transport.play" || command == "transport.stop") {
        focusPanel(ApplicationPanel::Transport);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "project.save" || command == "project.close") {
        route.enabled = status_.hasOpenProject;
        return route;
    }
    if (command == "edit.cut" || command == "edit.copy" || command == "edit.paste" ||
        command == "edit.delete" || command == "edit.duplicate" || command == "edit.split" ||
        command == "edit.trim") {
        route.enabled = status_.hasOpenProject;
        return route;
    }

    route.handled = false;
    route.enabled = false;
    return route;
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

void ApplicationSession::validatePreferences(const ApplicationPreferences& preferences) const {
    if (preferences.allowMcpProjectMutation && !preferences.mcpEnabled) {
        throw std::runtime_error("MCP project mutation requires MCP to be enabled");
    }

    for (const auto& midiInputId : preferences.enabledMidiInputIds) {
        if (midiInputId.empty()) {
            throw std::runtime_error("MIDI input preference id must not be empty");
        }
    }
    for (const auto& pluginSearchPath : preferences.pluginSearchPaths) {
        if (pluginSearchPath.empty()) {
            throw std::runtime_error("Plugin search path preference must not be empty");
        }
    }
    for (const auto& shortcut : preferences.keyboardShortcuts) {
        if (shortcut.command.empty() || shortcut.keyEquivalent.empty()) {
            throw std::runtime_error("Keyboard shortcut command and key equivalent are required");
        }
    }
}

} // namespace lamusica::session

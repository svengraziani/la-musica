#pragma once

#include "lamusica/session/ProjectDocument.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

struct ApplicationSessionStatus {
    bool hasOpenProject{false};
    bool dirty{false};
    std::filesystem::path projectPath;
    std::string projectName{"Untitled"};
};

enum class ApplicationPanel {
    Browser,
    Timeline,
    Inspector,
    Mixer,
    Transport,
};

struct MenuCommandRoute {
    std::string command;
    ApplicationPanel panel{ApplicationPanel::Timeline};
    bool handled{false};
    bool enabled{false};
};

struct KeyboardShortcutPreference {
    std::string command;
    std::string keyEquivalent;
};

struct ApplicationPreferences {
    std::string audioDeviceId;
    std::vector<std::string> enabledMidiInputIds;
    std::vector<std::string> pluginSearchPaths;
    bool mcpEnabled{false};
    bool allowMcpProjectMutation{false};
    std::vector<KeyboardShortcutPreference> keyboardShortcuts;
    bool allowUserFolderScanning{false};
    bool shareDiagnostics{false};
};

class ApplicationSession {
  public:
    [[nodiscard]] const ApplicationSessionStatus& status() const noexcept;
    [[nodiscard]] const ProjectDocument* currentDocument() const noexcept;
    [[nodiscard]] const std::vector<std::filesystem::path>& recentProjects() const noexcept;
    [[nodiscard]] const ApplicationPreferences& preferences() const noexcept;
    [[nodiscard]] ApplicationPanel focusedPanel() const noexcept;

    void createProject(std::filesystem::path path, std::string name);
    void openProject(std::filesystem::path path);
    void saveProject();
    void closeProject() noexcept;
    [[nodiscard]] bool recoverLastProject(const std::filesystem::path& path);
    void setPreferences(ApplicationPreferences preferences);
    void setKeyboardShortcut(std::string command, std::string keyEquivalent);
    void focusPanel(ApplicationPanel panel) noexcept;
    [[nodiscard]] MenuCommandRoute routeMenuCommand(std::string_view command);

  private:
    void updateStatus();
    void rememberRecentProject(const std::filesystem::path& path);
    void validatePreferences(const ApplicationPreferences& preferences) const;

    std::optional<ProjectDocument> document_;
    ApplicationSessionStatus status_;
    std::vector<std::filesystem::path> recentProjects_;
    ApplicationPreferences preferences_;
    ApplicationPanel focusedPanel_{ApplicationPanel::Timeline};
};

} // namespace lamusica::session

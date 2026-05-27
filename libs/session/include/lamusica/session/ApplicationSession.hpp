#pragma once

#include "lamusica/session/ProjectDocument.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lamusica::session {

struct ApplicationSessionStatus {
    bool hasOpenProject{false};
    bool dirty{false};
    std::filesystem::path projectPath;
    std::string projectName{"Untitled"};
};

class ApplicationSession {
  public:
    [[nodiscard]] const ApplicationSessionStatus& status() const noexcept;
    [[nodiscard]] const ProjectDocument* currentDocument() const noexcept;
    [[nodiscard]] const std::vector<std::filesystem::path>& recentProjects() const noexcept;

    void createProject(std::filesystem::path path, std::string name);
    void openProject(std::filesystem::path path);
    void saveProject();
    void closeProject() noexcept;
    [[nodiscard]] bool recoverLastProject(const std::filesystem::path& path);

  private:
    void updateStatus();
    void rememberRecentProject(const std::filesystem::path& path);

    std::optional<ProjectDocument> document_;
    ApplicationSessionStatus status_;
    std::vector<std::filesystem::path> recentProjects_;
};

} // namespace lamusica::session

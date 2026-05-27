#pragma once

#include "lamusica/session/Project.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <filesystem>

namespace lamusica::session {

class ProjectDocument {
  public:
    static constexpr auto manifestFileName = "project.json";

    static ProjectDocument createEmpty(std::filesystem::path path, std::string name);
    static ProjectDocument create(std::filesystem::path path, ProjectManifest manifest);
    static ProjectDocument open(std::filesystem::path path);

    void save() const;
    void close() noexcept;

    [[nodiscard]] const Project& project() const noexcept;
    [[nodiscard]] const ProjectManifest& manifest() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] bool isOpen() const noexcept;

  private:
    ProjectDocument(std::filesystem::path path, ProjectManifest manifest, bool open);

    std::filesystem::path path_;
    ProjectManifest manifest_;
    Project project_;
    bool open_{false};
};

} // namespace lamusica::session

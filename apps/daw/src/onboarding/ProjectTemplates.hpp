#pragma once

#include "lamusica/session/ApplicationSession.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace lamusica::daw::onboarding {

struct ProjectTemplate {
    std::string id;
    std::string nameKey;
    std::string descriptionKey;
    std::string iconName;
    int order{0};
};

[[nodiscard]] std::span<const ProjectTemplate> projectTemplates() noexcept;
[[nodiscard]] const ProjectTemplate* findProjectTemplate(std::string_view id) noexcept;
void createProjectFromTemplate(session::ApplicationSession& session, std::string_view templateId,
                               std::filesystem::path path, std::string name);

} // namespace lamusica::daw::onboarding

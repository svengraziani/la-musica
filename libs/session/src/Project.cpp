#include "lamusica/session/Project.hpp"

#include <utility>

namespace lamusica::session {

Project::Project(std::string name) : name_(std::move(name)) {}

std::string_view Project::name() const noexcept {
    return name_;
}

void Project::rename(std::string name) {
    name_ = std::move(name);
}

} // namespace lamusica::session

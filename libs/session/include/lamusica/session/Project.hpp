#pragma once

#include <string>
#include <string_view>

namespace lamusica::session {

class Project {
  public:
    explicit Project(std::string name);

    [[nodiscard]] std::string_view name() const noexcept;
    void rename(std::string name);

  private:
    std::string name_;
};

} // namespace lamusica::session

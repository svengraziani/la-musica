#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace lamusica::daw::a11y {

class LiveRegion {
  public:
    [[nodiscard]] bool announce(std::string message);
    [[nodiscard]] std::optional<std::string_view> lastAnnouncement() const noexcept;
    void clear() noexcept;

  private:
    std::string last_;
};

} // namespace lamusica::daw::a11y

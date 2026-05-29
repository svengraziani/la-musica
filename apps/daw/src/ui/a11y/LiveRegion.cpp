#include "ui/a11y/LiveRegion.hpp"

#include <utility>

namespace lamusica::daw::a11y {

bool LiveRegion::announce(std::string message) {
    if (message.empty() || message == last_) {
        return false;
    }
    last_ = std::move(message);
    return true;
}

std::optional<std::string_view> LiveRegion::lastAnnouncement() const noexcept {
    if (last_.empty()) {
        return std::nullopt;
    }
    return std::string_view{last_};
}

void LiveRegion::clear() noexcept {
    last_.clear();
}

} // namespace lamusica::daw::a11y

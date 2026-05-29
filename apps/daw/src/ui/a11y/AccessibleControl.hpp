#pragma once

#include "lamusica/audio/AudioEngine.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::daw::a11y {

enum class AccessibleRole {
    Unknown,
    Window,
    Region,
    Button,
    ToggleButton,
    Slider,
    Meter,
    Tree,
    ListItem,
    Text,
};

struct AccessibleControl {
    std::string id;
    AccessibleRole role{AccessibleRole::Unknown};
    std::string name;
    std::string valueText;
    std::string description;
    bool interactive{false};
    bool focusable{false};
    bool decorative{false};
    std::vector<AccessibleControl> children;
};

struct AccessibilityAuditIssue {
    std::string id;
    std::string message;
};

struct AccessibilityAuditResult {
    std::vector<AccessibilityAuditIssue> issues;

    [[nodiscard]] bool ok() const noexcept {
        return issues.empty();
    }
};

[[nodiscard]] std::string toString(AccessibleRole role);
[[nodiscard]] std::string formatGainDb(float gainDb);
[[nodiscard]] std::string formatPan(float pan);
[[nodiscard]] std::string formatMeter(float peakDb, bool clipped);
[[nodiscard]] std::string formatBarBeat(const audio::AudioEngine& engine, std::int64_t sample);
[[nodiscard]] AccessibilityAuditResult auditAccessibilityTree(const AccessibleControl& root);
[[nodiscard]] std::vector<std::string> focusOrder(const AccessibleControl& root);
[[nodiscard]] std::optional<const AccessibleControl*> findAccessibleControl(
    const AccessibleControl& root, std::string_view id) noexcept;

} // namespace lamusica::daw::a11y

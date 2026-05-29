#include "ui/a11y/AccessibleControl.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace lamusica::daw::a11y {
namespace {

void auditNode(const AccessibleControl& control, AccessibilityAuditResult& result,
               std::set<std::string>& ids) {
    if (control.id.empty()) {
        result.issues.push_back({.id = "<empty>", .message = "accessible control id is empty"});
    } else if (!ids.insert(control.id).second) {
        result.issues.push_back({.id = control.id, .message = "accessible control id is duplicated"});
    }
    if (!control.decorative && control.name.empty()) {
        result.issues.push_back({.id = control.id, .message = "accessible name is empty"});
    }
    if (control.interactive && !control.focusable && control.role != AccessibleRole::Meter) {
        result.issues.push_back({.id = control.id, .message = "interactive control is not focusable"});
    }
    if ((control.role == AccessibleRole::Slider || control.role == AccessibleRole::Meter ||
         control.role == AccessibleRole::ToggleButton) &&
        control.valueText.empty()) {
        result.issues.push_back({.id = control.id, .message = "value text is empty"});
    }
    for (const auto& child : control.children) {
        auditNode(child, result, ids);
    }
}

void appendFocusOrder(const AccessibleControl& control, std::vector<std::string>& order) {
    if (control.focusable) {
        order.push_back(control.id);
    }
    for (const auto& child : control.children) {
        appendFocusOrder(child, order);
    }
}

const AccessibleControl* findControl(const AccessibleControl& control, std::string_view id) noexcept {
    if (control.id == id) {
        return &control;
    }
    for (const auto& child : control.children) {
        if (const auto* found = findControl(child, id); found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

} // namespace

std::string toString(AccessibleRole role) {
    switch (role) {
    case AccessibleRole::Window:
        return "window";
    case AccessibleRole::Region:
        return "region";
    case AccessibleRole::Button:
        return "button";
    case AccessibleRole::ToggleButton:
        return "toggleButton";
    case AccessibleRole::Slider:
        return "slider";
    case AccessibleRole::Meter:
        return "meter";
    case AccessibleRole::Tree:
        return "tree";
    case AccessibleRole::ListItem:
        return "listItem";
    case AccessibleRole::Text:
        return "text";
    }
    return "region";
}

std::string formatGainDb(float gainDb) {
    if (gainDb <= -90.0F) {
        return "-inf dB";
    }
    std::ostringstream output;
    output << std::fixed << std::setprecision(1) << gainDb << " dB";
    return output.str();
}

std::string formatPan(float pan) {
    const auto clamped = std::clamp(pan, -1.0F, 1.0F);
    if (std::abs(clamped) < 0.005F) {
        return "Center";
    }
    std::ostringstream output;
    output << (clamped < 0.0F ? "L" : "R") << std::lround(std::abs(clamped) * 100.0F);
    return output.str();
}

std::string formatMeter(float peakDb, bool clipped) {
    std::ostringstream output;
    output << "Peak " << formatGainDb(peakDb) << (clipped ? ", clipped" : ", no clipping");
    return output.str();
}

std::string formatBarBeat(const audio::AudioEngine& engine, std::int64_t sample) {
    const auto position = engine.samplesToBarBeat(sample);
    std::ostringstream output;
    output << "Bar " << position.bar << ", beat " << position.beat;
    if (position.ppqOffset > 0.000001) {
        output << ", offset " << std::fixed << std::setprecision(3) << position.ppqOffset;
    }
    return output.str();
}

AccessibilityAuditResult auditAccessibilityTree(const AccessibleControl& root) {
    AccessibilityAuditResult result;
    std::set<std::string> ids;
    auditNode(root, result, ids);
    return result;
}

std::vector<std::string> focusOrder(const AccessibleControl& root) {
    std::vector<std::string> order;
    appendFocusOrder(root, order);
    return order;
}

std::optional<const AccessibleControl*> findAccessibleControl(const AccessibleControl& root,
                                                             std::string_view id) noexcept {
    if (const auto* found = findControl(root, id); found != nullptr) {
        return found;
    }
    return std::nullopt;
}

} // namespace lamusica::daw::a11y

#include "onboarding/WelcomeWindow.hpp"

#include "i18n/Localization.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace lamusica::daw::onboarding {
namespace {

const std::array<HelpMenuItem, 4> helpItems{{
    {.action = HelpAction::UserManual, .titleKey = "onboarding.help.userManual"},
    {.action = HelpAction::ShowWelcome, .titleKey = "onboarding.help.showWelcome"},
    {.action = HelpAction::RestartGuidedTour, .titleKey = "onboarding.help.restartTour"},
    {.action = HelpAction::KeyboardShortcuts, .titleKey = "onboarding.help.keyboardShortcuts"},
}};

std::string sanitizeId(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const auto ch : text) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch)) {
            result.push_back(static_cast<char>(std::tolower(uch)));
        } else if (!result.empty() && result.back() != '-') {
            result.push_back('-');
        }
    }
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }
    return result.empty() ? "item" : result;
}

a11y::AccessibleControl makeControl(std::string id, a11y::AccessibleRole role, std::string name,
                                    std::string valueText = {},
                                    std::string description = {},
                                    bool interactive = false,
                                    bool focusable = false) {
    return {.id = std::move(id),
            .role = role,
            .name = std::move(name),
            .valueText = std::move(valueText),
            .description = std::move(description),
            .interactive = interactive,
            .focusable = focusable,
            .decorative = false,
            .children = {}};
}

} // namespace

WelcomeChooserState welcomeChooserState(const session::ApplicationSession& session) noexcept {
    return {.templates = projectTemplates(),
            .recentProjects = session.recentProjects(),
            .canOpenMostRecent = !session.recentProjects().empty()};
}

WelcomeMotionPolicy welcomeMotionPolicy(const a11y::MotionPreferences& motionPreferences) noexcept {
    if (motionPreferences.reduceMotion()) {
        return {.animated = false, .transitionMilliseconds = 0U};
    }
    return {};
}

a11y::AccessibleControl welcomeAccessibilityTree(const session::ApplicationSession& session,
                                                 std::string_view locale) {
    i18n::LocalizationCatalog catalog;
    catalog.loadBundledTables();
    catalog.setActiveLocale(locale);
    const auto tr = [&catalog](std::string_view key) { return catalog.translate(key); };

    auto root = makeControl("welcome-window", a11y::AccessibleRole::Window,
                            tr("onboarding.welcome.title"), {},
                            tr("onboarding.chooseTemplateOrRecent"));
    auto templates =
        makeControl("welcome-templates", a11y::AccessibleRole::Tree,
                    tr("onboarding.welcome.projectTemplates"));
    for (const auto& projectTemplate : projectTemplates()) {
        templates.children.push_back(makeControl("welcome-template-" + projectTemplate.id,
                                                 a11y::AccessibleRole::ListItem,
                                                 tr(projectTemplate.nameKey), {},
                                                 tr(projectTemplate.descriptionKey), true, true));
    }

    auto recents =
        makeControl("welcome-recent-projects", a11y::AccessibleRole::Tree,
                    tr("onboarding.welcome.recentProjects"));
    for (const auto& recent : session.recentProjects()) {
        const auto filename = recent.filename().string();
        recents.children.push_back(makeControl("welcome-recent-" + sanitizeId(filename),
                                               a11y::AccessibleRole::ListItem, filename, {},
                                               tr("onboarding.welcome.openRecent.help"), true, true));
    }

    auto help = makeControl("welcome-help", a11y::AccessibleRole::Region, tr("Help"));
    for (const auto& item : helpMenuItems()) {
        help.children.push_back(makeControl("welcome-help-" + sanitizeId(item.titleKey),
                                            a11y::AccessibleRole::Button,
                                            tr(item.titleKey), {}, tr(item.titleKey), true, true));
    }

    root.children.push_back(std::move(templates));
    root.children.push_back(std::move(recents));
    root.children.push_back(
        makeControl("welcome-open", a11y::AccessibleRole::Button,
                    tr("onboarding.welcome.openProject"), {},
                    tr("onboarding.chooseTemplateOrRecent"), true, true));
    root.children.push_back(makeControl("welcome-open-most-recent", a11y::AccessibleRole::Button,
                                        tr("onboarding.welcome.openRecent"),
                                        session.recentProjects().empty() ? tr("status.no")
                                                                         : tr("status.yes"),
                                        tr("onboarding.welcome.openRecent.help"), true, true));
    root.children.push_back(std::move(help));
    return root;
}

void openMostRecentProject(session::ApplicationSession& session) {
    if (session.recentProjects().empty()) {
        throw std::runtime_error("No recent project is available");
    }
    session.openProject(session.recentProjects().front());
}

std::span<const HelpMenuItem> helpMenuItems() noexcept {
    return helpItems;
}

std::string userManualPreview(const std::filesystem::path& sourceRoot, std::string_view locale,
                              std::size_t maxBytes) {
    i18n::LocalizationCatalog catalog;
    catalog.loadBundledTables();
    catalog.setActiveLocale(locale);

    std::vector<std::filesystem::path> candidates;
    auto current = sourceRoot.empty() ? std::filesystem::current_path() : sourceRoot;
    for (int depth = 0; depth < 6; ++depth) {
        candidates.push_back(current / "docs" / "user-manual.md");
        candidates.push_back(current / "share" / "doc" / "LaMusica" / "user-manual.md");
        if (!current.has_parent_path() || current == current.parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    std::ifstream input;
    bool openedManual = false;
    for (const auto& candidate : candidates) {
        if (!std::filesystem::is_regular_file(candidate)) {
            continue;
        }
        input.open(candidate);
        if (input) {
            openedManual = true;
            break;
        }
        input.clear();
    }
    if (!openedManual) {
        return catalog.translate("onboarding.help.userManual.fallback");
    }

    std::ostringstream output;
    std::string line;
    const auto maxPreviewBytes = static_cast<std::streamoff>(
        std::min<std::size_t>(maxBytes, static_cast<std::size_t>(
                                            std::numeric_limits<std::streamoff>::max())));
    while (std::getline(input, line) && output.tellp() < maxPreviewBytes) {
        output << line << '\n';
    }
    return output.str();
}

} // namespace lamusica::daw::onboarding

#include "lamusica/mcp_bridge/Protocol.hpp"

#include "lamusica/mcp_bridge/QueryTools.hpp"

#include <exception>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace lamusica::mcp_bridge {
namespace {

std::vector<std::string> splitWords(std::string_view line) {
    std::istringstream input{std::string{line}};
    std::vector<std::string> words;
    std::string word;
    while (input >> word) {
        words.push_back(std::move(word));
    }
    return words;
}

bool equalsAny(std::string_view value, std::initializer_list<std::string_view> candidates) {
    for (const auto candidate : candidates) {
        if (value == candidate) {
            return true;
        }
    }
    return false;
}

bool startsWithAny(std::string_view value, std::initializer_list<std::string_view> prefixes) {
    for (const auto prefix : prefixes) {
        if (value.starts_with(prefix)) {
            return true;
        }
    }
    return false;
}

bool parseCapability(std::string_view value, Capability& capability) noexcept {
    if (value == "read_only") {
        capability = Capability::ReadOnly;
    } else if (value == "edit") {
        capability = Capability::Edit;
    } else if (value == "render") {
        capability = Capability::Render;
    } else if (value == "import_export") {
        capability = Capability::ImportExport;
    } else if (value == "plugin_control") {
        capability = Capability::PluginControl;
    } else if (value == "orchestration") {
        capability = Capability::Orchestration;
    } else {
        return false;
    }
    return true;
}

std::set<Capability> parseCapabilities(const std::vector<std::string>& words,
                                       std::size_t firstCapabilityIndex) {
    std::set<Capability> capabilities;
    for (std::size_t index = firstCapabilityIndex; index < words.size(); ++index) {
        Capability capability{Capability::ReadOnly};
        if (parseCapability(words[index], capability)) {
            capabilities.insert(capability);
        }
    }
    if (capabilities.empty()) {
        capabilities.insert(Capability::ReadOnly);
    }
    return capabilities;
}

bool hasUnknownCapability(const std::vector<std::string>& words, std::size_t firstCapabilityIndex,
                          std::string& unknownCapability) {
    for (std::size_t index = firstCapabilityIndex; index < words.size(); ++index) {
        Capability capability{Capability::ReadOnly};
        if (!parseCapability(words[index], capability)) {
            unknownCapability = words[index];
            return true;
        }
    }
    return false;
}

std::string joinWords(const std::vector<std::string>& words, std::size_t firstIndex) {
    std::ostringstream output;
    for (std::size_t index = firstIndex; index < words.size(); ++index) {
        if (index != firstIndex) {
            output << ' ';
        }
        output << words[index];
    }
    return output.str();
}

std::string healthBody(const DaemonSession& session) {
    const auto health = session.health();
    std::ostringstream output;
    output << "health=" << (health.ok ? "ok" : "error") << " state=" << health.message;
    output << " lifecycle=" << toString(session.lifecycleStatus());
    if (session.attached()) {
        output << " project=" << session.projectPath();
    }
    return output.str();
}

std::string logsBody(const DaemonSession& session) {
    std::ostringstream output;
    output << "logs=" << session.daemonLog().size();
    for (const auto& entry : session.daemonLog()) {
        output << " [" << entry.id << ':' << entry.event;
        if (!entry.detail.empty()) {
            output << ':' << entry.detail;
        }
        output << ']';
    }
    return output.str();
}

bool parsePage(const std::vector<std::string>& words, QueryPage& page) {
    if (words.size() <= 2) {
        return true;
    }
    try {
        page.offset = static_cast<std::size_t>(std::stoull(words[2]));
        if (words.size() > 3) {
            page.limit = static_cast<std::size_t>(std::stoull(words[3]));
        }
    } catch (const std::exception&) {
        return false;
    }
    return words.size() <= 4;
}

bool parseSampleRangeQuery(const std::vector<std::string>& words, QuerySampleRange& range,
                           QueryPage& page) {
    if (words.size() < 4) {
        return false;
    }
    try {
        range.startSample = std::stoll(words[2]);
        range.endSample = std::stoll(words[3]);
        if (words.size() > 4) {
            page.offset = static_cast<std::size_t>(std::stoull(words[4]));
        }
        if (words.size() > 5) {
            page.limit = static_cast<std::size_t>(std::stoull(words[5]));
        }
    } catch (const std::exception&) {
        return false;
    }
    return words.size() <= 6 && range.endSample > range.startSample;
}

ProtocolResponse handleQuery(DaemonSession& session, const ProtocolProjectState& state,
                             const std::vector<std::string>& words,
                             std::optional<std::string_view> authToken) {
    if (!authToken.has_value()) {
        return {.ok = false, .body = "auth_required"};
    }
    if (!session.validateAuthToken(*authToken)) {
        return {.ok = false, .body = "auth_invalid"};
    }
    if (!session.attached()) {
        return {.ok = false, .body = "query_requires_attached_project"};
    }
    if (!session.hasCapability(Capability::ReadOnly)) {
        return {.ok = false, .body = "query_requires_read_only_capability"};
    }
    if (words.size() < 2) {
        return {.ok = false, .body = "query_requires_tool_name"};
    }

    const auto& toolName = words[1];
    const auto forbidden = classifyForbiddenProtocolRequest(words.front(), toolName);
    if (forbidden != ForbiddenProtocolSurface::None) {
        const auto reason = std::string{toString(forbidden)};
        session.recordDeniedProtocolRequest(words.front() + std::string{" "} + toolName, reason);
        return {.ok = false, .body = "forbidden_" + reason};
    }

    const auto success = [&session, &toolName](std::string body) {
        session.recordRead(toolName);
        return ProtocolResponse{.ok = true, .body = std::move(body)};
    };

    static const session::ProjectManifest emptyManifest;
    static const session::TimelineSelection emptySelection;
    static const audio::TransportState emptyTransport;
    static const session::MixerState emptyMixer;
    static const session::PluginScanCache emptyPluginScanCache;
    static const session::AssetCatalog emptyAssetCatalog;

    const auto& manifest = state.manifest == nullptr ? emptyManifest : *state.manifest;
    QueryPage page;
    if (toolName == "automation_range" || toolName == "clips_range") {
        QuerySampleRange range;
        if (!parseSampleRangeQuery(words, range, page)) {
            return {.ok = false, .body = toolName + "_query_must_be_start_end_offset_and_limit"};
        }
        if (toolName == "clips_range") {
            return success(clipsInRangeJson(manifest, range, page));
        }
        return success(automationInRangeJson(manifest, range, page));
    }
    if (!parsePage(words, page)) {
        return {.ok = false, .body = "query_page_must_be_offset_and_limit"};
    }

    if (toolName == "project_summary") {
        return success(projectSummaryJson(manifest));
    }
    if (toolName == "tracks") {
        return success(tracksJson(manifest, page));
    }
    if (toolName == "clips") {
        return success(clipsJson(manifest, page));
    }
    if (toolName == "selection") {
        const auto& selection = state.selection == nullptr ? emptySelection : *state.selection;
        return success(selectionJson(selection));
    }
    if (toolName == "transport") {
        const auto& transport = state.transport == nullptr ? emptyTransport : *state.transport;
        return success(transportJson(transport));
    }
    if (toolName == "tempo") {
        return success(tempoJson(manifest));
    }
    if (toolName == "markers") {
        return success(markersJson(manifest, page));
    }
    if (toolName == "routing") {
        if (state.mixer == nullptr) {
            return success(routingJson(manifest));
        }
        return success(routingJson(*state.mixer));
    }
    if (toolName == "plugins") {
        if (state.pluginScanCache == nullptr) {
            return success(pluginsJson(manifest, page));
        }
        return success(pluginsJson(*state.pluginScanCache));
    }
    if (toolName == "automation") {
        return success(automationJson(manifest, page));
    }
    if (toolName == "assets") {
        if (state.assetCatalog == nullptr) {
            return success(assetsJson(manifest, page));
        }
        return success(assetCatalogJson(
            state.assetCatalog == nullptr ? emptyAssetCatalog : *state.assetCatalog, page));
    }
    if (toolName == "render_capabilities") {
        return success(renderCapabilitiesJson());
    }

    return {.ok = false, .body = "unknown_query_tool"};
}

bool commandRequiresActiveAuth(std::string_view command) noexcept {
    return equalsAny(command, {"detach", "connection_lost", "can_mutate"});
}

ProtocolResponse validateActiveAuth(const DaemonSession& session,
                                    std::optional<std::string_view> authToken) {
    if (!authToken.has_value()) {
        return {.ok = false, .body = "auth_required"};
    }
    if (!session.validateAuthToken(*authToken)) {
        return {.ok = false, .body = "auth_invalid"};
    }
    return {.ok = true, .body = "auth_ok"};
}

ProtocolResponse handleProtocolWords(DaemonSession& session, const ProtocolProjectState& state,
                                     const std::vector<std::string>& words,
                                     std::optional<std::string_view> authToken) {
    if (words.empty()) {
        return {.ok = false, .body = "empty_request"};
    }

    const auto forbidden = classifyForbiddenProtocolRequest(words.front());
    if (forbidden != ForbiddenProtocolSurface::None) {
        const auto reason = std::string{toString(forbidden)};
        session.recordDeniedProtocolRequest(joinWords(words, 0), reason);
        return {.ok = false, .body = "forbidden_" + reason};
    }

    if (words.front() == "query") {
        return handleQuery(session, state, words, authToken);
    }
    if (commandRequiresActiveAuth(words.front())) {
        if (const auto auth = validateActiveAuth(session, authToken); !auth.ok) {
            return auth;
        }
    }
    if (words.front() == "stop" && session.attached()) {
        if (const auto auth = validateActiveAuth(session, authToken); !auth.ok) {
            return auth;
        }
    }

    if (words.front() == "health") {
        return {.ok = true, .body = healthBody(session)};
    }
    if (words.front() == "install") {
        const auto label = words.size() > 1 ? words[1] : std::string{"com.lamusica.mcpd"};
        session.install(label);
        return {.ok = true, .body = "installed label=" + std::string{session.launchLabel()}};
    }
    if (words.front() == "launch") {
        session.launch();
        return {.ok = true, .body = "launched label=" + std::string{session.launchLabel()}};
    }
    if (words.front() == "stop") {
        session.stop();
        return {.ok = true, .body = "stopped"};
    }
    if (words.front() == "logs") {
        return {.ok = true, .body = logsBody(session)};
    }
    if (words.front() == "attach") {
        if (words.size() < 2) {
            return {.ok = false, .body = "attach_requires_project_path"};
        }
        if (session.attached()) {
            if (const auto auth = validateActiveAuth(session, authToken); !auth.ok) {
                return auth;
            }
        } else if (session.connectionInterrupted()) {
            if (!authToken.has_value() || !session.validateRecoveryAuthToken(*authToken)) {
                return {.ok = false, .body = "auth_invalid"};
            }
        }
        std::string unknownCapability;
        if (hasUnknownCapability(words, 2, unknownCapability)) {
            return {.ok = false, .body = "attach_unknown_capability=" + unknownCapability};
        }
        const auto token = session.attachProject(words[1], parseCapabilities(words, 2));
        return {.ok = true, .body = "attached token=" + token};
    }
    if (words.front() == "detach") {
        session.detachProject();
        return {.ok = true, .body = "detached"};
    }
    if (words.front() == "connection_lost") {
        session.markConnectionLost();
        return {.ok = true, .body = session.connectionInterrupted() ? "interrupted" : "idle"};
    }
    if (words.front() == "recover") {
        if (!authToken.has_value()) {
            return {.ok = false, .body = "auth_required"};
        }
        if (!session.recoverConnection(*authToken)) {
            return {.ok = false, .body = "auth_invalid"};
        }
        return {.ok = true, .body = "recovered token=" + session.authToken()};
    }
    if (words.front() == "can_mutate") {
        return {.ok = true, .body = session.canMutateProject() ? "true" : "false"};
    }

    return {.ok = false, .body = "unknown_request"};
}

} // namespace

std::string_view toString(ForbiddenProtocolSurface surface) noexcept {
    switch (surface) {
    case ForbiddenProtocolSurface::None:
        return "none";
    case ForbiddenProtocolSurface::ShellExecution:
        return "shell_execution";
    case ForbiddenProtocolSurface::ProcessExecution:
        return "process_execution";
    case ForbiddenProtocolSurface::FilesystemBrowsing:
        return "filesystem_browsing";
    }
    return "none";
}

ForbiddenProtocolSurface classifyForbiddenProtocolRequest(std::string_view command,
                                                          std::string_view toolName) noexcept {
    const auto value = toolName.empty() ? command : toolName;
    if (equalsAny(value, {"shell", "sh", "bash", "zsh", "run_shell", "exec_shell"}) ||
        startsWithAny(value, {"shell.", "shell/", "terminal.", "terminal/"})) {
        return ForbiddenProtocolSurface::ShellExecution;
    }
    if (equalsAny(value, {"exec", "spawn", "process", "run_process", "subprocess"}) ||
        startsWithAny(value, {"process.", "process/", "subprocess.", "subprocess/"})) {
        return ForbiddenProtocolSurface::ProcessExecution;
    }
    if (equalsAny(value, {"filesystem", "fs", "file", "files", "browse", "open", "cat", "ls",
                          "read_file", "write_file", "list_dir", "list_directory"}) ||
        startsWithAny(value, {"filesystem.", "filesystem/", "fs.", "fs/", "file.", "file/"})) {
        return ForbiddenProtocolSurface::FilesystemBrowsing;
    }
    return ForbiddenProtocolSurface::None;
}

ProtocolResponse handleProtocolLine(DaemonSession& session, std::string_view line) {
    const auto words = splitWords(line);
    if (words.size() >= 3 && words.front() == "auth") {
        return handleProtocolWords(session, {}, {words.begin() + 2, words.end()}, words[1]);
    }
    return handleProtocolWords(session, {}, words, std::nullopt);
}

ProtocolResponse handleProtocolLine(DaemonSession& session, const ProtocolProjectState& state,
                                    std::string_view line) {
    const auto words = splitWords(line);
    if (words.size() >= 3 && words.front() == "auth") {
        return handleProtocolWords(session, state, {words.begin() + 2, words.end()}, words[1]);
    }
    return handleProtocolWords(session, state, words, std::nullopt);
}

std::string serializeProtocolResponse(const ProtocolResponse& response) {
    return std::string{response.ok ? "ok " : "error "} + response.body;
}

} // namespace lamusica::mcp_bridge

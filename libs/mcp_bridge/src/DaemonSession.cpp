#include "lamusica/mcp_bridge/DaemonSession.hpp"

#include <sstream>
#include <utility>

namespace lamusica::mcp_bridge {

HealthStatus DaemonSession::health() const {
    return {.ok = true, .message = attached() ? "attached" : "idle"};
}

bool DaemonSession::attached() const noexcept {
    return !projectPath_.empty();
}

std::string_view DaemonSession::projectPath() const noexcept {
    return projectPath_;
}

std::string DaemonSession::attachProject(std::string projectPath,
                                         std::set<Capability> capabilities) {
    projectPath_ = std::move(projectPath);
    capabilities_ = std::move(capabilities);

    std::ostringstream token;
    token << "project:" << projectPath_ << ":capabilities:" << capabilities_.size();
    authToken_ = token.str();
    return authToken_;
}

void DaemonSession::detachProject() noexcept {
    projectPath_.clear();
    authToken_.clear();
    capabilities_.clear();
}

bool DaemonSession::hasCapability(Capability capability) const noexcept {
    return capabilities_.contains(capability);
}

bool DaemonSession::canMutateProject() const noexcept {
    return attached() && hasCapability(Capability::Edit);
}

std::string DaemonSession::authToken() const noexcept {
    return authToken_;
}

const std::vector<ReadAuditEntry>& DaemonSession::readAuditLog() const noexcept {
    return readAuditLog_;
}

const std::vector<DeniedProtocolEntry>& DaemonSession::deniedProtocolLog() const noexcept {
    return deniedProtocolLog_;
}

void DaemonSession::recordRead(std::string toolName, Capability capability) {
    std::ostringstream id;
    id << "read-" << (readAuditLog_.size() + 1);
    readAuditLog_.push_back({.id = id.str(),
                             .toolName = std::move(toolName),
                             .capability = std::string{toString(capability)}});
}

void DaemonSession::recordDeniedProtocolRequest(std::string request, std::string reason) {
    std::ostringstream id;
    id << "denied-" << (deniedProtocolLog_.size() + 1);
    deniedProtocolLog_.push_back(
        {.id = id.str(), .request = std::move(request), .reason = std::move(reason)});
}

} // namespace lamusica::mcp_bridge

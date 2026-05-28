#include "lamusica/mcp_bridge/DaemonSession.hpp"

#include <chrono>
#include <random>
#include <sstream>
#include <utility>

namespace lamusica::mcp_bridge {
namespace {

std::string makeAuthToken() {
    static std::random_device device;
    static std::mt19937_64 generator{device()};
    std::uniform_int_distribution<unsigned long long> distribution;
    const auto now = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::ostringstream token;
    token << "lmcp_" << std::hex << now << '_' << distribution(generator);
    return token.str();
}

} // namespace

HealthStatus DaemonSession::health() const {
    if (lifecycleStatus_ == DaemonLifecycleStatus::Stopped) {
        return {.ok = true, .message = "stopped"};
    }
    if (connectionInterrupted_) {
        return {.ok = true, .message = "interrupted"};
    }
    return {.ok = true, .message = attached() ? "attached" : "idle"};
}

bool DaemonSession::attached() const noexcept {
    return !projectPath_.empty();
}

bool DaemonSession::connectionInterrupted() const noexcept {
    return connectionInterrupted_;
}

bool DaemonSession::canRecoverConnection() const noexcept {
    return connectionInterrupted_ && !recoveryProjectPath_.empty();
}

std::string_view DaemonSession::projectPath() const noexcept {
    return projectPath_;
}

DaemonLifecycleStatus DaemonSession::lifecycleStatus() const noexcept {
    return lifecycleStatus_;
}

std::string_view DaemonSession::launchLabel() const noexcept {
    return launchLabel_;
}

void DaemonSession::install(std::string launchLabel) {
    launchLabel_ = std::move(launchLabel);
    lifecycleStatus_ = DaemonLifecycleStatus::Installed;
    recordDaemonEvent("install", launchLabel_);
}

void DaemonSession::launch() {
    if (lifecycleStatus_ == DaemonLifecycleStatus::NotInstalled) {
        install(launchLabel_);
    }
    lifecycleStatus_ = DaemonLifecycleStatus::Running;
    recordDaemonEvent("launch", launchLabel_);
}

void DaemonSession::stop() {
    detachProject();
    lifecycleStatus_ = DaemonLifecycleStatus::Stopped;
    recordDaemonEvent("stop", launchLabel_);
}

std::string DaemonSession::attachProject(std::string projectPath,
                                         std::set<Capability> capabilities) {
    if (lifecycleStatus_ == DaemonLifecycleStatus::Stopped) {
        launch();
    }
    projectPath_ = std::move(projectPath);
    capabilities_ = std::move(capabilities);
    recoveryProjectPath_ = projectPath_;
    recoveryCapabilities_ = capabilities_;
    recoveryAuthToken_.clear();
    connectionInterrupted_ = false;

    authToken_ = makeAuthToken();
    recordDaemonEvent("attach", projectPath_);
    return authToken_;
}

void DaemonSession::detachProject() {
    if (!projectPath_.empty()) {
        recordDaemonEvent("detach", projectPath_);
    }
    projectPath_.clear();
    authToken_.clear();
    recoveryAuthToken_.clear();
    capabilities_.clear();
    recoveryProjectPath_.clear();
    recoveryCapabilities_.clear();
    connectionInterrupted_ = false;
}

void DaemonSession::markConnectionLost() {
    if (!attached()) {
        connectionInterrupted_ = false;
        return;
    }
    recoveryProjectPath_ = projectPath_;
    recoveryCapabilities_ = capabilities_;
    recoveryAuthToken_ = authToken_;
    recordDaemonEvent("connection_lost", projectPath_);
    projectPath_.clear();
    authToken_.clear();
    capabilities_.clear();
    connectionInterrupted_ = true;
}

bool DaemonSession::recoverConnection() {
    if (!canRecoverConnection()) {
        return false;
    }
    attachProject(recoveryProjectPath_, recoveryCapabilities_);
    recordDaemonEvent("recover", projectPath_);
    return true;
}

bool DaemonSession::recoverConnection(std::string_view authToken) {
    if (!validateRecoveryAuthToken(authToken)) {
        return false;
    }
    return recoverConnection();
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

bool DaemonSession::validateAuthToken(std::string_view authToken) const noexcept {
    return attached() && !authToken_.empty() && authToken == authToken_;
}

bool DaemonSession::validateRecoveryAuthToken(std::string_view authToken) const noexcept {
    return canRecoverConnection() && !recoveryAuthToken_.empty() && authToken == recoveryAuthToken_;
}

const std::vector<DaemonLogEntry>& DaemonSession::daemonLog() const noexcept {
    return daemonLog_;
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

void DaemonSession::recordDaemonEvent(std::string event, std::string detail) {
    std::ostringstream id;
    id << "daemon-" << (daemonLog_.size() + 1);
    daemonLog_.push_back({.id = id.str(), .event = std::move(event), .detail = std::move(detail)});
}

std::string_view toString(DaemonLifecycleStatus status) noexcept {
    switch (status) {
    case DaemonLifecycleStatus::NotInstalled:
        return "not_installed";
    case DaemonLifecycleStatus::Installed:
        return "installed";
    case DaemonLifecycleStatus::Running:
        return "running";
    case DaemonLifecycleStatus::Stopped:
        return "stopped";
    }
    return "unknown";
}

} // namespace lamusica::mcp_bridge

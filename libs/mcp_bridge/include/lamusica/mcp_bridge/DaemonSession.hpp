#pragma once

#include "lamusica/mcp_bridge/Capability.hpp"

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::mcp_bridge {

enum class DaemonLifecycleStatus {
    NotInstalled,
    Installed,
    Running,
    Stopped,
};

struct HealthStatus {
    bool ok{true};
    std::string message{"ok"};
};

struct DaemonLogEntry {
    std::string id;
    std::string event;
    std::string detail;
};

struct ReadAuditEntry {
    std::string id;
    std::string toolName;
    std::string capability;
};

struct DeniedProtocolEntry {
    std::string id;
    std::string request;
    std::string reason;
};

class DaemonSession {
  public:
    [[nodiscard]] HealthStatus health() const;
    [[nodiscard]] bool attached() const noexcept;
    [[nodiscard]] bool connectionInterrupted() const noexcept;
    [[nodiscard]] bool canRecoverConnection() const noexcept;
    [[nodiscard]] std::string_view projectPath() const noexcept;
    [[nodiscard]] DaemonLifecycleStatus lifecycleStatus() const noexcept;
    [[nodiscard]] std::string_view launchLabel() const noexcept;

    void install(std::string launchLabel);
    void launch();
    void stop();
    std::string attachProject(std::string projectPath, std::set<Capability> capabilities);
    void detachProject();
    void markConnectionLost();
    [[nodiscard]] bool recoverConnection(std::string_view authToken);

    [[nodiscard]] bool hasCapability(Capability capability) const noexcept;
    [[nodiscard]] bool canMutateProject() const noexcept;
    [[nodiscard]] std::string authToken() const noexcept;
    [[nodiscard]] bool validateAuthToken(std::string_view authToken) const noexcept;
    [[nodiscard]] bool validateRecoveryAuthToken(std::string_view authToken) const noexcept;
    [[nodiscard]] const std::vector<DaemonLogEntry>& daemonLog() const noexcept;
    [[nodiscard]] const std::vector<ReadAuditEntry>& readAuditLog() const noexcept;
    [[nodiscard]] const std::vector<DeniedProtocolEntry>& deniedProtocolLog() const noexcept;
    void recordRead(std::string toolName, Capability capability = Capability::ReadOnly);
    void recordDeniedProtocolRequest(std::string request, std::string reason);

  private:
    void recordDaemonEvent(std::string event, std::string detail);
    [[nodiscard]] bool recoverConnection();

    DaemonLifecycleStatus lifecycleStatus_{DaemonLifecycleStatus::Running};
    std::string launchLabel_{"com.lamusica.mcpd"};
    std::string projectPath_;
    std::string authToken_;
    std::string recoveryAuthToken_;
    std::set<Capability> capabilities_;
    std::string recoveryProjectPath_;
    std::set<Capability> recoveryCapabilities_;
    bool connectionInterrupted_{false};
    std::vector<DaemonLogEntry> daemonLog_;
    std::vector<ReadAuditEntry> readAuditLog_;
    std::vector<DeniedProtocolEntry> deniedProtocolLog_;
};

[[nodiscard]] std::string_view toString(DaemonLifecycleStatus status) noexcept;

} // namespace lamusica::mcp_bridge

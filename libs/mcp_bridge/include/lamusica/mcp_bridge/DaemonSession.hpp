#pragma once

#include "lamusica/mcp_bridge/Capability.hpp"

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::mcp_bridge {

struct HealthStatus {
    bool ok{true};
    std::string message{"ok"};
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
    [[nodiscard]] std::string_view projectPath() const noexcept;

    std::string attachProject(std::string projectPath, std::set<Capability> capabilities);
    void detachProject() noexcept;

    [[nodiscard]] bool hasCapability(Capability capability) const noexcept;
    [[nodiscard]] bool canMutateProject() const noexcept;
    [[nodiscard]] std::string authToken() const noexcept;
    [[nodiscard]] const std::vector<ReadAuditEntry>& readAuditLog() const noexcept;
    [[nodiscard]] const std::vector<DeniedProtocolEntry>& deniedProtocolLog() const noexcept;
    void recordRead(std::string toolName, Capability capability = Capability::ReadOnly);
    void recordDeniedProtocolRequest(std::string request, std::string reason);

  private:
    std::string projectPath_;
    std::string authToken_;
    std::set<Capability> capabilities_;
    std::vector<ReadAuditEntry> readAuditLog_;
    std::vector<DeniedProtocolEntry> deniedProtocolLog_;
};

} // namespace lamusica::mcp_bridge

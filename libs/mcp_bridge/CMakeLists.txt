add_library(
  lamusica_mcp_bridge STATIC
  src/Capability.cpp
  src/DaemonSession.cpp
  src/EditTools.cpp
  src/Orchestration.cpp
  src/Protocol.cpp
  src/QueryTools.cpp
  src/RenderTools.cpp)
add_library(lamusica::mcp_bridge ALIAS lamusica_mcp_bridge)

target_include_directories(lamusica_mcp_bridge PUBLIC include)
target_link_libraries(lamusica_mcp_bridge PUBLIC lamusica::commands lamusica::session)
lamusica_configure_target(lamusica_mcp_bridge)

add_executable(lamusica_mcpd src/main.cpp)
target_link_libraries(lamusica_mcpd PRIVATE lamusica::mcp_bridge)
lamusica_configure_target(lamusica_mcpd)

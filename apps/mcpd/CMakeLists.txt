add_executable(lamusica_mcpd src/main.cpp)
target_link_libraries(lamusica_mcpd PRIVATE lamusica::build_info lamusica::crash_report
                                            lamusica::mcp_bridge)
lamusica_configure_target(lamusica_mcpd)

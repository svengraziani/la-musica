add_executable(lamusica_cli src/main.cpp)
target_link_libraries(lamusica_cli PRIVATE lamusica::session)
lamusica_configure_target(lamusica_cli)

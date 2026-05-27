add_library(lamusica_commands STATIC src/Command.cpp)
add_library(lamusica::commands ALIAS lamusica_commands)

target_include_directories(lamusica_commands PUBLIC include)
target_link_libraries(lamusica_commands PUBLIC lamusica::session)
lamusica_configure_target(lamusica_commands)

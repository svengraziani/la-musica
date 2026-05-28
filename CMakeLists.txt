cmake_minimum_required(VERSION 3.25)

project(
  LaMusica
  VERSION 0.1.0
  DESCRIPTION "Open-source macOS DAW with a local MCP daemon"
  LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LAMUSICA_ENABLE_TESTS "Build LaMusica tests" ON)
option(LAMUSICA_ENABLE_JUCE "Enable JUCE-backed targets" OFF)
option(LAMUSICA_ENABLE_ASAN "Build targets with AddressSanitizer instrumentation" OFF)
option(LAMUSICA_ENABLE_PROFILING "Build targets with profiling-friendly debug symbols" OFF)
set(LAMUSICA_JUCE_PATH
    ""
    CACHE PATH "Path to a checked-out JUCE 8 source tree")
set(LAMUSICA_JUCE_REQUIRED_MAJOR
    "8"
    CACHE STRING "Required JUCE major version")

include(cmake/CompilerWarnings.cmake)

if(LAMUSICA_ENABLE_JUCE)
  if(NOT LAMUSICA_JUCE_PATH)
    message(FATAL_ERROR "LAMUSICA_ENABLE_JUCE requires LAMUSICA_JUCE_PATH")
  endif()

  add_subdirectory("${LAMUSICA_JUCE_PATH}" external/JUCE)

  if(DEFINED JUCE_VERSION AND NOT JUCE_VERSION MATCHES "^${LAMUSICA_JUCE_REQUIRED_MAJOR}\\.")
    message(FATAL_ERROR
            "LaMusica requires JUCE ${LAMUSICA_JUCE_REQUIRED_MAJOR}.x, got ${JUCE_VERSION}")
  endif()
endif()

add_subdirectory(libs/audio)
add_subdirectory(libs/session)
add_subdirectory(libs/commands)
add_subdirectory(libs/mcp_bridge)
add_subdirectory(apps/daw)
add_subdirectory(apps/mcpd)
add_subdirectory(tools/cli)

if(LAMUSICA_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(TARGETS lamusica_daw lamusica_mcpd lamusica_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION .)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES README.md CHANGELOG.md SECURITY.md CONTRIBUTING.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY fixtures/examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/lamusica/examples)
install(DIRECTORY fixtures/empty.Project.lamusica
        DESTINATION ${CMAKE_INSTALL_DATADIR}/lamusica/examples)
install(DIRECTORY fixtures/tutorials/ DESTINATION ${CMAKE_INSTALL_DATADIR}/lamusica/tutorials)

set(CPACK_PACKAGE_NAME "LaMusica")
set(CPACK_PACKAGE_VENDOR "LaMusica")
set(CPACK_PACKAGE_CONTACT "security@lamusica.invalid")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "DragNDrop;TGZ")
set(CPACK_DMG_VOLUME_NAME "LaMusica ${PROJECT_VERSION}")
set(CPACK_DMG_FORMAT "UDZO")
include(CPack)

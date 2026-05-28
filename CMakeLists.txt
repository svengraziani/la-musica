cmake_minimum_required(VERSION 3.25)

project(
  LaMusica
  VERSION 0.1.0
  DESCRIPTION "Open-source macOS DAW with a local MCP daemon"
  LANGUAGES C CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LAMUSICA_ENABLE_TESTS "Build LaMusica tests" ON)
option(LAMUSICA_ENABLE_ASAN "Build targets with AddressSanitizer instrumentation" OFF)
option(LAMUSICA_ENABLE_PROFILING "Build targets with profiling-friendly debug symbols" OFF)
set(LAMUSICA_JUCE_VERSION
    "8.0.13"
    CACHE STRING "Pinned JUCE version")
set(LAMUSICA_JUCE_COMMIT
    "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2"
    CACHE STRING "Pinned JUCE commit")
set(LAMUSICA_JUCE_PATH
    ""
    CACHE PATH "Path to a checked-out JUCE ${LAMUSICA_JUCE_VERSION} source tree")

include(cmake/CompilerWarnings.cmake)

if(NOT LAMUSICA_JUCE_PATH)
  message(FATAL_ERROR "LaMusica requires JUCE ${LAMUSICA_JUCE_VERSION}. Set "
                      "LAMUSICA_JUCE_PATH to a checked-out JUCE source tree.")
endif()

if(NOT EXISTS "${LAMUSICA_JUCE_PATH}/CMakeLists.txt")
  message(FATAL_ERROR "LAMUSICA_JUCE_PATH does not point to a JUCE source tree: "
                      "${LAMUSICA_JUCE_PATH}")
endif()

add_subdirectory("${LAMUSICA_JUCE_PATH}" external/JUCE)

if(DEFINED JUCE_VERSION AND NOT JUCE_VERSION VERSION_EQUAL LAMUSICA_JUCE_VERSION)
  message(FATAL_ERROR "LaMusica requires JUCE ${LAMUSICA_JUCE_VERSION}, got ${JUCE_VERSION}")
endif()

find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${LAMUSICA_JUCE_PATH}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${LAMUSICA_JUCE_PATH}" rev-parse HEAD
    OUTPUT_VARIABLE LAMUSICA_JUCE_ACTUAL_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(NOT LAMUSICA_JUCE_ACTUAL_COMMIT STREQUAL LAMUSICA_JUCE_COMMIT)
    message(FATAL_ERROR "LaMusica requires JUCE ${LAMUSICA_JUCE_VERSION} at "
                        "${LAMUSICA_JUCE_COMMIT}, got ${LAMUSICA_JUCE_ACTUAL_COMMIT}")
  endif()
endif()

if(NOT TARGET juce::juce_gui_extra)
  message(FATAL_ERROR "JUCE checkout at ${LAMUSICA_JUCE_PATH} did not provide juce::juce_gui_extra")
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

set(LAMUSICA_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(LAMUSICA_DEPENDENCY_DOC "${LAMUSICA_ROOT}/docs/developer/dependencies.md")
set(LAMUSICA_PINNED_JUCE_COMMIT "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2")
set(LAMUSICA_PINNED_JUCE_CONTENT_SHA256
    "e2ee824cf139a72e3720e996c1cdc70e9ff9dac9653c7f74ccf7d40cf1e3d1c4")

if((NOT DEFINED LAMUSICA_JUCE_PATH OR LAMUSICA_JUCE_PATH STREQUAL "") AND
   DEFINED ENV{LAMUSICA_JUCE_PATH})
  set(LAMUSICA_JUCE_PATH "$ENV{LAMUSICA_JUCE_PATH}")
endif()

if(NOT EXISTS "${LAMUSICA_DEPENDENCY_DOC}")
  message(FATAL_ERROR "Missing dependency lock strategy: ${LAMUSICA_DEPENDENCY_DOC}")
endif()

file(READ "${LAMUSICA_DEPENDENCY_DOC}" LAMUSICA_DEPENDENCY_TEXT)
foreach(required_text IN ITEMS "JUCE" "8.0.13" "${LAMUSICA_PINNED_JUCE_COMMIT}"
                               "${LAMUSICA_PINNED_JUCE_CONTENT_SHA256}" "LAMUSICA_JUCE_PATH"
                               "content manifest checksum" "CMake" "Policy")
  if(NOT LAMUSICA_DEPENDENCY_TEXT MATCHES "${required_text}")
    message(FATAL_ERROR "Dependency lock strategy is missing required text: ${required_text}")
  endif()
endforeach()

file(
  GLOB_RECURSE LAMUSICA_BUILD_FILES
  LIST_DIRECTORIES false
  "${LAMUSICA_ROOT}/CMakeLists.txt"
  "${LAMUSICA_ROOT}/*/CMakeLists.txt"
  "${LAMUSICA_ROOT}/*.cmake")

set(filtered_build_files)
foreach(build_file IN LISTS LAMUSICA_BUILD_FILES)
  file(RELATIVE_PATH relative_build_file "${LAMUSICA_ROOT}" "${build_file}")
  if(relative_build_file MATCHES "^(build|external|_CPack_Packages)/")
    continue()
  endif()
  if(relative_build_file STREQUAL "cmake/CheckDependencyLock.cmake")
    continue()
  endif()
  list(APPEND filtered_build_files "${build_file}")
endforeach()
list(REMOVE_DUPLICATES filtered_build_files)

foreach(build_file IN LISTS filtered_build_files)
  file(READ "${build_file}" build_text)
  foreach(disallowed IN ITEMS "FetchContent_Declare" "CPMAddPackage" "ExternalProject_Add" "vcpkg")
    if(build_text MATCHES "${disallowed}")
      message(FATAL_ERROR
              "Build file ${build_file} uses ${disallowed}; update dependency lock strategy first")
    endif()
  endforeach()
endforeach()

if(DEFINED LAMUSICA_JUCE_PATH AND NOT LAMUSICA_JUCE_PATH STREQUAL "")
  if(NOT EXISTS "${LAMUSICA_JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "LAMUSICA_JUCE_PATH does not point at a JUCE checkout: ${LAMUSICA_JUCE_PATH}")
  endif()
  find_package(Git QUIET)
  if(Git_FOUND AND EXISTS "${LAMUSICA_JUCE_PATH}/.git")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${LAMUSICA_JUCE_PATH}" rev-parse HEAD
      OUTPUT_VARIABLE juce_commit
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    if(NOT juce_commit STREQUAL LAMUSICA_PINNED_JUCE_COMMIT)
      message(FATAL_ERROR "JUCE checkout commit mismatch: ${juce_commit}")
    endif()
  endif()
  file(
    GLOB_RECURSE juce_manifest_files
    LIST_DIRECTORIES false
    "${LAMUSICA_JUCE_PATH}/modules/*.h"
    "${LAMUSICA_JUCE_PATH}/modules/*.cpp"
    "${LAMUSICA_JUCE_PATH}/modules/*.mm"
    "${LAMUSICA_JUCE_PATH}/modules/*.cmake"
    "${LAMUSICA_JUCE_PATH}/CMakeLists.txt")
  list(SORT juce_manifest_files)
  set(juce_manifest "")
  foreach(juce_file IN LISTS juce_manifest_files)
    file(SHA256 "${juce_file}" juce_file_sha)
    file(RELATIVE_PATH juce_relative "${LAMUSICA_JUCE_PATH}" "${juce_file}")
    string(APPEND juce_manifest "${juce_file_sha}  ${juce_relative}\n")
  endforeach()
  string(SHA256 juce_manifest_sha "${juce_manifest}")
  message(STATUS "JUCE content manifest checksum: ${juce_manifest_sha}")
  set(expected_juce_content_sha "${LAMUSICA_PINNED_JUCE_CONTENT_SHA256}")
  if(DEFINED LAMUSICA_JUCE_CONTENT_SHA256 AND NOT LAMUSICA_JUCE_CONTENT_SHA256 STREQUAL "")
    set(expected_juce_content_sha "${LAMUSICA_JUCE_CONTENT_SHA256}")
  endif()
  if(NOT juce_manifest_sha STREQUAL expected_juce_content_sha)
    message(FATAL_ERROR
            "JUCE content manifest checksum mismatch: ${juce_manifest_sha}; expected ${expected_juce_content_sha}")
  endif()
endif()

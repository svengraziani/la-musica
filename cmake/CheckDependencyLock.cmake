set(LAMUSICA_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(LAMUSICA_DEPENDENCY_DOC "${LAMUSICA_ROOT}/docs/developer/dependencies.md")

if(NOT EXISTS "${LAMUSICA_DEPENDENCY_DOC}")
  message(FATAL_ERROR "Missing dependency lock strategy: ${LAMUSICA_DEPENDENCY_DOC}")
endif()

file(READ "${LAMUSICA_DEPENDENCY_DOC}" LAMUSICA_DEPENDENCY_TEXT)
foreach(required_text IN ITEMS "JUCE" "8.0.13" "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2"
                               "LAMUSICA_JUCE_PATH" "CMake" "Policy")
  if(NOT LAMUSICA_DEPENDENCY_TEXT MATCHES "${required_text}")
    message(FATAL_ERROR "Dependency lock strategy is missing required text: ${required_text}")
  endif()
endforeach()

set(LAMUSICA_BUILD_FILES
    "${LAMUSICA_ROOT}/CMakeLists.txt"
    "${LAMUSICA_ROOT}/cmake/CompilerWarnings.cmake")

foreach(build_file IN LISTS LAMUSICA_BUILD_FILES)
  file(READ "${build_file}" build_text)
  foreach(disallowed IN ITEMS "FetchContent_Declare" "CPMAddPackage" "ExternalProject_Add" "vcpkg")
    if(build_text MATCHES "${disallowed}")
      message(FATAL_ERROR
              "Build file ${build_file} uses ${disallowed}; update dependency lock strategy first")
    endif()
  endforeach()
endforeach()

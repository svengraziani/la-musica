if(NOT DEFINED PACKAGE)
  message(FATAL_ERROR "Set PACKAGE to the archive path to verify")
endif()

if(NOT EXISTS "${PACKAGE}")
  message(FATAL_ERROR "Package does not exist: ${PACKAGE}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar tf "${PACKAGE}"
  RESULT_VARIABLE list_result
  OUTPUT_VARIABLE package_listing
  ERROR_VARIABLE package_error)

if(NOT list_result EQUAL 0)
  message(FATAL_ERROR "Could not list package ${PACKAGE}: ${package_error}")
endif()

if(package_listing MATCHES "(^|\n)[^\n]*/LaMusica\\.app/Contents/MacOS/LaMusica(\n|$)")
  message(STATUS "Package contains macOS app bundle")
elseif(package_listing MATCHES "(^|\n)[^\n]*/bin/lamusica_daw(\n|$)")
  message(STATUS "Package contains non-macOS app smoke binary")
else()
  message(FATAL_ERROR "Package ${PACKAGE} is missing LaMusica app executable")
endif()

set(required_entries
    "(^|\n)[^\n]*/bin/lamusica_mcpd(\n|$)"
    "(^|\n)[^\n]*/bin/lamusica_cli(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/user-manual\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/architecture/architecture-baseline\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/build-and-test\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/command-api\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/mcp-tools\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/project-format\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/release-checklist\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/macos-distribution\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/security-disclosure\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/versioning\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/CHANGELOG\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/SECURITY\\.md(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/examples/empty\\.Project\\.lamusica/project\\.json(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/examples/generated-tone\\.Project\\.lamusica/project\\.json(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/tutorials/first-song\\.Project\\.lamusica/project\\.json(\n|$)"
)

foreach(required_entry IN LISTS required_entries)
  if(NOT package_listing MATCHES "${required_entry}")
    message(FATAL_ERROR "Package ${PACKAGE} is missing required entry matching ${required_entry}")
  endif()
endforeach()

message(STATUS "Verified package contents: ${PACKAGE}")

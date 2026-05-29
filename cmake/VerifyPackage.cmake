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
  set(has_app_bundle TRUE)
elseif(package_listing MATCHES "(^|\n)[^\n]*/bin/lamusica_daw(\n|$)")
  message(STATUS "Package contains non-macOS app smoke binary")
  set(has_app_bundle FALSE)
else()
  message(FATAL_ERROR "Package ${PACKAGE} is missing LaMusica app executable")
endif()

set(required_entries
    "(^|\n)[^\n]*/bin/lamusica_plugin_scan_worker(\n|$)"
    "(^|\n)[^\n]*/bin/lamusica_mcpd(\n|$)"
    "(^|\n)[^\n]*/bin/lamusica_cli(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/NOTICE(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/THIRD_PARTY-NOTICES\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/privacy-and-diagnostics\\.md(\n|$)"
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
    "(^|\n)[^\n]*/share/lamusica/i18n/en\\.txt(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/i18n/es\\.txt(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/i18n/fr\\.txt(\n|$)"
)

foreach(required_entry IN LISTS required_entries)
  if(NOT package_listing MATCHES "${required_entry}")
    message(FATAL_ERROR "Package ${PACKAGE} is missing required entry matching ${required_entry}")
  endif()
endforeach()

set(extract_dir "${CMAKE_CURRENT_BINARY_DIR}/verify-package")
file(REMOVE_RECURSE "${extract_dir}")
file(MAKE_DIRECTORY "${extract_dir}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar xf "${PACKAGE}"
  WORKING_DIRECTORY "${extract_dir}"
  RESULT_VARIABLE extract_result
  ERROR_VARIABLE extract_error)
if(NOT extract_result EQUAL 0)
  message(FATAL_ERROR "Could not extract package ${PACKAGE}: ${extract_error}")
endif()

file(GLOB_RECURSE extracted_files LIST_DIRECTORIES false "${extract_dir}/*")
foreach(extracted_file IN LISTS extracted_files)
  if(extracted_file MATCHES "\\.(md|txt|plist|json|cmake|hpp|cpp|h|in)$" OR extracted_file MATCHES
                                                                        "/NOTICE$")
    file(READ "${extracted_file}" extracted_text)
    if(extracted_text MATCHES "\\.invalid")
      message(FATAL_ERROR "Package contains placeholder .invalid contact in ${extracted_file}")
    endif()
  endif()
endforeach()

if(has_app_bundle)
  file(GLOB_RECURSE plist_files LIST_DIRECTORIES false "${extract_dir}/*/LaMusica.app/Contents/Info.plist")
  list(LENGTH plist_files plist_count)
  if(plist_count EQUAL 0)
    message(FATAL_ERROR "Package is missing LaMusica.app Contents/Info.plist")
  endif()
  list(GET plist_files 0 plist_file)
  file(READ "${plist_file}" plist_text)
  foreach(required_plist_text IN ITEMS "CFBundleIdentifier" "dev.lamusica.daw"
                                       "NSMicrophoneUsageDescription"
                                       "LSApplicationCategoryType" "public.app-category.music"
                                       "LSMinimumSystemVersion" "14.0")
    if(NOT plist_text MATCHES "${required_plist_text}")
      message(FATAL_ERROR "Info.plist is missing required text: ${required_plist_text}")
    endif()
  endforeach()
endif()

message(STATUS "Verified package contents: ${PACKAGE}")

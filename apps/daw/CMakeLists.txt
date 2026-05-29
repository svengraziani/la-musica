configure_file(Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)

juce_add_gui_app(
  lamusica_daw
  PRODUCT_NAME "LaMusica"
  BUNDLE_ID "dev.lamusica.daw"
  COMPANY_NAME "LaMusica"
  PLIST_TO_MERGE "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "LaMusica needs microphone access when recording audio into a project.")

set(LAMUSICA_DAW_A11Y_SOURCES
    src/ui/a11y/AccessibleControl.cpp
    src/ui/a11y/ContrastPreferences.cpp
    src/ui/a11y/LiveRegion.cpp
    src/ui/a11y/MotionPreferences.cpp)
set(LAMUSICA_DAW_I18N_SOURCES
    src/i18n/Localization.cpp
    src/i18n/NumberFormat.cpp
    src/i18n/StringTables.cpp)
set(LAMUSICA_DAW_ONBOARDING_SOURCES
    src/onboarding/GuidedTour.cpp
    src/onboarding/ProjectTemplates.cpp
    src/onboarding/WelcomeWindow.cpp)
set(LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/en.lproj/InfoPlist.strings"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/es.lproj/InfoPlist.strings"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/fr.lproj/InfoPlist.strings")
foreach(LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCE
        IN LISTS
        LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCES)
  get_filename_component(LAMUSICA_DAW_LOCALIZATION_DIR
                         "${LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCE}" DIRECTORY)
  get_filename_component(LAMUSICA_DAW_LOCALIZATION_NAME
                         "${LAMUSICA_DAW_LOCALIZATION_DIR}" NAME)
  set_source_files_properties(
    "${LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCE}"
    PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${LAMUSICA_DAW_LOCALIZATION_NAME}")
endforeach()

target_sources(lamusica_daw PRIVATE src/main_juce.cpp ${LAMUSICA_DAW_A11Y_SOURCES}
                                    ${LAMUSICA_DAW_I18N_SOURCES}
                                    ${LAMUSICA_DAW_ONBOARDING_SOURCES}
                                    ${LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCES})
target_include_directories(lamusica_daw PRIVATE src)

target_link_libraries(lamusica_daw PRIVATE lamusica::build_info lamusica::crash_report
                                           lamusica::session juce::juce_gui_extra)
set_target_properties(
  lamusica_daw
  PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
             "${CMAKE_CURRENT_SOURCE_DIR}/lamusica.entitlements"
             XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES)
target_compile_definitions(
  lamusica_daw
  PRIVATE JUCE_WEB_BROWSER=0
          JUCE_USE_CURL=0
          JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:lamusica_daw,JUCE_PRODUCT_NAME>"
          JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
lamusica_configure_target(lamusica_daw)

add_executable(lamusica_daw_smoke src/main.cpp ${LAMUSICA_DAW_A11Y_SOURCES}
                                  ${LAMUSICA_DAW_I18N_SOURCES}
                                  ${LAMUSICA_DAW_ONBOARDING_SOURCES})
target_include_directories(lamusica_daw_smoke PRIVATE src)
target_link_libraries(lamusica_daw_smoke PRIVATE lamusica::build_info lamusica::crash_report
                                                 lamusica::session)
lamusica_configure_target(lamusica_daw_smoke)

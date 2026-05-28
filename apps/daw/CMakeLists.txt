juce_add_gui_app(
  lamusica_daw
  PRODUCT_NAME "LaMusica"
  BUNDLE_ID "dev.lamusica.app"
  COMPANY_NAME "LaMusica")

target_sources(lamusica_daw PRIVATE src/main_juce.cpp)

target_link_libraries(lamusica_daw PRIVATE lamusica::session juce::juce_gui_extra)
target_compile_definitions(
  lamusica_daw
  PRIVATE JUCE_WEB_BROWSER=0
          JUCE_USE_CURL=0
          JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:lamusica_daw,JUCE_PRODUCT_NAME>"
          JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
lamusica_configure_target(lamusica_daw)

add_executable(lamusica_daw_smoke src/main.cpp)
target_link_libraries(lamusica_daw_smoke PRIVATE lamusica::session)
lamusica_configure_target(lamusica_daw_smoke)

if(APPLE)
  add_executable(lamusica_daw MACOSX_BUNDLE src/main.mm)
  set_target_properties(
    lamusica_daw
    PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
               OUTPUT_NAME "LaMusica")
  target_link_libraries(lamusica_daw PRIVATE "-framework Cocoa")
else()
  add_executable(lamusica_daw src/main.cpp)
endif()

target_link_libraries(lamusica_daw PRIVATE lamusica::session)
lamusica_configure_target(lamusica_daw)

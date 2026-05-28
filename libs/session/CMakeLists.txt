add_library(
  lamusica_session STATIC
  src/Project.cpp
  src/ApplicationSession.cpp
  src/AudioClipEditing.cpp
  src/Assets.cpp
  src/Automation.cpp
  src/ProjectDocument.cpp
  src/ProjectManifest.cpp
  src/Midi.cpp
  src/Mixer.cpp
  src/Pattern.cpp
  src/Performance.cpp
  src/Plugin.cpp
  src/DrumMachine.cpp
  src/Export.cpp
  src/GraphCompiler.cpp
  src/PianoRoll.cpp
  src/StarterProject.cpp
  src/Timeline.cpp
  src/Warp.cpp)
add_library(lamusica::session ALIAS lamusica_session)

target_include_directories(lamusica_session PUBLIC include)
target_link_libraries(lamusica_session PUBLIC lamusica::audio)
lamusica_configure_target(lamusica_session)

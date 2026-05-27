add_library(
  lamusica_audio STATIC
  src/AudioEngine.cpp
  src/AudioGraph.cpp
  src/Bounce.cpp
  src/Recording.cpp
  src/WavFile.cpp)
add_library(lamusica::audio ALIAS lamusica_audio)

target_include_directories(lamusica_audio PUBLIC include)
lamusica_configure_target(lamusica_audio)

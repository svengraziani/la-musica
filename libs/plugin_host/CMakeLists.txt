add_library(
  lamusica_plugin_host STATIC
  src/PluginHostCapabilities.cpp
  src/PluginScanCacheFile.cpp
  src/PluginScanWorker.cpp)
add_library(lamusica::plugin_host ALIAS lamusica_plugin_host)

target_include_directories(lamusica_plugin_host PUBLIC include)
target_link_libraries(lamusica_plugin_host PUBLIC lamusica::session)
lamusica_configure_target(lamusica_plugin_host)

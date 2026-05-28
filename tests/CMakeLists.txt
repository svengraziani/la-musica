add_executable(lamusica_unit_tests unit/bootstrap_tests.cpp)
target_link_libraries(
  lamusica_unit_tests
  PRIVATE lamusica::mcp_bridge)
lamusica_configure_target(lamusica_unit_tests)

add_test(NAME lamusica_unit_tests COMMAND lamusica_unit_tests)
set_tests_properties(lamusica_unit_tests PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

set(LAMUSICA_CLI_LIFECYCLE_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/cli-lifecycle.Project.lamusica")
add_test(NAME lamusica_cli_lifecycle_clean
         COMMAND ${CMAKE_COMMAND} -E rm -rf "${LAMUSICA_CLI_LIFECYCLE_PROJECT}")
add_test(NAME lamusica_cli_create_project
         COMMAND lamusica_cli create-project "${LAMUSICA_CLI_LIFECYCLE_PROJECT}"
                 "CLI Lifecycle")
add_test(NAME lamusica_cli_validate_created_project
         COMMAND lamusica_cli validate "${LAMUSICA_CLI_LIFECYCLE_PROJECT}")
add_test(NAME lamusica_cli_inspect_created_project
         COMMAND lamusica_cli inspect-project "${LAMUSICA_CLI_LIFECYCLE_PROJECT}")
add_test(NAME lamusica_cli_verify_examples
         COMMAND lamusica_cli verify-examples "${PROJECT_SOURCE_DIR}/fixtures/examples")
add_test(NAME lamusica_cli_benchmark_smoke COMMAND lamusica_cli benchmark-smoke)

set_tests_properties(lamusica_cli_create_project
                     PROPERTIES DEPENDS lamusica_cli_lifecycle_clean
                                PASS_REGULAR_EXPRESSION "created project: CLI Lifecycle")
set_tests_properties(lamusica_cli_validate_created_project
                     PROPERTIES DEPENDS lamusica_cli_create_project
                                PASS_REGULAR_EXPRESSION "valid project: CLI Lifecycle")
set_tests_properties(lamusica_cli_inspect_created_project
                     PROPERTIES DEPENDS lamusica_cli_validate_created_project
                                PASS_REGULAR_EXPRESSION
                                "schemaVersion=1 tracks=0 clips=0")
set_tests_properties(lamusica_cli_verify_examples
                     PROPERTIES PASS_REGULAR_EXPRESSION "verified example projects: 1")
set_tests_properties(lamusica_cli_benchmark_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION "benchmark smoke passed=true")

set(LAMUSICA_MISSING_ASSET_EXAMPLE
    "${CMAKE_CURRENT_BINARY_DIR}/missing-assets/bad.Project.lamusica")
file(MAKE_DIRECTORY "${LAMUSICA_MISSING_ASSET_EXAMPLE}")
file(WRITE "${LAMUSICA_MISSING_ASSET_EXAMPLE}/project.json"
     [=[
{
  "schemaVersion": 1,
  "name": "Missing Asset Example",
  "tempoMap": [
    {"samplePosition": 0, "bpm": 120}
  ],
  "timeSignatures": [
    {"samplePosition": 0, "numerator": 4, "denominator": 4}
  ],
  "markers": [],
  "assets": [
    {"id": "missing-audio", "relativePath": "Audio/missing.wav", "mediaType": "audio/wav"}
  ],
  "tracks": [
    {"id": "track-a", "name": "Track A", "type": "audio"}
  ],
  "clips": [
    {
      "id": "clip-a",
      "trackId": "track-a",
      "type": "audio",
      "startSample": 0,
      "lengthSamples": 48000,
      "assetId": "missing-audio"
    }
  ],
  "midiClips": [],
  "routing": [],
  "plugins": [],
  "automation": [],
  "mcpAuditLog": []
}
]=])
add_test(NAME lamusica_cli_verify_examples_rejects_missing_assets
         COMMAND lamusica_cli verify-examples "${CMAKE_CURRENT_BINARY_DIR}/missing-assets")
set_tests_properties(lamusica_cli_verify_examples_rejects_missing_assets
                     PROPERTIES WILL_FAIL TRUE
                                PASS_REGULAR_EXPRESSION "missing asset")

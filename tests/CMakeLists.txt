add_executable(lamusica_unit_tests unit/bootstrap_tests.cpp)
target_link_libraries(
  lamusica_unit_tests
  PRIVATE lamusica::mcp_bridge)
lamusica_configure_target(lamusica_unit_tests)

add_executable(lamusica_render_determinism_tests
               determinism/render_determinism_tests.cpp)
target_link_libraries(lamusica_render_determinism_tests PRIVATE lamusica::commands
                                                                lamusica::session)
lamusica_configure_target(lamusica_render_determinism_tests)

add_executable(lamusica_plugin_host_tests plugin_hosting/plugin_host_tests.cpp)
target_link_libraries(lamusica_plugin_host_tests PRIVATE lamusica::plugin_host lamusica::session)
lamusica_configure_target(lamusica_plugin_host_tests)

add_executable(lamusica_warp_spectral_tests audio_correctness/warp_spectral_tests.cpp)
target_link_libraries(lamusica_warp_spectral_tests PRIVATE lamusica::session)
lamusica_configure_target(lamusica_warp_spectral_tests)

add_executable(lamusica_session_behavior_tests behavior/session_behavior_tests.cpp)
target_link_libraries(lamusica_session_behavior_tests PRIVATE lamusica::session)
lamusica_configure_target(lamusica_session_behavior_tests)

add_executable(lamusica_rt_deadline_bench perf/rt_deadline_bench.cpp)
target_link_libraries(lamusica_rt_deadline_bench PRIVATE lamusica::session)
lamusica_configure_target(lamusica_rt_deadline_bench)

add_executable(
  lamusica_onboarding_tests
  unit/onboarding_tests.cpp
  ../apps/daw/src/onboarding/ProjectTemplates.cpp
  ../apps/daw/src/onboarding/WelcomeWindow.cpp
  ../apps/daw/src/onboarding/GuidedTour.cpp)
target_include_directories(lamusica_onboarding_tests PRIVATE ../apps/daw/src)
target_link_libraries(lamusica_onboarding_tests PRIVATE lamusica::session)
lamusica_configure_target(lamusica_onboarding_tests)

add_executable(lamusica_mcp_fixture_client fixtures/mcp_fixture_client.cpp)
lamusica_configure_target(lamusica_mcp_fixture_client)

add_executable(lamusica_artifact_assertions fixtures/artifact_assertions.cpp)
target_link_libraries(lamusica_artifact_assertions PRIVATE lamusica::session)
lamusica_configure_target(lamusica_artifact_assertions)

add_executable(
  lamusica_i18n_tests
  unit/i18n_tests.cpp
  ../apps/daw/src/i18n/Localization.cpp
  ../apps/daw/src/i18n/NumberFormat.cpp
  ../apps/daw/src/i18n/StringTables.cpp)
target_include_directories(lamusica_i18n_tests PRIVATE ../apps/daw/src)
target_link_libraries(lamusica_i18n_tests PRIVATE lamusica::session)
lamusica_configure_target(lamusica_i18n_tests)

add_test(NAME lamusica_unit_tests COMMAND lamusica_unit_tests)
set_tests_properties(lamusica_unit_tests PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
add_test(NAME lamusica_i18n_tests COMMAND lamusica_i18n_tests "${PROJECT_SOURCE_DIR}")
set_tests_properties(lamusica_i18n_tests PROPERTIES LABELS i18n)

add_test(NAME lamusica_render_determinism_tests
         COMMAND lamusica_render_determinism_tests
                 "${CMAKE_CURRENT_BINARY_DIR}/determinism" "${PROJECT_SOURCE_DIR}")
set_tests_properties(lamusica_render_determinism_tests PROPERTIES LABELS determinism)

add_test(NAME lamusica_plugin_host_tests
         COMMAND lamusica_plugin_host_tests $<TARGET_FILE:lamusica_plugin_scan_worker>)
set_tests_properties(lamusica_plugin_host_tests PROPERTIES LABELS plugin-hosting)

add_test(NAME lamusica_warp_spectral_tests COMMAND lamusica_warp_spectral_tests)
set_tests_properties(lamusica_warp_spectral_tests PROPERTIES LABELS audio-correctness)

add_test(NAME lamusica_session_behavior_tests
         COMMAND lamusica_session_behavior_tests "${CMAKE_CURRENT_BINARY_DIR}/behavior")
set_tests_properties(lamusica_session_behavior_tests PROPERTIES LABELS behavior)

add_test(NAME lamusica_rt_deadline_bench
         COMMAND lamusica_rt_deadline_bench "${CMAKE_CURRENT_BINARY_DIR}/perf")
set_tests_properties(lamusica_rt_deadline_bench PROPERTIES LABELS perf)

add_test(NAME lamusica_onboarding_tests
         COMMAND lamusica_onboarding_tests "${CMAKE_CURRENT_BINARY_DIR}/onboarding")
set_tests_properties(lamusica_onboarding_tests PROPERTIES LABELS onboarding)

add_test(NAME lamusica_mcpd_fixture_client
         COMMAND lamusica_mcp_fixture_client $<TARGET_FILE:lamusica_mcpd>
                 "${PROJECT_SOURCE_DIR}/fixtures/empty.Project.lamusica")
add_test(NAME lamusica_mcpd_health COMMAND lamusica_mcpd --health)
add_test(NAME lamusica_daw_first_track_smoke
         COMMAND lamusica_daw_smoke --first-track-smoke)
add_test(NAME lamusica_daw_app_session_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-first-track-smoke)
add_test(NAME lamusica_daw_app_session_verify_first_track_project_smoke
         COMMAND lamusica_daw_smoke --app-session-verify-first-track-project-smoke)
add_test(NAME lamusica_daw_app_session_render_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-render-first-track-smoke)
add_test(NAME lamusica_daw_app_session_preferences_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-preferences-first-track-smoke)
add_test(NAME lamusica_daw_app_session_transpose_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-transpose-first-track-smoke)
add_test(NAME lamusica_daw_app_session_mix_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-mix-first-track-smoke)
add_test(NAME lamusica_daw_app_session_loop_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-loop-first-track-smoke)
add_test(NAME lamusica_daw_app_session_transport_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-transport-first-track-smoke)
add_test(NAME lamusica_daw_app_session_arrange_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-arrange-first-track-smoke)
add_test(NAME lamusica_daw_app_session_stem_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-stem-first-track-smoke)
add_test(NAME lamusica_daw_app_session_package_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-package-first-track-smoke)
add_test(NAME lamusica_daw_app_session_record_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-record-first-track-smoke)
add_test(NAME lamusica_daw_app_session_record_package_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-record-package-first-track-smoke)
add_test(NAME lamusica_daw_app_session_import_package_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-import-package-first-track-smoke)
add_test(NAME lamusica_daw_app_session_import_edit_first_track_smoke
         COMMAND lamusica_daw_smoke --app-session-import-edit-first-track-smoke)
add_test(NAME lamusica_daw_accessibility_audit COMMAND lamusica_daw_smoke --a11y-audit)
set_tests_properties(lamusica_daw_accessibility_audit PROPERTIES LABELS a11y)
add_test(NAME lamusica_daw_diagnostics_consent
         COMMAND lamusica_daw_smoke --app-session-diagnostics-consent-smoke)
set_tests_properties(lamusica_daw_diagnostics_consent PROPERTIES LABELS privacy)
add_test(NAME lamusica_daw_onboarding_templates
         COMMAND lamusica_daw_smoke --onboarding-templates-smoke)
set_tests_properties(lamusica_daw_onboarding_templates PROPERTIES LABELS onboarding)

set(LAMUSICA_DAW_FIRST_TRACK_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/daw-first-track.Project.lamusica")
set(LAMUSICA_DAW_FIRST_TRACK_RENDER
    "${CMAKE_CURRENT_BINARY_DIR}/daw-first-track.wav")
set(LAMUSICA_DAW_HEADLESS_BINDING_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/daw-headless-binding.Project.lamusica")
add_test(NAME lamusica_daw_first_track_clean
         COMMAND ${CMAKE_COMMAND} -E rm -rf "${LAMUSICA_DAW_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_DAW_FIRST_TRACK_RENDER}" "${LAMUSICA_DAW_HEADLESS_BINDING_PROJECT}")
add_test(NAME lamusica_daw_create_first_track
         COMMAND lamusica_daw_smoke --create-first-track "${LAMUSICA_DAW_FIRST_TRACK_PROJECT}"
                 "DAW First Track")
add_test(NAME lamusica_daw_inspect_first_track_project
         COMMAND lamusica_artifact_assertions assert-first-track-ready
                 "${LAMUSICA_DAW_FIRST_TRACK_PROJECT}" 96000 3 2 3 3)
add_test(NAME lamusica_daw_render_first_track
         COMMAND lamusica_daw_smoke --render-project "${LAMUSICA_DAW_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_DAW_FIRST_TRACK_RENDER}")
add_test(NAME lamusica_daw_inspect_first_track_render
         COMMAND lamusica_artifact_assertions assert-wav "${LAMUSICA_DAW_FIRST_TRACK_RENDER}" 96000
                 16 0.001)
add_test(NAME lamusica_daw_headless_binding
         COMMAND lamusica_daw --headless-binding "${LAMUSICA_DAW_HEADLESS_BINDING_PROJECT}")
add_test(NAME lamusica_daw_headless_binding_artifact
         COMMAND lamusica_artifact_assertions assert-project
                 "${LAMUSICA_DAW_HEADLESS_BINDING_PROJECT}" 3 2)

set(LAMUSICA_CLI_LIFECYCLE_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/cli-lifecycle.Project.lamusica")
set(LAMUSICA_FIRST_TRACK_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/cli-first-track.Project.lamusica")
set(LAMUSICA_FIRST_TRACK_PACKAGE
    "${CMAKE_CURRENT_BINARY_DIR}/cli-first-track-package")
set(LAMUSICA_FIRST_TRACK_RENDER
    "${CMAKE_CURRENT_BINARY_DIR}/cli-first-track.wav")
set(LAMUSICA_TUTORIAL_FIRST_TRACK_RENDER
    "${CMAKE_CURRENT_BINARY_DIR}/tutorial-first-track.wav")
set(LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/cli-edit-first-track.Project.lamusica")
set(LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT
    "${CMAKE_CURRENT_BINARY_DIR}/cli-edit-first-track-import.wav")
set(LAMUSICA_CLI_EDIT_FIRST_TRACK_PACKAGE
    "${CMAKE_CURRENT_BINARY_DIR}/cli-edit-first-track-package")
set(LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/cli-punch-first-track.Project.lamusica")
set(LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/cli-mix-first-track.Project.lamusica")
set(LAMUSICA_CLI_GENERIC_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/cli-generic.Project.lamusica")
set(LAMUSICA_CLI_GENERIC_RENDER "${CMAKE_CURRENT_BINARY_DIR}/cli-generic-range.wav")
set(LAMUSICA_CLI_GENERIC_STEMS "${CMAKE_CURRENT_BINARY_DIR}/cli-generic-stems")
set(LAMUSICA_CLI_SCHEMA_JSON "${CMAKE_CURRENT_BINARY_DIR}/cli-schema.json")
set(LAMUSICA_CLI_GENERIC_QUERY_JSON "${CMAKE_CURRENT_BINARY_DIR}/cli-generic-query.json")
set(LAMUSICA_CLI_GENERIC_PREVIEW_JSON "${CMAKE_CURRENT_BINARY_DIR}/cli-generic-preview.json")
set(LAMUSICA_CLI_GENERIC_RENDER_JSON "${CMAKE_CURRENT_BINARY_DIR}/cli-generic-render.json")
add_test(NAME lamusica_cli_first_track_lifecycle_clean
         COMMAND ${CMAKE_COMMAND} -E rm -rf "${LAMUSICA_CLI_LIFECYCLE_PROJECT}"
                 "${LAMUSICA_FIRST_TRACK_PROJECT}" "${LAMUSICA_FIRST_TRACK_PACKAGE}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PACKAGE}"
                 "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_GENERIC_PROJECT}" "${LAMUSICA_CLI_GENERIC_RENDER}"
                 "${LAMUSICA_CLI_GENERIC_STEMS}" "${LAMUSICA_CLI_SCHEMA_JSON}"
                 "${LAMUSICA_CLI_GENERIC_QUERY_JSON}" "${LAMUSICA_CLI_GENERIC_PREVIEW_JSON}"
                 "${LAMUSICA_CLI_GENERIC_RENDER_JSON}")
add_test(
  NAME lamusica_cli_schema_json_contract
  COMMAND
    /bin/sh -c
    "$<TARGET_FILE:lamusica_cli> schema --json > \"${LAMUSICA_CLI_SCHEMA_JSON}\" && $<TARGET_FILE:lamusica_artifact_assertions> assert-cli-json \"${LAMUSICA_CLI_SCHEMA_JSON}\" schema")
add_test(NAME lamusica_cli_create_first_track_dependency_project
         COMMAND lamusica_cli create-project "${LAMUSICA_CLI_LIFECYCLE_PROJECT}"
                 "CLI Lifecycle")
add_test(NAME lamusica_cli_validate_first_track_dependency_project
         COMMAND lamusica_cli validate "${LAMUSICA_CLI_LIFECYCLE_PROJECT}")
add_test(NAME lamusica_cli_inspect_first_track_dependency_project
         COMMAND lamusica_artifact_assertions assert-project "${LAMUSICA_CLI_LIFECYCLE_PROJECT}" 0
                 0)
add_test(NAME lamusica_cli_reject_unready_first_track_project
         COMMAND lamusica_cli verify-first-track-project "${LAMUSICA_CLI_LIFECYCLE_PROJECT}")
add_test(NAME lamusica_cli_create_first_track
         COMMAND lamusica_cli create-first-track "${LAMUSICA_FIRST_TRACK_PROJECT}"
                 "CLI First Track")
add_test(NAME lamusica_cli_validate_first_track
         COMMAND lamusica_cli validate "${LAMUSICA_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_inspect_first_track_project
         COMMAND lamusica_artifact_assertions assert-first-track-ready
                 "${LAMUSICA_FIRST_TRACK_PROJECT}" 96000 3 2 3 3)
add_test(NAME lamusica_cli_verify_first_track_project
         COMMAND lamusica_cli verify-first-track-project "${LAMUSICA_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_export_first_track_package
         COMMAND lamusica_cli export-first-track-package "${LAMUSICA_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_FIRST_TRACK_PACKAGE}")
add_test(NAME lamusica_cli_verify_first_track_package
         COMMAND lamusica_cli verify-first-track-package "${LAMUSICA_FIRST_TRACK_PACKAGE}")
add_test(NAME lamusica_cli_assert_first_track_package
         COMMAND lamusica_artifact_assertions assert-package "${LAMUSICA_FIRST_TRACK_PACKAGE}"
                 96000 2 3 2 0)
add_test(NAME lamusica_cli_verify_examples
         COMMAND lamusica_cli verify-examples "${PROJECT_SOURCE_DIR}/fixtures/examples")
add_test(NAME lamusica_cli_assert_examples
         COMMAND lamusica_artifact_assertions assert-examples
                 "${PROJECT_SOURCE_DIR}/fixtures/examples" 1)
add_test(NAME lamusica_cli_create_punch_first_track
         COMMAND lamusica_cli create-first-track "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}"
                 "CLI Punch First Track")
add_test(NAME lamusica_cli_plan_punch_first_track
         COMMAND lamusica_cli plan-first-track-recording
                 "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}" 48000 0 2 48000 96000)
add_test(NAME lamusica_cli_record_punch_first_track
         COMMAND lamusica_cli record-first-track-take
                 "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}" 48000 0 2 48000 96000)
add_test(NAME lamusica_cli_assert_punch_first_track_take
         COMMAND lamusica_artifact_assertions assert-clip-timing
                 "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}" recorded-take-1 48000 48000 0)
add_test(NAME lamusica_cli_list_punch_first_track_takes
         COMMAND lamusica_cli list-first-track-takes "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_create_mix_first_track
         COMMAND lamusica_cli create-first-track "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}"
                 "CLI Mix First Track")
add_test(NAME lamusica_cli_set_mix_first_track
         COMMAND lamusica_cli set-first-track-track-mix
                 "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}" drums -12 0.25 false false)
add_test(NAME lamusica_cli_assert_mix_first_track
         COMMAND lamusica_artifact_assertions assert-track-mix
                 "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}" drums -12 0.25 false false)
add_test(NAME lamusica_cli_list_mix_first_track
         COMMAND lamusica_cli list-first-track-track-mix "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_create_generic_project
         COMMAND lamusica_cli create-first-track "${LAMUSICA_CLI_GENERIC_PROJECT}"
                 "CLI Generic")
add_test(NAME lamusica_cli_query_generic_project_json
         COMMAND lamusica_cli query "${LAMUSICA_CLI_GENERIC_PROJECT}" --json)
add_test(
  NAME lamusica_cli_query_generic_project_json_contract
  COMMAND
    /bin/sh -c
    "$<TARGET_FILE:lamusica_cli> query \"${LAMUSICA_CLI_GENERIC_PROJECT}\" --json > \"${LAMUSICA_CLI_GENERIC_QUERY_JSON}\" && $<TARGET_FILE:lamusica_artifact_assertions> assert-cli-json \"${LAMUSICA_CLI_GENERIC_QUERY_JSON}\" query")
add_test(NAME lamusica_cli_generic_edit_clip_gain_preview
         COMMAND lamusica_cli edit "${LAMUSICA_CLI_GENERIC_PROJECT}" set-clip-gain drum-loop -6
                 --dry-run)
add_test(NAME lamusica_cli_generic_edit_clip_gain_preview_json
         COMMAND lamusica_cli edit "${LAMUSICA_CLI_GENERIC_PROJECT}" set-clip-gain drum-loop -6
                 --dry-run --json)
add_test(
  NAME lamusica_cli_generic_edit_clip_gain_preview_json_contract
  COMMAND
    /bin/sh -c
    "$<TARGET_FILE:lamusica_cli> edit \"${LAMUSICA_CLI_GENERIC_PROJECT}\" set-clip-gain drum-loop -6 --dry-run --json > \"${LAMUSICA_CLI_GENERIC_PREVIEW_JSON}\" && $<TARGET_FILE:lamusica_artifact_assertions> assert-cli-json \"${LAMUSICA_CLI_GENERIC_PREVIEW_JSON}\" preview")
add_test(NAME lamusica_cli_generic_edit_preview_kept_project
         COMMAND lamusica_artifact_assertions assert-clip-gain
                 "${LAMUSICA_CLI_GENERIC_PROJECT}" drum-loop -9)
add_test(NAME lamusica_cli_generic_edit_clip_gain_confirm
         COMMAND lamusica_cli edit "${LAMUSICA_CLI_GENERIC_PROJECT}" set-clip-gain drum-loop -6
                 --confirm set-clip-gain:clip-drum-loop:confirm)
add_test(NAME lamusica_cli_generic_edit_clip_gain_artifact
         COMMAND lamusica_artifact_assertions assert-clip-gain
                 "${LAMUSICA_CLI_GENERIC_PROJECT}" drum-loop -6)
add_test(NAME lamusica_cli_generic_render_range_24bit
         COMMAND lamusica_cli render "${LAMUSICA_CLI_GENERIC_PROJECT}"
                 "${LAMUSICA_CLI_GENERIC_RENDER}" --format wav --bit-depth 24 --range 0:48000
                 --json)
add_test(
  NAME lamusica_cli_generic_render_range_24bit_json_contract
  COMMAND
    /bin/sh -c
    "$<TARGET_FILE:lamusica_cli> render \"${LAMUSICA_CLI_GENERIC_PROJECT}\" \"${LAMUSICA_CLI_GENERIC_RENDER}\" --format wav --bit-depth 24 --range 0:48000 --json > \"${LAMUSICA_CLI_GENERIC_RENDER_JSON}\" && $<TARGET_FILE:lamusica_artifact_assertions> assert-cli-json \"${LAMUSICA_CLI_GENERIC_RENDER_JSON}\" render")
add_test(NAME lamusica_cli_generic_render_rejects_bad_format
         COMMAND lamusica_cli render "${LAMUSICA_CLI_GENERIC_PROJECT}"
                 "${CMAKE_CURRENT_BINARY_DIR}/cli-generic.flac" --format flac)
add_test(NAME lamusica_cli_generic_render_range_artifact
         COMMAND lamusica_artifact_assertions assert-wav "${LAMUSICA_CLI_GENERIC_RENDER}" 48000
                 24 0.001)
add_test(NAME lamusica_cli_generic_render_stems_24bit
         COMMAND lamusica_cli render "${LAMUSICA_CLI_GENERIC_PROJECT}"
                 "${LAMUSICA_CLI_GENERIC_STEMS}" --format wav --bit-depth 24 --range 0:48000
                 --stems --json)
add_test(NAME lamusica_cli_generic_render_stems_artifact
         COMMAND lamusica_artifact_assertions assert-stems "${LAMUSICA_CLI_GENERIC_STEMS}" 2
                 48000 24 0.001)
add_test(NAME lamusica_cli_first_track_render_clean
         COMMAND ${CMAKE_COMMAND} -E rm -f "${LAMUSICA_FIRST_TRACK_RENDER}"
                 "${LAMUSICA_TUTORIAL_FIRST_TRACK_RENDER}")
add_test(NAME lamusica_cli_render_created_first_track
         COMMAND lamusica_cli render-project "${LAMUSICA_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_FIRST_TRACK_RENDER}")
add_test(NAME lamusica_cli_render_first_track_tutorial
         COMMAND lamusica_cli render-project
                 "${PROJECT_SOURCE_DIR}/fixtures/tutorials/first-song.Project.lamusica"
                 "${LAMUSICA_TUTORIAL_FIRST_TRACK_RENDER}")
add_test(NAME lamusica_cli_inspect_created_first_track_render
         COMMAND lamusica_artifact_assertions assert-wav "${LAMUSICA_FIRST_TRACK_RENDER}" 96000
                 16 0.001)
add_test(NAME lamusica_cli_inspect_first_track_render
         COMMAND lamusica_artifact_assertions assert-wav "${LAMUSICA_TUTORIAL_FIRST_TRACK_RENDER}"
                 96000 16 0.001)
add_test(NAME lamusica_cli_inspect_first_track_tutorial_project
         COMMAND lamusica_artifact_assertions assert-first-track-ready
                 "${PROJECT_SOURCE_DIR}/fixtures/tutorials/first-song.Project.lamusica" 96000 3 2
                 3 3)
add_test(NAME lamusica_cli_benchmark_smoke COMMAND lamusica_cli benchmark-smoke)
add_test(NAME lamusica_cli_create_edit_first_track
         COMMAND lamusica_cli create-first-track "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 "CLI Edited First Track")
add_test(NAME lamusica_cli_list_initial_first_track_clips
         COMMAND lamusica_cli list-first-track-clips "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_assert_initial_first_track_clips
         COMMAND lamusica_artifact_assertions assert-project
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 3 2)
add_test(NAME lamusica_cli_transpose_edit_first_track
         COMMAND lamusica_cli transpose-first-track-bass
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 12)
add_test(NAME lamusica_cli_assert_transpose_edit_first_track
         COMMAND lamusica_artifact_assertions assert-first-track-state
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 96000 12 true 0 96000 3 2)
add_test(NAME lamusica_cli_gain_edit_first_track
         COMMAND lamusica_cli set-first-track-clip-gain
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" drum-loop -18)
add_test(NAME lamusica_cli_assert_gain_edit_first_track
         COMMAND lamusica_artifact_assertions assert-clip-gain
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" drum-loop -18)
add_test(NAME lamusica_cli_loop_edit_first_track
         COMMAND lamusica_cli set-first-track-loop-intro
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_assert_loop_edit_first_track
         COMMAND lamusica_artifact_assertions assert-first-track-state
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 96000 12 true 0 96000 3 2)
add_test(NAME lamusica_cli_extend_edit_first_track
         COMMAND lamusica_cli extend-first-track-verse
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_assert_extend_edit_first_track
         COMMAND lamusica_artifact_assertions assert-first-track-state
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 192000 12 true 0 96000 3 2)
add_test(NAME lamusica_cli_duplicate_bass_first_track
         COMMAND lamusica_cli duplicate-first-track-clip
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" bass-pattern bass-pattern-copy 96000)
add_test(NAME lamusica_cli_assert_duplicate_bass_first_track
         COMMAND lamusica_artifact_assertions assert-clip-timing
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" bass-pattern-copy 96000 192000 0)
add_test(NAME lamusica_cli_remove_bass_duplicate_first_track
         COMMAND lamusica_cli remove-first-track-clip "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 bass-pattern-copy)
add_test(NAME lamusica_cli_assert_removed_bass_duplicate_first_track
         COMMAND lamusica_artifact_assertions assert-clip-missing
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" bass-pattern-copy)
add_test(NAME lamusica_cli_plan_record_edit_first_track
         COMMAND lamusica_cli plan-first-track-recording
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 24000 48000 2)
add_test(NAME lamusica_cli_record_edit_first_track
         COMMAND lamusica_cli record-first-track-take
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 24000)
add_test(NAME lamusica_cli_assert_record_edit_first_track_take
         COMMAND lamusica_artifact_assertions assert-clip-timing
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 0 24000 0)
add_test(NAME lamusica_cli_fade_recorded_first_track_take
         COMMAND lamusica_cli set-first-track-clip-fades
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 512 1024)
add_test(NAME lamusica_cli_assert_fade_recorded_first_track_take
         COMMAND lamusica_artifact_assertions assert-clip-fades
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 512 1024)
add_test(NAME lamusica_cli_reverse_recorded_first_track_take
         COMMAND lamusica_cli set-first-track-clip-reversed
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 true)
add_test(NAME lamusica_cli_assert_reverse_recorded_first_track_take
         COMMAND lamusica_artifact_assertions assert-clip-reversed
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 true)
add_test(NAME lamusica_cli_list_recorded_first_track_takes
         COMMAND lamusica_cli list-first-track-takes "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_mute_recorded_first_track_take
         COMMAND lamusica_cli mute-first-track-take
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 true)
add_test(NAME lamusica_cli_assert_mute_recorded_first_track_take
         COMMAND lamusica_artifact_assertions assert-clip-muted
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 true)
add_test(NAME lamusica_cli_list_muted_first_track_takes
         COMMAND lamusica_cli list-first-track-takes "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_unmute_recorded_first_track_take
         COMMAND lamusica_cli mute-first-track-take
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 false)
add_test(NAME lamusica_cli_assert_unmute_recorded_first_track_take
         COMMAND lamusica_artifact_assertions assert-clip-muted
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 false)
add_test(NAME lamusica_cli_render_edit_first_track_import_source
         COMMAND lamusica_cli render-test-tone "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}")
add_test(NAME lamusica_cli_assert_edit_first_track_import_source
         COMMAND lamusica_artifact_assertions assert-wav "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}"
                 48000 16 0.001)
add_test(NAME lamusica_cli_import_edit_first_track
         COMMAND lamusica_cli import-first-track-audio
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}" 48000)
add_test(NAME lamusica_cli_assert_import_edit_first_track
         COMMAND lamusica_artifact_assertions assert-clip-timing
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-1 48000 48000 0)
add_test(NAME lamusica_cli_timing_edit_imported_first_track
         COMMAND lamusica_cli set-first-track-clip-timing
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-1 72000 24000 12000)
add_test(NAME lamusica_cli_assert_timing_edit_imported_first_track
         COMMAND lamusica_artifact_assertions assert-clip-timing
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-1 72000 24000 12000)
add_test(NAME lamusica_cli_duplicate_imported_first_track
         COMMAND lamusica_cli duplicate-first-track-clip
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-1 imported-audio-copy
                 108000)
add_test(NAME lamusica_cli_assert_duplicate_imported_first_track
         COMMAND lamusica_artifact_assertions assert-clip-timing
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-copy 108000 24000 12000)
add_test(NAME lamusica_cli_mute_imported_first_track
         COMMAND lamusica_cli set-first-track-clip-muted
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-copy true)
add_test(NAME lamusica_cli_assert_mute_imported_first_track
         COMMAND lamusica_artifact_assertions assert-clip-muted
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-copy true)
add_test(NAME lamusica_cli_unmute_imported_first_track
         COMMAND lamusica_cli set-first-track-clip-muted
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-copy false)
add_test(NAME lamusica_cli_assert_unmute_imported_first_track
         COMMAND lamusica_artifact_assertions assert-clip-muted
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-copy false)
add_test(NAME lamusica_cli_list_edited_first_track_clips
         COMMAND lamusica_cli list-first-track-clips "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_inspect_edit_first_track
         COMMAND lamusica_cli inspect-project "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_assert_edit_first_track_state
         COMMAND lamusica_artifact_assertions assert-first-track-state
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 192000 12 true 0 96000 5 5)
add_test(NAME lamusica_cli_export_edit_first_track_package
         COMMAND lamusica_cli export-first-track-package
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PACKAGE}")
add_test(NAME lamusica_cli_verify_edit_first_track_package
         COMMAND lamusica_cli verify-first-track-package
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PACKAGE}")
add_test(NAME lamusica_cli_assert_edit_first_track_package
         COMMAND lamusica_artifact_assertions assert-package
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PACKAGE}" 192000 4 5 5 2)
add_test(NAME lamusica_cli_remove_edit_first_track_media
         COMMAND ${CMAKE_COMMAND} -E rm -f
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}/Assets/imported-audio-1.wav")
add_test(NAME lamusica_cli_assert_edit_first_track_missing_media
         COMMAND lamusica_artifact_assertions assert-assets
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 1 1)
add_test(NAME lamusica_cli_inspect_edit_first_track_missing_media
         COMMAND lamusica_cli inspect-project "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_daw_inspect_edit_first_track_missing_media
         COMMAND lamusica_daw_smoke --inspect-project "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_relink_edit_first_track_media
         COMMAND lamusica_cli relink-first-track-audio
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-1
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}")
add_test(NAME lamusica_cli_assert_edit_first_track_relinked_media
         COMMAND lamusica_artifact_assertions assert-assets
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 2 0)
add_test(NAME lamusica_cli_inspect_edit_first_track_relinked_media
         COMMAND lamusica_cli inspect-project "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")

set_tests_properties(lamusica_daw_create_first_track
                     PROPERTIES DEPENDS lamusica_daw_first_track_clean)
set_tests_properties(lamusica_daw_inspect_first_track_project
                     PROPERTIES DEPENDS lamusica_daw_create_first_track)
set_tests_properties(lamusica_daw_render_first_track
                     PROPERTIES DEPENDS lamusica_daw_inspect_first_track_project)
set_tests_properties(lamusica_daw_inspect_first_track_render
                     PROPERTIES DEPENDS lamusica_daw_render_first_track)
set_tests_properties(lamusica_daw_headless_binding
                     PROPERTIES DEPENDS lamusica_daw_first_track_clean LABELS gui)
set_tests_properties(lamusica_daw_headless_binding_artifact
                     PROPERTIES DEPENDS lamusica_daw_headless_binding LABELS gui)
set_tests_properties(lamusica_cli_create_first_track_dependency_project
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean)
set_tests_properties(lamusica_cli_validate_first_track_dependency_project
                     PROPERTIES DEPENDS lamusica_cli_create_first_track_dependency_project)
set_tests_properties(lamusica_cli_inspect_first_track_dependency_project
                     PROPERTIES DEPENDS lamusica_cli_validate_first_track_dependency_project)
set_tests_properties(lamusica_cli_reject_unready_first_track_project
                     PROPERTIES DEPENDS lamusica_cli_inspect_first_track_dependency_project
                                WILL_FAIL TRUE)
set_tests_properties(lamusica_cli_create_first_track
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean)
set_tests_properties(lamusica_cli_validate_first_track
                     PROPERTIES DEPENDS lamusica_cli_create_first_track)
set_tests_properties(lamusica_cli_inspect_first_track_project
                     PROPERTIES DEPENDS lamusica_cli_validate_first_track)
set_tests_properties(lamusica_cli_verify_first_track_project
                     PROPERTIES DEPENDS lamusica_cli_inspect_first_track_project)
set_tests_properties(lamusica_cli_export_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_verify_first_track_project)
set_tests_properties(lamusica_cli_verify_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_export_first_track_package)
set_tests_properties(lamusica_cli_assert_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_verify_first_track_package LABELS cli)
set_tests_properties(lamusica_cli_verify_examples PROPERTIES LABELS cli)
set_tests_properties(lamusica_cli_assert_examples
                     PROPERTIES DEPENDS lamusica_cli_verify_examples LABELS cli)
set_tests_properties(lamusica_cli_create_punch_first_track
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean)
set_tests_properties(lamusica_cli_plan_punch_first_track
                     PROPERTIES DEPENDS lamusica_cli_create_punch_first_track)
set_tests_properties(lamusica_cli_record_punch_first_track
                     PROPERTIES DEPENDS lamusica_cli_plan_punch_first_track)
set_tests_properties(lamusica_cli_assert_punch_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_record_punch_first_track)
set_tests_properties(lamusica_cli_list_punch_first_track_takes
                     PROPERTIES DEPENDS lamusica_cli_assert_punch_first_track_take)
set_tests_properties(lamusica_cli_create_mix_first_track
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean)
set_tests_properties(lamusica_cli_set_mix_first_track
                     PROPERTIES DEPENDS lamusica_cli_create_mix_first_track)
set_tests_properties(lamusica_cli_assert_mix_first_track
                     PROPERTIES DEPENDS lamusica_cli_set_mix_first_track)
set_tests_properties(lamusica_cli_list_mix_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_mix_first_track)
set_tests_properties(lamusica_cli_create_generic_project
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean)
set_tests_properties(lamusica_cli_query_generic_project_json
                     PROPERTIES DEPENDS lamusica_cli_create_generic_project LABELS cli)
set_tests_properties(lamusica_cli_generic_edit_clip_gain_preview
                     PROPERTIES DEPENDS lamusica_cli_query_generic_project_json LABELS cli)
set_tests_properties(lamusica_cli_generic_edit_clip_gain_preview_json
                     PROPERTIES DEPENDS lamusica_cli_generic_edit_clip_gain_preview LABELS cli)
set_tests_properties(lamusica_cli_generic_edit_preview_kept_project
                     PROPERTIES DEPENDS lamusica_cli_generic_edit_clip_gain_preview_json LABELS cli)
set_tests_properties(lamusica_cli_generic_edit_clip_gain_confirm
                     PROPERTIES DEPENDS lamusica_cli_generic_edit_preview_kept_project LABELS cli)
set_tests_properties(lamusica_cli_generic_edit_clip_gain_artifact
                     PROPERTIES DEPENDS lamusica_cli_generic_edit_clip_gain_confirm LABELS cli)
set_tests_properties(lamusica_cli_generic_render_range_24bit
                     PROPERTIES DEPENDS lamusica_cli_generic_edit_clip_gain_artifact LABELS cli)
set_tests_properties(lamusica_cli_generic_render_rejects_bad_format
                     PROPERTIES DEPENDS lamusica_cli_generic_edit_clip_gain_artifact LABELS cli
                                WILL_FAIL TRUE)
set_tests_properties(lamusica_cli_generic_render_range_artifact
                     PROPERTIES DEPENDS
                                "lamusica_cli_generic_render_range_24bit;lamusica_cli_generic_render_rejects_bad_format"
                                LABELS cli)
set_tests_properties(lamusica_cli_generic_render_stems_24bit
                     PROPERTIES DEPENDS lamusica_cli_generic_edit_clip_gain_artifact LABELS cli)
set_tests_properties(lamusica_cli_generic_render_stems_artifact
                     PROPERTIES DEPENDS lamusica_cli_generic_render_stems_24bit LABELS cli)
set_tests_properties(lamusica_cli_render_created_first_track
                     PROPERTIES DEPENDS
                                "lamusica_cli_inspect_first_track_project;lamusica_cli_first_track_render_clean")
set_tests_properties(lamusica_cli_render_first_track_tutorial
                     PROPERTIES DEPENDS lamusica_cli_first_track_render_clean)
set_tests_properties(lamusica_cli_inspect_created_first_track_render
                     PROPERTIES DEPENDS lamusica_cli_render_created_first_track)
set_tests_properties(lamusica_cli_inspect_first_track_render
                     PROPERTIES DEPENDS lamusica_cli_render_first_track_tutorial)
set_tests_properties(lamusica_cli_inspect_first_track_tutorial_project PROPERTIES LABELS cli)
set_tests_properties(lamusica_cli_benchmark_smoke PROPERTIES LABELS perf)
set_tests_properties(lamusica_cli_create_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean)
set_tests_properties(lamusica_cli_list_initial_first_track_clips
                     PROPERTIES DEPENDS lamusica_cli_create_edit_first_track)
set_tests_properties(lamusica_cli_assert_initial_first_track_clips
                     PROPERTIES DEPENDS lamusica_cli_list_initial_first_track_clips)
set_tests_properties(lamusica_cli_transpose_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_initial_first_track_clips)
set_tests_properties(lamusica_cli_assert_transpose_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_transpose_edit_first_track)
set_tests_properties(lamusica_cli_gain_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_transpose_edit_first_track)
set_tests_properties(lamusica_cli_assert_gain_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_gain_edit_first_track)
set_tests_properties(lamusica_cli_loop_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_gain_edit_first_track)
set_tests_properties(lamusica_cli_assert_loop_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_loop_edit_first_track)
set_tests_properties(lamusica_cli_extend_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_loop_edit_first_track)
set_tests_properties(lamusica_cli_assert_extend_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_extend_edit_first_track)
set_tests_properties(lamusica_cli_duplicate_bass_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_extend_edit_first_track)
set_tests_properties(lamusica_cli_assert_duplicate_bass_first_track
                     PROPERTIES DEPENDS lamusica_cli_duplicate_bass_first_track)
set_tests_properties(lamusica_cli_remove_bass_duplicate_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_duplicate_bass_first_track)
set_tests_properties(lamusica_cli_assert_removed_bass_duplicate_first_track
                     PROPERTIES DEPENDS lamusica_cli_remove_bass_duplicate_first_track)
set_tests_properties(lamusica_cli_plan_record_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_removed_bass_duplicate_first_track)
set_tests_properties(lamusica_cli_record_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_plan_record_edit_first_track)
set_tests_properties(lamusica_cli_assert_record_edit_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_record_edit_first_track)
set_tests_properties(lamusica_cli_fade_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_assert_record_edit_first_track_take)
set_tests_properties(lamusica_cli_assert_fade_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_fade_recorded_first_track_take)
set_tests_properties(lamusica_cli_reverse_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_assert_fade_recorded_first_track_take)
set_tests_properties(lamusica_cli_assert_reverse_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_reverse_recorded_first_track_take)
set_tests_properties(lamusica_cli_list_recorded_first_track_takes
                     PROPERTIES DEPENDS lamusica_cli_assert_reverse_recorded_first_track_take)
set_tests_properties(lamusica_cli_mute_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_list_recorded_first_track_takes)
set_tests_properties(lamusica_cli_assert_mute_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_mute_recorded_first_track_take)
set_tests_properties(lamusica_cli_list_muted_first_track_takes
                     PROPERTIES DEPENDS lamusica_cli_assert_mute_recorded_first_track_take)
set_tests_properties(lamusica_cli_unmute_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_list_muted_first_track_takes)
set_tests_properties(lamusica_cli_assert_unmute_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_unmute_recorded_first_track_take)
set_tests_properties(lamusica_cli_render_edit_first_track_import_source
                     PROPERTIES DEPENDS lamusica_cli_assert_unmute_recorded_first_track_take)
set_tests_properties(lamusica_cli_assert_edit_first_track_import_source
                     PROPERTIES DEPENDS lamusica_cli_render_edit_first_track_import_source)
set_tests_properties(lamusica_cli_import_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_edit_first_track_import_source)
set_tests_properties(lamusica_cli_assert_import_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_import_edit_first_track)
set_tests_properties(lamusica_cli_timing_edit_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_import_edit_first_track)
set_tests_properties(lamusica_cli_assert_timing_edit_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_timing_edit_imported_first_track)
set_tests_properties(lamusica_cli_duplicate_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_timing_edit_imported_first_track)
set_tests_properties(lamusica_cli_assert_duplicate_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_duplicate_imported_first_track)
set_tests_properties(lamusica_cli_mute_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_duplicate_imported_first_track)
set_tests_properties(lamusica_cli_assert_mute_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_mute_imported_first_track)
set_tests_properties(lamusica_cli_unmute_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_assert_mute_imported_first_track)
set_tests_properties(lamusica_cli_assert_unmute_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_unmute_imported_first_track)
set_tests_properties(lamusica_cli_list_edited_first_track_clips
                     PROPERTIES DEPENDS lamusica_cli_assert_unmute_imported_first_track)
set_tests_properties(lamusica_cli_inspect_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_list_edited_first_track_clips)
set_tests_properties(lamusica_cli_assert_edit_first_track_state
                     PROPERTIES DEPENDS lamusica_cli_inspect_edit_first_track)
set_tests_properties(lamusica_cli_export_edit_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_assert_edit_first_track_state)
set_tests_properties(lamusica_cli_verify_edit_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_export_edit_first_track_package)
set_tests_properties(lamusica_cli_assert_edit_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_verify_edit_first_track_package LABELS cli)
set_tests_properties(lamusica_cli_remove_edit_first_track_media
                     PROPERTIES DEPENDS lamusica_cli_assert_edit_first_track_package)
set_tests_properties(lamusica_cli_assert_edit_first_track_missing_media
                     PROPERTIES DEPENDS lamusica_cli_remove_edit_first_track_media)
set_tests_properties(lamusica_cli_inspect_edit_first_track_missing_media
                     PROPERTIES DEPENDS lamusica_cli_assert_edit_first_track_missing_media)
set_tests_properties(lamusica_daw_inspect_edit_first_track_missing_media
                     PROPERTIES DEPENDS lamusica_cli_assert_edit_first_track_missing_media)
set_tests_properties(lamusica_cli_relink_edit_first_track_media
                     PROPERTIES DEPENDS lamusica_daw_inspect_edit_first_track_missing_media)
set_tests_properties(lamusica_cli_assert_edit_first_track_relinked_media
                     PROPERTIES DEPENDS lamusica_cli_relink_edit_first_track_media)
set_tests_properties(lamusica_cli_inspect_edit_first_track_relinked_media
                     PROPERTIES DEPENDS lamusica_cli_assert_edit_first_track_relinked_media)

set(LAMUSICA_MISSING_ASSET_EXAMPLE
    "${CMAKE_CURRENT_BINARY_DIR}/missing-assets/bad.Project.lamusica")
file(MAKE_DIRECTORY "${LAMUSICA_MISSING_ASSET_EXAMPLE}")
file(WRITE "${LAMUSICA_MISSING_ASSET_EXAMPLE}/project.json"
     [=[
{
  "schemaVersion": 3,
  "name": "Missing Asset Example",
  "projectSampleRate": 48000.0,
  "loopEnabled": false,
  "loopStartSample": 0,
  "loopEndSample": 0,
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
      "sourceOffsetSamples": 0,
      "fadeInSamples": 0,
      "fadeOutSamples": 0,
      "gainDb": 0.0,
      "muted": false,
      "reversed": false,
      "assetId": "missing-audio"
    }
  ],
  "midiClips": [],
  "takeLanes": [],
  "comps": [],
  "routing": [],
  "trackMix": [],
  "plugins": [],
  "automation": [],
  "mcpAuditLog": []
}
]=])
add_test(NAME lamusica_cli_verify_examples_rejects_missing_assets
         COMMAND lamusica_cli verify-examples "${CMAKE_CURRENT_BINARY_DIR}/missing-assets")
set_tests_properties(lamusica_cli_verify_examples_rejects_missing_assets
                     PROPERTIES WILL_FAIL TRUE)

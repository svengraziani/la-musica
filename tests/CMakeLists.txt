add_executable(lamusica_unit_tests unit/bootstrap_tests.cpp)
target_link_libraries(
  lamusica_unit_tests
  PRIVATE lamusica::mcp_bridge)
lamusica_configure_target(lamusica_unit_tests)

add_executable(lamusica_mcp_fixture_client fixtures/mcp_fixture_client.cpp)
lamusica_configure_target(lamusica_mcp_fixture_client)

add_test(NAME lamusica_unit_tests COMMAND lamusica_unit_tests)
set_tests_properties(lamusica_unit_tests PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

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

set(LAMUSICA_DAW_FIRST_TRACK_PROJECT
    "${CMAKE_CURRENT_BINARY_DIR}/daw-first-track.Project.lamusica")
set(LAMUSICA_DAW_FIRST_TRACK_RENDER
    "${CMAKE_CURRENT_BINARY_DIR}/daw-first-track.wav")
add_test(NAME lamusica_daw_first_track_clean
         COMMAND ${CMAKE_COMMAND} -E rm -rf "${LAMUSICA_DAW_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_DAW_FIRST_TRACK_RENDER}")
add_test(NAME lamusica_daw_create_first_track
         COMMAND lamusica_daw_smoke --create-first-track "${LAMUSICA_DAW_FIRST_TRACK_PROJECT}"
                 "DAW First Track")
add_test(NAME lamusica_daw_inspect_first_track_project
         COMMAND lamusica_daw_smoke --inspect-project "${LAMUSICA_DAW_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_daw_render_first_track
         COMMAND lamusica_daw_smoke --render-project "${LAMUSICA_DAW_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_DAW_FIRST_TRACK_RENDER}")
add_test(NAME lamusica_daw_inspect_first_track_render
         COMMAND lamusica_cli inspect-wav "${LAMUSICA_DAW_FIRST_TRACK_RENDER}")

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
add_test(NAME lamusica_cli_first_track_lifecycle_clean
         COMMAND ${CMAKE_COMMAND} -E rm -rf "${LAMUSICA_CLI_LIFECYCLE_PROJECT}"
                 "${LAMUSICA_FIRST_TRACK_PROJECT}" "${LAMUSICA_FIRST_TRACK_PACKAGE}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PACKAGE}"
                 "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_create_first_track_dependency_project
         COMMAND lamusica_cli create-project "${LAMUSICA_CLI_LIFECYCLE_PROJECT}"
                 "CLI Lifecycle")
add_test(NAME lamusica_cli_validate_first_track_dependency_project
         COMMAND lamusica_cli validate "${LAMUSICA_CLI_LIFECYCLE_PROJECT}")
add_test(NAME lamusica_cli_inspect_first_track_dependency_project
         COMMAND lamusica_cli inspect-project "${LAMUSICA_CLI_LIFECYCLE_PROJECT}")
add_test(NAME lamusica_cli_reject_unready_first_track_project
         COMMAND lamusica_cli verify-first-track-project "${LAMUSICA_CLI_LIFECYCLE_PROJECT}")
add_test(NAME lamusica_cli_create_first_track
         COMMAND lamusica_cli create-first-track "${LAMUSICA_FIRST_TRACK_PROJECT}"
                 "CLI First Track")
add_test(NAME lamusica_cli_validate_first_track
         COMMAND lamusica_cli validate "${LAMUSICA_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_inspect_first_track_project
         COMMAND lamusica_cli inspect-project "${LAMUSICA_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_verify_first_track_project
         COMMAND lamusica_cli verify-first-track-project "${LAMUSICA_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_export_first_track_package
         COMMAND lamusica_cli export-first-track-package "${LAMUSICA_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_FIRST_TRACK_PACKAGE}")
add_test(NAME lamusica_cli_verify_first_track_package
         COMMAND lamusica_cli verify-first-track-package "${LAMUSICA_FIRST_TRACK_PACKAGE}")
add_test(NAME lamusica_cli_verify_examples
         COMMAND lamusica_cli verify-examples "${PROJECT_SOURCE_DIR}/fixtures/examples")
add_test(NAME lamusica_cli_create_punch_first_track
         COMMAND lamusica_cli create-first-track "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}"
                 "CLI Punch First Track")
add_test(NAME lamusica_cli_plan_punch_first_track
         COMMAND lamusica_cli plan-first-track-recording
                 "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}" 48000 0 2 48000 96000)
add_test(NAME lamusica_cli_record_punch_first_track
         COMMAND lamusica_cli record-first-track-take
                 "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}" 48000 0 2 48000 96000)
add_test(NAME lamusica_cli_list_punch_first_track_takes
         COMMAND lamusica_cli list-first-track-takes "${LAMUSICA_CLI_PUNCH_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_create_mix_first_track
         COMMAND lamusica_cli create-first-track "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}"
                 "CLI Mix First Track")
add_test(NAME lamusica_cli_set_mix_first_track
         COMMAND lamusica_cli set-first-track-track-mix
                 "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}" drums -12 0.25 false false)
add_test(NAME lamusica_cli_list_mix_first_track
         COMMAND lamusica_cli list-first-track-track-mix "${LAMUSICA_CLI_MIX_FIRST_TRACK_PROJECT}")
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
         COMMAND lamusica_cli inspect-wav "${LAMUSICA_FIRST_TRACK_RENDER}")
add_test(NAME lamusica_cli_inspect_first_track_render
         COMMAND lamusica_cli inspect-wav "${LAMUSICA_TUTORIAL_FIRST_TRACK_RENDER}")
add_test(NAME lamusica_cli_inspect_first_track_tutorial_project
         COMMAND lamusica_cli inspect-project
                 "${PROJECT_SOURCE_DIR}/fixtures/tutorials/first-song.Project.lamusica")
add_test(NAME lamusica_cli_benchmark_smoke COMMAND lamusica_cli benchmark-smoke)
add_test(NAME lamusica_cli_create_edit_first_track
         COMMAND lamusica_cli create-first-track "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 "CLI Edited First Track")
add_test(NAME lamusica_cli_list_initial_first_track_clips
         COMMAND lamusica_cli list-first-track-clips "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_transpose_edit_first_track
         COMMAND lamusica_cli transpose-first-track-bass
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 12)
add_test(NAME lamusica_cli_gain_edit_first_track
         COMMAND lamusica_cli set-first-track-clip-gain
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" drum-loop -18)
add_test(NAME lamusica_cli_loop_edit_first_track
         COMMAND lamusica_cli set-first-track-loop-intro
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_extend_edit_first_track
         COMMAND lamusica_cli extend-first-track-verse
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_duplicate_bass_first_track
         COMMAND lamusica_cli duplicate-first-track-clip
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" bass-pattern bass-pattern-copy 96000)
add_test(NAME lamusica_cli_remove_bass_duplicate_first_track
         COMMAND lamusica_cli remove-first-track-clip "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 bass-pattern-copy)
add_test(NAME lamusica_cli_plan_record_edit_first_track
         COMMAND lamusica_cli plan-first-track-recording
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 24000 48000 2)
add_test(NAME lamusica_cli_record_edit_first_track
         COMMAND lamusica_cli record-first-track-take
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" 24000)
add_test(NAME lamusica_cli_fade_recorded_first_track_take
         COMMAND lamusica_cli set-first-track-clip-fades
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 512 1024)
add_test(NAME lamusica_cli_reverse_recorded_first_track_take
         COMMAND lamusica_cli set-first-track-clip-reversed
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 true)
add_test(NAME lamusica_cli_list_recorded_first_track_takes
         COMMAND lamusica_cli list-first-track-takes "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_mute_recorded_first_track_take
         COMMAND lamusica_cli mute-first-track-take
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 true)
add_test(NAME lamusica_cli_list_muted_first_track_takes
         COMMAND lamusica_cli list-first-track-takes "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_unmute_recorded_first_track_take
         COMMAND lamusica_cli mute-first-track-take
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" recorded-take-1 false)
add_test(NAME lamusica_cli_render_edit_first_track_import_source
         COMMAND lamusica_cli render-test-tone "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}")
add_test(NAME lamusica_cli_import_edit_first_track
         COMMAND lamusica_cli import-first-track-audio
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}" 48000)
add_test(NAME lamusica_cli_timing_edit_imported_first_track
         COMMAND lamusica_cli set-first-track-clip-timing
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-1 72000 24000 12000)
add_test(NAME lamusica_cli_duplicate_imported_first_track
         COMMAND lamusica_cli duplicate-first-track-clip
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-1 imported-audio-copy
                 108000)
add_test(NAME lamusica_cli_mute_imported_first_track
         COMMAND lamusica_cli set-first-track-clip-muted
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-copy true)
add_test(NAME lamusica_cli_unmute_imported_first_track
         COMMAND lamusica_cli set-first-track-clip-muted
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-copy false)
add_test(NAME lamusica_cli_list_edited_first_track_clips
         COMMAND lamusica_cli list-first-track-clips "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_inspect_edit_first_track
         COMMAND lamusica_cli inspect-project "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_export_edit_first_track_package
         COMMAND lamusica_cli export-first-track-package
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}"
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PACKAGE}")
add_test(NAME lamusica_cli_verify_edit_first_track_package
         COMMAND lamusica_cli verify-first-track-package
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PACKAGE}")
add_test(NAME lamusica_cli_remove_edit_first_track_media
         COMMAND ${CMAKE_COMMAND} -E rm -f
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}/Assets/imported-audio-1.wav")
add_test(NAME lamusica_cli_inspect_edit_first_track_missing_media
         COMMAND lamusica_cli inspect-project "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_daw_inspect_edit_first_track_missing_media
         COMMAND lamusica_daw_smoke --inspect-project "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")
add_test(NAME lamusica_cli_relink_edit_first_track_media
         COMMAND lamusica_cli relink-first-track-audio
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}" imported-audio-1
                 "${LAMUSICA_CLI_EDIT_FIRST_TRACK_IMPORT}")
add_test(NAME lamusica_cli_inspect_edit_first_track_relinked_media
         COMMAND lamusica_cli inspect-project "${LAMUSICA_CLI_EDIT_FIRST_TRACK_PROJECT}")

set_tests_properties(lamusica_mcpd_health
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "lamusica-mcpd health=ok state=idle lifecycle=running")
set_tests_properties(lamusica_daw_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION "ready=true")
set_tests_properties(lamusica_daw_app_session_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*firstTrackEditable=true.*mediaReady=true.*tracks=3.*clips=2.*plugins=3.*automation=3.*starterMidiNotes=8.*renderFrames=96000")
set_tests_properties(lamusica_daw_app_session_verify_first_track_project_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "unreadyFirstTrackReady=false.*unreadyFirstTrackEditable=false.*unreadyMediaReady=true.*unreadyStarterStructure=false.*unreadyRenderable=false.*readyFirstTrackReady=true.*readyFirstTrackEditable=true.*readyMediaReady=true.*readyStarterStructure=true.*readyRenderable=true.*readyLoopEnabled=true.*readyLoopFrames=96000.*readyRenderFrames=96000.*readyTracks=3.*readyClips=2.*readyPlugins=3.*readyAutomation=3.*readyStarterMidiNotes=8.*unreadyMissingRequirements=19.*readyMissingRequirements=0")
set_tests_properties(lamusica_daw_app_session_render_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*frames=96000.*statusFrames=96000.*starterMidiNotes=8")
set_tests_properties(lamusica_daw_app_session_preferences_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "audioDevice=first-track-output.*midiInputs=1.*pluginPaths=1.*mcpEnabled=true.*mutationAllowed=true.*shortcuts=1.*playShortcut=p.*privacyScan=true.*rejectedUnsafeMutation=true")
set_tests_properties(lamusica_daw_app_session_transpose_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*starterMidiNotes=8.*bassTranspose=12.*renderFrames=96000")
set_tests_properties(lamusica_daw_app_session_mix_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*drumGainDb=-18.*bassGainDb=-3.*drumTrackVolumeDb=-12.*drumPan=0.25.*bassPan=-0.25.*bassMuted=true.*frames=96000")
set_tests_properties(lamusica_daw_app_session_loop_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*loopEnabled=true.*loopStart=0.*loopEnd=96000.*exportFrames=96000")
set_tests_properties(lamusica_daw_app_session_transport_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*playing=false.*playhead=64.*loopEnabled=true.*auditionFrames=128")
set_tests_properties(lamusica_daw_app_session_arrange_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*renderFrames=192000.*loopFrames=96000.*bassNotes=16")
set_tests_properties(lamusica_daw_app_session_stem_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*stemCount=2.*stemFrames=192000.*filesExist=true")
set_tests_properties(lamusica_daw_app_session_package_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*mixFrames=192000.*loopFrames=96000.*stemCount=2.*packageManifest=true.*packageVerified=true.*projectSnapshotVerified=true.*projectAssets=0.*filesExist=true")
set_tests_properties(lamusica_daw_app_session_record_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*recordedTakeCount=1.*tracks=4.*clips=3.*recordedFrames=48000.*reopenedLastRecordingClip=recorded-take-1.*fileExists=true")
set_tests_properties(lamusica_daw_app_session_record_package_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*recordedTakeCount=1.*mixFrames=192000.*loopFrames=96000.*stemCount=3.*recordedTrackVolumeDb=-6.*recordedPan=0.5.*hasRecordedStem=true.*packageManifest=true.*packageVerified=true.*projectSnapshotVerified=true.*projectAssets=1.*filesExist=true")
set_tests_properties(lamusica_daw_app_session_import_package_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*importedAudioClipCount=1.*importFrames=24000.*mixFrames=192000.*loopFrames=96000.*stemCount=3.*importedTrackVolumeDb=-9.*importedPan=-0.5.*reopenedLastImportClip=imported-audio-1.*hasImportedStem=true.*packageManifest=true.*packageVerified=true.*projectSnapshotVerified=true.*projectAssets=1.*filesExist=true")
set_tests_properties(lamusica_daw_app_session_import_edit_first_track_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*verified=true.*importedAudioClipCount=1.*importedFrames=48000.*duplicateClip=imported-audio-edit-copy.*reopenedLastImportClip=imported-audio-edit-copy.*copiedReady=true.*clips=3.*tracks=4")
set_tests_properties(lamusica_daw_create_first_track
                     PROPERTIES DEPENDS lamusica_daw_first_track_clean
                                PASS_REGULAR_EXPRESSION
                                "created first-track project: DAW First Track")
set_tests_properties(lamusica_daw_inspect_first_track_project
                     PROPERTIES DEPENDS lamusica_daw_create_first_track
                                PASS_REGULAR_EXPRESSION
                                "firstTrackReady=true.*firstTrackEditable=true.*mediaReady=true.*tempoBpm=120.*timeSignature=4/4.*sections=2.*firstSection=\"Intro\".*finalSection=\"Verse\".*tracks=3.*clips=2.*starterMidiNotes=8.*plugins=3.*automation=3.*frames=96000")
set_tests_properties(lamusica_daw_render_first_track
                     PROPERTIES DEPENDS lamusica_daw_inspect_first_track_project
                                PASS_REGULAR_EXPRESSION "rendered project: DAW First Track")
set_tests_properties(lamusica_daw_inspect_first_track_render
                     PROPERTIES DEPENDS lamusica_daw_render_first_track
                                PASS_REGULAR_EXPRESSION "sampleRate=48000 channels=2 frames=96000")
set_tests_properties(lamusica_cli_create_first_track_dependency_project
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean
                                PASS_REGULAR_EXPRESSION "created project: CLI Lifecycle")
set_tests_properties(lamusica_cli_validate_first_track_dependency_project
                     PROPERTIES DEPENDS lamusica_cli_create_first_track_dependency_project
                                PASS_REGULAR_EXPRESSION "valid project: CLI Lifecycle")
set_tests_properties(lamusica_cli_inspect_first_track_dependency_project
                     PROPERTIES DEPENDS lamusica_cli_validate_first_track_dependency_project
                                PASS_REGULAR_EXPRESSION "schemaVersion=1.*tracks=0 clips=0")
set_tests_properties(lamusica_cli_reject_unready_first_track_project
                     PROPERTIES DEPENDS lamusica_cli_inspect_first_track_dependency_project
                                WILL_FAIL TRUE)
set_tests_properties(lamusica_cli_create_first_track
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean
                                PASS_REGULAR_EXPRESSION
                                "created first-track project: CLI First Track")
set_tests_properties(lamusica_cli_validate_first_track
                     PROPERTIES DEPENDS lamusica_cli_create_first_track
                                PASS_REGULAR_EXPRESSION "valid project: CLI First Track")
set_tests_properties(lamusica_cli_inspect_first_track_project
                     PROPERTIES DEPENDS lamusica_cli_validate_first_track
                                PASS_REGULAR_EXPRESSION
                                "schemaVersion=1 tempoBpm=120.*timeSignature=4/4.*sections=2.*tracks=3 clips=2.*plugins=3 automation=3.*firstTrackReady=true.*firstTrackEditable=true.*mediaReady=true.*midiRefs=1.*starterMidiNotes=8")
set_tests_properties(lamusica_cli_verify_first_track_project
                     PROPERTIES DEPENDS lamusica_cli_inspect_first_track_project
                                PASS_REGULAR_EXPRESSION
                                "verified first-track project: project=\"CLI First Track\".*firstTrackReady=true.*firstTrackEditable=true.*mediaReady=true.*starterStructure=true.*renderable=true.*loopEnabled=true.*loopFrames=96000.*renderFrames=96000.*tracks=3.*clips=2.*plugins=3.*automation=3.*starterMidiNotes=8.*missingRequirements=0")
set_tests_properties(lamusica_cli_export_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_verify_first_track_project
                                PASS_REGULAR_EXPRESSION
                                "exported first-track package: project=CLI First Track.*mixFrames=96000.*loopFrames=96000.*stemCount=2.*packageManifest=true")
set_tests_properties(lamusica_cli_verify_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_export_first_track_package
                                PASS_REGULAR_EXPRESSION
                                "verified first-track package:.*project=\"CLI First Track\".*renderFrames=96000.*loopFrames=96000.*stemCount=2.*trackCount=3.*clipCount=2.*projectAssets=0.*recordedTakeCount=0.*importedAudioClipCount=0.*projectSnapshotVerified=true")
set_tests_properties(lamusica_cli_verify_examples
                     PROPERTIES PASS_REGULAR_EXPRESSION "verified example projects: 1")
set_tests_properties(lamusica_cli_create_punch_first_track
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean
                                PASS_REGULAR_EXPRESSION
                                "created first-track project: CLI Punch First Track")
set_tests_properties(lamusica_cli_plan_punch_first_track
                     PROPERTIES DEPENDS lamusica_cli_create_punch_first_track
                                PASS_REGULAR_EXPRESSION
                                "planned first-track recording: project=\"CLI Punch First Track\".*startSample=48000.*recordFrames=48000.*countInBars=2.*countInSamples=192000.*prerollStart=0.*punchEnabled=true.*punchIn=48000.*punchOut=96000")
set_tests_properties(lamusica_cli_record_punch_first_track
                     PROPERTIES DEPENDS lamusica_cli_plan_punch_first_track
                                PASS_REGULAR_EXPRESSION
                                "recorded first-track take: project=\"CLI Punch First Track\".*frames=48000.*startSample=48000.*countInSamples=192000.*prerollStart=0.*punchEnabled=true.*punchIn=48000.*punchOut=96000.*recordedTakeCount=1.*tracks=4.*clips=3")
set_tests_properties(lamusica_cli_list_punch_first_track_takes
                     PROPERTIES DEPENDS lamusica_cli_record_punch_first_track
                                PASS_REGULAR_EXPRESSION
                                "first-track takes: project=\"CLI Punch First Track\" count=1 muted=0.*take clipId=recorded-take-1.*startSample=48000.*frames=48000.*mediaAvailable=true")
set_tests_properties(lamusica_cli_create_mix_first_track
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean
                                PASS_REGULAR_EXPRESSION
                                "created first-track project: CLI Mix First Track")
set_tests_properties(lamusica_cli_set_mix_first_track
                     PROPERTIES DEPENDS lamusica_cli_create_mix_first_track
                                PASS_REGULAR_EXPRESSION
                                "set first-track track mix: project=\"CLI Mix First Track\".*track=\"drums\".*volumeDb=-12.*pan=0.25.*muted=false.*solo=false")
set_tests_properties(lamusica_cli_list_mix_first_track
                     PROPERTIES DEPENDS lamusica_cli_set_mix_first_track
                                PASS_REGULAR_EXPRESSION
                                "first-track track mix: project=\"CLI Mix First Track\" count=3.*track trackId=drums.*trackName=\"Generated Drums\".*volumeDb=-12.*pan=0.25.*muted=false.*solo=false")
set_tests_properties(lamusica_cli_render_created_first_track
                     PROPERTIES DEPENDS
                                "lamusica_cli_inspect_first_track_project;lamusica_cli_first_track_render_clean"
                                PASS_REGULAR_EXPRESSION
                                "rendered project: CLI First Track.*frames=96000")
set_tests_properties(lamusica_cli_render_first_track_tutorial
                     PROPERTIES DEPENDS lamusica_cli_first_track_render_clean
                                PASS_REGULAR_EXPRESSION
                                "rendered project: First Song Tutorial.*frames=96000")
set_tests_properties(lamusica_cli_inspect_created_first_track_render
                     PROPERTIES DEPENDS lamusica_cli_render_created_first_track
                                PASS_REGULAR_EXPRESSION "sampleRate=48000 channels=2 frames=96000")
set_tests_properties(lamusica_cli_inspect_first_track_render
                     PROPERTIES DEPENDS lamusica_cli_render_first_track_tutorial
                                PASS_REGULAR_EXPRESSION "sampleRate=48000 channels=2 frames=96000")
set_tests_properties(lamusica_cli_inspect_first_track_tutorial_project
                     PROPERTIES PASS_REGULAR_EXPRESSION
                                "plugins=3 automation=3.*firstTrackReady=true.*firstTrackEditable=true.*mediaReady=true.*midiRefs=1.*starterMidiNotes=8")
set_tests_properties(lamusica_cli_benchmark_smoke
                     PROPERTIES PASS_REGULAR_EXPRESSION "benchmark smoke passed=true")
set_tests_properties(lamusica_cli_create_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_first_track_lifecycle_clean
                                PASS_REGULAR_EXPRESSION
                                "created first-track project: CLI Edited First Track")
set_tests_properties(lamusica_cli_list_initial_first_track_clips
                     PROPERTIES DEPENDS lamusica_cli_create_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "first-track clips: project=\"CLI Edited First Track\" count=2.*clip clipId=drum-loop.*trackId=drums.*type=audio.*clip clipId=bass-pattern.*trackId=bass.*type=midi")
set_tests_properties(lamusica_cli_transpose_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_list_initial_first_track_clips
                                PASS_REGULAR_EXPRESSION
                                "transposed first-track bass: project=\"CLI Edited First Track\".*semitones=12.*firstTrackReady=true")
set_tests_properties(lamusica_cli_gain_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_transpose_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "set first-track clip gain: project=\"CLI Edited First Track\".*clip=\"drum-loop\".*drumGainDb=-18")
set_tests_properties(lamusica_cli_loop_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_gain_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "set first-track intro loop: project=\"CLI Edited First Track\".*loopEnabled=true.*loopStart=0.*loopEnd=96000.*loopFrames=96000")
set_tests_properties(lamusica_cli_extend_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_loop_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "extended first-track verse: project=\"CLI Edited First Track\".*renderFrames=192000.*loopFrames=96000.*firstTrackReady=true")
set_tests_properties(lamusica_cli_duplicate_bass_first_track
                     PROPERTIES DEPENDS lamusica_cli_extend_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "duplicated first-track clip: project=\"CLI Edited First Track\".*sourceClip=\"bass-pattern\".*clip=\"bass-pattern-copy\".*track=\"bass\".*startSample=96000.*clips=3")
set_tests_properties(lamusica_cli_remove_bass_duplicate_first_track
                     PROPERTIES DEPENDS lamusica_cli_duplicate_bass_first_track
                                PASS_REGULAR_EXPRESSION
                                "removed first-track clip: project=\"CLI Edited First Track\".*clip=\"bass-pattern-copy\".*track=\"bass\".*removedMidiReferences=1.*clips=2")
set_tests_properties(lamusica_cli_plan_record_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_remove_bass_duplicate_first_track
                                PASS_REGULAR_EXPRESSION
                                "planned first-track recording: project=\"CLI Edited First Track\".*startSample=48000.*recordFrames=24000.*countInBars=2.*countInSamples=192000.*prerollStart=0.*punchEnabled=false")
set_tests_properties(lamusica_cli_record_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_plan_record_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "recorded first-track take: project=\"CLI Edited First Track\".*frames=24000.*startSample=0.*countInSamples=96000.*recordedTakeCount=1.*tracks=4.*clips=3")
set_tests_properties(lamusica_cli_fade_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_record_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "set first-track clip fades: project=\"CLI Edited First Track\".*clip=\"recorded-take-1\".*fadeInSamples=512.*fadeOutSamples=1024")
set_tests_properties(lamusica_cli_reverse_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_fade_recorded_first_track_take
                                PASS_REGULAR_EXPRESSION
                                "set first-track clip reversed: project=\"CLI Edited First Track\".*clip=\"recorded-take-1\".*reversed=true")
set_tests_properties(lamusica_cli_list_recorded_first_track_takes
                     PROPERTIES DEPENDS lamusica_cli_reverse_recorded_first_track_take
                                PASS_REGULAR_EXPRESSION
                                "first-track takes: project=\"CLI Edited First Track\" count=1 muted=0.*take clipId=recorded-take-1.*frames=24000.*fadeInSamples=512.*fadeOutSamples=1024.*reversed=true.*muted=false.*mediaAvailable=true")
set_tests_properties(lamusica_cli_mute_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_list_recorded_first_track_takes
                                PASS_REGULAR_EXPRESSION
                                "muted first-track take: project=\"CLI Edited First Track\".*clipId=recorded-take-1.*muted=true.*mutedRecordedTakeCount=1")
set_tests_properties(lamusica_cli_list_muted_first_track_takes
                     PROPERTIES DEPENDS lamusica_cli_mute_recorded_first_track_take
                                PASS_REGULAR_EXPRESSION
                                "first-track takes: project=\"CLI Edited First Track\" count=1 muted=1.*take clipId=recorded-take-1.*muted=true.*mediaAvailable=true")
set_tests_properties(lamusica_cli_unmute_recorded_first_track_take
                     PROPERTIES DEPENDS lamusica_cli_list_muted_first_track_takes
                                PASS_REGULAR_EXPRESSION
                                "muted first-track take: project=\"CLI Edited First Track\".*clipId=recorded-take-1.*muted=false.*mutedRecordedTakeCount=0")
set_tests_properties(lamusica_cli_render_edit_first_track_import_source
                     PROPERTIES DEPENDS lamusica_cli_unmute_recorded_first_track_take
                                PASS_REGULAR_EXPRESSION "rendered test tone:")
set_tests_properties(lamusica_cli_import_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_render_edit_first_track_import_source
                                PASS_REGULAR_EXPRESSION
                                "imported first-track audio: project=\"CLI Edited First Track\".*frames=48000.*channels=2.*importedAudioClipCount=1.*tracks=5.*clips=4")
set_tests_properties(lamusica_cli_timing_edit_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_import_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "set first-track clip timing: project=\"CLI Edited First Track\".*clip=\"imported-audio-1\".*startSample=72000.*lengthSamples=24000.*sourceOffsetSamples=12000")
set_tests_properties(lamusica_cli_duplicate_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_timing_edit_imported_first_track
                                PASS_REGULAR_EXPRESSION
                                "duplicated first-track clip: project=\"CLI Edited First Track\".*sourceClip=\"imported-audio-1\".*clip=\"imported-audio-copy\".*track=\"imported-audio\".*startSample=108000.*lengthSamples=24000.*clips=5")
set_tests_properties(lamusica_cli_mute_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_duplicate_imported_first_track
                                PASS_REGULAR_EXPRESSION
                                "set first-track clip muted: project=\"CLI Edited First Track\".*clip=\"imported-audio-copy\".*muted=true")
set_tests_properties(lamusica_cli_unmute_imported_first_track
                     PROPERTIES DEPENDS lamusica_cli_mute_imported_first_track
                                PASS_REGULAR_EXPRESSION
                                "set first-track clip muted: project=\"CLI Edited First Track\".*clip=\"imported-audio-copy\".*muted=false")
set_tests_properties(lamusica_cli_list_edited_first_track_clips
                     PROPERTIES DEPENDS lamusica_cli_unmute_imported_first_track
                                PASS_REGULAR_EXPRESSION
                                "first-track clips: project=\"CLI Edited First Track\" count=5.*clip clipId=drum-loop.*trackId=drums.*type=audio.*clip clipId=recorded-take-1.*trackId=recorded-takes.*fadeInSamples=512.*fadeOutSamples=1024.*reversed=true.*muted=false.*assetBacked=true.*mediaAvailable=true.*clip clipId=imported-audio-1.*trackId=imported-audio.*startSample=72000.*lengthSamples=24000.*sourceOffsetSamples=12000.*clip clipId=imported-audio-copy.*trackId=imported-audio.*startSample=108000.*lengthSamples=24000.*sourceOffsetSamples=12000.*muted=false")
set_tests_properties(lamusica_cli_inspect_edit_first_track
                     PROPERTIES DEPENDS lamusica_cli_list_edited_first_track_clips
                                PASS_REGULAR_EXPRESSION
                                "project name=\"CLI Edited First Track\".*tracks=5 clips=5.*firstTrackReady=true.*firstTrackEditable=true.*mediaReady=true.*bassTranspose=12.*loopEnabled=true.*renderFrames=192000")
set_tests_properties(lamusica_cli_export_edit_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_inspect_edit_first_track
                                PASS_REGULAR_EXPRESSION
                                "exported first-track package: project=CLI Edited First Track.*mixFrames=192000.*loopFrames=96000.*stemCount=4.*packageManifest=true")
set_tests_properties(lamusica_cli_verify_edit_first_track_package
                     PROPERTIES DEPENDS lamusica_cli_export_edit_first_track_package
                                PASS_REGULAR_EXPRESSION
                                "verified first-track package:.*project=\"CLI Edited First Track\".*renderFrames=192000.*loopFrames=96000.*stemCount=4.*trackCount=5.*clipCount=5.*projectAssets=2.*recordedTakeCount=1.*importedAudioClipCount=2.*projectSnapshotVerified=true")
set_tests_properties(lamusica_cli_remove_edit_first_track_media
                     PROPERTIES DEPENDS lamusica_cli_verify_edit_first_track_package)
set_tests_properties(lamusica_cli_inspect_edit_first_track_missing_media
                     PROPERTIES DEPENDS lamusica_cli_remove_edit_first_track_media
                                PASS_REGULAR_EXPRESSION
                                "project name=\"CLI Edited First Track\".*firstTrackReady=false.*firstTrackEditable=false.*mediaReady=false.*media error:")
set_tests_properties(lamusica_daw_inspect_edit_first_track_missing_media
                     PROPERTIES DEPENDS lamusica_cli_remove_edit_first_track_media
                                PASS_REGULAR_EXPRESSION
                                "firstTrackReady=false.*firstTrackEditable=false.*mediaReady=false.*media error:")
set_tests_properties(lamusica_cli_relink_edit_first_track_media
                     PROPERTIES DEPENDS lamusica_daw_inspect_edit_first_track_missing_media
                                PASS_REGULAR_EXPRESSION
                                "relinked first-track audio: project=\"CLI Edited First Track\".*assetId=imported-audio-1.*frames=48000.*channels=2.*mediaReady=true.*firstTrackReady=true.*firstTrackEditable=true")
set_tests_properties(lamusica_cli_inspect_edit_first_track_relinked_media
                     PROPERTIES DEPENDS lamusica_cli_relink_edit_first_track_media
                                PASS_REGULAR_EXPRESSION
                                "project name=\"CLI Edited First Track\".*firstTrackReady=true.*firstTrackEditable=true.*mediaReady=true")

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

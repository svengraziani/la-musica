# 031 Plugin Hosting And Plugin UI

## Objective

Turn the current plugin-state scaffolding into real plugin hosting and user workflows.

## Dependencies

- 026 Pin JUCE And Production App Target.
- 027 Replace Bootstrap Cocoa Shell With DAW Shell.

## Deliverables

- Audio Unit and VST3 hosting where licensing and platform constraints allow.
- Plugin scanner, cache, validation, blacklist, rescan, and recovery UI.
- Insert chains, instrument slots, plugin windows, preset handling, and parameter discovery.
- Crash containment strategy for scanning and bad plugin behavior.
- Tests with mocks and known redistributable test plugins.

## Acceptance Gates

- Bad plugins cannot prevent normal app launch.
- Plugin state saves, reloads, and participates in undoable insert/remove/reorder commands.
- Parameter IDs remain stable enough for automation across reloads.

## Notes For Agents

Record SDK licensing constraints before enabling a plugin format in release builds.

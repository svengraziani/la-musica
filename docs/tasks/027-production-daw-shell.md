# 027 Replace Bootstrap Cocoa Shell With DAW Shell

## Objective

Replace the current text-panel bootstrap window with the real DAW application shell.

## Dependencies

- 026 Pin JUCE And Production App Target.

## Deliverables

- Production main window with transport, arrangement, browser, inspector, mixer, piano roll, plugin,
  and preferences surfaces.
- Native macOS menu structure using conventional app, file, edit, view, transport, audio, MIDI,
  tools, window, and help organization.
- Document lifecycle for create, open, save, save as, close, recent projects, autosave/recovery, and
  dirty state.
- Keyboard focus and shortcut routing across panels.
- Removal or demotion of first-track bootstrap commands from the main product menu.

## Decisions To Make

- Whether the bootstrap Cocoa shell remains buildable for smoke tests.
- Minimum supported window sizes and panel docking behavior.
- Initial visual system: colors, spacing, typography, icons, and component states.

## Acceptance Gates

- App opens to a usable DAW shell, not text-only status panels.
- File/project menu behavior matches macOS expectations.
- Project create/open/save/close flows are command-backed and tested.
- Screenshots at common desktop and laptop sizes show no overlapping controls or clipped text.

## Notes For Agents

Keep the app surface dense and work-focused. Avoid landing-page or demo-style UI.

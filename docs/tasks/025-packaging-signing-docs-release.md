# 025 Packaging, Signing, Docs, And Release

## Objective

Prepare the complete DAW for public open-source distribution on macOS.

## Dependencies

- 024 Performance, Stress, And Realtime Verification.

## Deliverables

- macOS packaging, signing, notarization, installer or disk image, and update strategy.
- User manual covering recording, editing, MIDI, drums, sequencing, mixing, plugins, rendering, and MCP.
- Developer docs for architecture, build, tests, project format, command API, and MCP tools.
- Release checklist, versioning policy, changelog, and security disclosure process.
- Example projects and tutorial fixtures using redistributable assets.

## Acceptance Gates

- Fresh user can install and launch the app without developer tools.
- Fresh contributor can build and run tests from documentation.
- Release artifacts pass signing/notarization checks where applicable.
- Example projects load without missing assets.

## Notes For Agents

Do not ship assets, plugins, or examples without clear redistribution rights.

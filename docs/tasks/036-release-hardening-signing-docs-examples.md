# 036 Release Hardening, Signing, Docs, And Examples

## Objective

Prepare the production DAW for public macOS distribution.

## Dependencies

- 035 Performance, Stress, And Realtime Verification.

## Deliverables

- Signing, notarization, disk image or installer, and update strategy.
- User manual covering recording, editing, MIDI, drums, sequencing, mixing, plugins, rendering, and
  MCP workflows.
- Developer docs for architecture, build, tests, project format, command API, MCP tools, plugin
  integration, and release process.
- Release checklist, versioning policy, changelog, and security disclosure process.
- Example projects and tutorial fixtures using redistributable assets.

## Acceptance Gates

- Fresh user can install and launch the app without developer tools.
- Fresh contributor can build and run tests from documentation.
- Release artifacts pass signing and notarization checks where applicable.
- Example projects load without missing assets.

## Notes For Agents

Do not ship assets, plugins, or examples without clear redistribution rights.

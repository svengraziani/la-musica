# 026 Pin JUCE And Production App Target

## Objective

Make JUCE 8 a reproducible, explicitly pinned dependency for the production DAW target while
preserving the existing clean checkout behavior.

## Dependencies

- Current baseline.

## Deliverables

- Documented JUCE 8 source pin: tag or commit, license notes, and update process.
- CMake option path that builds JUCE-backed app targets when `LAMUSICA_ENABLE_JUCE=ON`.
- Clear separation between bootstrap Cocoa shell and production JUCE app target.
- CI or local verification path for JUCE-enabled builds.
- Updated dependency docs explaining how contributors supply or fetch the pinned checkout.

## Decisions To Make

- Whether JUCE is vendored, submoduled, supplied through a script, or kept as an external checkout.
- Whether the Cocoa bootstrap target remains as a smoke-test harness or is removed after the JUCE
  app is stable.
- How AGPL/commercial JUCE licensing is represented in release docs.

## Acceptance Gates

- Clean default configure still works without JUCE.
- `LAMUSICA_ENABLE_JUCE=ON` fails with an actionable message when the pinned checkout is missing.
- JUCE-enabled configure and build succeed when the documented checkout is supplied.
- Dependency lock checks cover the selected JUCE pin.

## Notes For Agents

Do not add package-manager downloads or unpinned `FetchContent`. Update
`docs/developer/dependencies.md` and `docs/architecture/002-build-and-dependencies.md` with the
chosen workflow.

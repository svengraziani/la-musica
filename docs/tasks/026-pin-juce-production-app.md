# 026 Pin JUCE And Production App Target

## Objective

Make JUCE 8.0.13 a required, reproducible, explicitly pinned dependency for the production DAW
target.

## Dependencies

- Current baseline.

## Deliverables

- Documented JUCE 8.0.13 source pin, license notes, and update process.
- CMake path that requires a JUCE checkout through `LAMUSICA_JUCE_PATH`.
- Production `lamusica_daw` target backed by JUCE.
- Separate non-product smoke harness for CLI-style DAW app-session tests.
- CI or local verification path for JUCE-enabled builds.
- Updated dependency docs explaining how contributors supply or fetch the pinned checkout.

## Decisions To Make

- Whether JUCE remains an external checkout or later moves to a submodule.
- Whether the old Cocoa bootstrap source is deleted once the JUCE app shell is stable.
- How AGPL/commercial JUCE licensing is represented in release docs.

## Acceptance Gates

- Configure fails with an actionable message when the pinned checkout is missing.
- JUCE-enabled configure and build succeed when the documented checkout is supplied.
- Dependency lock checks cover the selected JUCE pin.

## Notes For Agents

Do not add package-manager downloads or unpinned `FetchContent`. Update
`docs/developer/dependencies.md` and `docs/architecture/002-build-and-dependencies.md` with the
chosen workflow.

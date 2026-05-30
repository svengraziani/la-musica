# Production Tasks (P01–P34)

Concrete, code-grounded tasks for taking LaMusica from its current `scaffold` state to a full
production macOS DAW. **Start here:** [../production-roadmap.md](../production-roadmap.md) for the
phase order, dependency graph, and critical path. For the audit that produced these, see
[../production-readiness-assessment.md](../production-readiness-assessment.md).

P35 records the 2026-05-29 corrective audit for an English-only product scope and current
run-readiness blockers.

Each task file follows the repo task template (Objective · Dependencies · Deliverables · Decisions
To Make · Compromises And Tradeoffs · Acceptance Gates · Notes For Agents) and cites real
`file:line` evidence in the current tree. Work tasks in dependency order; do not start one until its
dependencies are complete and verified (per [agent-execution-rules](../../process/agent-execution-rules.md)).

## Current P26-P34 Status

As of 2026-05-29, the active branch has substantial local implementation and verification coverage
for P26-P34: determinism, plugin-hosting, audio-correctness, behavior, perf, accessibility,
localization, privacy, onboarding, package verification, release evidence validation, and release
helper self-tests all have executable gates. The remaining acceptance evidence for this range is
mostly external to a Linux workspace: a real macOS Developer ID release run, notarization and
stapling, Gatekeeper online/offline launch from a clean account, microphone TCC prompt evidence,
macOS dSYM symbolication, and the manual VoiceOver evidence pass. Treat the individual P26-P34 task
files and the release evidence templates as the authoritative checklist before declaring the range
complete.

Latest local verification evidence:

- `ctest --test-dir build --output-on-failure` passes all 144 configured tests, including the
  `determinism`, `plugin-hosting`, `audio-correctness`, `behavior`, `perf`, `a11y`, `i18n`,
  `onboarding`, `privacy`, `gui`, and `cli` labels.
- `cmake --build build --target package` produces `build/LaMusica-0.1.0-Linux.tar.gz`.
- `cmake -DPACKAGE=build/LaMusica-0.1.0-Linux.tar.gz -P cmake/VerifyPackage.cmake` verifies the
  packaged documentation, schemas, examples, release evidence templates, privacy docs, and release
  metadata requirements.
- `cmake -P cmake/CheckMarkdown.cmake`, `cmake -P cmake/CheckDependencyLock.cmake`,
  `cmake -DLAMUSICA_DEPENDENCY_LOCK_SELF_TEST=ON -P cmake/CheckDependencyLock.cmake`, and
  `cmake -DLAMUSICA_VERIFY_PACKAGE_SELF_TEST=ON -P cmake/VerifyPackage.cmake` pass.
- Release helper self-tests pass for `archive-dsyms.sh`, `sign-macos.sh`, `notarize-macos.sh`,
  `verify-signature.sh`, `verify-provenance.sh`, `verify-symbolication.sh`, `sbom.sh`,
  `sign-checksums.sh`, and `verify-release-evidence.sh`.
- `git diff --check` reports no whitespace errors.

| Phase | Tasks |
| --- | --- |
| 0 — Foundation hygiene | P01, P02, P03 |
| 1 — Realtime audio spine | P04, P05, P06 |
| 2 — Generic authoring + faithful render | P07, P08, P09, P10, P28, P29 |
| 3 — Plugin hosting | P11, P12, P13, P14 |
| 4 — Mixer, MIDI, recording, automation | P15, P16, P17, P18, P33 |
| 5 — Interactive DAW UI | P19, P20, P21, P22, P23, P30, P31, P34 |
| 6 — MCP over the wire | P24, P25 |
| 7 — Verification & release | P26, P27, P32 |

**Begin with P01 and P04** (no dependencies) — together with P03/P05 they gate the entire product.

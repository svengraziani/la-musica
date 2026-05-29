# Production Tasks (P01–P34)

Concrete, code-grounded tasks for taking LaMusica from its current `scaffold` state to a full
production macOS DAW. **Start here:** [../production-roadmap.md](../production-roadmap.md) for the
phase order, dependency graph, and critical path. For the audit that produced these, see
[../production-readiness-assessment.md](../production-readiness-assessment.md).

Each task file follows the repo task template (Objective · Dependencies · Deliverables · Decisions
To Make · Compromises And Tradeoffs · Acceptance Gates · Notes For Agents) and cites real
`file:line` evidence in the current tree. Work tasks in dependency order; do not start one until its
dependencies are complete and verified (per [agent-execution-rules](../../process/agent-execution-rules.md)).

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

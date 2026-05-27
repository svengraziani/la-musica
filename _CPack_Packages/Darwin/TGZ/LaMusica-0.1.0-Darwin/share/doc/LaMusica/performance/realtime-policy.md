# Realtime Policy

Audio callback code must avoid:

- Allocation.
- Locks.
- File I/O.
- Logging.
- JSON parsing.
- MCP work.

Realtime checks should be enforced through tests, code review, and instrumentation as the live engine matures.

## Stress Fixtures

Fixed stress fixtures must include tracks, clips, plugin references, automation lanes, MIDI note
data, assets, markers, and MCP audit/query activity. Benchmarks should record machine context
alongside save/load time, query latency, and render realtime factor so regressions can be
interpreted against documented thresholds.

The fixed stress benchmark measures:

- project manifest serialize/parse/validate as the save/load proxy;
- representative query traversal across tracks, clips, plugins, automation, assets, MIDI notes,
  and MCP audit records;
- short offline graph rendering as a realtime-factor proxy.

CI thresholds should be intentionally conservative for regular checks and tighter in nightly or
dedicated performance jobs.

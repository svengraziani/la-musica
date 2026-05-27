# Realtime Policy

Audio callback code must avoid:

- Allocation.
- Locks.
- File I/O.
- Logging.
- JSON parsing.
- MCP work.

Realtime checks should be enforced through tests, code review, and instrumentation as the live engine matures.

Transport advancement must stay sample based and deterministic. Looping render callbacks wrap at
the configured sample loop end without consulting wall-clock time, file state, JSON, or project
metadata; fixture tests cover silence, oscillator, and metronome paths crossing loop boundaries.

## Stress Fixtures

Fixed stress fixtures must include tracks, clips, plugin references, automation lanes with point
data, MIDI note data, assets, markers, and MCP audit/query activity. Benchmarks should record machine context
alongside startup, plugin scan, CPU/query work, memory footprint, disk footprint, save/load time,
query latency, and render realtime factor so regressions can be interpreted against documented
thresholds.

The fixed stress benchmark measures:

- stress fixture construction and validation as the startup proxy;
- plugin reference traversal as the plugin scan proxy;
- repeated representative query traversal as the CPU work proxy;
- deterministic in-memory object size and serialized project size as memory and disk proxies;
- project manifest serialize/parse/validate as the save/load proxy;
- representative query traversal across tracks, clips, plugins, automation, assets, MIDI notes,
  and MCP audit records;
- short offline graph rendering as a realtime-factor proxy.

CI thresholds should be intentionally conservative for regular checks and tighter in nightly or
dedicated performance jobs.

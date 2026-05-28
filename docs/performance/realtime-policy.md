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
Timeline clock helpers convert samples, PPQ, and bar/beat positions from the engine sample rate,
tempo, and time signature only; fixture tests cover sample-rate-dependent PPQ math and 6/8
bar/beat conversion.
The engine graph callback uses bounded node/connection arrays and constructor-allocated scratch
buffers sized from the configured maximum block size. General offline graph rendering may allocate,
but `AudioEngine::renderGraphBlock` must keep graph scheduling and audio buffers inside those
preallocated limits.
`auditRealtimeGraphCallback` runs a fixed compiled graph through the realtime callback with
preallocated output, records the allowed callback operations, checks policy violations, verifies
transport advancement, and compares callback time against the block deadline.

## Stress Fixtures

Fixed stress fixtures must include tracks, clips, plugin references, automation lanes with point
data, MIDI note data, assets, markers, and MCP audit/query activity. Benchmarks should record machine context
alongside startup, plugin scan, CPU/query work, memory footprint, disk footprint, save/load time,
edit latency, MCP query latency, realtime callback latency, query latency, and render realtime
factor so regressions can be interpreted against documented thresholds.

The fixed stress benchmark measures:

- stress fixture construction and validation as the startup proxy;
- plugin reference traversal as the plugin scan proxy;
- repeated representative query traversal as the CPU work proxy;
- representative timeline edit validation;
- deterministic in-memory object size and serialized project size as memory and disk proxies;
- project manifest serialize/parse/validate as the save/load proxy;
- representative query traversal across tracks, clips, plugins, automation, assets, MIDI notes,
  and MCP audit records;
- representative MCP audit/query traversal;
- instrumented realtime graph callback latency and policy status;
- short offline graph rendering as a realtime-factor proxy.

CI thresholds should be intentionally conservative for regular checks and tighter in nightly or
dedicated performance jobs.

## Automated Regression Checks

`lamusica_cli benchmark-smoke` runs a fixed small stress project with conservative thresholds and
prints machine context plus startup, plugin scan, CPU work, save/load, query, render, memory, and
disk measurements, including edit latency, MCP query latency, and instrumented realtime callback
status. CI runs this smoke benchmark on every push and pull request as a hard regression gate. Larger
stress sizes and tighter thresholds belong in scheduled or dedicated performance jobs.

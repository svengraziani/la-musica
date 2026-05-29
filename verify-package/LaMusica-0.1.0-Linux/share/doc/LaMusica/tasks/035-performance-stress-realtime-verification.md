# 035 Performance, Stress, And Realtime Verification

## Objective

Prove the production DAW remains responsive, stable, and realtime-safe under professional workloads.

## Dependencies

- Tasks 028 through 034.

## Deliverables

- Realtime safety test suite and instrumentation.
- Stress projects with many tracks, clips, plugins, automation lanes, MIDI notes, assets, and MCP
  activity.
- CPU, memory, disk, startup, plugin scan, save/load, and render benchmarks.
- Regression thresholds in CI or scheduled automation.
- Profiling documentation for contributors.

## Acceptance Gates

- Audio callback avoids allocation, locks, file I/O, logging, JSON parsing, and daemon work.
- Stress projects play, edit, save, render, and respond to MCP queries within documented thresholds.
- Performance regressions are caught by automated checks.

## Notes For Agents

Benchmarks must use fixed fixtures and record machine context so regressions are interpretable.

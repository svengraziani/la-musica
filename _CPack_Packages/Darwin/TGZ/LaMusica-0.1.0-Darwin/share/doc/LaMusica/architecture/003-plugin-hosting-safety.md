# ADR 003: Plugin Hosting Safety Baseline

## Status

Accepted for implementation foundation.

## Decision

- Model plugin scan cache, blacklist, insert chains, instrument slots, plugin editor windows, and
  stable parameter addresses before loading real Audio Unit or VST3 binaries.
- Keep plugin scanning isolated from normal app startup.
- Treat blacklisted or invalid plugins as unavailable rather than fatal.
- Treat scan crashes and timeouts as handled outcomes that are recorded, blacklisted by policy, and
  skipped on later launches.
- Use stable parameter addresses in the form `pluginIdentifier::parameterId`.
- Serialize insert-chain state, instrument slots, editor-window state, and presets through
  deterministic project-local records so parameter values can save, reload, and participate in
  undoable editing before real plugin binaries load.

## Consequences

The DAW can save, validate, and reason about plugin state before the JUCE-backed host is complete. Real AU/VST3 loading still requires SDK integration, licensing review, and crash-containment work.

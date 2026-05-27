# ADR 003: Plugin Hosting Safety Baseline

## Status

Accepted for implementation foundation.

## Decision

- Model plugin scan cache, blacklist, insert chains, and stable parameter addresses before loading real Audio Unit or VST3 binaries.
- Keep plugin scanning isolated from normal app startup.
- Treat blacklisted or invalid plugins as unavailable rather than fatal.
- Use stable parameter addresses in the form `pluginIdentifier::parameterId`.

## Consequences

The DAW can save, validate, and reason about plugin state before the JUCE-backed host is complete. Real AU/VST3 loading still requires SDK integration, licensing review, and crash-containment work.

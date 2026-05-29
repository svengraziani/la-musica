# Privacy And Diagnostics

LaMusica does not send crash reports or telemetry unless the user grants diagnostics consent and
enables diagnostics sharing. The default state is private: consent is undecided and sharing is off.

## Crash Reports

When enabled, a crash report may include:

- application name;
- app version and Git commit;
- operating system version;
- crash signal;
- scrubbed stack frames or backtrace text when available from a local report or symbolication.

The in-process signal handler writes only a minimal local marker with signal, pid, and a deferred backtrace marker.
It uses async-signal-safe operations. Scrubbing and any upload preparation happen later, outside the
signal handler.

Crash reports must not include project contents, audio, MIDI, usernames, home-directory paths,
absolute file paths, or project bundle names. The diagnostics scrubber replaces those values with
`<path>` or `<project>` before an upload client can transmit a payload.

## Telemetry

Usage telemetry is a separate opt-in from crash reports, and LaMusica currently emits no usage telemetry events.
Future telemetry must use the same consent state and must be documented here before release.

## Endpoint

The default diagnostics endpoint for signed releases is:

```text
https://diagnostics.lamusica.dev/v1/crash
```

Self-hosted AGPL deployments may set `ApplicationPreferences::diagnosticsEndpoint` to an HTTPS URL
or use the `LAMUSICA_DIAGNOSTICS_ENDPOINT` environment override documented in the release guide.
The environment override must resolve to a non-empty explicit HTTPS URL; missing or non-HTTPS override values block upload.
Invalid non-HTTPS endpoints are rejected by preference validation.

## Retention And Opt Out

Crash reports are intended for short-term release triage and should be deleted from the hosted
service within 30 days. Users can opt out at any time by setting diagnostics consent to Declined or
turning diagnostics sharing off in Preferences > Privacy.

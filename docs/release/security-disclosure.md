# Security Disclosure Process

Report vulnerabilities privately to `security@lamusica.dev`. Maintainers should acknowledge credible
reports within 3 business days and coordinate disclosure once a fix and release plan are available.
Add a PGP key here before the first public stable release if encrypted intake is required.

## Intake

Security reports should include:

- affected version or commit;
- affected component, such as project loading, media import, plugin scanning, MCP authorization, or
  rendering;
- reproduction steps or a minimal project fixture when safe to share;
- whether the report involves code execution, arbitrary file access, denial of service, or data
  exposure.

## Handling

- Acknowledge receipt before public discussion.
- Keep exploit details private until a fix and release plan are available.
- Add regression tests or fixtures for confirmed vulnerabilities.
- Note fixed versions in `CHANGELOG.md`.
- Credit reporters when they consent.

## High-Risk Areas

- MCP capability bypass or unauthorized project mutation.
- Shell, process, or arbitrary filesystem access through daemon protocols.
- Unsafe project, preset, plugin, or media parsing.
- Plugin scanning crashes, hangs, or quarantine bypass.
- Destructive edit or render actions without confirmation tokens.

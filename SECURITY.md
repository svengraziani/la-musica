# Security

## Supported Versions

LaMusica has no released supported version yet. Security-sensitive design work should still be tracked from the start, especially around project files, plugins, media import, and MCP access.

## Reporting A Vulnerability

Report vulnerabilities privately to `security@lamusica.dev`.

Maintainers should acknowledge credible reports within 3 business days, keep exploit details private
until a fix and release plan are available, and publish advisory notes for fixed releases. Add a PGP
key here before the first public stable release if encrypted intake is required.

Do not publish exploit details for vulnerabilities involving:

- MCP authorization or capability bypass.
- Arbitrary filesystem access.
- Plugin scanning or loading.
- Project file parsing.
- Media import parsing.
- Code execution through scripts, commands, or agent workflows.

## Security Principles

- The MCP daemon must not expose shell execution.
- Project mutation must require explicit project-scoped capability.
- Destructive operations must require confirmation tokens.
- Untrusted project, media, preset, plugin, and MCP inputs must be validated before use.

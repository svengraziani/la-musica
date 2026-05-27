# Task Index

Execute these tasks in order. Each numbered file is intended to be small enough for a dedicated agent pass while still preserving the full DAW target.

| Order | Task | Depends On |
| --- | --- | --- |
| 001 | [Repository Bootstrap](001-repository-bootstrap.md) | None |
| 002 | [Build, Tooling, And CI](002-build-tooling-ci.md) | 001 |
| 003 | [Application Shell](003-application-shell.md) | 002 |
| 004 | [Audio Engine Core](004-audio-engine-core.md) | 003 |
| 005 | [Session Model And Project Format](005-session-model-project-format.md) | 004 |
| 006 | [Undoable Command Layer](006-undoable-command-layer.md) | 005 |
| 007 | [Arrangement Timeline](007-arrangement-timeline.md) | 006 |
| 008 | [Audio Clip Editing](008-audio-clip-editing.md) | 007 |
| 009 | [Audio Import, Recording, And Export](009-audio-import-recording-export.md) | 008 |
| 010 | [MIDI Core](010-midi-core.md) | 006 |
| 011 | [Piano Roll And MIDI Editing](011-piano-roll-midi-editing.md) | 010 |
| 012 | [Drum Machine](012-drum-machine.md) | 010 |
| 013 | [Step Sequencer And Patterns](013-step-sequencer-patterns.md) | 012 |
| 014 | [Plugin Hosting](014-plugin-hosting.md) | 004, 006 |
| 015 | [Mixer And Routing](015-mixer-routing.md) | 004, 014 |
| 016 | [Automation System](016-automation-system.md) | 006, 015 |
| 017 | [Warping, Stretching, And Pitch](017-warp-stretch-pitch.md) | 008 |
| 018 | [Browser, Assets, And Media Analysis](018-browser-assets-analysis.md) | 005, 009 |
| 019 | [MCP Daemon Foundation](019-mcp-daemon-foundation.md) | 006 |
| 020 | [MCP DAW Query Tools](020-mcp-daw-query-tools.md) | 019 |
| 021 | [MCP Editing Tools](021-mcp-editing-tools.md) | 020, 006 |
| 022 | [MCP Audio And Render Tools](022-mcp-audio-render-tools.md) | 021, 009 |
| 023 | [AI Orchestration Workflows](023-ai-orchestration-workflows.md) | 021, 011, 013 |
| 024 | [Performance, Stress, And Realtime Verification](024-performance-stress-realtime.md) | 004-023 |
| 025 | [Packaging, Signing, Docs, And Release](025-packaging-signing-docs-release.md) | 024 |

Use [000-task-template.md](000-task-template.md) for any new task added later.

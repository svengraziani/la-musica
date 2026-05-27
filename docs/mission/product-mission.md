# Product Mission

## Mission

Build a full-featured open-source macOS Digital Audio Workstation that combines professional audio/MIDI production with a system-level MCP interface for AI-assisted orchestration, editing, arrangement, mixing, and repetitive production tasks.

## Non-Negotiable Product Scope

- Native macOS desktop application.
- Real-time audio engine with low-latency playback, recording, monitoring, routing, automation, and plugin hosting.
- Full arrangement timeline with audio clips, MIDI clips, automation lanes, tempo maps, markers, editing tools, snapping, grouping, and comping.
- Piano roll, MIDI event editor, drum machine, step sequencer, and clip launcher style pattern workflows.
- Full mixer with channel strips, sends, buses, inserts, meters, gain staging, pan, mute, solo, record arm, freeze, bounce, and mix export.
- High-level audio editing: cut, trim, split, fade, normalize, reverse, time-stretch, pitch-shift, warp markers, transient operations, and nondestructive processing.
- System-level MCP daemon exposing safe, deterministic tools for DAW state inspection, editing commands, rendering, asset management, orchestration helpers, and project automation.
- Deterministic project format suitable for version control and external automation.
- Professional packaging, signing, crash reporting hooks, profiling, documentation, and contributor workflows.

## Definition Of Complete Product

The product is complete only when a user can create a multitrack project from scratch, record audio and MIDI, edit arrangements, use instruments/effects, sequence drums and melodies, mix through buses and automation, export final audio, and safely delegate supported operations to AI agents through MCP without corrupting the session.

## Preferred Technology Direction

- Primary application and DSP: C++20 or newer with JUCE 8.
- Performance-sensitive services may use Rust only where FFI boundaries are narrow and justified.
- Project data should use stable structured formats, not opaque binary-only session files.
- All agent-accessible operations must flow through validated command APIs, never direct mutation of live engine internals.

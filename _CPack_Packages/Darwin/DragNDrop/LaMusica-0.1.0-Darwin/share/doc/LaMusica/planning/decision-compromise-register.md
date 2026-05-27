# Decision And Compromise Register

This register works through every planned task and records the decisions that must be made, recommended defaults, and compromises to accept or reject. Agents should read the matching section before executing a task.

## Global Product Decisions

### Decisions To Make

- License: permissive license for wide adoption, copyleft license for ecosystem reciprocity, or dual license.
  - permissive license
- Primary implementation language: C++23 with JUCE 8 for LaMusica-owned code, with JUCE/plugin SDK boundaries kept compatible with verified toolchain requirements.
- Plugin formats: Audio Unit first on macOS, VST3 if SDK/licensing and distribution constraints are acceptable.
  - VST3 pls
- Project format: directory-based readable session format instead of single opaque binary files.
  - directory-based readable 
- MCP security model: local-only capability-scoped daemon with project-scoped authorization and no shell access.
- Distribution model: developer builds first, then signed/notarized public releases.

### Recommended Defaults

- Use C++23 and JUCE 8 for LaMusica-owned DAW, audio engine, UI, and plugin hosting code, with compatibility checks around external framework boundaries.
- Use a separate `lamusica-mcpd` process for MCP so AI-agent activity cannot run inside the realtime audio process.
- Use stable structured project files with schema versioning and golden fixtures from the first implementation.
- Use command-layer mutation for UI, CLI, and MCP edits.

### Compromises And Tradeoffs

- JUCE accelerates audio app development but creates dependency, licensing, and framework constraints that must be tracked.
- C++23 gives a modern language baseline and the clearest JUCE path, but requires strict engineering discipline for safety and CI proof across SDK boundaries.
- Rust can improve service reliability, but mixed-language builds and FFI can slow early development.
- Human-readable project files improve version control and MCP automation, but require careful schema design and migration tests.

## 001 Repository Bootstrap

### Concrete Planning

- Create the legal, contribution, and directory foundation before writing product code.
- Establish source boundaries early: app, daemon, CLI, audio, session, commands, MCP bridge, tests, and fixtures.

### Decisions To Make

- Choose the repository license and document why it is compatible with JUCE, plugin SDKs, and bundled assets.
- Decide whether the repo starts as a monorepo containing all app, daemon, docs, tests, and fixtures.
- Decide naming conventions for targets, namespaces, source directories, and test fixtures.

### Recommended Default

- Use a monorepo and defer submodule/package splitting until build times or ownership boundaries justify it.

### Compromises

- A monorepo is simpler for agent execution and refactors, but can become heavy as sample fixtures and docs grow.
- Do not include real sample packs or plugin SDK binaries until redistribution rights are confirmed.

## 002 Build, Tooling, And CI

### Concrete Planning

- Make command-line builds authoritative so agents and CI can work without opening Xcode.
- Add formatting, static checks, test presets, and macOS CI before feature work expands.

### Decisions To Make

- Use CMake with JUCE integration or another generator such as Meson/XcodeGen.
- Select package management for JUCE and third-party libraries.
- Decide CI provider, macOS runner versions, cache policy, and artifact retention.
- Decide whether sanitizers and static analysis run on every PR or nightly.

### Recommended Default

- Use CMake presets with JUCE as a pinned dependency and generated Xcode projects as secondary artifacts.

### Compromises

- CMake adds ceremony, but gives reproducible agent and CI workflows.
- Full sanitizer coverage can be slow on macOS CI; run fast checks per PR and deeper checks nightly.

## 003 Application Shell

### Concrete Planning

- Build the real product shell with panels, command routing, document lifecycle, preferences, and saved UI state.
- Avoid disposable demo UI because later tasks depend on stable surfaces.

### Decisions To Make

- Decide single-window versus detachable panels.
- Decide document model: one project per process or multiple open projects.
- Decide UI state persistence boundaries between global preferences and project state.
- Decide macOS minimum version.

### Recommended Default

- Start with one project per main window and support one active project per process until multi-project complexity is justified.

### Compromises

- A single-window model simplifies routing, MCP attachment, and testing, but advanced users may later want detachable mixer/plugin windows.
- Native macOS behavior improves user trust, but increases platform-specific QA.

## 004 Audio Engine Core

### Concrete Planning

- Implement realtime-safe audio scheduling, transport, buffers, clocks, and device configuration before editor features.
- Add an engine fixture harness immediately.

### Decisions To Make

- Decide graph execution model: static snapshots, dynamic graph with lock-free updates, or hybrid.
- Decide internal sample format and channel layout representation.
- Decide supported sample rates, buffer sizes, and latency reporting.
- Decide how realtime safety is verified in tests.

### Recommended Default

- Use immutable audio-graph snapshots swapped through realtime-safe handoff, with commands preparing changes off the audio thread.

### Compromises

- Snapshot graphs can use more memory and make tiny edits less direct, but are easier to reason about safely.
- Supporting every device edge case early delays features; define a tested support matrix and expand it deliberately.

## 005 Session Model And Project Format

### Concrete Planning

- Define schema, fixture projects, validation, migrations, and load/save semantics before deep editing features.
- Keep generated caches separate from authoritative session data.

### Decisions To Make

- Choose serialization format: JSON, YAML, TOML, SQLite, custom binary, or hybrid.
- Decide asset storage, relinking, cache, and relative path rules.
- Decide schema versioning and migration policy.
- Decide whether command history is persisted inside the project.

### Recommended Default

- Use a directory format with JSON manifests and stable asset/cache subdirectories, backed by schema validation and golden fixtures.

### Compromises

- JSON is verbose but easy for tools, diffs, and MCP agents.
- SQLite can help with large metadata but makes merge/review harder; reserve it for cache or index data if needed.

## 006 Undoable Command Layer

### Concrete Planning

- Make command validation, preview, apply, undo, redo, serialization, and audit IDs the only mutation path.
- Treat this as the contract shared by UI, CLI, and MCP.

### Decisions To Make

- Decide command serialization format and versioning.
- Decide transaction semantics for grouped edits.
- Decide undo history lifetime and persistence.
- Decide conflict behavior when commands target stale state.

### Recommended Default

- Use typed command objects with serializable payloads, deterministic validation, explicit affected-object IDs, and transaction wrappers.

### Compromises

- A strict command layer slows initial feature coding, but prevents MCP and UI divergence.
- Persisting full undo history helps auditability but can bloat projects; support bounded persistence first.

## 007 Arrangement Timeline

### Concrete Planning

- Build multitrack editing around command-backed selections, tools, snapping, rulers, markers, and track lanes.
- Include large-session performance tests early.

### Decisions To Make

- Decide rendering strategy for large timelines: JUCE components, custom painting, OpenGL/Metal-backed views, or hybrid.
- Decide selection model for clips, ranges, tracks, automation, and MIDI.
- Decide snapping priority and conflict rules.
- Decide keyboard shortcut defaults and customization format.

### Recommended Default

- Use custom-painted timeline views with model-backed hit testing and command-backed edit gestures.

### Compromises

- Custom painting takes more engineering than component-per-clip, but scales better for real sessions.
- Rich snapping improves precision but can feel unpredictable; make snap targets visible and configurable.

## 008 Audio Clip Editing

### Concrete Planning

- Implement nondestructive clip metadata operations before destructive processing.
- Build waveform cache, fades, crossfades, comp foundations, and fixture render tests.

### Decisions To Make

- Decide fade curve models and default crossfade behavior.
- Decide waveform cache resolution and invalidation.
- Decide take/comp data model.
- Decide where clip gain, envelopes, and normalization live in the processing graph.

### Recommended Default

- Store edits as nondestructive clip metadata and render through the engine path.

### Compromises

- Nondestructive editing is more complex than rewriting files, but protects source media and supports undo/MCP previews.
- High-resolution waveform caches improve UX but consume disk and invalidation complexity.

## 009 Audio Import, Recording, And Export

### Concrete Planning

- Support import, recording, monitoring, latency compensation, recovery, and offline rendering through one coherent asset pipeline.

### Decisions To Make

- Decide supported import/export formats for the first complete release.
- Decide recording file format defaults.
- Decide input latency measurement and compensation strategy.
- Decide crash/interruption recovery behavior for active recordings.

### Recommended Default

- Record to WAV/BWF-compatible assets, store metadata in the project, and support export presets for common delivery formats.

### Compromises

- Broad codec support improves convenience but increases licensing and QA surface.
- Exact latency compensation requires measurement UX and test fixtures, but guessed offsets are not acceptable for a DAW.

## 010 MIDI Core

### Concrete Planning

- Implement MIDI devices, data model, timing conversions, recording modes, playback, and deterministic transforms.

### Decisions To Make

- Decide internal MIDI event representation and timestamp precision.
- Decide MPE, MIDI 2.0, and legacy MIDI support boundaries.
- Decide quantization grid, swing, groove, and humanize data models.
- Decide external MIDI clock and sync scope.

### Recommended Default

- Support MIDI 1.0 robustly first with extensible event structures that can later carry MPE/MIDI 2.0 data.

### Compromises

- Full MIDI 2.0 early would slow the core editor; designing extensibility now prevents a dead end.
- Musical-time editing is user-friendly, while sample-time playback is precise; store enough data to convert explicitly.

## 011 Piano Roll And MIDI Editing

### Concrete Planning

- Build note editing, controller lanes, velocity, audition, scale tools, ghost notes, fold mode, and event list editing.

### Decisions To Make

- Decide piano roll rendering and virtualization strategy.
- Decide controller lane types and editing gestures.
- Decide note naming and scale/chord assistance rules.
- Decide keyboard-first workflows for power users.

### Recommended Default

- Use a custom-painted grid with reusable command-backed gestures shared with the arrangement timeline where possible.

### Compromises

- Advanced helpers such as chord labels and scales add value, but must not obscure precise manual editing.
- Event-list precision is less visually appealing but essential for professional MIDI work.

## 012 Drum Machine

### Concrete Planning

- Build sample pads, playback engine, choke groups, routing, presets, and asset integration.

### Decisions To Make

- Decide drum machine architecture: native instrument plugin, built-in track device, or hybrid.
- Decide sample layering, round-robin, velocity zones, and modulation scope.
- Decide pad routing into mixer channels.
- Decide bundled kit licensing.

### Recommended Default

- Implement as a built-in instrument/device hosted by the same engine infrastructure as plugins where practical.

### Compromises

- Built-in integration enables deep MCP and routing control, but a plugin-like boundary improves modularity.
- Bundled sounds improve onboarding, but only open-license or self-produced assets are acceptable.

## 013 Step Sequencer And Patterns

### Concrete Planning

- Add pattern clips, step properties, deterministic probability, swing, ratchets, variations, and MIDI conversion.

### Decisions To Make

- Decide pattern data model and relationship to MIDI clips.
- Decide deterministic random/probability seed behavior.
- Decide pattern chaining and variation storage.
- Decide whether patterns are global library objects or project-local clips.

### Recommended Default

- Store patterns as project-local clip data with explicit conversion to MIDI clips and deterministic seeds.

### Compromises

- Pattern-specific data supports rich sequencing, but requires conversion paths for interoperability.
- Probability makes music less static, but renders and tests need fixed seeds.

## 014 Plugin Hosting

### Concrete Planning

- Add scanning, cache, validation, insert chains, state save/load, parameters, plugin windows, and failure containment.

### Decisions To Make

- Decide plugin formats and SDK licensing path.
- Decide in-process versus out-of-process hosting.
- Decide scan isolation and blacklist rules.
- Decide plugin state serialization and compatibility policy.

### Recommended Default

- Host Audio Units first on macOS, evaluate VST3 with documented SDK/legal constraints, and isolate scanning from normal startup.

### Compromises

- In-process hosting has lower latency and simpler implementation but worse crash isolation.
- Out-of-process hosting is safer but much harder for latency, UI embedding, and plugin compatibility.

## 015 Mixer And Routing

### Concrete Planning

- Implement channels, buses, sends, inserts, meters, hardware I/O, sidechains, and routing validation.

### Decisions To Make

- Decide track/channel type model.
- Decide routing graph representation and cycle prevention.
- Decide meter standards and ballistics.
- Decide solo/mute behavior, including solo-safe and pre/post-fader sends.

### Recommended Default

- Use one routing graph model shared by engine, session, mixer UI, and MCP queries.

### Compromises

- Flexible routing is powerful but can confuse users; provide clear graph validation and simple defaults.
- Detailed metering helps mixing but costs CPU and UI bandwidth; make expensive meters optional when needed.

## 016 Automation System

### Concrete Planning

- Add automation data model, lanes, write modes, interpolation, plugin parameters, and sample-accurate playback.

### Decisions To Make

- Decide automation curve representation and interpolation modes.
- Decide automation write batching and undo behavior.
- Decide parameter identity stability for plugins.
- Decide trim/read/touch/latch/off semantics.

### Recommended Default

- Store automation as parameter-addressed lanes with explicit point/segment types and sample-accurate render evaluation.

### Compromises

- Sample-accurate automation is harder than buffer-rate updates, but required for professional results.
- Recording automation into many points is precise but can bloat sessions; add thinning with clear tolerances.

## 017 Warping, Stretching, And Pitch

### Concrete Planning

- Add transient detection, warp markers, time-stretch, pitch-shift, beat slicing, tempo conforming, and render cache.

### Decisions To Make

- Choose DSP algorithms or libraries and confirm licensing.
- Decide quality modes and CPU tradeoffs.
- Decide warp marker data model.
- Decide live preview versus offline cache behavior.

### Recommended Default

- Use pluggable processing interfaces so algorithms can improve without changing clip/session semantics.

### Compromises

- High-quality stretching is hard to implement from scratch; third-party DSP may be justified if license-compatible.
- Live stretching improves workflow but can be CPU-heavy; cache expensive results predictably.

## 018 Browser, Assets, And Media Analysis

### Concrete Planning

- Build asset browser, relink, collect, tags, preview, search, and background analysis without blocking audio.

### Decisions To Make

- Decide asset index storage: project manifest, cache database, or hybrid.
- Decide privacy boundaries for scanning user folders.
- Decide analysis algorithms for tempo, key, loudness, and transients.
- Decide preview engine routing and safety.

### Recommended Default

- Keep authoritative asset references in project files and store large searchable analysis indexes in rebuildable cache data.

### Compromises

- Background analysis improves browsing but adds CPU/disk load; throttle and pause during critical recording/playback.
- Automatic folder scanning is convenient but risky for privacy and performance; require explicit user selection.

## 019 MCP Daemon Foundation

### Concrete Planning

- Create a separate local daemon with health, lifecycle, auth, capabilities, project attachment, and protocol tests.

### Decisions To Make

- Decide daemon language and runtime.
- Decide local transport: stdio, Unix domain socket, localhost HTTP, or named pipe.
- Decide authentication and project-scoped authorization.
- Decide install/launch model on macOS.

### Recommended Default

- Start with a local-only daemon process using a narrow authenticated transport and explicit project attachment tokens.

### Compromises

- A separate daemon improves safety and lifecycle isolation, but adds IPC, versioning, and installation complexity.
- System-level integration is powerful, but must remain project-scoped and auditable.

## 020 MCP DAW Query Tools

### Concrete Planning

- Expose read-only project, track, clip, mixer, plugin, automation, transport, asset, and capability queries.

### Decisions To Make

- Decide tool naming and schema versioning.
- Decide pagination and region filtering for large sessions.
- Decide privacy logging for read access.
- Decide snapshot consistency guarantees.

### Recommended Default

- Provide narrow, versioned tools with explicit filters and stable JSON schemas.

### Compromises

- Narrow tools require more calls, but reduce privacy leakage and improve reliability.
- Full project dumps are easy for agents but dangerous for performance and sensitive data exposure.

## 021 MCP Editing Tools

### Concrete Planning

- Map MCP mutation tools to command-layer validation, preview, apply, undo, redo, and audit flows.

### Decisions To Make

- Decide which edits require confirmation tokens.
- Decide batch plan preview format.
- Decide error taxonomy for invalid edits.
- Decide how MCP edits appear in the UI undo history.

### Recommended Default

- Every MCP edit returns validation details, preview summary, command id, audit id, and undo state.

### Compromises

- Confirmation and previews add friction, but protect projects from agent mistakes.
- Direct free-form editing would be faster for agents, but is unacceptable for a professional DAW.

## 022 MCP Audio And Render Tools

### Concrete Planning

- Add bounded MCP tools for analysis, renders, bounce, freeze, normalize, reverse, batch export, progress, and cancellation.

### Decisions To Make

- Decide render job queue architecture.
- Decide export path permissions.
- Decide generated asset naming and registration.
- Decide cancellation and partial-output cleanup.

### Recommended Default

- Treat MCP renders as explicit jobs that produce manifests, project assets, or user-confirmed exports.

### Compromises

- Job queues add complexity, but long-running audio work cannot block MCP clients or the DAW UI.
- Allowing arbitrary export paths is convenient but risky; constrain paths by capability and confirmation.

## 023 AI Orchestration Workflows

### Concrete Planning

- Build higher-level workflows that produce command previews for arrangement, harmonization, drum variations, labeling, and mix preparation.

### Decisions To Make

- Decide workflow template format.
- Decide deterministic model/tool settings and fallback behavior.
- Decide human approval UI for multi-step plans.
- Decide how generated musical suggestions are attributed and audited.

### Recommended Default

- Store workflows as templates that resolve into command plans, with human approval before mutation.

### Compromises

- AI orchestration can accelerate creation, but model output must remain advisory.
- Fully automatic application is tempting, but preview/approval is necessary for trust and recoverability.

## 024 Performance, Stress, And Realtime Verification

### Concrete Planning

- Build stress projects, realtime instrumentation, benchmarks, thresholds, and regression automation.

### Decisions To Make

- Decide representative stress project sizes.
- Decide benchmark machines and reporting format.
- Decide CI versus nightly performance split.
- Decide hard thresholds for callback safety, UI responsiveness, load/save, and render time.

### Recommended Default

- Keep fast regression checks in PR CI and heavier performance suites in scheduled macOS runs with recorded machine context.

### Compromises

- Performance tests can be noisy, but no professional DAW can rely on anecdotal performance.
- Strict thresholds may fail on shared runners; classify hard invariants separately from trend benchmarks.

## 025 Packaging, Signing, Docs, And Release

### Concrete Planning

- Package, sign, notarize, document, provide examples, and define release/security processes.

### Decisions To Make

- Decide release channels: nightly, beta, stable.
- Decide updater strategy.
- Decide docs hosting and versioning.
- Decide example project and asset licensing.
- Decide crash reporting and telemetry policy.

### Recommended Default

- Ship signed/notarized macOS releases with explicit user-controlled diagnostics and fully redistributable example projects.

### Compromises

- Signing/notarization adds operational overhead, but macOS users expect it and security prompts harm adoption.
- Telemetry can improve quality, but default-off or explicitly consented diagnostics better fits open-source trust.

# Performance History

The P26 realtime deadline benchmark writes JSONL run history from CTest into the build tree under
`tests/perf/rt-history.jsonl`. Each row is keyed with the detected `MachineContext` fields and
contains RSS, measured WAV disk bytes, and a scaling curve of p99 block time, buffer deadline,
and xrun count for 8, 16, 32, and 64 track sessions. The CTest gate uses a 1024-frame buffer at
48 kHz so Debug/TSan CI still exercises the 64-track graph against a real buffer period without
turning scheduler jitter in shared runners into release-blocking noise.

The gate is blocking when any scale point reports an xrun or when p99 block time is greater than or
equal to the buffer period. Keep history files in the build tree; do not commit local JSONL runs as
source fixtures. If the threshold or reference track counts change, update this file and the release
notes in the same review so perf policy changes are visible.

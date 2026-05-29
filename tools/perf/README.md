# Performance History

The P26 realtime deadline benchmark writes JSONL run history from CTest into the build tree under
`tests/perf/rt-history.jsonl`. Each row is keyed with the detected `MachineContext` fields and
contains RSS, measured WAV disk bytes, and a scaling curve of p99 block time, buffer deadline,
and xrun count for 8, 16, and 32 track sessions.

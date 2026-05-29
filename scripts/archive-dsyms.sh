#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 [--build-dir build/macos-release-universal] [--dsymutil dsymutil] [--self-test]"
}

build_dir="build/macos-release-universal"
dsymutil_cmd="xcrun dsymutil"
self_test=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      build_dir="${2:-}"
      shift 2
      ;;
    --dsymutil)
      dsymutil_cmd="${2:-}"
      shift 2
      ;;
    --self-test)
      self_test=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

validate_dsym_payload() {
  local dsym="$1"
  if [[ ! -d "$dsym" ]]; then
    echo "dsymutil did not produce $dsym" >&2
    return 1
  fi
  local dwarf_dir="${dsym}/Contents/Resources/DWARF"
  if [[ ! -d "$dwarf_dir" ]]; then
    echo "dSYM is missing DWARF payload directory: $dwarf_dir" >&2
    return 1
  fi
  local expected_dwarf_file="${dwarf_dir}/$(basename "${dsym%.dSYM}")"
  if [[ ! -s "$expected_dwarf_file" ]]; then
    echo "dSYM is missing expected non-empty DWARF payload: $expected_dwarf_file" >&2
    return 1
  fi
}

release_binaries() {
  printf '%s\n' \
    "${build_dir}/apps/daw/lamusica_daw_artefacts/LaMusica.app/Contents/MacOS/LaMusica" \
    "${build_dir}/apps/plugin_scan_worker/lamusica_plugin_scan_worker" \
    "${build_dir}/apps/mcpd/lamusica_mcpd" \
    "${build_dir}/tools/cli/lamusica_cli"
}

validate_release_binaries() {
  local binary
  while IFS= read -r binary; do
    if [[ ! -f "$binary" ]]; then
      echo "missing binary for dSYM generation: $binary" >&2
      return 1
    fi
  done < <(release_binaries)
}

if [[ "$self_test" == true ]]; then
  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT
  mkdir -p "$tmpdir/good.dSYM/Contents/Resources/DWARF"
  printf 'debug-symbols\n' > "$tmpdir/good.dSYM/Contents/Resources/DWARF/good"
  validate_dsym_payload "$tmpdir/good.dSYM"
  mkdir -p "$tmpdir/missing-dwarf.dSYM/Contents/Resources"
  if validate_dsym_payload "$tmpdir/missing-dwarf.dSYM" 2>/dev/null; then
    echo "archive-dsyms self-test failed to reject missing DWARF directory" >&2
    exit 1
  fi
  mkdir -p "$tmpdir/empty.dSYM/Contents/Resources/DWARF"
  if validate_dsym_payload "$tmpdir/empty.dSYM" 2>/dev/null; then
    echo "archive-dsyms self-test failed to reject empty DWARF payload" >&2
    exit 1
  fi
  mkdir -p "$tmpdir/wrong-name.dSYM/Contents/Resources/DWARF"
  printf 'debug-symbols\n' > "$tmpdir/wrong-name.dSYM/Contents/Resources/DWARF/other"
  if validate_dsym_payload "$tmpdir/wrong-name.dSYM" 2>/dev/null; then
    echo "archive-dsyms self-test failed to reject wrong-named DWARF payload" >&2
    exit 1
  fi
  build_dir="$tmpdir/build"
  mkdir -p "$build_dir/apps/daw/lamusica_daw_artefacts/LaMusica.app/Contents/MacOS" \
    "$build_dir/apps/plugin_scan_worker" "$build_dir/apps/mcpd" "$build_dir/tools/cli"
  printf 'app\n' > "$build_dir/apps/daw/lamusica_daw_artefacts/LaMusica.app/Contents/MacOS/LaMusica"
  printf 'worker\n' > "$build_dir/apps/plugin_scan_worker/lamusica_plugin_scan_worker"
  printf 'mcpd\n' > "$build_dir/apps/mcpd/lamusica_mcpd"
  printf 'cli\n' > "$build_dir/tools/cli/lamusica_cli"
  validate_release_binaries
  rm -f "$build_dir/tools/cli/lamusica_cli"
  if validate_release_binaries >/dev/null 2>&1; then
    echo "archive-dsyms self-test failed to reject a missing release binary" >&2
    exit 1
  fi
  echo "archive-dsyms self-test passed"
  exit 0
fi

dsymutil_launcher="${dsymutil_cmd%% *}"
if [[ -z "$dsymutil_launcher" ]] || ! command -v "$dsymutil_launcher" >/dev/null 2>&1; then
  echo "${dsymutil_launcher:-dsymutil} is required to run dsymutil" >&2
  exit 1
fi

generate_dsym() {
  local binary="$1"
  read -r -a dsymutil_args <<< "$dsymutil_cmd"
  "${dsymutil_args[@]}" "$binary" -o "${binary}.dSYM"
  validate_dsym_payload "${binary}.dSYM" || exit 1
}

validate_release_binaries || exit 1
while IFS= read -r binary; do
  generate_dsym "$binary"
done < <(release_binaries)

#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --identity 'Developer ID Application: Name (TEAMID)' [--build-dir build/macos-release-universal]"
}

identity=""
build_dir="build/macos-release-universal"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --identity)
      identity="${2:-}"
      shift 2
      ;;
    --build-dir)
      build_dir="${2:-}"
      shift 2
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

if [[ -z "$identity" ]]; then
  usage >&2
  exit 2
fi

app="${build_dir}/apps/daw/lamusica_daw_artefacts/LaMusica.app"
entitlements="apps/daw/lamusica.entitlements"

sign_path() {
  local target="$1"
  if [[ -e "$target" ]]; then
    codesign --force --options runtime --timestamp --sign "$identity" "$target"
  fi
}

for binary in \
  "${build_dir}/apps/plugin_scan_worker/lamusica_plugin_scan_worker" \
  "${build_dir}/apps/mcpd/lamusica_mcpd" \
  "${build_dir}/tools/cli/lamusica_cli"; do
  sign_path "$binary"
done

if [[ ! -d "$app" ]]; then
  echo "missing app bundle: $app" >&2
  exit 1
fi

while IFS= read -r nested; do
  sign_path "$nested"
done < <(find "$app/Contents" -depth -mindepth 1 \( -name '*.framework' -o -type f \( -name '*.dylib' -o -name '*.so' -o -perm -111 \) \) -print)

codesign --force --options runtime --timestamp --entitlements "$entitlements" --sign "$identity" "$app"
codesign --verify --strict --deep --verbose=2 "$app"

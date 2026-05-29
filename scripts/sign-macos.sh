#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --identity 'Developer ID Application: Name (TEAMID)' [--build-dir build/macos-release-universal] [--self-test]"
}

identity=""
build_dir="build/macos-release-universal"
self_test=false

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

app="${build_dir}/apps/daw/lamusica_daw_artefacts/LaMusica.app"
entitlements="apps/daw/lamusica.entitlements"

require_entitlement_true() {
  local source="$1"
  local key="$2"
  if ! awk -v key="$key" '
    BEGIN { found = 0; ok = 0 }
    index($0, "<key>" key "</key>") { found = 1; next }
    found && /<true[[:space:]]*\/>/ { ok = 1; exit }
    found && /<key>/ { exit }
    END { exit ok ? 0 : 1 }
  ' "$source"; then
    echo "missing required true entitlement ${key} in ${source}" >&2
    return 1
  fi
}

require_path() {
  local target="$1"
  if [[ ! -e "$target" ]]; then
    echo "missing required release binary: $target" >&2
    return 1
  fi
}

validate_entitlements_file() {
  require_path "$entitlements" || exit 1
  require_entitlement_true "$entitlements" "com.apple.security.device.audio-input" || exit 1
  require_entitlement_true "$entitlements" "com.apple.security.automation.apple-events" || exit 1
}

validate_request() {
  if [[ -z "$identity" ]]; then
    usage >&2
    return 2
  fi
  if [[ ! "$identity" =~ ^Developer\ ID\ Application:\ .+\ \([A-Z0-9]{10}\)$ ]]; then
    echo "signing identity must be a Developer ID Application identity with Team ID: $identity" >&2
    return 2
  fi
  for required_binary in \
    "${build_dir}/apps/plugin_scan_worker/lamusica_plugin_scan_worker" \
    "${build_dir}/apps/mcpd/lamusica_mcpd" \
    "${build_dir}/tools/cli/lamusica_cli" \
    "$app/Contents/MacOS/LaMusica" \
    "$app"; do
    require_path "$required_binary" || return 1
  done
}

verify_signed_app_entitlements() {
  local signed_app="$1"
  local extracted
  extracted="$(mktemp)"
  codesign -d --entitlements :- "$signed_app" > "$extracted"
  require_entitlement_true "$extracted" "com.apple.security.device.audio-input" || exit 1
  require_entitlement_true "$extracted" "com.apple.security.automation.apple-events" || exit 1
  rm -f "$extracted"
}

if [[ "$self_test" == true ]]; then
  validate_entitlements_file
  temp_dir="$(mktemp -d)"
  trap 'rm -rf "$temp_dir"' EXIT
  false_entitlements="${temp_dir}/false-entitlements.plist"
  printf '%s\n' \
    '<plist>' \
    '<dict>' \
    '<key>com.apple.security.device.audio-input</key>' \
    '<false/>' \
    '<key>com.apple.security.automation.apple-events</key>' \
    '<true/>' \
    '</dict>' \
    '</plist>' > "$false_entitlements"
  if require_entitlement_true "$false_entitlements" "com.apple.security.device.audio-input" \
    >/dev/null 2>&1; then
    echo "sign-macos self-test failed to reject false entitlement" >&2
    exit 1
  fi
  identity="Developer ID Application: LaMusica Test (ABCDE12345)"
  build_dir="${temp_dir}/build"
  app="${build_dir}/apps/daw/lamusica_daw_artefacts/LaMusica.app"
  mkdir -p "${build_dir}/apps/plugin_scan_worker" "${build_dir}/apps/mcpd" \
    "${build_dir}/tools/cli" "$app/Contents/MacOS"
  printf 'worker\n' > "${build_dir}/apps/plugin_scan_worker/lamusica_plugin_scan_worker"
  printf 'mcpd\n' > "${build_dir}/apps/mcpd/lamusica_mcpd"
  printf 'cli\n' > "${build_dir}/tools/cli/lamusica_cli"
  printf 'app\n' > "$app/Contents/MacOS/LaMusica"
  validate_request
  rm -f "$app/Contents/MacOS/LaMusica"
  if validate_request >/dev/null 2>&1; then
    echo "sign-macos self-test failed to reject missing app executable" >&2
    exit 1
  fi
  printf 'app\n' > "$app/Contents/MacOS/LaMusica"
  identity=""
  if validate_request >/dev/null 2>&1; then
    echo "sign-macos self-test failed to reject missing signing identity" >&2
    exit 1
  fi
  identity="-"
  if validate_request >/dev/null 2>&1; then
    echo "sign-macos self-test failed to reject ad-hoc signing identity" >&2
    exit 1
  fi
  identity="Developer ID Application: LaMusica Test (ABCDE12345)"
  rm -f "${build_dir}/tools/cli/lamusica_cli"
  if validate_request >/dev/null 2>&1; then
    echo "sign-macos self-test failed to reject missing release binary" >&2
    exit 1
  fi
  echo "sign-macos entitlement self-test passed"
  exit 0
fi

validate_request || exit $?

sign_path() {
  local target="$1"
  require_path "$target" || exit 1
  codesign --force --options runtime --timestamp --sign "$identity" "$target"
  codesign --verify --strict --verbose=2 "$target"
}

validate_entitlements_file

for binary in \
  "${build_dir}/apps/plugin_scan_worker/lamusica_plugin_scan_worker" \
  "${build_dir}/apps/mcpd/lamusica_mcpd" \
  "${build_dir}/tools/cli/lamusica_cli"; do
  sign_path "$binary"
done

require_path "$app"

while IFS= read -r nested; do
  codesign --force --options runtime --timestamp --sign "$identity" "$nested"
  codesign --verify --strict --verbose=2 "$nested"
done < <(find "$app/Contents" -depth -mindepth 1 \( -name '*.framework' -o -type f \( -name '*.dylib' -o -name '*.so' -o -perm -111 \) \) -print)

codesign --force --options runtime --timestamp --entitlements "$entitlements" --sign "$identity" "$app"
codesign --verify --strict --deep --verbose=2 "$app"
verify_signed_app_entitlements "$app"

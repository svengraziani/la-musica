#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --app LaMusica.app [--binary path]... [--artifact LaMusica-0.1.0-Darwin.dmg] [--self-test]"
}

app=""
artifact=""
declare -a binaries=()
self_test=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app)
      app="${2:-}"
      shift 2
      ;;
    --binary)
      binaries+=("${2:-}")
      shift 2
      ;;
    --artifact)
      artifact="${2:-}"
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
    echo "signed app is missing required true entitlement ${key}" >&2
    return 1
  fi
}

validate_request() {
  if [[ -z "$app" || ! -e "$app" ]]; then
    usage >&2
    return 2
  fi
  if [[ -n "$artifact" ]]; then
    if [[ ! -f "$artifact" ]]; then
      usage >&2
      return 2
    fi
    case "$artifact" in
      *.dmg)
        artifact_name="$(basename "$artifact")"
        if [[ ! "$artifact_name" =~ ^LaMusica-[0-9]+(\.[0-9]+){2}([-+][A-Za-z0-9._-]+)?-Darwin\.dmg$ ]]; then
          echo "signature verification artifact must be named LaMusica-<version>-Darwin.dmg: $artifact_name" >&2
          return 2
        fi
        ;;
      *)
        echo "signature verification artifact must be the stapled release .dmg: $artifact" >&2
        return 2
        ;;
    esac
  fi
  local binary
  local binary_names=""
  for binary in "${binaries[@]}"; do
    if [[ -z "$binary" || ! -f "$binary" ]]; then
      usage >&2
      return 2
    fi
    binary_names="${binary_names}$(basename "$binary")"$'\n'
  done
  if [[ -n "$artifact" ]]; then
    for required_name in lamusica_plugin_scan_worker lamusica_mcpd lamusica_cli; do
      if ! grep -Fxq "$required_name" <<<"$binary_names"; then
        echo "release signature verification requires --binary ${required_name}" >&2
        return 2
      fi
    done
  fi
}

if [[ "$self_test" == true ]]; then
  temp_dir="$(mktemp -d)"
  trap 'rm -rf "$temp_dir"' EXIT
  app="${temp_dir}/LaMusica.app"
  artifact="${temp_dir}/LaMusica-0.1.0-Darwin.dmg"
  binaries=("${temp_dir}/lamusica_plugin_scan_worker" "${temp_dir}/lamusica_mcpd" "${temp_dir}/lamusica_cli")
  mkdir -p "$app"
  printf 'dmg\n' > "$artifact"
  printf 'worker\n' > "${binaries[0]}"
  printf 'mcpd\n' > "${binaries[1]}"
  printf 'cli\n' > "${binaries[2]}"
  validate_request
  app=""
  if validate_request >/dev/null 2>&1; then
    echo "verify-signature self-test failed to reject missing app" >&2
    exit 1
  fi
  app="${temp_dir}/LaMusica.app"
  artifact="${temp_dir}/missing.dmg"
  if validate_request >/dev/null 2>&1; then
    echo "verify-signature self-test failed to reject missing artifact" >&2
    exit 1
  fi
  artifact="${temp_dir}/LaMusica.zip"
  printf 'zip\n' > "$artifact"
  if validate_request >/dev/null 2>&1; then
    echo "verify-signature self-test failed to reject non-DMG release artifact" >&2
    exit 1
  fi
  artifact="${temp_dir}/LaMusica.dmg"
  printf 'wrong dmg name\n' > "$artifact"
  if validate_request >/dev/null 2>&1; then
    echo "verify-signature self-test failed to reject non-Darwin DMG artifact name" >&2
    exit 1
  fi
  artifact="${temp_dir}/LaMusica-0.1.0-Darwin.dmg"
  binaries=("${temp_dir}/lamusica_mcpd" "${temp_dir}/lamusica_cli")
  if validate_request >/dev/null 2>&1; then
    echo "verify-signature self-test failed to reject missing plugin scan worker verification" >&2
    exit 1
  fi
  artifact=""
  binaries=("${temp_dir}/missing-helper")
  if validate_request >/dev/null 2>&1; then
    echo "verify-signature self-test failed to reject missing helper binary" >&2
    exit 1
  fi
  binaries=()
  entitlements="${temp_dir}/entitlements.plist"
  printf '%s\n' \
    '<plist>' \
    '<key>com.apple.security.device.audio-input</key>' \
    '<true/>' \
    '<key>com.apple.security.automation.apple-events</key>' \
    '<true/>' \
    '</plist>' > "$entitlements"
  require_entitlement_true "$entitlements" "com.apple.security.device.audio-input"
  require_entitlement_true "$entitlements" "com.apple.security.automation.apple-events"
  if require_entitlement_true "$entitlements" "com.apple.security.files.user-selected.read-write" \
    >/dev/null 2>&1; then
    echo "verify-signature self-test failed to reject missing entitlement" >&2
    exit 1
  fi
  false_entitlements="${temp_dir}/false-entitlements.plist"
  printf '%s\n' \
    '<plist>' \
    '<key>com.apple.security.device.audio-input</key>' \
    '<false/>' \
    '<key>com.apple.security.automation.apple-events</key>' \
    '<true/>' \
    '</plist>' > "$false_entitlements"
  if require_entitlement_true "$false_entitlements" "com.apple.security.device.audio-input" \
    >/dev/null 2>&1; then
    echo "verify-signature self-test failed to reject false entitlement" >&2
    exit 1
  fi
  echo "verify-signature self-test passed"
  exit 0
fi

validate_request || exit 2

verify_signed_app_entitlements() {
  local extracted
  extracted="$(mktemp)"
  codesign -d --entitlements :- "$app" > "$extracted"
  require_entitlement_true "$extracted" "com.apple.security.device.audio-input" || exit 1
  require_entitlement_true "$extracted" "com.apple.security.automation.apple-events" || exit 1
  rm -f "$extracted"
}

codesign --verify --strict --deep --verbose=2 "$app"
verify_signed_app_entitlements
spctl --assess --type execute --verbose "$app"

for binary in "${binaries[@]}"; do
  codesign --verify --strict --verbose=2 "$binary"
done

if [[ -n "$artifact" ]]; then
  xcrun stapler validate "$artifact"
fi

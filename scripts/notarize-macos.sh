#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --artifact LaMusica-0.1.0-Darwin.dmg (--keychain-profile profile | --key key.p8 --key-id KEYID --issuer ISSUER) [--self-test]"
}

artifact=""
profile=""
api_key=""
key_id=""
issuer=""
self_test=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --artifact)
      artifact="${2:-}"
      shift 2
      ;;
    --keychain-profile)
      profile="${2:-}"
      shift 2
      ;;
    --key)
      api_key="${2:-}"
      shift 2
      ;;
    --key-id)
      key_id="${2:-}"
      shift 2
      ;;
    --issuer)
      issuer="${2:-}"
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

validate_request() {
  if [[ -z "$artifact" || ! -f "$artifact" ]]; then
    usage >&2
    return 2
  fi
  case "$artifact" in
    *.dmg)
      artifact_name="$(basename "$artifact")"
      if [[ ! "$artifact_name" =~ ^LaMusica-[0-9]+(\.[0-9]+){2}([-+][A-Za-z0-9._-]+)?-Darwin\.dmg$ ]]; then
        echo "notarization DMG artifact must be named LaMusica-<version>-Darwin.dmg: $artifact_name" >&2
        return 2
      fi
      ;;
    *.zip)
      ;;
    *)
      echo "notarization artifact must be a .dmg or .zip file: $artifact" >&2
      return 2
      ;;
  esac

  if [[ -n "$profile" ]]; then
    if [[ -n "$api_key" || -n "$key_id" || -n "$issuer" ]]; then
      echo "use either --keychain-profile or --key/--key-id/--issuer, not both" >&2
      return 2
    fi
    return 0
  fi

  if [[ -z "$api_key" || -z "$key_id" || -z "$issuer" || ! -f "$api_key" ]]; then
    usage >&2
    return 2
  fi
  if [[ ! "$key_id" =~ ^[A-Z0-9]{10}$ ]]; then
    echo "notarization key id must be a 10-character App Store Connect key id: $key_id" >&2
    return 2
  fi
  if [[ ! "$issuer" =~ ^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$ ]]; then
    echo "notarization issuer must be an App Store Connect issuer UUID: $issuer" >&2
    return 2
  fi
}

if [[ "$self_test" == true ]]; then
  temp_dir="$(mktemp -d)"
  trap 'rm -rf "$temp_dir"' EXIT
  artifact="${temp_dir}/LaMusica-0.1.0-Darwin.dmg"
  api_key="${temp_dir}/AuthKey.p8"
  printf 'dmg\n' > "$artifact"
  printf 'key\n' > "$api_key"

  profile="release-profile"
  api_key=""
  key_id=""
  issuer=""
  validate_request

  profile=""
  api_key="${temp_dir}/AuthKey.p8"
  key_id="ABCDE12345"
  issuer="12345678-1234-1234-1234-123456789abc"
  validate_request

  profile="release-profile"
  if validate_request >/dev/null 2>&1; then
    echo "notarize self-test failed to reject mixed credential modes" >&2
    exit 1
  fi

  profile=""
  api_key="${temp_dir}/missing.p8"
  if validate_request >/dev/null 2>&1; then
    echo "notarize self-test failed to reject missing API key" >&2
    exit 1
  fi

  api_key="${temp_dir}/AuthKey.p8"
  key_id=""
  issuer="12345678-1234-1234-1234-123456789abc"
  if validate_request >/dev/null 2>&1; then
    echo "notarize self-test failed to reject missing key id" >&2
    exit 1
  fi

  key_id="ABCDE12345"
  issuer=""
  if validate_request >/dev/null 2>&1; then
    echo "notarize self-test failed to reject missing issuer" >&2
    exit 1
  fi

  key_id="bad-key"
  issuer="12345678-1234-1234-1234-123456789abc"
  if validate_request >/dev/null 2>&1; then
    echo "notarize self-test failed to reject malformed key id" >&2
    exit 1
  fi

  key_id="ABCDE12345"
  issuer="not-a-uuid"
  if validate_request >/dev/null 2>&1; then
    echo "notarize self-test failed to reject malformed issuer UUID" >&2
    exit 1
  fi

  artifact="${temp_dir}/LaMusica.tar.gz"
  printf 'archive\n' > "$artifact"
  issuer="12345678-1234-1234-1234-123456789abc"
  if validate_request >/dev/null 2>&1; then
    echo "notarize self-test failed to reject unsupported artifact type" >&2
    exit 1
  fi

  artifact="${temp_dir}/LaMusica.dmg"
  printf 'wrong dmg name\n' > "$artifact"
  if validate_request >/dev/null 2>&1; then
    echo "notarize self-test failed to reject non-Darwin DMG artifact name" >&2
    exit 1
  fi

  echo "notarize self-test passed"
  exit 0
fi

validate_request

if [[ -n "$profile" ]]; then
  xcrun notarytool submit "$artifact" --keychain-profile "$profile" --wait
else
  xcrun notarytool submit "$artifact" --key "$api_key" --key-id "$key_id" --issuer "$issuer" --wait
fi
xcrun stapler staple "$artifact"
xcrun stapler validate "$artifact"

#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --identity 'Developer ID Application: Name (TEAMID)' [--checksums build/release-metadata/SHA256SUMS] [--self-test]"
}

identity=""
checksums="build/release-metadata/SHA256SUMS"
self_test=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --identity)
      identity="${2:-}"
      shift 2
      ;;
    --checksums)
      checksums="${2:-}"
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

validate_checksums_file() {
  local file="$1"
  if [[ ! -s "$file" ]]; then
    echo "checksum file is missing or empty: $file" >&2
    return 1
  fi
  local line digest artifact_name seen_names="" has_release_dmg=false has_dsym_archive=false
  while IFS= read -r line || [[ -n "$line" ]]; do
    if [[ ! "$line" =~ ^[0-9a-f]{64}[[:space:]][[:space:]][A-Za-z0-9._+-]+$ ]]; then
      echo "invalid SHA256SUMS line: $line" >&2
      return 1
    fi
    digest="${line%%  *}"
    artifact_name="${line#*  }"
    if [[ "$artifact_name" == */* || "$artifact_name" == *\\* ]]; then
      echo "checksum artifact must be a safe basename: $artifact_name" >&2
      return 1
    fi
    if [[ "$artifact_name" =~ \.dmg$ ]]; then
      if [[ ! "$artifact_name" =~ ^LaMusica-[0-9]+(\.[0-9]+){2}([-+][A-Za-z0-9._-]+)?-Darwin\.dmg$ ]]; then
        echo "checksum release DMG row must be named LaMusica-<version>-Darwin.dmg: $artifact_name" >&2
        return 1
      fi
      has_release_dmg=true
    fi
    if [[ "$artifact_name" == "LaMusica-dSYMs.tar.gz" ]]; then
      has_dsym_archive=true
    fi
    if grep -Fxq "$artifact_name" <<<"$seen_names"; then
      echo "duplicate checksum artifact row: $artifact_name" >&2
      return 1
    fi
    seen_names="${seen_names}${artifact_name}"$'\n'
  done < "$file"
  if [[ "$has_release_dmg" == true && "$has_dsym_archive" != true ]]; then
    echo "checksum file for a release DMG must include LaMusica-dSYMs.tar.gz" >&2
    return 1
  fi
}

validate_identity() {
  if [[ -z "$identity" ]]; then
    usage >&2
    return 2
  fi
  if [[ ! "$identity" =~ ^Developer\ ID\ Application:\ .+\ \([A-Z0-9]{10}\)$ ]]; then
    echo "checksum signing identity must be a Developer ID Application identity with Team ID: $identity" >&2
    return 2
  fi
}

if [[ "$self_test" == true ]]; then
  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT
  printf '%064d  LaMusica-0.1.0-Darwin.dmg\n%064d  LaMusica-dSYMs.tar.gz\n' 0 1 > "$tmpdir/SHA256SUMS.good"
  validate_checksums_file "$tmpdir/SHA256SUMS.good"
  printf 'not-a-digest  LaMusica-test.dmg\n' > "$tmpdir/SHA256SUMS.bad"
  if validate_checksums_file "$tmpdir/SHA256SUMS.bad" 2>/dev/null; then
    echo "sign-checksums self-test failed to reject malformed checksum file" >&2
    exit 1
  fi
  printf 'abcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcd  LaMusica-test.dmg\n' \
    | tr 'a-f' 'A-F' > "$tmpdir/SHA256SUMS.uppercase"
  if validate_checksums_file "$tmpdir/SHA256SUMS.uppercase" 2>/dev/null; then
    echo "sign-checksums self-test failed to reject uppercase checksum digest" >&2
    exit 1
  fi
  printf '%064d  ../LaMusica-test.dmg\n' 0 > "$tmpdir/SHA256SUMS.unsafe"
  if validate_checksums_file "$tmpdir/SHA256SUMS.unsafe" 2>/dev/null; then
    echo "sign-checksums self-test failed to reject unsafe artifact name" >&2
    exit 1
  fi
  printf '%064d  LaMusica-dSYMs.tar.gz\n%064d  LaMusica-dSYMs.tar.gz\n' 0 1 > "$tmpdir/SHA256SUMS.duplicate"
  if validate_checksums_file "$tmpdir/SHA256SUMS.duplicate" 2>/dev/null; then
    echo "sign-checksums self-test failed to reject duplicate artifact rows" >&2
    exit 1
  fi
  printf '%064d  LaMusica.dmg\n%064d  LaMusica-dSYMs.tar.gz\n' 0 1 > "$tmpdir/SHA256SUMS.wrong-dmg-name"
  if validate_checksums_file "$tmpdir/SHA256SUMS.wrong-dmg-name" 2>/dev/null; then
    echo "sign-checksums self-test failed to reject non-Darwin release DMG row" >&2
    exit 1
  fi
  printf '%064d  LaMusica-0.1.0-Darwin.dmg\n' 0 > "$tmpdir/SHA256SUMS.missing-dsyms"
  if validate_checksums_file "$tmpdir/SHA256SUMS.missing-dsyms" 2>/dev/null; then
    echo "sign-checksums self-test failed to reject release DMG without dSYM archive row" >&2
    exit 1
  fi
  identity="Developer ID Application: LaMusica Test (ABCDE12345)"
  validate_identity
  identity=""
  if validate_identity >/dev/null 2>&1; then
    echo "sign-checksums self-test failed to reject missing signing identity" >&2
    exit 1
  fi
  identity="-"
  if validate_identity >/dev/null 2>&1; then
    echo "sign-checksums self-test failed to reject ad-hoc signing identity" >&2
    exit 1
  fi
  identity="Developer ID Application: LaMusica Test (bad-team)"
  if validate_identity >/dev/null 2>&1; then
    echo "sign-checksums self-test failed to reject malformed Team ID" >&2
    exit 1
  fi
  echo "sign-checksums self-test passed"
  exit 0
fi

if [[ ! -f "$checksums" ]]; then
  usage >&2
  exit 2
fi

validate_identity || exit $?
validate_checksums_file "$checksums" || exit 1
signature="${checksums}.sig"
codesign --force --timestamp --sign "$identity" --detached "$signature" "$checksums"
if [[ ! -s "$signature" ]]; then
  echo "codesign did not produce a non-empty detached signature: $signature" >&2
  exit 1
fi
codesign --verify --strict --detached "$signature" "$checksums"

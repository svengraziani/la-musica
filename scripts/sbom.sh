#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --artifact LaMusica-0.1.0-Darwin.dmg --artifact LaMusica-dSYMs.tar.gz --output build/release-metadata [--self-test]"
}

artifacts=()
output_dir="build/release-metadata"
self_test=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --artifact)
      artifacts+=("${2:-}")
      shift 2
      ;;
    --output)
      output_dir="${2:-}"
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

validate_artifact_name() {
  local name="$1"
  if [[ ! "$name" =~ ^[A-Za-z0-9._+-]+$ ]]; then
    echo "artifact filename contains characters unsafe for SPDX JSON/checksum output: $name" >&2
    return 1
  fi
}

validate_metadata() {
  local sbom_file="$1"
  local checksums_file="$2"
  [[ -s "$sbom_file" ]] || { echo "SBOM was not written: $sbom_file" >&2; return 1; }
  [[ -s "$checksums_file" ]] || { echo "SHA256SUMS was not written: $checksums_file" >&2; return 1; }
  grep -Fq '"spdxVersion": "SPDX-2.3"' "$sbom_file" ||
    { echo "SBOM is missing SPDX-2.3 marker" >&2; return 1; }
  grep -Fq '"licenseDeclared": "AGPL-3.0-or-later"' "$sbom_file" ||
    { echo "SBOM is missing LaMusica AGPL license declaration" >&2; return 1; }
  grep -Fq '"name": "JUCE"' "$sbom_file" ||
    { echo "SBOM is missing JUCE package entry" >&2; return 1; }
  local line digest_value artifact_name
  while IFS= read -r line || [[ -n "$line" ]]; do
    digest_value="$(awk '{print $1}' <<<"$line")"
    artifact_name="$(cut -d' ' -f3- <<<"$line")"
    grep -Fq "\"checksumValue\": \"${digest_value}\"" "$sbom_file" ||
      { echo "SBOM checksum does not include artifact digest: ${artifact_name}" >&2; return 1; }
    grep -Fxq "${digest_value}  ${artifact_name}" "$checksums_file" ||
      { echo "SHA256SUMS does not match artifact digest/name: ${artifact_name}" >&2; return 1; }
  done < "$checksums_file"
}

if [[ "$self_test" == true ]]; then
  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT
  printf 'LaMusica SBOM self-test\n' > "$tmpdir/LaMusica-0.1.0-Darwin.dmg"
  printf 'LaMusica dSYM self-test\n' > "$tmpdir/LaMusica-dSYMs.tar.gz"
  "$0" --artifact "$tmpdir/LaMusica-0.1.0-Darwin.dmg" \
    --artifact "$tmpdir/LaMusica-dSYMs.tar.gz" \
    --output "$tmpdir/metadata" >/dev/null
  [[ -s "$tmpdir/metadata/LaMusica.spdx.json" && -s "$tmpdir/metadata/SHA256SUMS" ]] ||
    { echo "sbom self-test did not produce metadata" >&2; exit 1; }
  if [[ "$(wc -l < "$tmpdir/metadata/SHA256SUMS")" -ne 2 ]]; then
    echo "sbom self-test did not checksum every artifact" >&2
    exit 1
  fi
  grep -Fq "  LaMusica-0.1.0-Darwin.dmg" "$tmpdir/metadata/SHA256SUMS" ||
    { echo "sbom self-test is missing the DMG checksum row" >&2; exit 1; }
  grep -Fq "  LaMusica-dSYMs.tar.gz" "$tmpdir/metadata/SHA256SUMS" ||
    { echo "sbom self-test is missing the dSYM checksum row" >&2; exit 1; }
  grep -Fq '"name": "LaMusica-0.1.0-Darwin.dmg"' "$tmpdir/metadata/LaMusica.spdx.json" ||
    { echo "sbom self-test is missing the DMG SBOM package" >&2; exit 1; }
  grep -Fq '"name": "LaMusica-dSYMs.tar.gz"' "$tmpdir/metadata/LaMusica.spdx.json" ||
    { echo "sbom self-test is missing the dSYM SBOM package" >&2; exit 1; }
  if "$0" --artifact "$tmpdir/LaMusica-0.1.0-Darwin.dmg" \
    --output "$tmpdir/missing-dsyms-metadata" >/dev/null 2>&1; then
    echo "sbom self-test failed to reject a release DMG without LaMusica-dSYMs.tar.gz" >&2
    exit 1
  fi
  mkdir -p "$tmpdir/also"
  printf 'duplicate artifact basename\n' > "$tmpdir/also/LaMusica-0.1.0-Darwin.dmg"
  if "$0" --artifact "$tmpdir/LaMusica-0.1.0-Darwin.dmg" \
    --artifact "$tmpdir/also/LaMusica-0.1.0-Darwin.dmg" \
    --output "$tmpdir/duplicate-metadata" >/dev/null 2>&1; then
    echo "sbom self-test failed to reject duplicate artifact basenames" >&2
    exit 1
  fi
  printf 'artifact from another directory\n' > "$tmpdir/also/LaMusica-0.2.0-Darwin.dmg"
  if "$0" --artifact "$tmpdir/LaMusica-0.1.0-Darwin.dmg" \
    --artifact "$tmpdir/also/LaMusica-0.2.0-Darwin.dmg" \
    --output "$tmpdir/cross-dir-metadata" >/dev/null 2>&1; then
    echo "sbom self-test failed to reject artifacts from different directories" >&2
    exit 1
  fi
  printf 'unsafe artifact basename\n' > "$tmpdir/LaMusica unsafe.dmg"
  if "$0" --artifact "$tmpdir/LaMusica unsafe.dmg" \
    --output "$tmpdir/unsafe-metadata" >/dev/null 2>&1; then
    echo "sbom self-test failed to reject unsafe artifact basenames" >&2
    exit 1
  fi
  printf 'wrong DMG name\n' > "$tmpdir/LaMusica.dmg"
  if "$0" --artifact "$tmpdir/LaMusica.dmg" \
    --artifact "$tmpdir/LaMusica-dSYMs.tar.gz" \
    --output "$tmpdir/wrong-dmg-name-metadata" >/dev/null 2>&1; then
    echo "sbom self-test failed to reject a non-Darwin release DMG name" >&2
    exit 1
  fi
  echo "sbom self-test passed"
  exit 0
fi

if [[ "${#artifacts[@]}" -eq 0 ]]; then
  usage >&2
  exit 2
fi

mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd -P)"
sbom="${output_dir}/LaMusica.spdx.json"
checksums="${output_dir}/SHA256SUMS"

checksum_rows=()
package_entries=()
primary_artifact_name=""
artifact_root=""
artifact_names=""
has_release_dmg=false
has_dsym_archive=false
for artifact in "${artifacts[@]}"; do
  if [[ -z "$artifact" || ! -f "$artifact" ]]; then
    usage >&2
    exit 2
  fi
  artifact_dir="$(cd "$(dirname "$artifact")" && pwd -P)"
  basename_artifact="$(basename "$artifact")"
  validate_artifact_name "$basename_artifact" || exit 1
  if [[ "$basename_artifact" =~ \.dmg$ ]]; then
    if [[ ! "$basename_artifact" =~ ^LaMusica-[0-9]+(\.[0-9]+){2}([-+][A-Za-z0-9._-]+)?-Darwin\.dmg$ ]]; then
      echo "release DMG artifact must be named LaMusica-<version>-Darwin.dmg: $basename_artifact" >&2
      exit 2
    fi
    has_release_dmg=true
  fi
  if [[ "$basename_artifact" == "LaMusica-dSYMs.tar.gz" ]]; then
    has_dsym_archive=true
  fi
  if grep -Fxq "$basename_artifact" <<<"$artifact_names"; then
    echo "duplicate release artifact basename: $basename_artifact" >&2
    exit 2
  fi
  artifact_names="${artifact_names}${basename_artifact}"$'\n'
  if [[ -z "$artifact_root" ]]; then
    artifact_root="$artifact_dir"
  elif [[ "$artifact_dir" != "$artifact_root" ]]; then
    echo "all release artifacts must be in the same directory so SHA256SUMS verifies reproducibly" >&2
    exit 2
  fi
  artifact_path="${artifact_dir}/${basename_artifact}"
  if [[ -z "$primary_artifact_name" ]]; then
    primary_artifact_name="$basename_artifact"
  fi
  if command -v shasum >/dev/null 2>&1; then
    digest="$(shasum -a 256 "$artifact_path" | awk '{print $1}')"
  else
    digest="$(sha256sum "$artifact_path" | awk '{print $1}')"
  fi
  checksum_rows+=("${digest}  ${basename_artifact}")
  package_entries+=("    {
      \"name\": \"${basename_artifact}\",
      \"SPDXID\": \"SPDXRef-Artifact-${#package_entries[@]}\",
      \"downloadLocation\": \"NOASSERTION\",
      \"filesAnalyzed\": false,
      \"licenseConcluded\": \"AGPL-3.0-or-later\",
      \"licenseDeclared\": \"AGPL-3.0-or-later\",
      \"checksums\": [{\"algorithm\": \"SHA256\", \"checksumValue\": \"${digest}\"}]
    }")
done

if [[ "$has_release_dmg" == true && "$has_dsym_archive" != true ]]; then
  echo "release SBOM/checksums for a DMG must include LaMusica-dSYMs.tar.gz" >&2
  exit 2
fi

packages_json="$(IFS=,; echo "${package_entries[*]}")"

cat > "$sbom" <<EOF
{
  "spdxVersion": "SPDX-2.3",
  "dataLicense": "CC0-1.0",
  "SPDXID": "SPDXRef-DOCUMENT",
  "name": "LaMusica release artifact",
  "documentNamespace": "https://lamusica.dev/spdx/${primary_artifact_name}",
  "creationInfo": {
    "created": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "creators": ["Tool: scripts/sbom.sh"]
  },
  "packages": [
${packages_json},
    {
      "name": "JUCE",
      "SPDXID": "SPDXRef-Package-JUCE",
      "versionInfo": "8.0.13",
      "downloadLocation": "https://github.com/juce-framework/JUCE",
      "filesAnalyzed": false,
      "licenseConcluded": "GPL-3.0-or-later",
      "licenseDeclared": "GPL-3.0-or-later"
    }
  ]
}
EOF

printf '%s\n' "${checksum_rows[@]}" > "$checksums"

(
  cd "$artifact_root"
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 -c "$checksums"
  else
    sha256sum -c "$checksums"
  fi
)
validate_metadata "$sbom" "$checksums" || exit 1
echo "wrote $sbom"
echo "wrote $checksums"

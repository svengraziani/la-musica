#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --artifact LaMusica-0.1.0-Darwin.tar.gz --output build/release-metadata"
}

artifact=""
output_dir="build/release-metadata"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --artifact)
      artifact="${2:-}"
      shift 2
      ;;
    --output)
      output_dir="${2:-}"
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

if [[ -z "$artifact" || ! -f "$artifact" ]]; then
  usage >&2
  exit 2
fi

mkdir -p "$output_dir"
sbom="${output_dir}/LaMusica.spdx.json"
checksums="${output_dir}/SHA256SUMS"

if command -v shasum >/dev/null 2>&1; then
  digest="$(shasum -a 256 "$artifact" | awk '{print $1}')"
else
  digest="$(sha256sum "$artifact" | awk '{print $1}')"
fi
basename_artifact="$(basename "$artifact")"

cat > "$sbom" <<EOF
{
  "spdxVersion": "SPDX-2.3",
  "dataLicense": "CC0-1.0",
  "SPDXID": "SPDXRef-DOCUMENT",
  "name": "LaMusica release artifact",
  "documentNamespace": "https://lamusica.dev/spdx/${basename_artifact}",
  "creationInfo": {
    "created": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "creators": ["Tool: scripts/sbom.sh"]
  },
  "packages": [
    {
      "name": "LaMusica",
      "SPDXID": "SPDXRef-Package-LaMusica",
      "downloadLocation": "NOASSERTION",
      "filesAnalyzed": false,
      "licenseConcluded": "AGPL-3.0-or-later",
      "licenseDeclared": "AGPL-3.0-or-later",
      "checksums": [{"algorithm": "SHA256", "checksumValue": "${digest}"}]
    },
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

printf '%s  %s\n' "$digest" "$basename_artifact" > "$checksums"

(
  cd "$(dirname "$artifact")"
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 -c "$(realpath "$checksums")"
  else
    sha256sum -c "$(realpath "$checksums")"
  fi
)
echo "wrote $sbom"
echo "wrote $checksums"

#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --app LaMusica.app [--artifact LaMusica.dmg]"
}

app=""
artifact=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app)
      app="${2:-}"
      shift 2
      ;;
    --artifact)
      artifact="${2:-}"
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

if [[ -z "$app" ]]; then
  usage >&2
  exit 2
fi

codesign --verify --strict --deep --verbose=2 "$app"
spctl --assess --type execute --verbose "$app"

if [[ -n "$artifact" ]]; then
  xcrun stapler validate "$artifact"
fi

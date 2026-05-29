#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --artifact LaMusica.dmg --keychain-profile profile"
}

artifact=""
profile=""

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

if [[ -z "$artifact" || -z "$profile" ]]; then
  usage >&2
  exit 2
fi

xcrun notarytool submit "$artifact" --keychain-profile "$profile" --wait
xcrun stapler staple "$artifact"
xcrun stapler validate "$artifact"

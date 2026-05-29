#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --app LaMusica --mcpd lamusica_mcpd --cli lamusica_cli [--source-dir .] [--allow-dirty] [--self-test]"
}

app=""
mcpd=""
cli=""
source_dir="."
allow_dirty=false
self_test=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app)
      app="${2:-}"
      shift 2
      ;;
    --mcpd)
      mcpd="${2:-}"
      shift 2
      ;;
    --cli)
      cli="${2:-}"
      shift 2
      ;;
    --source-dir)
      source_dir="${2:-}"
      shift 2
      ;;
    --allow-dirty)
      allow_dirty=true
      shift
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

expected_commit=""
if git -C "$source_dir" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  expected_commit="$(git -C "$source_dir" rev-parse --short=12 HEAD)"
fi

verify_binary() {
  local label="$1"
  local binary="$2"
  if [[ ! -x "$binary" ]]; then
    echo "missing executable for provenance check: $binary" >&2
    return 1
  fi

  local output
  output="$("$binary" --version)"
  [[ "$output" =~ ^[^[:space:]]+[[:space:]][0-9]+\.[0-9]+\.[0-9]+[[:space:]] ]] || {
    echo "$label version output missing semantic version prefix: $output" >&2
    return 1
  }
  [[ "$output" == *"commit="* ]] || { echo "$label version output missing commit: $output" >&2; return 1; }
  [[ "$output" == *"dirty="* ]] || { echo "$label version output missing dirty flag: $output" >&2; return 1; }
  [[ "$output" == *"buildDate="* ]] || { echo "$label version output missing build date: $output" >&2; return 1; }
  [[ "$output" =~ (^|[[:space:]])commit=([0-9a-f]{12}|unknown)([[:space:]]|$) ]] || {
    echo "$label version output has invalid commit field: $output" >&2
    return 1
  }
  local commit="${BASH_REMATCH[2]}"
  [[ "$output" =~ (^|[[:space:]])dirty=(true|false)([[:space:]]|$) ]] || {
    echo "$label version output has invalid dirty field: $output" >&2
    return 1
  }
  local dirty="${BASH_REMATCH[2]}"
  [[ "$output" =~ (^|[[:space:]])buildDate=[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z([[:space:]]|$) ]] || {
    echo "$label version output has invalid buildDate field: $output" >&2
    return 1
  }
  if [[ -n "$expected_commit" && "$commit" != "$expected_commit" ]]; then
    echo "$label version output does not match HEAD ${expected_commit}: $output" >&2
    return 1
  fi
  if [[ "$allow_dirty" != true && "$dirty" != false ]]; then
    echo "$label version output is from a dirty tree; release provenance requires dirty=false: $output" >&2
    return 1
  fi
}

if [[ "$self_test" == true ]]; then
  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT
  make_fake_binary() {
    local path="$1"
    local output="$2"
    cat > "$path" <<EOF
#!/usr/bin/env bash
if [[ "\${1:-}" == "--version" ]]; then
  printf '%s\n' "$output"
  exit 0
fi
exit 1
EOF
    chmod +x "$path"
  }
  make_fake_binary "$tmpdir/app" "LaMusica 0.1.0 commit=${expected_commit:-123456789abc} dirty=false buildDate=2026-05-29T00:00:00Z"
  make_fake_binary "$tmpdir/mcpd" "lamusica-mcpd 0.1.0 commit=${expected_commit:-123456789abc} dirty=false buildDate=2026-05-29T00:00:00Z"
  make_fake_binary "$tmpdir/cli" "lamusica-cli 0.1.0 commit=${expected_commit:-123456789abc} dirty=false buildDate=2026-05-29T00:00:00Z"
  verify_binary LaMusica "$tmpdir/app" || exit 1
  verify_binary lamusica_mcpd "$tmpdir/mcpd" || exit 1
  verify_binary lamusica_cli "$tmpdir/cli" || exit 1
  if verify_binary missing "$tmpdir/missing-cli" 2>/dev/null; then
    echo "verify-provenance self-test failed to reject missing executable" >&2
    exit 1
  fi
  make_fake_binary "$tmpdir/bad" "lamusica-cli 0.1.0 commit=bad dirty=maybe buildDate=today"
  if verify_binary bad "$tmpdir/bad" 2>/dev/null; then
    echo "verify-provenance self-test failed to reject malformed version output" >&2
    exit 1
  fi
  make_fake_binary "$tmpdir/dirty" "lamusica-cli 0.1.0 commit=${expected_commit:-123456789abc} dirty=true buildDate=2026-05-29T00:00:00Z"
  if verify_binary dirty "$tmpdir/dirty" 2>/dev/null; then
    echo "verify-provenance self-test failed to reject dirty release provenance" >&2
    exit 1
  fi
  allow_dirty=true
  verify_binary dirty-allowed "$tmpdir/dirty" || exit 1
  allow_dirty=false
  make_fake_binary "$tmpdir/wrong-commit" "lamusica-cli 0.1.0 commit=000000000000 dirty=false buildDate=2026-05-29T00:00:00Z"
  if [[ -n "$expected_commit" ]] && verify_binary wrong-commit "$tmpdir/wrong-commit" \
    2>/dev/null; then
    echo "verify-provenance self-test failed to reject a commit that does not match HEAD" >&2
    exit 1
  fi
  echo "verify-provenance self-test passed"
  exit 0
fi

if [[ -z "$app" || -z "$mcpd" || -z "$cli" ]]; then
  usage >&2
  exit 2
fi

verify_binary LaMusica "$app" || exit 1
verify_binary lamusica_mcpd "$mcpd" || exit 1
verify_binary lamusica_cli "$cli" || exit 1

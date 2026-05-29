#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --binary path --dsym path.dSYM [--symbol main] [--expect main] [--arch arm64] [--dwarfdump 'xcrun dwarfdump'] [--atos 'xcrun atos'] [--self-test]"
}

binary=""
dsym=""
symbol="main"
expect=""
arch="arm64"
dwarfdump_cmd="xcrun dwarfdump"
atos_cmd="xcrun atos"
self_test=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --binary)
      binary="${2:-}"
      shift 2
      ;;
    --dsym)
      dsym="${2:-}"
      shift 2
      ;;
    --symbol)
      symbol="${2:-}"
      shift 2
      ;;
    --expect)
      expect="${2:-}"
      shift 2
      ;;
    --arch)
      arch="${2:-}"
      shift 2
      ;;
    --dwarfdump)
      dwarfdump_cmd="${2:-}"
      shift 2
      ;;
    --atos)
      atos_cmd="${2:-}"
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

validate_dsym_payload() {
  local candidate="$1"
  if [[ ! -d "$candidate" ]]; then
    echo "missing dSYM bundle: $candidate" >&2
    return 1
  fi
  local dwarf_dir="${candidate}/Contents/Resources/DWARF"
  if [[ ! -d "$dwarf_dir" ]]; then
    echo "dSYM is missing DWARF payload directory: $dwarf_dir" >&2
    return 1
  fi
  local dwarf_file
  dwarf_file="$(find "$dwarf_dir" -type f -print -quit)"
  if [[ -z "$dwarf_file" || ! -s "$dwarf_file" ]]; then
    echo "dSYM has no non-empty DWARF payload: $candidate" >&2
    return 1
  fi
}

first_word() {
  local command_line="$1"
  read -r word _ <<< "$command_line"
  printf '%s' "$word"
}

extract_low_pc() {
  sed -nE 's/.*DW_AT_low_pc[^(]*\(0x([0-9A-Fa-f]+)\).*/0x\1/p' | head -n 1
}

verify_symbolication() {
  local expected="${expect:-$symbol}"
  if [[ -z "$binary" || -z "$dsym" || -z "$symbol" || -z "$arch" ]]; then
    usage >&2
    return 2
  fi
  if [[ ! -f "$binary" ]]; then
    echo "missing binary for symbolication: $binary" >&2
    return 1
  fi
  validate_dsym_payload "$dsym" || return 1

  local dwarfdump_launcher atos_launcher
  dwarfdump_launcher="$(first_word "$dwarfdump_cmd")"
  atos_launcher="$(first_word "$atos_cmd")"
  if ! command -v "$dwarfdump_launcher" >/dev/null 2>&1; then
    echo "$dwarfdump_launcher is required for symbol lookup" >&2
    return 1
  fi
  if ! command -v "$atos_launcher" >/dev/null 2>&1; then
    echo "$atos_launcher is required for address symbolication" >&2
    return 1
  fi

  local lookup_output address symbolicated resolved_symbol
  read -r -a dwarfdump_args <<< "$dwarfdump_cmd"
  lookup_output="$("${dwarfdump_args[@]}" --name "$symbol" "$dsym")"
  address="$(printf '%s\n' "$lookup_output" | extract_low_pc)"
  if [[ -z "$address" ]]; then
    echo "could not find DW_AT_low_pc for symbol $symbol in $dsym" >&2
    return 1
  fi

  read -r -a atos_args <<< "$atos_cmd"
  symbolicated="$("${atos_args[@]}" -o "$binary" -arch "$arch" "$address")"
  resolved_symbol="$(sed -E 's/[[:space:](].*$//' <<< "$symbolicated")"
  if [[ "$symbolicated" == *"??"* || "$resolved_symbol" != "$expected" ]]; then
    echo "symbolication did not resolve $address to $expected: $symbolicated" >&2
    return 1
  fi
  echo "symbolication ok: $address -> $symbolicated"
}

if [[ "$self_test" == true ]]; then
  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT
  binary="${tmpdir}/lamusica_cli"
  dsym="${tmpdir}/lamusica_cli.dSYM"
  mkdir -p "${dsym}/Contents/Resources/DWARF" "${tmpdir}/bin"
  printf 'binary\n' > "$binary"
  printf 'debug-symbols\n' > "${dsym}/Contents/Resources/DWARF/lamusica_cli"
  cat > "${tmpdir}/bin/fake-dwarfdump" <<'EOF'
#!/usr/bin/env bash
printf '0x0000000100003f80: DW_TAG_subprogram\n'
printf '  DW_AT_low_pc	(0x0000000100003f80)\n'
printf '  DW_AT_name	("main")\n'
EOF
  cat > "${tmpdir}/bin/fake-atos" <<'EOF'
#!/usr/bin/env bash
printf 'main (in lamusica_cli) (main.cpp:42)\n'
EOF
  chmod +x "${tmpdir}/bin/fake-dwarfdump" "${tmpdir}/bin/fake-atos"
  dwarfdump_cmd="${tmpdir}/bin/fake-dwarfdump"
  atos_cmd="${tmpdir}/bin/fake-atos"
  symbol="main"
  expect="main"
  arch="arm64"
  verify_symbolication >/dev/null
  missing_binary="${tmpdir}/missing-lamusica_cli"
  binary="$missing_binary"
  if verify_symbolication >/dev/null 2>&1; then
    echo "verify-symbolication self-test failed to reject missing binary" >&2
    exit 1
  fi
  binary="${tmpdir}/lamusica_cli"
  cat > "${tmpdir}/bin/fake-atos-unresolved" <<'EOF'
#!/usr/bin/env bash
printf '??\n'
EOF
  chmod +x "${tmpdir}/bin/fake-atos-unresolved"
  atos_cmd="${tmpdir}/bin/fake-atos-unresolved"
  if verify_symbolication >/dev/null 2>&1; then
    echo "verify-symbolication self-test failed to reject unresolved atos output" >&2
    exit 1
  fi
  atos_cmd="${tmpdir}/bin/fake-atos"
  cat > "${tmpdir}/bin/fake-atos-wrong-symbol" <<'EOF'
#!/usr/bin/env bash
printf 'not_main (in lamusica_cli) (other.cpp:9)\n'
EOF
  chmod +x "${tmpdir}/bin/fake-atos-wrong-symbol"
  atos_cmd="${tmpdir}/bin/fake-atos-wrong-symbol"
  if verify_symbolication >/dev/null 2>&1; then
    echo "verify-symbolication self-test failed to reject wrong symbolication target" >&2
    exit 1
  fi
  atos_cmd="${tmpdir}/bin/fake-atos"
  cat > "${tmpdir}/bin/fake-atos-prefix-symbol" <<'EOF'
#!/usr/bin/env bash
printf 'mainly (in lamusica_cli) (other.cpp:10)\n'
EOF
  chmod +x "${tmpdir}/bin/fake-atos-prefix-symbol"
  atos_cmd="${tmpdir}/bin/fake-atos-prefix-symbol"
  if verify_symbolication >/dev/null 2>&1; then
    echo "verify-symbolication self-test failed to reject prefix-only symbolication target" >&2
    exit 1
  fi
  atos_cmd="${tmpdir}/bin/fake-atos"
  cat > "${tmpdir}/bin/fake-dwarfdump-missing-low-pc" <<'EOF'
#!/usr/bin/env bash
printf '0x0000000100003f80: DW_TAG_subprogram\n'
printf '  DW_AT_name	("main")\n'
EOF
  chmod +x "${tmpdir}/bin/fake-dwarfdump-missing-low-pc"
  dwarfdump_cmd="${tmpdir}/bin/fake-dwarfdump-missing-low-pc"
  if verify_symbolication >/dev/null 2>&1; then
    echo "verify-symbolication self-test failed to reject missing DW_AT_low_pc" >&2
    exit 1
  fi
  dwarfdump_cmd="${tmpdir}/bin/fake-dwarfdump"
  rm -f "${dsym}/Contents/Resources/DWARF/lamusica_cli"
  if verify_symbolication >/dev/null 2>&1; then
    echo "verify-symbolication self-test failed to reject empty dSYM payload" >&2
    exit 1
  fi
  echo "verify-symbolication self-test passed"
  exit 0
fi

verify_symbolication

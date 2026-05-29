#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: $0 --macos macos-release-evidence.md --voiceover accessibility-voiceover-evidence.md [--self-test]"
}

macos_file=""
voiceover_file=""
self_test=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --macos)
      macos_file="${2:-}"
      shift 2
      ;;
    --voiceover)
      voiceover_file="${2:-}"
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

require_file() {
  local file="$1"
  if [[ -z "$file" || ! -s "$file" ]]; then
    echo "release evidence file is missing or empty: $file" >&2
    return 1
  fi
}

require_term() {
  local file="$1"
  local term="$2"
  if ! grep -Fq "$term" "$file"; then
    echo "$file is missing required evidence field: $term" >&2
    return 1
  fi
}

require_field_regex() {
  local file="$1"
  local field="$2"
  local pattern="$3"
  if ! grep -Eq "^[[:space:]]*[-*]?[[:space:]]*${field}:[[:space:]]*${pattern}[[:space:]]*$" "$file"; then
    echo "$file has invalid or missing ${field} value" >&2
    return 1
  fi
}

field_value() {
  local file="$1"
  local field="$2"
  awk -v field="$field" '
    $0 ~ "^[[:space:]]*[-*]?[[:space:]]*" field ":[[:space:]]*" {
      sub("^[[:space:]]*[-*]?[[:space:]]*" field ":[[:space:]]*", "")
      sub("[[:space:]]*$", "")
      print
      exit
    }
  ' "$file"
}

require_field_value_regex() {
  local file="$1"
  local field="$2"
  local pattern="$3"
  local value
  value="$(field_value "$file" "$field")"
  if [[ -z "$value" ]] || ! grep -Eq "^${pattern}$" <<<"$value"; then
    echo "$file has invalid or missing ${field} value" >&2
    return 1
  fi
}

require_prefixed_field_value_regex() {
  local file="$1"
  local field_prefix="$2"
  local pattern="$3"
  if ! grep -Eq "^[[:space:]]*[-*]?[[:space:]]*${field_prefix}[^:]*:[[:space:]]*${pattern}[[:space:]]*$" "$file"; then
    echo "$file has invalid or missing ${field_prefix} value" >&2
    return 1
  fi
}

require_matching_field() {
  local left_file="$1"
  local right_file="$2"
  local field="$3"
  local left_value
  local right_value
  left_value="$(field_value "$left_file" "$field")"
  right_value="$(field_value "$right_file" "$field")"
  if [[ -z "$left_value" || -z "$right_value" || "$left_value" != "$right_value" ]]; then
    echo "completed macOS and VoiceOver evidence disagree on ${field}" >&2
    return 1
  fi
}

require_artifact_match() {
  local macos_file="$1"
  local voiceover_file="$2"
  local release_version
  local voiceover_artifact
  release_version="$(field_value "$macos_file" "Release version")"
  voiceover_artifact="$(field_value "$voiceover_file" "Artifact name")"
  if [[ -z "$voiceover_artifact" ]]; then
    echo "$voiceover_file has invalid or missing Artifact name value" >&2
    return 1
  fi
  if [[ "$voiceover_artifact" != "LaMusica-${release_version}-Darwin.dmg" ]]; then
    echo "completed VoiceOver artifact does not match release version ${release_version}: $voiceover_artifact" >&2
    return 1
  fi
  if ! grep -Fq "$voiceover_artifact" "$macos_file"; then
    echo "completed VoiceOver artifact is not listed in macOS release evidence: $voiceover_artifact" >&2
    return 1
  fi
}

require_universal_lipo_row() {
  local file="$1"
  local binary="$2"
  if ! awk -F'|' -v binary="$binary" '
    function trim(value) {
      sub(/^[[:space:]]+/, "", value)
      sub(/[[:space:]]+$/, "", value)
      return value
    }
    BEGIN { found = 0; ok = 0 }
    NF >= 4 && trim($2) == binary {
      found = 1
      archs = tolower($3)
      if (archs ~ /(^|[^[:alnum:]_])arm64([^[:alnum:]_]|$)/ &&
          archs ~ /(^|[^[:alnum:]_])x86_64([^[:alnum:]_]|$)/) {
        ok = 1
      }
    }
    END { exit(found && ok ? 0 : 1) }
  ' "$file"; then
    echo "$file is missing universal arm64+x86_64 lipo evidence for ${binary}" >&2
    return 1
  fi
}

reject_unfilled_placeholders() {
  local file="$1"
  if grep -Eiq '(^|[^[:alpha:]])(pass/fail|yes/no)([^[:alpha:]]|$)' "$file"; then
    echo "$file still contains pass/fail or yes/no placeholders" >&2
    return 1
  fi
  if grep -Eiq '(^|[^[:alnum:]])(tbd|todo|to be completed|to-be-completed|pending evidence)([^[:alnum:]]|$)' "$file"; then
    echo "$file still contains unresolved evidence placeholders" >&2
    return 1
  fi
  if grep -Eiq '(^|[^[:alnum:]])(n/a|not applicable|not run|not tested|skipped)([^[:alnum:]]|$)' "$file"; then
    echo "$file still contains skipped or not-run evidence placeholders" >&2
    return 1
  fi
  if grep -Eiq '(^|[|:])[[:space:]]*(fail|failed|error|denied|rejected|blocked)[[:space:]]*([|#[:space:]]|$)' "$file"; then
    echo "$file contains explicit failing release evidence results" >&2
    return 1
  fi
  if grep -Eq '^- [^:]+:[[:space:]]*$' "$file"; then
    echo "$file still contains empty bullet fields" >&2
    return 1
  fi
  if grep -Eq '(^|[^`])\|[[:space:]]*\|' "$file"; then
    echo "$file still contains empty table cells" >&2
    return 1
  fi
}

validate_macos_evidence() {
  local file="$1"
  require_file "$file" || return 1
  for term in \
    "Release version:" \
    "Git commit:" \
    "CI run:" \
    "Xcode version:" \
    "Artifact names:" \
    "Signing identity:" \
    "Notarization request id:" \
    "LaMusica.app/Contents/MacOS/LaMusica" \
    "lamusica_plugin_scan_worker" \
    "lamusica_mcpd" \
    "lamusica_cli" \
    "codesign --verify --strict --deep" \
    "codesign -d --entitlements" \
    "spctl --assess" \
    "xcrun notarytool submit --wait" \
    "xcrun stapler validate" \
    "cmake -DPACKAGE=" \
    "Online launch from" \
    "Offline launch from" \
    "Tester:" \
    "Date:" \
    "macOS version:" \
    "Hardware:" \
    "Fresh user account name:" \
    "No Gatekeeper override required" \
    "First record attempt triggered microphone TCC prompt" \
    "Bundled CLI tools ran from package" \
    "dSYMs archived" \
    "atos" \
    "llvm-symbolizer" \
    "DAW induced crash produced local report" \
    "lamusica_mcpd induced crash produced local report" \
    "Diagnostics upload stayed disabled without consent" \
    "DMG" \
    "dSYMs archive" \
    "SBOM" \
    "SHA256SUMS" \
    "SHA256SUMS.sig" \
    "Blocking failures:" \
    "Non-blocking observations:" \
    "Follow-up issue links:" \
    "Release approved by:"; do
    require_term "$file" "$term" || return 1
  done
  require_field_regex "$file" "Release version" "[0-9]+\\.[0-9]+\\.[0-9]+" || return 1
  require_field_regex "$file" "Git commit" "[0-9a-f]{12,40}" || return 1
  local release_version
  local release_version_pattern
  release_version="$(field_value "$file" "Release version")"
  release_version_pattern="${release_version//./\\.}"
  require_field_value_regex "$file" "CI run" "(https?://[^[:space:]]+|[^[:space:]]+/[^[:space:]]+)" ||
    return 1
  require_field_value_regex "$file" "Xcode version" "[0-9]+(\\.[0-9]+){1,2}.*" || return 1
  require_field_value_regex "$file" "Artifact names" ".*LaMusica-${release_version_pattern}-Darwin\\.dmg.*" ||
    return 1
  require_universal_lipo_row "$file" "LaMusica.app/Contents/MacOS/LaMusica" || return 1
  require_universal_lipo_row "$file" "lamusica_plugin_scan_worker" || return 1
  require_universal_lipo_row "$file" "lamusica_mcpd" || return 1
  require_universal_lipo_row "$file" "lamusica_cli" || return 1
  require_field_regex "$file" "Signing identity" "Developer ID Application: .+ \\([A-Z0-9]{10}\\)" ||
    return 1
  require_field_regex "$file" "Notarization request id" "[A-Za-z0-9._:-]+" || return 1
  require_field_value_regex "$file" "Tester" ".+" || return 1
  require_field_value_regex "$file" "Date" "[0-9]{4}-[0-9]{2}-[0-9]{2}" || return 1
  require_field_value_regex "$file" "macOS version" ".+" || return 1
  require_field_value_regex "$file" "Hardware" ".+" || return 1
  require_field_value_regex "$file" "Fresh user account name" ".+" || return 1
  require_prefixed_field_value_regex "$file" "Online launch from" ".+" || return 1
  require_prefixed_field_value_regex "$file" "Offline launch from" ".+" || return 1
  require_field_value_regex "$file" "No Gatekeeper override required" ".+" || return 1
  require_field_value_regex "$file" "First record attempt triggered microphone TCC prompt" ".+" ||
    return 1
  require_prefixed_field_value_regex "$file" "Bundled CLI tools ran from package" ".+" || return 1
  require_field_value_regex "$file" "Blocking failures" ".+" || return 1
  require_field_value_regex "$file" "Non-blocking observations" ".+" || return 1
  require_field_value_regex "$file" "Follow-up issue links" ".+" || return 1
  require_field_value_regex "$file" "Release approved by" ".+" || return 1
  reject_unfilled_placeholders "$file"
}

validate_voiceover_evidence() {
  local file="$1"
  require_file "$file" || return 1
  for term in \
    "Release version:" \
    "Git commit:" \
    "Artifact name:" \
    "Signing identity:" \
    "Notarization request id:" \
    "Stapled artifact validated:" \
    "Tester:" \
    "Date:" \
    "macOS version:" \
    "Hardware:" \
    "VoiceOver enabled:" \
    "Full Keyboard Access enabled:" \
    "Reduce Motion tested:" \
    "Increase Contrast tested:" \
    "lamusica_daw_accessibility_audit" \
    "Result:" \
    "Log or CI run:" \
    "Transport play/stop" \
    "Record/arm/monitor controls" \
    "Mixer fader" \
    "Pan control" \
    "Meter" \
    "Timeline clip" \
    "Time ruler or playhead" \
    "Piano-roll note" \
    "Drum pad or step cell" \
    "Browser tree" \
    "Inspector fields" \
    "Plugin chooser/control" \
    "Export dialog" \
    "Welcome/templates" \
    "Guided tour" \
    "Start and stop transport" \
    "Arm a track and toggle monitoring" \
    "Select and edit a timeline clip" \
    "Change a mixer fader value" \
    "Inspect a plugin control" \
    "Cancel and confirm export" \
    "Choose an onboarding template" \
    "Restart and skip guided tour" \
    "Completed without mouse" \
    "VoiceOver evidence" \
    "Blocking failures:" \
    "Non-blocking observations:" \
    "Follow-up issue links:" \
    "Release approved by:"; do
    require_term "$file" "$term" || return 1
  done
  require_field_regex "$file" "Release version" "[0-9]+\\.[0-9]+\\.[0-9]+" || return 1
  require_field_regex "$file" "Git commit" "[0-9a-f]{12,40}" || return 1
  local release_version
  local release_version_pattern
  release_version="$(field_value "$file" "Release version")"
  release_version_pattern="${release_version//./\\.}"
  require_field_value_regex "$file" "Artifact name" "LaMusica-${release_version_pattern}-Darwin\\.dmg" ||
    return 1
  require_field_regex "$file" "Signing identity" "Developer ID Application: .+ \\([A-Z0-9]{10}\\)" ||
    return 1
  require_field_regex "$file" "Notarization request id" "[A-Za-z0-9._:-]+" || return 1
  require_field_value_regex "$file" "Stapled artifact validated" ".+" || return 1
  require_field_value_regex "$file" "Tester" ".+" || return 1
  require_field_value_regex "$file" "Date" "[0-9]{4}-[0-9]{2}-[0-9]{2}" || return 1
  require_field_value_regex "$file" "macOS version" ".+" || return 1
  require_field_value_regex "$file" "Hardware" ".+" || return 1
  require_field_value_regex "$file" "VoiceOver enabled" ".+" || return 1
  require_field_value_regex "$file" "Full Keyboard Access enabled" ".+" || return 1
  require_field_value_regex "$file" "Reduce Motion tested" ".+" || return 1
  require_field_value_regex "$file" "Increase Contrast tested" ".+" || return 1
  require_field_value_regex "$file" "Result" ".+" || return 1
  require_field_value_regex "$file" "Log or CI run" "(https?://[^[:space:]]+|[^[:space:]]+/[^[:space:]]+)" ||
    return 1
  require_field_value_regex "$file" "Blocking failures" ".+" || return 1
  require_field_value_regex "$file" "Non-blocking observations" ".+" || return 1
  require_field_value_regex "$file" "Follow-up issue links" ".+" || return 1
  require_field_value_regex "$file" "Release approved by" ".+" || return 1
  reject_unfilled_placeholders "$file"
}

validate_evidence_pair() {
  local macos_file="$1"
  local voiceover_file="$2"
  validate_macos_evidence "$macos_file" || return 1
  validate_voiceover_evidence "$voiceover_file" || return 1
  require_matching_field "$macos_file" "$voiceover_file" "Release version" || return 1
  require_matching_field "$macos_file" "$voiceover_file" "Git commit" || return 1
  require_matching_field "$macos_file" "$voiceover_file" "Signing identity" || return 1
  require_matching_field "$macos_file" "$voiceover_file" "Notarization request id" || return 1
  require_artifact_match "$macos_file" "$voiceover_file" || return 1
}

if [[ "$self_test" == true ]]; then
  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT
  macos_file="$tmpdir/macos.md"
  voiceover_file="$tmpdir/voiceover.md"
  cat > "$macos_file" <<'EOF'
# macOS Release Evidence
- Release version: 0.1.0
- Git commit: 123456789abc
- CI run: https://github.com/example/LaMusica/actions/runs/123456789
- Xcode version: 15.4
- Artifact names: LaMusica-0.1.0-Darwin.dmg, LaMusica-dSYMs.tar.gz
- Signing identity: Developer ID Application: LaMusica (ABCDE12345)
- Notarization request id: notary-123
| Binary | lipo -archs output | Result |
| --- | --- | --- |
| LaMusica.app/Contents/MacOS/LaMusica | arm64 x86_64 | pass |
| lamusica_plugin_scan_worker | arm64 x86_64 | pass |
| lamusica_mcpd | arm64 x86_64 | pass |
| lamusica_cli | arm64 x86_64 | pass |
| Gate | Command or evidence | Result | Notes |
| --- | --- | --- | --- |
| App signature | codesign --verify --strict --deep LaMusica.app | pass | verified |
| Entitlements | codesign -d --entitlements :- LaMusica.app | pass | mic and Apple Events |
| Gatekeeper | spctl --assess LaMusica.app | pass | accepted |
| Notarization | xcrun notarytool submit --wait result | pass | accepted |
| Stapling | xcrun stapler validate LaMusica-0.1.0-Darwin.dmg | pass | accepted |
| Package verifier | cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake | pass | verified |
- Tester: Release Tester
- Date: 2026-05-29
- macOS version: 14.5
- Hardware: Mac mini M2
- Fresh user account name: lamusica-release-test
- Online launch from /Applications: pass
- Offline launch from /Applications: pass
- No Gatekeeper override required: confirmed
- First record attempt triggered microphone TCC prompt: confirmed
- Bundled CLI tools ran from package bin: confirmed
| Gate | Evidence | Result | Notes |
| --- | --- | --- | --- |
| dSYMs archived | LaMusica-dSYMs.tar.gz | pass | archived |
| atos/llvm-symbolizer resolved known address | main | pass | resolved |
| DAW induced crash produced local report | local report | pass | no upload |
| lamusica_mcpd induced crash produced local report | local report | pass | no upload |
| Diagnostics upload stayed disabled without consent | local report only | pass | disabled |
| Asset | SHA-256 row present | Included in SBOM | Uploaded |
| --- | --- | --- | --- |
| DMG | present | present | uploaded |
| dSYMs archive | present | present | uploaded |
| SBOM | present | present | uploaded |
| SHA256SUMS | present | present | uploaded |
| SHA256SUMS.sig | present | present | uploaded |
- Blocking failures: None
- Non-blocking observations: None
- Follow-up issue links: None
- Release approved by: Release Manager
EOF
  cat > "$voiceover_file" <<'EOF'
# Accessibility VoiceOver Evidence
- Release version: 0.1.0
- Git commit: 123456789abc
- Artifact name: LaMusica-0.1.0-Darwin.dmg
- Signing identity: Developer ID Application: LaMusica (ABCDE12345)
- Notarization request id: notary-123
- Stapled artifact validated: confirmed
- Tester: Accessibility Tester
- Date: 2026-05-29
- macOS version: 14.5
- Hardware: Mac mini M2
- VoiceOver enabled: enabled
- Full Keyboard Access enabled: enabled
- Reduce Motion tested: pass
- Increase Contrast tested: pass
- Command: ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure
- Result: passed
- Log or CI run: https://github.com/example/LaMusica/actions/runs/123456789
| Surface | Role/name/value evidence | Result | Notes |
| --- | --- | --- | --- |
| Transport play/stop | button named Play, value stopped | pass | clear |
| Record/arm/monitor controls | toggle named Record arm | pass | announces armed |
| Mixer fader | slider named Master volume | pass | dB value |
| Pan control | slider named Master pan | pass | centered |
| Meter | meter named Master peak | pass | dB value |
| Timeline clip | item named Clip 1 | pass | selected |
| Time ruler or playhead | ruler announces bar 1 beat 1 | pass | current position |
| Piano-roll note | note C4 | pass | duration |
| Drum pad or step cell | toggle named Kick step 1 | pass | row and column |
| Browser tree | tree named Browser | pass | expanded |
| Inspector fields | text field named Clip name | pass | editable |
| Plugin chooser/control | button named Add plugin | pass | opens |
| Export dialog | dialog named Export | pass | focus trapped |
| Welcome/templates | list named Templates | pass | template selectable |
| Guided tour | button named Skip tour | pass | dismissible |
| Workflow | Completed without mouse | VoiceOver evidence | Notes |
| --- | --- | --- | --- |
| Start and stop transport | completed | announced states | pass |
| Arm a track and toggle monitoring | completed | announced toggles | pass |
| Select and edit a timeline clip | completed | announced selection | pass |
| Change a mixer fader value | completed | announced dB value | pass |
| Inspect a plugin control | completed | announced role/name/value | pass |
| Cancel and confirm export | completed | announced dialog actions | pass |
| Choose an onboarding template | completed | announced template | pass |
| Restart and skip guided tour | completed | announced tour controls | pass |
- Blocking failures: None
- Non-blocking observations: None
- Follow-up issue links: None
- Release approved by: Accessibility Reviewer
EOF
  validate_evidence_pair "$macos_file" "$voiceover_file"
  cp docs/release/macos-release-evidence-template.md "$tmpdir/blank-macos.md"
  if validate_macos_evidence "$tmpdir/blank-macos.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject blank macOS template" >&2
    exit 1
  fi
  cp docs/release/accessibility-voiceover-evidence-template.md "$tmpdir/blank-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/blank-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject blank VoiceOver template" >&2
    exit 1
  fi
  printf '%s\n' "# macOS Release Evidence" "- Release version:" > "$tmpdir/incomplete.md"
  if validate_macos_evidence "$tmpdir/incomplete.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject incomplete evidence" >&2
    exit 1
  fi
  cp "$macos_file" "$tmpdir/bad-identity-macos.md"
  awk '{ gsub(/Developer ID Application: LaMusica \(ABCDE12345\)/, "Developer ID Application: LaMusica (TEAMID)"); print }' \
    "$tmpdir/bad-identity-macos.md" > "$tmpdir/bad-identity-macos.tmp"
  mv "$tmpdir/bad-identity-macos.tmp" "$tmpdir/bad-identity-macos.md"
  if validate_macos_evidence "$tmpdir/bad-identity-macos.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject malformed signing identity evidence" >&2
    exit 1
  fi
  cp "$voiceover_file" "$tmpdir/bad-commit-voiceover.md"
  awk '{ gsub(/Git commit: 123456789abc/, "Git commit: unknown"); print }' \
    "$tmpdir/bad-commit-voiceover.md" > "$tmpdir/bad-commit-voiceover.tmp"
  mv "$tmpdir/bad-commit-voiceover.tmp" "$tmpdir/bad-commit-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/bad-commit-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject malformed VoiceOver commit evidence" >&2
    exit 1
  fi
  cp "$voiceover_file" "$tmpdir/bad-identity-voiceover.md"
  awk '{ gsub(/Developer ID Application: LaMusica \(ABCDE12345\)/, "Developer ID Application: LaMusica (TEAMID)"); print }' \
    "$tmpdir/bad-identity-voiceover.md" > "$tmpdir/bad-identity-voiceover.tmp"
  mv "$tmpdir/bad-identity-voiceover.tmp" "$tmpdir/bad-identity-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/bad-identity-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject malformed VoiceOver signing identity evidence" >&2
    exit 1
  fi
  cp "$voiceover_file" "$tmpdir/bad-notary-voiceover.md"
  awk '{ gsub(/Notarization request id: notary-123/, "Notarization request id: "); print }' \
    "$tmpdir/bad-notary-voiceover.md" > "$tmpdir/bad-notary-voiceover.tmp"
  mv "$tmpdir/bad-notary-voiceover.tmp" "$tmpdir/bad-notary-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/bad-notary-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject malformed VoiceOver notarization evidence" >&2
    exit 1
  fi
  awk '{ gsub(/Artifact names: LaMusica-0.1.0-Darwin.dmg, LaMusica-dSYMs.tar.gz/, "Artifact names: LaMusica-0.1.0-Darwin.zip, LaMusica-dSYMs.tar.gz"); print }' \
    "$macos_file" > "$tmpdir/mismatched-artifact.md"
  if validate_evidence_pair "$tmpdir/mismatched-artifact.md" "$voiceover_file" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject mismatched macOS/VoiceOver artifact evidence" >&2
    exit 1
  fi
  awk '{ gsub(/Git commit: 123456789abc/, "Git commit: 123456789abd"); print }' \
    "$voiceover_file" > "$tmpdir/mismatched-commit.md"
  if validate_evidence_pair "$macos_file" "$tmpdir/mismatched-commit.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject mismatched macOS/VoiceOver commit evidence" >&2
    exit 1
  fi
  awk '{ gsub(/Artifact name: LaMusica-0.1.0-Darwin.dmg/, "Artifact name: LaMusica-0.2.0-Darwin.dmg"); print }' \
    "$voiceover_file" > "$tmpdir/wrong-artifact-version.md"
  if validate_voiceover_evidence "$tmpdir/wrong-artifact-version.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject artifact version mismatch" >&2
    exit 1
  fi
  awk '{ gsub(/lamusica_mcpd \\| arm64 x86_64/, "lamusica_mcpd | arm64"); print }' \
    "$macos_file" > "$tmpdir/missing-universal-arch.md"
  if validate_macos_evidence "$tmpdir/missing-universal-arch.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject missing universal lipo evidence" >&2
    exit 1
  fi
  awk '{ gsub(/Date: 2026-05-29/, "Date: 05-29-2026"); print }' \
    "$macos_file" > "$tmpdir/bad-date-macos.md"
  if validate_macos_evidence "$tmpdir/bad-date-macos.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject malformed macOS date evidence" >&2
    exit 1
  fi
  awk '{ gsub(/CI run: https:\/\/github.com\/example\/LaMusica\/actions\/runs\/123456789/, "CI run: release workflow passed"); print }' \
    "$macos_file" > "$tmpdir/vague-macos-ci.md"
  if validate_macos_evidence "$tmpdir/vague-macos-ci.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject vague macOS CI evidence" >&2
    exit 1
  fi
  awk '{ gsub(/Log or CI run: https:\/\/github.com\/example\/LaMusica\/actions\/runs\/123456789/, "Log or CI run: "); print }' \
    "$voiceover_file" > "$tmpdir/missing-voiceover-ci.md"
  if validate_voiceover_evidence "$tmpdir/missing-voiceover-ci.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject missing VoiceOver CI evidence" >&2
    exit 1
  fi
  awk '{ gsub(/Log or CI run: https:\/\/github.com\/example\/LaMusica\/actions\/runs\/123456789/, "Log or CI run: accessibility passed"); print }' \
    "$voiceover_file" > "$tmpdir/vague-voiceover-ci.md"
  if validate_voiceover_evidence "$tmpdir/vague-voiceover-ci.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject vague VoiceOver CI evidence" >&2
    exit 1
  fi
  awk '{ gsub(/Result: passed/, "Result: "); print }' \
    "$voiceover_file" > "$tmpdir/missing-voiceover-result.md"
  if validate_voiceover_evidence "$tmpdir/missing-voiceover-result.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject missing VoiceOver automated result evidence" >&2
    exit 1
  fi
  awk '{ gsub(/No Gatekeeper override required: confirmed/, "No Gatekeeper override required: "); print }' \
    "$macos_file" > "$tmpdir/missing-gatekeeper-confirmation.md"
  if validate_macos_evidence "$tmpdir/missing-gatekeeper-confirmation.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject missing Gatekeeper launch confirmation" >&2
    exit 1
  fi
  awk '{ gsub(/Stapled artifact validated: confirmed/, "Stapled artifact validated: "); print }' \
    "$voiceover_file" > "$tmpdir/missing-stapled-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/missing-stapled-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject missing VoiceOver stapled artifact validation" >&2
    exit 1
  fi
  awk '{ gsub(/VoiceOver enabled: enabled/, "VoiceOver enabled: "); print }' \
    "$voiceover_file" > "$tmpdir/missing-voiceover-enabled.md"
  if validate_voiceover_evidence "$tmpdir/missing-voiceover-enabled.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject missing VoiceOver enabled evidence" >&2
    exit 1
  fi
  cp "$macos_file" "$tmpdir/tbd-macos.md"
  printf '%s\n' "- Non-blocking observations: TBD" >> "$tmpdir/tbd-macos.md"
  if validate_macos_evidence "$tmpdir/tbd-macos.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject unresolved macOS evidence placeholders" >&2
    exit 1
  fi
  cp "$voiceover_file" "$tmpdir/todo-voiceover.md"
  printf '%s\n' "- Follow-up issue links: TODO" >> "$tmpdir/todo-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/todo-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject unresolved VoiceOver evidence placeholders" >&2
    exit 1
  fi
  cp "$macos_file" "$tmpdir/not-run-macos.md"
  printf '%s\n' "- Clean account retest: not run" >> "$tmpdir/not-run-macos.md"
  if validate_macos_evidence "$tmpdir/not-run-macos.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject not-run macOS evidence placeholders" >&2
    exit 1
  fi
  cp "$voiceover_file" "$tmpdir/skipped-voiceover.md"
  printf '%s\n' "- Manual pass retest: skipped" >> "$tmpdir/skipped-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/skipped-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject skipped VoiceOver evidence placeholders" >&2
    exit 1
  fi
  cp "$macos_file" "$tmpdir/failing-macos.md"
  printf '%s\n' "- Offline launch from /Applications: fail" >> "$tmpdir/failing-macos.md"
  if validate_macos_evidence "$tmpdir/failing-macos.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject explicit macOS fail results" >&2
    exit 1
  fi
  cp "$voiceover_file" "$tmpdir/failing-voiceover.md"
  printf '%s\n' "| Extra workflow | completed | fail | regression |" >> "$tmpdir/failing-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/failing-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject explicit VoiceOver fail results" >&2
    exit 1
  fi
  cp "$macos_file" "$tmpdir/error-macos.md"
  printf '%s\n' "| Extra gate | error | release gate returned non-zero |" >> "$tmpdir/error-macos.md"
  if validate_macos_evidence "$tmpdir/error-macos.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject explicit macOS error results" >&2
    exit 1
  fi
  cp "$voiceover_file" "$tmpdir/denied-voiceover.md"
  printf '%s\n' "| Extra workflow | completed | denied | permission missing |" >> "$tmpdir/denied-voiceover.md"
  if validate_voiceover_evidence "$tmpdir/denied-voiceover.md" 2>/dev/null; then
    echo "verify-release-evidence self-test failed to reject explicit VoiceOver denied results" >&2
    exit 1
  fi
  echo "verify-release-evidence self-test passed"
  exit 0
fi

if [[ -z "$macos_file" || -z "$voiceover_file" ]]; then
  usage >&2
  exit 2
fi

validate_evidence_pair "$macos_file" "$voiceover_file" || exit 1
echo "release evidence is complete"

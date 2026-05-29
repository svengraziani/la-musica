#!/usr/bin/env bash
set -euo pipefail

require_workflow_text() {
  local text="$1"
  local workflow="$2"
  if ! grep -Fq -- "$text" "$workflow"; then
    echo "release workflow is missing required text: $text" >&2
    workflow_verify_failed=1
  fi
}

verify_workflow() {
  local workflow="$1"
  workflow_verify_failed=0

  require_workflow_text "workflow_dispatch:" "$workflow"
  require_workflow_text "release_tag:" "$workflow"
  require_workflow_text "required: true" "$workflow"
  require_workflow_text "completed_macos_evidence:" "$workflow"
  require_workflow_text "completed_voiceover_evidence:" "$workflow"
  require_workflow_text "runs-on: macos-14" "$workflow"
  require_workflow_text "contents: write" "$workflow"
  require_workflow_text "ref: \${{ github.event_name == 'workflow_dispatch' && github.event.inputs.release_tag || github.ref }}" "$workflow"
  require_workflow_text "Checkout JUCE" "$workflow"
  require_workflow_text "Dependency lock check" "$workflow"
  require_workflow_text "run: cmake -P cmake/CheckDependencyLock.cmake" "$workflow"
  require_workflow_text "maxim-lobanov/setup-xcode@v1" "$workflow"
  require_workflow_text "xcode-version: '15.4'" "$workflow"
  require_workflow_text "cmake --preset release-universal" "$workflow"
  require_workflow_text "cmake --build --preset release-universal" "$workflow"
  require_workflow_text "ctest --preset release-universal" "$workflow"
  require_workflow_text "cpack -G DragNDrop --config build/macos-release-universal/CPackConfig.cmake" "$workflow"
  require_workflow_text "cpack -G TGZ --config build/macos-release-universal/CPackConfig.cmake" "$workflow"
  require_workflow_text "Validate release secrets" "$workflow"
  require_workflow_text "require_secret LAMUSICA_CODESIGN_CERT_P12_BASE64" "$workflow"
  require_workflow_text "require_secret LAMUSICA_CODESIGN_CERT_PASSWORD" "$workflow"
  require_workflow_text "require_secret LAMUSICA_CODESIGN_IDENTITY" "$workflow"
  require_workflow_text "require_secret LAMUSICA_NOTARY_KEY_ID" "$workflow"
  require_workflow_text "require_secret LAMUSICA_NOTARY_ISSUER_ID" "$workflow"
  require_workflow_text "require_secret LAMUSICA_NOTARY_KEY_P8_BASE64" "$workflow"
  require_workflow_text "security find-identity -v -p codesigning" "$workflow"
  require_workflow_text "Verify universal architectures" "$workflow"
  require_workflow_text "lipo -archs" "$workflow"
  require_workflow_text "missing arm64" "$workflow"
  require_workflow_text "missing x86_64" "$workflow"
  require_workflow_text "Generate dSYMs" "$workflow"
  require_workflow_text "scripts/archive-dsyms.sh" "$workflow"
  require_workflow_text "Verify dSYM symbolication" "$workflow"
  require_workflow_text "scripts/verify-symbolication.sh" "$workflow"
  require_workflow_text "--expect main" "$workflow"
  require_workflow_text "Verify provenance stamping" "$workflow"
  require_workflow_text "scripts/verify-provenance.sh" "$workflow"
  require_workflow_text "Validate release examples" "$workflow"
  require_workflow_text "build/macos-release-universal/tools/cli/lamusica_cli verify-examples fixtures/examples" "$workflow"
  require_workflow_text "build/macos-release-universal/tools/cli/lamusica_cli verify-examples fixtures/tutorials" "$workflow"
  require_workflow_text "Sign binaries" "$workflow"
  require_workflow_text "scripts/sign-macos.sh --identity" "$workflow"
  require_workflow_text "Notarize and staple" "$workflow"
  require_workflow_text "scripts/notarize-macos.sh" "$workflow"
  require_workflow_text "--key-id" "$workflow"
  require_workflow_text "--issuer" "$workflow"
  require_workflow_text "Verify release signature" "$workflow"
  require_workflow_text "scripts/verify-signature.sh" "$workflow"
  require_workflow_text "--binary build/macos-release-universal/apps/plugin_scan_worker/lamusica_plugin_scan_worker" "$workflow"
  require_workflow_text "--binary build/macos-release-universal/apps/mcpd/lamusica_mcpd" "$workflow"
  require_workflow_text "--binary build/macos-release-universal/tools/cli/lamusica_cli" "$workflow"
  require_workflow_text "Verify package contents" "$workflow"
  require_workflow_text "cmake -DPACKAGE=\"\$(ls LaMusica-*-Darwin.tar.gz)\" -P cmake/VerifyPackage.cmake" "$workflow"
  require_workflow_text "Archive dSYMs" "$workflow"
  require_workflow_text "LaMusica-dSYMs.tar.gz" "$workflow"
  require_workflow_text "Build SBOM and checksums" "$workflow"
  require_workflow_text "scripts/sbom.sh" "$workflow"
  require_workflow_text "--artifact LaMusica-dSYMs.tar.gz" "$workflow"
  require_workflow_text "Sign checksums" "$workflow"
  require_workflow_text "scripts/sign-checksums.sh --identity" "$workflow"
  require_workflow_text "Release script self-tests" "$workflow"
  require_workflow_text "scripts/archive-dsyms.sh --self-test" "$workflow"
  require_workflow_text "scripts/sign-macos.sh --self-test" "$workflow"
  require_workflow_text "scripts/notarize-macos.sh --self-test" "$workflow"
  require_workflow_text "scripts/verify-signature.sh --self-test" "$workflow"
  require_workflow_text "scripts/sign-checksums.sh --self-test" "$workflow"
  require_workflow_text "scripts/sbom.sh --self-test" "$workflow"
  require_workflow_text "scripts/verify-provenance.sh --self-test" "$workflow"
  require_workflow_text "scripts/verify-release-evidence.sh --self-test" "$workflow"
  require_workflow_text "scripts/verify-release-workflow.sh --self-test" "$workflow"
  require_workflow_text "scripts/verify-ci-workflow.sh --self-test" "$workflow"
  require_workflow_text "scripts/verify-symbolication.sh --self-test" "$workflow"
  require_workflow_text "cmake -DLAMUSICA_DEPENDENCY_LOCK_SELF_TEST=ON -P cmake/CheckDependencyLock.cmake" "$workflow"
  require_workflow_text "cmake -DLAMUSICA_VERIFY_PACKAGE_SELF_TEST=ON -P cmake/VerifyPackage.cmake" "$workflow"
  require_workflow_text "LaMusica-release-candidate" "$workflow"
  require_workflow_text "Validate completed release evidence before publication" "$workflow"
  require_workflow_text "MACOS_EVIDENCE_INPUT: \${{ github.event.inputs.completed_macos_evidence }}" "$workflow"
  require_workflow_text "VOICEOVER_EVIDENCE_INPUT: \${{ github.event.inputs.completed_voiceover_evidence }}" "$workflow"
  require_workflow_text "scripts/verify-release-evidence.sh --macos" "$workflow"
  require_workflow_text "cp \"\$macos_evidence\" build/release-metadata/completed-macos-release-evidence.md" "$workflow"
  require_workflow_text "cp \"\$voiceover_evidence\" build/release-metadata/completed-accessibility-voiceover-evidence.md" "$workflow"
  require_workflow_text "Vulnerability scan" "$workflow"
  require_workflow_text "uses: anchore/scan-action@v4" "$workflow"
  require_workflow_text "path: build/release-metadata/LaMusica.spdx.json" "$workflow"
  require_workflow_text "fail-build: true" "$workflow"
  require_workflow_text "severity-cutoff: critical" "$workflow"
  require_workflow_text "startsWith(github.ref, 'refs/tags/') || github.event_name == 'workflow_dispatch'" "$workflow"
  require_workflow_text "Manual release publication requires a semantic v* release_tag input" "$workflow"
  require_workflow_text "tag_name:" "$workflow"
  require_workflow_text "fail_on_unmatched_files: true" "$workflow"
  require_workflow_text "completed-macos-release-evidence.md" "$workflow"
  require_workflow_text "completed-accessibility-voiceover-evidence.md" "$workflow"
  require_workflow_text "Upload release artifacts" "$workflow"
  require_workflow_text "name: LaMusica-release" "$workflow"

  if [[ "$workflow_verify_failed" -ne 0 ]]; then
    return 1
  fi

  if [[ "$(grep -Fc "if-no-files-found: error" "$workflow")" -lt 2 ]]; then
    echo "release workflow artifact uploads must fail when expected files are missing" >&2
    return 1
  fi

  if ! awk '
  /Checkout JUCE/ { juce = NR }
  /Dependency lock check/ { dependency_lock = NR }
  /Select Xcode/ { xcode = NR }
  /Validate release secrets/ { secrets = NR }
  /Import signing certificate/ { import_signing = NR }
  /Configure universal release/ { configure = NR }
  /Build universal release/ { build = NR }
  /Verify universal architectures/ { arches = NR }
  /Generate dSYMs/ { generate_dsyms = NR }
  /Verify dSYM symbolication/ { symbolication = NR }
  /Verify provenance stamping/ { provenance = NR }
  /Test universal release/ { test = NR }
  /Validate release examples/ { examples = NR }
  /Sign binaries/ { sign = NR }
  /Package DMG/ { dmg = NR }
  /Package verification archive/ { tgz = NR }
  /Notarize and staple/ { notarize = NR }
  /Verify release signature/ { signature = NR }
  /Verify package contents/ { package_verify = NR }
  /Archive dSYMs/ { archive_dsyms = NR }
  /Build SBOM and checksums/ { sbom = NR }
  /Sign checksums/ { checksums = NR }
  /Upload candidate artifacts for manual evidence/ { candidate = NR }
  /Validate completed release evidence before publication/ { validate = NR }
  /Vulnerability scan/ { vulnerability = NR }
  /Publish GitHub release/ { publish = NR }
  /Upload release artifacts/ { final_upload = NR }
  END {
    exit !(juce > 0 &&
            dependency_lock > juce &&
            xcode > dependency_lock &&
            secrets > xcode &&
            import_signing > secrets &&
            configure > import_signing &&
            build > configure &&
            arches > build &&
            generate_dsyms > arches &&
            symbolication > generate_dsyms &&
            provenance > symbolication &&
            test > provenance &&
            examples > test &&
            sign > examples &&
            dmg > sign &&
            tgz > dmg &&
            notarize > tgz &&
            signature > notarize &&
            package_verify > signature &&
            archive_dsyms > package_verify &&
            sbom > archive_dsyms &&
            checksums > sbom &&
            candidate > checksums &&
            validate > candidate &&
            vulnerability > validate &&
            publish > vulnerability &&
            final_upload > publish)
  }
' "$workflow"; then
    echo "release workflow must run release gates in order from secret validation through vulnerability scan before publishing" >&2
    return 1
  fi
}

expect_failure() {
  local label="$1"
  local workflow="$2"
  if verify_workflow "$workflow" >/dev/null 2>&1; then
    echo "release workflow self-test failed: accepted invalid workflow fixture: $label" >&2
    exit 1
  fi
}

self_test() {
  local source_workflow="${1:-.github/workflows/release.yml}"
  local tmp_dir
  tmp_dir="$(mktemp -d)"

  verify_workflow "$source_workflow" >/dev/null

  grep -v "fail_on_unmatched_files: true" "$source_workflow" > "$tmp_dir/no-fail-on-unmatched.yml"
  expect_failure "missing fail_on_unmatched_files" "$tmp_dir/no-fail-on-unmatched.yml"

  grep -v "require_secret LAMUSICA_NOTARY_KEY_P8_BASE64" "$source_workflow" > "$tmp_dir/missing-notary-secret.yml"
  expect_failure "missing notary key secret validation" "$tmp_dir/missing-notary-secret.yml"

  grep -v "scripts/verify-release-workflow.sh --self-test" "$source_workflow" > "$tmp_dir/missing-workflow-self-test.yml"
  expect_failure "missing release workflow self-test" "$tmp_dir/missing-workflow-self-test.yml"

  grep -v "scripts/verify-ci-workflow.sh --self-test" "$source_workflow" > "$tmp_dir/missing-ci-workflow-self-test.yml"
  expect_failure "missing CI workflow self-test" "$tmp_dir/missing-ci-workflow-self-test.yml"

  grep -v "run: cmake -P cmake/CheckDependencyLock.cmake" "$source_workflow" > "$tmp_dir/missing-release-dependency-lock.yml"
  expect_failure "missing release dependency lock check" "$tmp_dir/missing-release-dependency-lock.yml"

  grep -v "build/macos-release-universal/tools/cli/lamusica_cli verify-examples fixtures/examples" "$source_workflow" > "$tmp_dir/missing-release-example-verification.yml"
  expect_failure "missing release example verification" "$tmp_dir/missing-release-example-verification.yml"

  grep -v "completed_voiceover_evidence:" "$source_workflow" > "$tmp_dir/missing-voiceover-evidence-input.yml"
  expect_failure "missing completed VoiceOver evidence input" "$tmp_dir/missing-voiceover-evidence-input.yml"

  grep -v 'cp "$voiceover_evidence" build/release-metadata/completed-accessibility-voiceover-evidence.md' "$source_workflow" > "$tmp_dir/missing-voiceover-evidence-copy.yml"
  expect_failure "missing completed VoiceOver evidence copy" "$tmp_dir/missing-voiceover-evidence-copy.yml"

  grep -v "uses: anchore/scan-action@v4" "$source_workflow" > "$tmp_dir/missing-vulnerability-scan-action.yml"
  expect_failure "missing vulnerability scan action" "$tmp_dir/missing-vulnerability-scan-action.yml"

  grep -v "path: build/release-metadata/LaMusica.spdx.json" "$source_workflow" > "$tmp_dir/missing-vulnerability-sbom-input.yml"
  expect_failure "missing vulnerability scan SBOM input" "$tmp_dir/missing-vulnerability-sbom-input.yml"

  cp "$source_workflow" "$tmp_dir/swapped-publish-scan.yml"
  python3 - "$tmp_dir/swapped-publish-scan.yml" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
sentinel = "__LAMUSICA_TEMP_RELEASE_STEP__"
text = text.replace("- name: Vulnerability scan", f"- name: {sentinel}", 1)
text = text.replace("- name: Publish GitHub release", "- name: Vulnerability scan", 1)
text = text.replace(f"- name: {sentinel}", "- name: Publish GitHub release", 1)
path.write_text(text, encoding="utf-8")
PY
  expect_failure "publication before vulnerability scan" "$tmp_dir/swapped-publish-scan.yml"

  cp "$source_workflow" "$tmp_dir/swapped-sign-notarize.yml"
  python3 - "$tmp_dir/swapped-sign-notarize.yml" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
sentinel = "__LAMUSICA_TEMP_RELEASE_STEP__"
text = text.replace("- name: Sign binaries", f"- name: {sentinel}", 1)
text = text.replace("- name: Notarize and staple", "- name: Sign binaries", 1)
text = text.replace(f"- name: {sentinel}", "- name: Notarize and staple", 1)
path.write_text(text, encoding="utf-8")
PY
  expect_failure "notarization before signing" "$tmp_dir/swapped-sign-notarize.yml"

  cp "$source_workflow" "$tmp_dir/swapped-final-upload-publish.yml"
  python3 - "$tmp_dir/swapped-final-upload-publish.yml" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
sentinel = "__LAMUSICA_TEMP_RELEASE_STEP__"
text = text.replace("- name: Publish GitHub release", f"- name: {sentinel}", 1)
text = text.replace("- name: Upload release artifacts", "- name: Publish GitHub release", 1)
text = text.replace(f"- name: {sentinel}", "- name: Upload release artifacts", 1)
path.write_text(text, encoding="utf-8")
PY
  expect_failure "final upload before GitHub release publication" "$tmp_dir/swapped-final-upload-publish.yml"

  rm -rf "$tmp_dir"
  echo "verify-release-workflow self-test passed"
}

if [[ "${1:-}" == "--self-test" ]]; then
  self_test "${2:-.github/workflows/release.yml}"
  exit 0
fi

workflow="${1:-.github/workflows/release.yml}"
verify_workflow "$workflow"
echo "release workflow verification passed"

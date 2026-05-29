#!/usr/bin/env bash
set -euo pipefail

require_workflow_text() {
  local text="$1"
  local workflow="$2"
  if ! grep -Fq -- "$text" "$workflow"; then
    echo "CI workflow is missing required text: $text" >&2
    ci_workflow_verify_failed=1
  fi
}

verify_workflow() {
  local workflow="$1"
  ci_workflow_verify_failed=0

  require_workflow_text "runs-on: macos-14" "$workflow"
  require_workflow_text "maxim-lobanov/setup-xcode@v1" "$workflow"
  require_workflow_text "xcode-version: '15.4'" "$workflow"
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
  require_workflow_text "ctest --preset debug" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L determinism --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L plugin-hosting --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L audio-correctness --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L behavior --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L cli --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L a11y --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L i18n --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L onboarding --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L privacy --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L perf --output-on-failure" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-debug -L gui --output-on-failure" "$workflow"
  require_workflow_text "build/unix-debug/tools/cli/lamusica_cli benchmark-smoke" "$workflow"
  require_workflow_text "cmake -P cmake/CheckMarkdown.cmake" "$workflow"
  require_workflow_text "cmake -P cmake/CheckDependencyLock.cmake" "$workflow"
  require_workflow_text "cmake --preset asan" "$workflow"
  require_workflow_text "ctest --preset asan" "$workflow"
  require_workflow_text "cmake --preset tsan" "$workflow"
  require_workflow_text "ctest --test-dir build/unix-tsan -L plugin-hosting --output-on-failure" "$workflow"
  require_workflow_text "cmake --preset release" "$workflow"
  require_workflow_text "cmake --build --preset release" "$workflow"
  require_workflow_text "build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/examples" "$workflow"
  require_workflow_text "verify-examples fixtures/tutorials" "$workflow"
  require_workflow_text "cpack -G TGZ --config build/unix-release/CPackConfig.cmake" "$workflow"
  require_workflow_text "scripts/sbom.sh --artifact" "$workflow"
  require_workflow_text "anchore/scan-action@v4" "$workflow"
  require_workflow_text "path: build/release-metadata/LaMusica.spdx.json" "$workflow"
  require_workflow_text "fail-build: true" "$workflow"
  require_workflow_text "severity-cutoff: critical" "$workflow"
  require_workflow_text "cmake -DPACKAGE=\"\$(ls LaMusica-*-Darwin.tar.gz)\" -P cmake/VerifyPackage.cmake" "$workflow"
  require_workflow_text "uses: actions/upload-artifact@v4" "$workflow"
  require_workflow_text "LaMusica-macos-tgz" "$workflow"

  if [[ "$ci_workflow_verify_failed" -ne 0 ]]; then
    return 1
  fi

  if ! awk '
  /Release script self-tests/ { selftests = NR }
  /Checkout JUCE/ { juce = NR }
  /Select Xcode/ { xcode = NR }
  /Configure$/ { configure = NR }
  /Build$/ { build = NR }
  /Test$/ { test = NR }
  /Determinism gate/ { determinism = NR }
  /Plugin hosting gate/ { plugin = NR }
  /Audio correctness gate/ { audio = NR }
  /Behavior artifact gate/ { behavior = NR }
  /CLI production gate/ { cli = NR }
  /Accessibility gate/ { a11y = NR }
  /Localization gate/ { i18n = NR }
  /Onboarding gate/ { onboarding = NR }
  /Privacy diagnostics gate/ { privacy = NR }
  /Realtime deadline gate/ { perf = NR }
  /Headless GUI binding gate/ { gui = NR }
  /Configure ASan/ { asan_configure = NR }
  /Test ASan/ { asan_test = NR }
  /Configure TSan/ { tsan_configure = NR }
  /Plugin hosting TSan gate/ { tsan_plugin = NR }
  /Configure release/ { release_configure = NR }
  /Build release/ { release_build = NR }
  /Validate examples/ { validate_examples = NR }
  /Package release/ { package = NR }
  /Build SBOM and checksums/ { sbom = NR }
  /Vulnerability scan/ { vuln = NR }
  /Verify package contents/ { verify_package = NR }
  /Upload package/ { upload = NR }
  END {
    exit !(selftests > 0 &&
            juce > selftests &&
            xcode > juce &&
            configure > xcode &&
            build > configure &&
            test > build &&
            determinism > test &&
            plugin > determinism &&
            audio > plugin &&
            behavior > audio &&
            cli > behavior &&
            a11y > cli &&
            i18n > a11y &&
            onboarding > i18n &&
            privacy > onboarding &&
            perf > privacy &&
            gui > perf &&
            asan_configure > gui &&
            asan_test > asan_configure &&
            tsan_configure > asan_test &&
            tsan_plugin > tsan_configure &&
            release_configure > tsan_plugin &&
            release_build > release_configure &&
            validate_examples > release_build &&
            package > validate_examples &&
            sbom > package &&
            vuln > sbom &&
            verify_package > vuln &&
            upload > verify_package)
  }
' "$workflow"; then
    echo "CI workflow must keep debug gates, sanitizer gates, release package verification, vulnerability scan, and upload in order" >&2
    return 1
  fi
}

expect_failure() {
  local label="$1"
  local workflow="$2"
  if verify_workflow "$workflow" >/dev/null 2>&1; then
    echo "CI workflow self-test failed: accepted invalid workflow fixture: $label" >&2
    exit 1
  fi
}

self_test() {
  local source_workflow="${1:-.github/workflows/ci.yml}"
  local tmp_dir
  tmp_dir="$(mktemp -d)"

  verify_workflow "$source_workflow" >/dev/null

  grep -v "ctest --test-dir build/unix-debug -L determinism --output-on-failure" "$source_workflow" > "$tmp_dir/missing-determinism.yml"
  expect_failure "missing determinism gate" "$tmp_dir/missing-determinism.yml"

  grep -v "ctest --test-dir build/unix-debug -L perf --output-on-failure" "$source_workflow" > "$tmp_dir/missing-perf.yml"
  expect_failure "missing realtime perf gate" "$tmp_dir/missing-perf.yml"

  grep -v "ctest --test-dir build/unix-tsan -L plugin-hosting --output-on-failure" "$source_workflow" > "$tmp_dir/missing-tsan-plugin.yml"
  expect_failure "missing plugin-hosting TSan gate" "$tmp_dir/missing-tsan-plugin.yml"

  grep -v "scripts/verify-ci-workflow.sh --self-test" "$source_workflow" > "$tmp_dir/missing-ci-self-test.yml"
  expect_failure "missing CI workflow self-test" "$tmp_dir/missing-ci-self-test.yml"

  grep -v "build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/examples" "$source_workflow" > "$tmp_dir/missing-example-verification.yml"
  expect_failure "missing example verification" "$tmp_dir/missing-example-verification.yml"

  grep -v "anchore/scan-action@v4" "$source_workflow" > "$tmp_dir/missing-vulnerability-scan-action.yml"
  expect_failure "missing vulnerability scan action" "$tmp_dir/missing-vulnerability-scan-action.yml"

  grep -v "path: build/release-metadata/LaMusica.spdx.json" "$source_workflow" > "$tmp_dir/missing-vulnerability-sbom-input.yml"
  expect_failure "missing vulnerability scan SBOM input" "$tmp_dir/missing-vulnerability-sbom-input.yml"

  cp "$source_workflow" "$tmp_dir/swapped-upload-verify.yml"
  python3 - "$tmp_dir/swapped-upload-verify.yml" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
sentinel = "__LAMUSICA_TEMP_CI_STEP__"
text = text.replace("- name: Verify package contents", f"- name: {sentinel}", 1)
text = text.replace("- name: Upload package", "- name: Verify package contents", 1)
text = text.replace(f"- name: {sentinel}", "- name: Upload package", 1)
path.write_text(text, encoding="utf-8")
PY
  expect_failure "upload before package verification" "$tmp_dir/swapped-upload-verify.yml"

  cp "$source_workflow" "$tmp_dir/swapped-vuln-sbom.yml"
  python3 - "$tmp_dir/swapped-vuln-sbom.yml" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
sentinel = "__LAMUSICA_TEMP_CI_STEP__"
text = text.replace("- name: Build SBOM and checksums", f"- name: {sentinel}", 1)
text = text.replace("- name: Vulnerability scan", "- name: Build SBOM and checksums", 1)
text = text.replace(f"- name: {sentinel}", "- name: Vulnerability scan", 1)
path.write_text(text, encoding="utf-8")
PY
  expect_failure "vulnerability scan before SBOM" "$tmp_dir/swapped-vuln-sbom.yml"

  rm -rf "$tmp_dir"
  echo "verify-ci-workflow self-test passed"
}

if [[ "${1:-}" == "--self-test" ]]; then
  self_test "${2:-.github/workflows/ci.yml}"
  exit 0
fi

workflow="${1:-.github/workflows/ci.yml}"
verify_workflow "$workflow"
echo "CI workflow verification passed"

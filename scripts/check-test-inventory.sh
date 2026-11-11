#!/bin/sh
# Fail closed when the registered CTest set drifts from the reviewed inventory.
set -eu
LC_ALL=C
export LC_ALL
set -f

BASELINE_CORE='unit.common
unit.json-seeds
unit.json
unit.json-fuzz
gov.claims.selftest
gov.claims.repo
gov.claims.seeds
unit.schema-sanitize
gov.manifest.seeds
gov.manifest.schema-seeds
gov.manifest.repo
gov.manifest.require-fixtures
gov.provenance.repo
gov.provenance.selftest
repo.docs
repo.docs-selftest
repo.ci-yml
repo.ci-yml-selftest
repo.spike-decision-selftest
gov.rights.selftest
gov.rights.repo
gov.pii.selftest
gov.pii.repo
unit.classify
unit.result-goldens
unit.result-seeds
unit.suite-seeds'

BASELINE_CONTROLS='controls.positive
controls.empty-halt
controls.spin
controls.abort
controls.segv
controls.exit0-no-result
controls.exit-nonzero
controls.hang
controls.log-flood-backstop
controls.deleted-fixture
controls.tampered-digest
controls.wrong-counts
controls.fail-reported
controls.fail-after-done
controls.truncated-verdict
controls.log-invalid-utf8
controls.log-over-limit
controls.empty-bound
controls.positive-unhandled
controls.suite
controls.suite-dropped
controls.suite-truncated
controls.inventory-drift
controls.inventory
controls.hostile.baseline
controls.hostile.fifo-report
controls.hostile.fifo-capture
controls.hostile.symlink-report
controls.hostile.short-capture
controls.hostile.missing-capture
controls.hostile.orphan
controls.hostile.wall-ms-max
controls.hostile.result-tmp-fifo
controls.hostile.seal-tmp-symlink'

PHASE02_TESTS='unit.core.tracer
unit.core.profile
unit.core.lifecycle
unit.core.rom-admission
unit.core.scheduler
unit.core.digest-trace
unit.core.bus
unit.core.pi-dma
unit.core.sp-dma
unit.core.profile-result
unit.core.determinism
unit.model-result'

PHASE03_TESTS='unit.core.boot-entry
unit.core.cpu-path
unit.core.cp0-cache
unit.core.warm-device-policy
unit.core.isviewer
runner.fixture-descriptor
runner.result-v2
controls.real.source-replaced
controls.real.input-partition'

ASAN_ONLY='fuzz.rom-admission
sanitizer.negative-control'

usage() {
  echo "Usage: sh scripts/check-test-inventory.sh --preset gate|asan" >&2
  echo "       sh scripts/check-test-inventory.sh --selftest" >&2
}

expected_names() {
  _preset=$1
  case $_preset in
    gate) _extra= ;;
    asan) _extra=$ASAN_ONLY ;;
    *) echo "unsupported preset: $_preset" >&2; return 2 ;;
  esac
  {
    printf '%s\n' "$BASELINE_CORE" "$BASELINE_CONTROLS" "$PHASE02_TESTS" "$PHASE03_TESTS" "$_extra"
  } | awk 'NF' | sort
}

count_names() {
  printf '%s\n' "$1" | awk 'NF { count++ } END { print count+0 }'
}

compare_inventory() {
  _preset=$1
  _test_dir=$2
  _expected=$(expected_names "$_preset") || return 1
  _expected_count=$(count_names "$_expected")
  if [ "$_test_dir" ]; then
    if ! ctest --test-dir "$_test_dir" -N >"$TMP_ROOT/ctest.out" 2>&1; then
      echo "inventory enumeration error: ctest failed for $_test_dir"
      cat "$TMP_ROOT/ctest.out"
      return 1
    fi
  else
    if ! ctest --preset "$_preset" -N >"$TMP_ROOT/ctest.out" 2>&1; then
      echo "inventory enumeration error: ctest failed for preset $_preset"
      cat "$TMP_ROOT/ctest.out"
      return 1
    fi
  fi

  if ! awk '
    /^[[:space:]]*Test[[:space:]]+#[0-9][0-9]*:[[:space:]]/ {
      name=$0
      sub(/^[[:space:]]*Test[[:space:]]+#[0-9][0-9]*:[[:space:]]*/, "", name)
      if (name ~ / \(Disabled\)$/) {
        sub(/ \(Disabled\)$/, "", name)
        kind="DISABLED"
      } else {
        kind="TEST"
      }
      if (name !~ /^[A-Za-z0-9._-]+$/) { invalid=1 }
      print kind, name
      count++
    }
    /^Total Tests:[[:space:]]*[0-9]+$/ { total=$NF; total_lines++ }
    END {
      if (invalid) exit 2
      if (total_lines != 1 || total != count) exit 2
      if (count == 0) { print "EMPTY"; exit 0 }
    }
  ' "$TMP_ROOT/ctest.out" >"$TMP_ROOT/parsed.out"; then
    echo "inventory enumeration error: malformed or incomplete ctest -N output"
    cat "$TMP_ROOT/ctest.out"
    return 1
  fi

  if grep '^EMPTY$' "$TMP_ROOT/parsed.out" >/dev/null 2>&1; then
    echo "empty inventory: ctest enumerated no tests"
    return 1
  fi
  _disabled=$(awk '$1 == "DISABLED" { print $2; exit }' "$TMP_ROOT/parsed.out")
  if [ "$_disabled" ]; then
    echo "disabled required test: $_disabled"
    return 1
  fi

  _actual=$(awk '$1 == "TEST" { print $2 }' "$TMP_ROOT/parsed.out" | sort)
  _duplicates=$(printf '%s\n' "$_actual" | sort | uniq -d)
  if [ "$_duplicates" ]; then
    echo "duplicate test registration: $(printf '%s\n' "$_duplicates" | sed -n '1p')"
    return 1
  fi

  printf '%s\n' "$_expected" >"$TMP_ROOT/expected.sorted"
  printf '%s\n' "$_actual" >"$TMP_ROOT/actual.sorted"
  _missing=$(comm -23 "$TMP_ROOT/expected.sorted" "$TMP_ROOT/actual.sorted" | sed -n '1p')
  if [ "$_missing" ]; then
    echo "missing required test: $_missing"
    return 1
  fi
  _unexpected=$(comm -13 "$TMP_ROOT/expected.sorted" "$TMP_ROOT/actual.sorted" | sed -n '1p')
  if [ "$_unexpected" ]; then
    echo "unexpected test: $_unexpected"
    return 1
  fi
  if [ "$(count_names "$_actual")" -ne "$_expected_count" ]; then
    echo "inventory count mismatch: expected $_expected_count, measured $(count_names "$_actual")"
    return 1
  fi
  echo "test inventory PASS preset=$_preset count=$_expected_count"
}

write_ctest_tree() {
  _tree=$1
  _names=$2
  _skip=${3:-}
  mkdir -p "$_tree"
  printf '%s\n' "$_names" | while IFS= read -r _name; do
    [ "$_name" ] || continue
    [ "$_name" = "$_skip" ] && continue
    printf 'add_test(%s "/bin/sh" "-c" "exit 0")\n' "$_name"
  done >"$_tree/CTestTestfile.cmake"
}

selftest_rejection() {
  _preset=$1
  _tree=$2
  _pattern=$3
  if compare_inventory "$_preset" "$_tree" >"$TMP_ROOT/selftest.out" 2>&1; then
    echo "selftest failed: checker accepted $_pattern" >&2
    return 1
  fi
  if ! grep -F "$_pattern" "$TMP_ROOT/selftest.out" >/dev/null 2>&1; then
    echo "selftest failed: expected rejection '$_pattern'" >&2
    cat "$TMP_ROOT/selftest.out" >&2
    return 1
    return 1
  fi
}

run_mode_selftest() {
  _preset=$1
  _expected=$(expected_names "$_preset")
  _count=$(count_names "$_expected")
  _full="$TMP_ROOT/$_preset/full"
  write_ctest_tree "$_full" "$_expected"
  compare_inventory "$_preset" "$_full" >/dev/null

  _removed=0
  for _name in $_expected; do
    _tree="$TMP_ROOT/$_preset/remove-$_name"
    write_ctest_tree "$_tree" "$_expected" "$_name"
    selftest_rejection "$_preset" "$_tree" "missing required test: $_name"
    _removed=$((_removed + 1))
  done
  echo "inventory selftest removals preset=$_preset count=$_removed expected=$_count"
}

selftest() {
  run_mode_selftest gate
  run_mode_selftest asan

  _names=$(expected_names gate)
  _dupe="$TMP_ROOT/duplicate"
  write_ctest_tree "$_dupe" "$_names"
  printf 'add_test(unit.common "/bin/sh" "-c" "exit 0")\n' >>"$_dupe/CTestTestfile.cmake"
  selftest_rejection gate "$_dupe" "duplicate test registration: unit.common"

  _unexpected="$TMP_ROOT/unexpected"
  write_ctest_tree "$_unexpected" "$_names"
  printf 'add_test(selftest.unexpected "/bin/sh" "-c" "exit 0")\n' >>"$_unexpected/CTestTestfile.cmake"
  selftest_rejection gate "$_unexpected" "unexpected test: selftest.unexpected"

  _disabled="$TMP_ROOT/disabled"
  write_ctest_tree "$_disabled" "$_names"
  printf 'set_tests_properties(unit.common PROPERTIES DISABLED TRUE)\n' >>"$_disabled/CTestTestfile.cmake"
  selftest_rejection gate "$_disabled" "disabled required test: unit.common"

  _empty="$TMP_ROOT/empty"
  mkdir -p "$_empty"
  : >"$_empty/CTestTestfile.cmake"
  selftest_rejection gate "$_empty" "empty inventory:"

  _error="$TMP_ROOT/enumeration-error"
  mkdir -p "$_error"
  printf 'include("missing-inventory-testfile.cmake")\n' >"$_error/CTestTestfile.cmake"
  selftest_rejection gate "$_error" "inventory enumeration error:"

  echo "test inventory selftest PASS removal_controls=148 malformed_controls=5"
}

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
cd "$ROOT"
TMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/t64-test-inventory.XXXXXX")
trap 'rm -rf "$TMP_ROOT"' 0 HUP INT TERM

case ${1:-} in
  --preset)
    [ "$#" -eq 2 ] || { usage; exit 2; }
    compare_inventory "$2" ""
    ;;
  --selftest)
    [ "$#" -eq 1 ] || { usage; exit 2; }
    selftest
    ;;
  *) usage; exit 2 ;;
esac

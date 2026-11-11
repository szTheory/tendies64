#!/bin/sh
# Compare the two strictly checked records from one exact hosted run attempt.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
TMP=""
cleanup() { [ -z "$TMP" ] || rm -rf "$TMP"; }
trap cleanup EXIT HUP INT TERM

die() {
  echo "compare-boot-evidence: $*" >&2
  exit 1
}

valid_sha() {
  case $1 in *[!0-9a-f]*|'') return 1 ;; esac
  [ "${#1}" -eq 40 ]
}

valid_decimal() {
  case $1 in *[!0-9]*|'') return 1 ;; esac
  [ "$1" != 0 ] && [ "${1#0}" = "$1" ]
}

compare() {
  sha=$1
  run=$2
  attempt=$3
  linux=$4
  macos=$5
  valid_sha "$sha" || die "--sha must be 40 lowercase hexadecimal characters"
  valid_decimal "$run" || die "--run must be a canonical positive decimal string"
  valid_decimal "$attempt" || die "--attempt must be a canonical positive decimal string"
  [ "$linux" != "$macos" ] || die "Linux and macOS records must be separate files"
  command -v shasum >/dev/null 2>&1 || die "shasum is required to bind the authored transcript"
  command -v jq >/dev/null 2>&1 || die "jq is required for canonical identity comparison"
  CHECKER=$ROOT/build/gate/runner/t64-result-check
  [ -x "$CHECKER" ] || die "strict result checker is not built"
  for file in "$linux" "$macos"; do
    [ -f "$file" ] && [ ! -L "$file" ] || die "record is absent, non-regular, or a symlink: $file"
    [ "$(wc -c < "$file" | tr -d ' ')" -le 65536 ] || die "record exceeds the 64 KiB bound: $file"
  done
  "$CHECKER" boot-evidence --sha "$sha" --run "$run" --attempt "$attempt" --job gate-linux "$linux" >/dev/null ||
    die "Linux wrapper failed strict expected-selector validation"
  "$CHECKER" boot-evidence --sha "$sha" --run "$run" --attempt "$attempt" --job gate-macos "$macos" >/dev/null ||
    die "macOS wrapper failed strict expected-selector validation"
  transcript_sha=$(printf '%s\n' 'Tendies64 hello via open IPL3' | shasum -a 256 | awk '{print $1}')
  transcript_bytes=$(printf '%s\n' 'Tendies64 hello via open IPL3' | wc -c | tr -d ' ')
  jq -s -e --arg sha "$sha" --arg run "$run" --arg attempt "$attempt" \
    --arg log_sha "$transcript_sha" --arg log_bytes "$transcript_bytes" '
      length == 2 and
      .[0].schema == "t64.boot-evidence" and .[1].schema == "t64.boot-evidence" and
      .[0].schema_version == 1 and .[1].schema_version == 1 and
      .[0].commit_sha == $sha and .[1].commit_sha == $sha and
      .[0].run_id == $run and .[1].run_id == $run and
      .[0].run_attempt == $attempt and .[1].run_attempt == $attempt and
      .[0].job_name == "gate-linux" and .[1].job_name == "gate-macos" and
      .[0].result.source_revision == $sha and .[1].result.source_revision == $sha and
      .[0].result.outcome == "pass" and .[1].result.outcome == "pass" and
      .[0].result.detail == "positive-match" and .[1].result.detail == "positive-match" and
      .[0].result.guest_stop.kind == "authored-completion" and
      .[1].result.guest_stop.kind == "authored-completion" and
      .[0].result.log_sha256 == $log_sha and .[1].result.log_sha256 == $log_sha and
      .[0].result.counts.log_bytes == $log_bytes and
      .[0].result.counts.log_bytes_total == $log_bytes and
      .[1].result.counts.log_bytes == $log_bytes and
      .[1].result.counts.log_bytes_total == $log_bytes and
      (.[0].result | del(.build_id)) == (.[1].result | del(.build_id))
    ' "$linux" "$macos" >/dev/null || die "guest identities, bounds, state, or authored transcript differ across hosts"
  echo "compare-boot-evidence: Linux and macOS guest identities, bounds, state, and authored transcript match for $sha run $run attempt $attempt"
}

selftest() {
  CHECKER=$ROOT/build/gate/runner/t64-result-check
  SUPERVISOR=$ROOT/build/gate/runner/t64-supervisor
  FIXTURES=$ROOT/build/fixtures
  [ -x "$CHECKER" ] && [ -x "$SUPERVISOR" ] || die "build the gate preset before comparator selftest"
  [ -f "$FIXTURES/hello.z64" ] && [ -f "$FIXTURES/hello.input.json" ] || die "qualified fixture files are required for comparator selftest"
  TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64-boot-compare-selftest.XXXXXX") || die "cannot create comparator selftest directory"
  chmod 700 "$TMP" || die "cannot protect comparator selftest directory"
  mkdir "$TMP/work"
  sha=$(git -C "$ROOT" rev-parse HEAD) || die "cannot determine comparator selftest source revision"
  "$SUPERVISOR" --real --result "$TMP/result.json" --meta "$TMP/meta.json" \
    --work-dir "$TMP/work" --wall-ms 30000 --fixture "$FIXTURES/hello.z64" \
    --input "$FIXTURES/hello.input.json" --capture-out "$TMP/capture.bin" \
    >"$TMP/supervisor.stdout" 2>"$TMP/supervisor.stderr" || die "real local supervisor fixture failed in comparator selftest"
  printf '%s\n' 'Tendies64 hello via open IPL3' > "$TMP/expected.log"
  cmp -s "$TMP/expected.log" "$TMP/capture.bin" || die "local comparator result lacks the authored transcript"
  jq -cnS --arg commit "$sha" --arg run 123 --arg attempt 1 --arg job gate-linux \
    --slurpfile result "$TMP/result.json" \
    '{schema:"t64.boot-evidence",schema_version:1,commit_sha:$commit,run_id:$run,run_attempt:$attempt,job_name:$job,result:$result[0]}' \
    > "$TMP/linux.json"
  jq -cS --arg build "t64-build-v1-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" \
    '.result.build_id=$build' "$TMP/linux.json" > "$TMP/macos.json.tmp"
  jq -cS --arg job gate-macos '.job_name=$job' "$TMP/macos.json.tmp" > "$TMP/macos.json"
  if ! compare "$sha" 123 1 "$TMP/linux.json" "$TMP/macos.json" >/dev/null; then
    die "truthful differing build ids alone must not fail the comparator"
  fi

  jq -cS '.result.state_digest = "0000000000000000000000000000000000000000000000000000000000000000"' \
    "$TMP/macos.json" > "$TMP/state-mismatch.json"
  jq -cS '.result.input_digest = "0000000000000000000000000000000000000000000000000000000000000000"' \
    "$TMP/macos.json" > "$TMP/input-mismatch.json"
  jq -cS '.result.guest_stop.bound_tick = "600001"' \
    "$TMP/macos.json" > "$TMP/bound-mismatch.json"
  "$CHECKER" boot-evidence --sha "$sha" --run 123 --attempt 1 --job gate-macos "$TMP/state-mismatch.json" >/dev/null || die "selftest state-mismatch record is not independently valid"
  "$CHECKER" boot-evidence --sha "$sha" --run 123 --attempt 1 --job gate-macos "$TMP/input-mismatch.json" >/dev/null || die "selftest input-mismatch record is not independently valid"
  "$CHECKER" boot-evidence --sha "$sha" --run 123 --attempt 1 --job gate-macos "$TMP/bound-mismatch.json" >/dev/null || die "selftest bound-mismatch record is not independently valid"
  expect_reject() {
    label=$1
    shift
    if sh "$ROOT/scripts/compare-boot-evidence.sh" --sha "$@" >/dev/null 2>&1; then
      die "comparator accepted $label"
    fi
  }
  expect_reject "unequal guest state digests" "$sha" --run 123 --attempt 1 "$TMP/linux.json" "$TMP/state-mismatch.json"
  expect_reject "unequal input identities" "$sha" --run 123 --attempt 1 "$TMP/linux.json" "$TMP/input-mismatch.json"
  expect_reject "unequal guest bounds" "$sha" --run 123 --attempt 1 "$TMP/linux.json" "$TMP/bound-mismatch.json"
  expect_reject "records for another source SHA" 0000000000000000000000000000000000000000 --run 123 --attempt 1 "$TMP/linux.json" "$TMP/macos.json"
  expect_reject "records for another run" "$sha" --run 124 --attempt 1 "$TMP/linux.json" "$TMP/macos.json"
  expect_reject "records for another attempt" "$sha" --run 123 --attempt 2 "$TMP/linux.json" "$TMP/macos.json"
  expect_reject "a duplicated Linux record as both jobs" "$sha" --run 123 --attempt 1 "$TMP/linux.json" "$TMP/linux.json"
  expect_reject "a missing macOS record" "$sha" --run 123 --attempt 1 "$TMP/linux.json" "$TMP/missing.json"
  echo "compare-boot-evidence: selftest passed (valid pair with differing build IDs; 8 mismatch cases; local records are not hosted evidence)"
}

case ${1:-} in
  --sha)
    [ "$#" -eq 8 ] && [ "$3" = "--run" ] && [ "$5" = "--attempt" ] ||
      die "usage: compare-boot-evidence.sh --sha SHA --run RUN --attempt ATTEMPT LINUX.json MACOS.json"
    compare "$2" "$4" "$6" "$7" "$8"
    ;;
  --selftest)
    [ "$#" -eq 1 ] || die "--selftest takes no extra arguments"
    selftest
    ;;
  *)
    echo "usage: compare-boot-evidence.sh --sha SHA --run RUN --attempt ATTEMPT LINUX.json MACOS.json | --selftest" >&2
    exit 2
    ;;
esac

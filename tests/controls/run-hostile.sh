#!/bin/sh
# Hostile-runner controls for t64-supervisor (review findings WR-01, WR-02,
# WR-04, WR-05).
# POSIX sh.
#
#   run-hostile.sh --bin-dir DIR --mode MODE
#
# Each mode runs the supervisor once against a small shell runner generated
# here, in a mktemp directory, that misbehaves the way the mode names. These
# are supervisor tests, not matrix rows: a real-core runner never replays them.
# The fixture is a plain text file generated with printf. A watchdog kills a
# supervisor that has not returned after WATCHDOG seconds, so a hang fails the
# test instead of stalling CTest.
#
# Modes and the exact outcome/detail each must produce:
#   baseline       an honest runner (Done!, matching report)  pass/positive-match
#   fifo-report    report replaced by a FIFO after writing    harness-error/report-invalid
#   fifo-capture   capture replaced by a FIFO                 harness-error/report-invalid
#   symlink-report report replaced by a symlink               harness-error/report-invalid
#   short-capture  log_bytes_total 500, capture "Done!\n"     harness-error/report-invalid
#   missing-capture capture deleted, report claims 6 bytes    harness-error/report-invalid
#   orphan         honest output, then exits 0 leaving a background child and
#                  grandchild in its process group           pass/positive-match,
#                  and neither descendant may outlive the supervisor
#   wall-ms-max    the honest runner with --wall-ms 2^64-1    pass/positive-match
#                  (the deadline saturates instead of wrapping to wall-backstop)
#
# Prints "ok MODE (...)" or "FAIL MODE: reason" and exits 0 or 1; 2 on usage.
set -eu
LC_ALL=C
export LC_ALL

BIN_DIR=""
MODE=""
WATCHDOG=20
while [ $# -gt 0 ]; do
  case "$1" in
    --bin-dir) BIN_DIR=${2:-}; shift 2 ;;
    --mode) MODE=${2:-}; shift 2 ;;
    *) echo "run-hostile: unknown argument: $1" >&2; exit 2 ;;
  esac
done
if [ -z "$BIN_DIR" ] || [ -z "$MODE" ]; then
  echo "usage: run-hostile.sh --bin-dir DIR --mode MODE" >&2
  exit 2
fi
SUP="$BIN_DIR/t64-supervisor"
CHECK="$BIN_DIR/t64-result-check"

case "$MODE" in
  baseline | orphan | wall-ms-max)
    WANT_OUT=pass; WANT_DET=positive-match; WANT_RC=0 ;;
  fifo-report | fifo-capture | symlink-report | short-capture | missing-capture)
    WANT_OUT=harness-error; WANT_DET=report-invalid; WANT_RC=1 ;;
  *) echo "run-hostile: unknown mode: $MODE" >&2; exit 2 ;;
esac
WALL_MS=5000
RUNNER_MODE=$MODE
if [ "$MODE" = wall-ms-max ]; then
  WALL_MS=18446744073709551615
  RUNNER_MODE=baseline
fi

T=$(mktemp -d "${TMPDIR:-/tmp}/t64hostile.XXXXXX")
# kill_recorded: SIGKILL any descendant pid the orphan runner recorded, so a
# failing run never leaves a straggler behind.
kill_recorded() {
  for _pf in "$T"/child.pid "$T"/grandchild.pid; do
    [ -f "$_pf" ] || continue
    _p=$(cat "$_pf")
    case "$_p" in '' | *[!0-9]*) continue ;; esac
    kill -KILL "$_p" 2> /dev/null || :
  done
}
cleanup() {
  kill_recorded
  rm -rf "$T"
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP

die() {
  echo "FAIL $MODE: $1" >&2
  exit 1
}

sha256_of() {
  if command -v sha256sum > /dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

printf 'hostile runner fixture\n' > "$T/fixture.txt"
SHA=$(sha256_of "$T/fixture.txt")
mkdir "$T/work"

# The runner: argv is MODE STATE_DIR, then the arguments the supervisor appends.
cat > "$T/runner.sh" <<'EOF'
mode=$1
state=$2
shift 2
report=""
capture=""
while [ $# -gt 0 ]; do
  case "$1" in
    --report) report=$2 ;;
    --capture) capture=$2 ;;
  esac
  shift 2
done
[ -n "$report" ] && [ -n "$capture" ] || exit 2
report_with_total() {
  printf 't64-report 1\nstop_reason guest-halt\nguest_tick 40\nlog_bytes_total %s\nend\n' "$1" > "$report"
}
case "$mode" in
  baseline)
    printf 'Done!\n' > "$capture"
    report_with_total 6
    ;;
  fifo-report)
    printf 'Done!\n' > "$capture"
    rm -f "$report"
    mkfifo "$report"
    ;;
  fifo-capture)
    report_with_total 6
    rm -f "$capture"
    mkfifo "$capture"
    ;;
  symlink-report)
    printf 'Done!\n' > "$capture"
    report_with_total 6
    mv "$report" "$state/real-report"
    ln -s "$state/real-report" "$report"
    ;;
  short-capture)
    printf 'Done!\n' > "$capture"
    report_with_total 500
    ;;
  missing-capture)
    report_with_total 6
    rm -f "$capture"
    ;;
  orphan)
    printf 'Done!\n' > "$capture"
    report_with_total 6
    # A child that itself starts a grandchild; all stay in the runner's
    # process group (no job control in a non-interactive sh).
    sh -c 'sleep 300 & echo $! > "$1/grandchild.pid"; wait' sh "$state" &
    echo $! > "$state/child.pid"
    _n=0
    while [ ! -s "$state/grandchild.pid" ] && [ "$_n" -lt 50 ]; do
      sleep 0.1 2> /dev/null || sleep 1
      _n=$((_n + 1))
    done
    ;;
  *) exit 2 ;;
esac
exit 0
EOF

# run_sup ARGS...: runs the supervisor under the watchdog; sets RC.
run_sup() {
  "$SUP" "$@" > "$T/sup.out" 2> "$T/sup.err" &
  _sp=$!
  (
    _i=0
    while [ "$_i" -lt "$WATCHDOG" ]; do
      sleep 1
      kill -0 "$_sp" 2> /dev/null || exit 0
      _i=$((_i + 1))
    done
    kill -KILL "$_sp" 2> /dev/null || :
    : > "$T/watchdog-fired"
  ) > /dev/null 2>&1 &
  _wd=$!
  RC=0
  wait "$_sp" || RC=$?
  kill "$_wd" 2> /dev/null || :
  wait "$_wd" 2> /dev/null || :
  if [ -f "$T/watchdog-fired" ]; then
    die "the supervisor did not return within ${WATCHDOG}s (hang)"
  fi
}

run_sup --identity synthetic --result "$T/result.json" --meta "$T/meta.json" \
  --work-dir "$T/work" --wall-ms "$WALL_MS" --bound 1000 --log-limit 65536 \
  --fixture-id "hostile-$MODE" --fixture "$T/fixture.txt" --fixture-sha256 "$SHA" \
  --expect-pass 0 -- sh "$T/runner.sh" "$RUNNER_MODE" "$T"

[ -f "$T/result.json" ] || die "no result written (exit $RC): $(head -n 1 "$T/sup.err")"
"$CHECK" result "$T/result.json" > /dev/null 2> "$T/check.err" ||
  die "result-check rejected the result: $(head -n 1 "$T/check.err")"
grep -q "\"outcome\":\"$WANT_OUT\"" "$T/result.json" ||
  die "outcome is not $WANT_OUT: $(head -n 1 "$T/sup.err")"
grep -q "\"detail\":\"$WANT_DET\"" "$T/result.json" ||
  die "detail is not $WANT_DET: $(head -n 1 "$T/sup.err")"
[ "$RC" = "$WANT_RC" ] || die "exit $RC, wanted $WANT_RC"
# The private work directory is always removed, whatever the runner left.
_left=$(find "$T/work" -mindepth 1 -print | head -n 1)
[ -z "$_left" ] || die "the supervisor left files in the work dir: $_left"
if [ "$MODE" = orphan ]; then
  [ -s "$T/child.pid" ] && [ -s "$T/grandchild.pid" ] ||
    die "the runner did not record its descendants"
  # A killed descendant is reparented and reaped by init; allow a few seconds
  # for that, then any pid still alive is a survivor.
  for _pf in "$T/child.pid" "$T/grandchild.pid"; do
    _p=$(cat "$_pf")
    _n=0
    while kill -0 "$_p" 2> /dev/null; do
      _n=$((_n + 1))
      [ "$_n" -le 50 ] || die "descendant $_p ($(basename "$_pf" .pid)) survived the supervisor"
      sleep 0.1 2> /dev/null || sleep 1
    done
  done
fi
echo "ok $MODE ($WANT_OUT/$WANT_DET, exit $RC)"

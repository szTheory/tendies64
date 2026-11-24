#!/bin/sh
# Tendies64 Phase 10 preflight (AUTONOMY v1.2 execution rules, plan 10-01).
# POSIX sh, no bashisms. One scripted local check, cited by every Phase 10
# plan, that replays the Phase 9 lessons in seconds before a long gate or a push.
#
#   sh scripts/preflight-phase10.sh
#
# Steps, cheap first, each printing "[preflight] NAME ok (Ns)" or
# "[preflight] NAME FAILED" (exit 1):
#   oracle-authority  the owner-gated oracle request check, read-only
#   ctest-counts      every row of scripts/preflight-phase10-regexes.txt:
#                     ctest -N -R count against the expected count
# then "[preflight] summary:" lines, "[preflight] total Ns" and "[preflight] PASS".
#
# Timing: whole seconds from one date +%s source. Each step starts where the
# previous one ended, so the total equals the sum of the step seconds.
# Environment: T64_PREFLIGHT_REGEXES names an alternative regex file (used by
# the plan 10-01 mismatch control). Exit codes: 0 pass, 1 a step failed,
# 2 usage or build/gate not configured.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
cd "$ROOT"

REGEXES=${T64_PREFLIGHT_REGEXES:-scripts/preflight-phase10-regexes.txt}

if [ "$#" -ne 0 ]; then
  echo "usage: sh scripts/preflight-phase10.sh" >&2
  exit 2
fi

if [ ! -f build/gate/CTestTestfile.cmake ]; then
  echo "[preflight] build/gate is not configured (no build/gate/CTestTestfile.cmake): run cmake --preset gate and cmake --build --preset gate first" >&2
  exit 2
fi

PF_T0=$(date +%s)
PF_PREV=$PF_T0
PF_SUM=0
PF_STEPS=""

# step NAME CMD...: run CMD, print "[preflight] NAME ok (Ns)" or "[preflight] NAME FAILED".
# The command's output is never piped, so its exit status is the step status.
step() {
  _pf_name=$1
  shift
  if "$@"; then
    :
  else
    echo "[preflight] $_pf_name FAILED"
    exit 1
  fi
  _pf_now=$(date +%s)
  _pf_dt=$((_pf_now - PF_PREV))
  PF_PREV=$_pf_now
  PF_SUM=$((PF_SUM + _pf_dt))
  echo "[preflight] $_pf_name ok (${_pf_dt}s)"
  PF_STEPS="${PF_STEPS}${_pf_name} ${_pf_dt}s
"
}

# oracle-authority: the owner-gated Phase 7 request must still verify and stay
# blocked. Read-only: this script never edits the checker or its pin.
check_oracle_authority() {
  python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked
}

# ctest-counts: each non-comment row is "<expected count><TAB><regex>".
check_ctest_counts() {
  if [ ! -f "$REGEXES" ]; then
    echo "[preflight] regex file is missing: $REGEXES" >&2
    return 1
  fi
  _pf_tab=$(printf '\t')
  _pf_rows=0
  _pf_bad=0
  while IFS=$_pf_tab read -r _pf_want _pf_regex || [ -n "${_pf_want:-}" ]; do
    case $_pf_want in
      '' | '#'*) continue ;;
    esac
    case $_pf_want in
      *[!0-9]*)
        echo "[preflight] malformed regex row (count is not a number): $_pf_want" >&2
        return 1
        ;;
    esac
    if [ -z "$_pf_regex" ]; then
      echo "[preflight] malformed regex row (no regex after the count $_pf_want)" >&2
      return 1
    fi
    if ! _pf_out=$(ctest --test-dir build/gate -N -R "$_pf_regex" 2>&1); then
      echo "[preflight] ctest -N failed for: $_pf_regex" >&2
      return 1
    fi
    _pf_got=$(printf '%s\n' "$_pf_out" | awk '/^ *Test +#[0-9]+:/ { n++ } END { print n + 0 }')
    _pf_total=$(printf '%s\n' "$_pf_out" | awk '/^Total Tests: / { print $3 }')
    if [ "$_pf_total" != "$_pf_got" ]; then
      echo "[preflight] ctest -N output is inconsistent for: $_pf_regex (Test lines $_pf_got, Total Tests ${_pf_total:-absent})" >&2
      return 1
    fi
    _pf_rows=$((_pf_rows + 1))
    if [ "$_pf_got" != "$_pf_want" ]; then
      echo "[preflight] count mismatch: $_pf_regex expected $_pf_want got $_pf_got"
      _pf_bad=$((_pf_bad + 1))
    fi
  done < "$REGEXES"
  if [ "$_pf_rows" -eq 0 ]; then
    echo "[preflight] no regex rows were read from $REGEXES (zero rows is a failure)" >&2
    return 1
  fi
  echo "[preflight] ctest-counts: $_pf_rows regex rows, $_pf_bad mismatch(es)"
  [ "$_pf_bad" -eq 0 ]
}

step oracle-authority check_oracle_authority
step ctest-counts check_ctest_counts

PF_WALL=$(($(date +%s) - PF_T0))
_pf_gap=$((PF_WALL - PF_SUM))
if [ "$_pf_gap" -lt -1 ] || [ "$_pf_gap" -gt 1 ]; then
  echo "[preflight] timing inconsistent: total $PF_WALL s, step sum $PF_SUM s"
  exit 1
fi
echo "[preflight] summary:"
printf '%s' "$PF_STEPS" | while IFS= read -r _pf_line; do
  echo "[preflight]   $_pf_line"
done
echo "[preflight] total ${PF_SUM}s"
echo "[preflight] PASS"

#!/bin/sh
# Tendies64 Phase 10 preflight (AUTONOMY v1.2 execution rules, plan 10-01).
# POSIX sh, no bashisms. One scripted local check, cited by every Phase 10
# plan, that replays the Phase 9 lessons in seconds before a long gate or a push.
#
#   sh scripts/preflight-phase10.sh [--no-replay] [--base REF]
#
#   --no-replay   skip the post-Gate replay (for a plan that changes no
#                 retained-evidence producer); the reason is printed
#   --base REF    the commit the linux-compile step compares against
#                 (default: git merge-base HEAD origin/main)
#
# Steps, cheap first, each printing "[preflight] NAME ok (Ns)" or
# "[preflight] NAME FAILED" (exit 1):
#   oracle-authority  the owner-gated oracle request check, read-only
#   ctest-counts      every row of scripts/preflight-phase10-regexes.txt
#                     (ctest -N -R count against the expected count) and the
#                     tests of ctest -N -L phase10 against the names of the
#                     PHASE10_TESTS block of scripts/check-test-inventory.sh
#   host-load         the 1-minute load average; WARN above 8, never fails
#   head-ci           hosted ci.yml result of origin/main (F-107 lesson); fails
#                     only on a completed failure; gh unreachable is not a failure
#   timeout-audit     every registered TIMEOUT against 3.5 times the recorded
#                     native seconds (F-108); tests without cost data are listed
#   pii               scripts/pii-scan.sh --tree, and --range origin/main..HEAD
#   linux-compile     the gate compile units rerun with the Ubuntu cc of the act
#                     image already on disk (F-102), only when a C change exists
#   post-gate-replay  the ci.yml run lines after Gate, replayed against the
#                     local build with a fresh run id (F-107, F-113); run it on
#                     a clean tree: the emit steps bind the hosted source
#                     inputs to HEAD, so uncommitted edits of bound sources fail
# then "[preflight] summary:" lines, "[preflight] total Ns" and "[preflight] PASS".
#
# Timing: whole seconds from one date +%s source. Each step starts where the
# previous one ended, so the total equals the sum of the step seconds. The
# first run after a commit can include a rebuild: the revision annotation of
# the core units changes with HEAD.
#
# Read-only against the oracle authority record: this script never edits
# scripts/check-rdp-oracle-authority.py, PINNED_REQUEST_SHA256 or a bound file.
# Environment: T64_PREFLIGHT_REGEXES names an alternative regex file (the plan
# 10-01 mismatch control). Exit codes: 0 pass, 1 a step failed, 2 usage or
# build/gate not configured.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
cd "$ROOT"

REGEXES=${T64_PREFLIGHT_REGEXES:-scripts/preflight-phase10-regexes.txt}
PF_WORK=build/preflight-phase10
PF_ACT_IMAGE=ghcr.io/catthehacker/ubuntu:act-24.04
PF_REPLAY_JOB=gate-macos
PF_REPLAY_FIRST="Run authored audio qualification records"
PF_REPLAY_LAST="PII scan authored Phase 9 evidence"
PF_NO_REPLAY=0
PF_BASE=

usage() {
  echo "usage: sh scripts/preflight-phase10.sh [--no-replay] [--base REF]" >&2
  exit 2
}

while [ "$#" -gt 0 ]; do
  case $1 in
    --no-replay) PF_NO_REPLAY=1 ;;
    --base)
      [ "$#" -ge 2 ] || usage
      PF_BASE=$2
      shift
      ;;
    *) usage ;;
  esac
  shift
done

if [ -n "$PF_BASE" ] && ! git rev-parse --verify -q "$PF_BASE^{commit}" >/dev/null 2>&1; then
  echo "[preflight] --base is not a commit: $PF_BASE" >&2
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

# The scratch directory of this script, under the ignored build tree. Only this
# directory is ever deleted wholesale; it holds nothing but this script's files.
fresh_work() {
  rm -rf "$PF_WORK"
  mkdir -p "$PF_WORK"
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
  # phase10-label: every name of the PHASE10_TESTS inventory block carries the
  # label phase10 and no other test does.
  _pf_inv=$(awk -v q="'" '
    /^PHASE10_TESTS=/ { inblock = 1; sub("^PHASE10_TESTS=" q, "") }
    inblock {
      line = $0
      closing = (line ~ (q "$"))
      sub(q "$", "", line)
      if (line != "") n++
      if (closing) { inblock = 0; found = 1 }
    }
    END { if (found) print n + 0; else print "absent" }
  ' scripts/check-test-inventory.sh)
  if ! _pf_labeled=$(ctest --test-dir build/gate -N -L phase10 2>&1); then
    echo "[preflight] ctest -N -L phase10 failed" >&2
    return 1
  fi
  _pf_nlabel=$(printf '%s\n' "$_pf_labeled" | awk '/^ *Test +#[0-9]+:/ { n++ } END { print n + 0 }')
  if [ "$_pf_inv" = absent ]; then
    echo "[preflight] phase10-label: no PHASE10_TESTS block in scripts/check-test-inventory.sh" >&2
    _pf_bad=$((_pf_bad + 1))
  elif [ "$_pf_inv" != "$_pf_nlabel" ]; then
    echo "[preflight] phase10-label mismatch: $_pf_nlabel tests carry the label phase10, the PHASE10_TESTS block names $_pf_inv"
    _pf_bad=$((_pf_bad + 1))
  else
    echo "[preflight] phase10-label: $_pf_nlabel tests carry the label phase10, matching the PHASE10_TESTS block"
  fi
  [ "$_pf_bad" -eq 0 ]
}

# host-load: native timings and the 90 s target assume a quiet host.
check_host_load() {
  _pf_load=
  if _pf_la=$(sysctl -n vm.loadavg 2>/dev/null); then
    _pf_load=$(printf '%s\n' "$_pf_la" | awk '{ gsub(/[{}]/, ""); print $1 }')
  elif [ -r /proc/loadavg ]; then
    _pf_load=$(awk '{ print $1 }' /proc/loadavg)
  fi
  if [ -z "$_pf_load" ]; then
    echo "[preflight] host-load: unavailable"
    return 0
  fi
  if awk -v l="$_pf_load" 'BEGIN { exit !(l + 0 > 8) }'; then
    echo "[preflight] host-load: WARN 1-minute load average $_pf_load is above 8; native timings and the 90 s target do not hold"
  else
    echo "[preflight] host-load: 1-minute load average $_pf_load"
  fi
  return 0
}

# head-ci: the newest hosted ci.yml run whose head is origin/main. Fails only
# when that run completed with conclusion failure (a red head makes a long
# native qualification wasted work).
check_head_ci() {
  if ! _pf_head=$(git rev-parse --verify -q origin/main 2>/dev/null); then
    echo "[preflight] head-ci: no origin/main ref; skipped"
    return 0
  fi
  if ! command -v gh >/dev/null 2>&1; then
    echo "[preflight] head-ci: gh unavailable"
    return 0
  fi
  if ! _pf_runs=$(gh run list --workflow ci.yml --branch main --limit 20 \
      --json headSha,conclusion,status \
      --jq '.[] | "\(.headSha) \(.status) \(.conclusion)"' 2>/dev/null); then
    echo "[preflight] head-ci: gh unavailable"
    return 0
  fi
  _pf_hit=$(printf '%s\n' "$_pf_runs" | awk -v sha="$_pf_head" '$1 == sha { print; exit }')
  if [ -z "$_pf_hit" ]; then
    echo "[preflight] head-ci: no run for origin/main ($_pf_head)"
    _pf_latest=$(printf '%s\n' "$_pf_runs" | awk 'NF { print; exit }')
    [ -z "$_pf_latest" ] || echo "[preflight] head-ci: newest hosted run on main is $_pf_latest"
    return 0
  fi
  _pf_status=$(printf '%s\n' "$_pf_hit" | awk '{ print $2 }')
  _pf_conclusion=$(printf '%s\n' "$_pf_hit" | awk '{ print $3 }')
  echo "[preflight] head-ci: origin/main $_pf_head status=$_pf_status conclusion=${_pf_conclusion:-none}"
  if [ "$_pf_status" = completed ] && [ "$_pf_conclusion" = failure ]; then
    echo "[preflight] head-ci: the hosted head is red; a long native run on it is wasted work" >&2
    return 1
  fi
  return 0
}

# timeout-audit: a registered TIMEOUT must be at least 3.5 times the recorded
# native seconds, the hosted macOS ratio upper bound of F-108.
check_timeout_audit() {
  fresh_work
  ctest --test-dir build/gate --show-only=json-v1 > "$PF_WORK/tests.json" || return 1
  python3 -B -I - "$PF_WORK/tests.json" build/gate/Testing/Temporary/CTestCostData.txt <<'PYEOF'
import json
import sys

RATIO = 3.5
with open(sys.argv[1], encoding="utf-8") as handle:
    tests = json.load(handle)["tests"]
cost = {}
try:
    with open(sys.argv[2], encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("---"):
                break
            parts = line.split()
            if len(parts) == 3:
                try:
                    cost[parts[0]] = float(parts[2])
                except ValueError:
                    pass
except OSError:
    pass
checked = 0
unmeasured = []
low = []
worst = None
for test in tests:
    props = {p["name"]: p["value"] for p in test.get("properties", [])}
    if "TIMEOUT" not in props:
        continue
    name = test["name"]
    timeout = float(props["TIMEOUT"])
    seconds = cost.get(name)
    if seconds is None or seconds <= 0.0:
        unmeasured.append(name)
        continue
    checked += 1
    ratio = timeout / seconds
    if worst is None or ratio < worst[0]:
        worst = (ratio, name, timeout, seconds)
    if timeout < RATIO * seconds:
        low.append((name, timeout, seconds))
for name, timeout, seconds in low:
    print("[preflight] timeout-audit: %s TIMEOUT %g s is below %.1f times the recorded %.2f s"
          % (name, timeout, RATIO, seconds))
detail = ""
if worst is not None:
    detail = ", lowest ratio %.1f (%s: TIMEOUT %g s, recorded %.2f s)" % (
        worst[0], worst[1], worst[2], worst[3])
print("[preflight] timeout-audit: %d registered TIMEOUTs checked, %d too low, %d unmeasured%s"
      % (checked, len(low), len(unmeasured), detail))
if unmeasured:
    print("[preflight] timeout-audit: unmeasured: " + " ".join(unmeasured))
sys.exit(1 if low else 0)
PYEOF
}

# pii: tracked and untracked non-ignored files, and the history not on origin/main.
check_pii() {
  sh scripts/pii-scan.sh --tree || return 1
  if git rev-parse --verify -q origin/main >/dev/null 2>&1; then
    sh scripts/pii-scan.sh --range origin/main..HEAD || return 1
  fi
  return 0
}

# linux-compile: rerun the gate compile commands with the Ubuntu cc of the act
# image already on disk (no new image is acquired, no --platform is forced).
check_linux_compile() {
  _pf_base=$PF_BASE
  if [ -z "$_pf_base" ] && git rev-parse --verify -q origin/main >/dev/null 2>&1; then
    _pf_base=$(git merge-base HEAD origin/main 2>/dev/null) || _pf_base=
  fi
  if [ -n "$_pf_base" ]; then
    _pf_cchange=$({ git diff --name-only "$_pf_base" -- ; git ls-files --others --exclude-standard; } |
      awk '/\.[ch]$/ || /(^|\/)CMakeLists\.txt$/ { print; exit }')
    if [ -z "$_pf_cchange" ]; then
      echo "[preflight] linux-compile skipped: no C change since BASE ($(git rev-parse --short "$_pf_base"))"
      return 0
    fi
    echo "[preflight] linux-compile: C change since BASE ($(git rev-parse --short "$_pf_base")), first path $_pf_cchange"
  else
    echo "[preflight] linux-compile: no origin/main and no --base, compiling to be safe"
  fi
  if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
    echo "[preflight] linux-compile: docker is unavailable and a C change exists" >&2
    return 1
  fi
  if ! docker image inspect "$PF_ACT_IMAGE" >/dev/null 2>&1; then
    echo "[preflight] linux-compile: $PF_ACT_IMAGE is not on disk; acquiring a new image needs the package and provenance audit (AUTONOMY section 2)" >&2
    return 1
  fi
  if [ -f build/gate/build.ninja ] && [ -n "$(find . -path ./build -prune -o -name CMakeLists.txt -newer build/gate/build.ninja -print 2>/dev/null | head -n 1)" ]; then
    echo "[preflight] linux-compile: WARN a CMakeLists.txt is newer than build/gate/build.ninja; run cmake --build --preset gate so the unit list is current"
  fi
  fresh_work
  mkdir -p "$PF_WORK/linux-compile"
  # The 09-03.1 recipe: the ninja compile commands with the checkout path
  # mapped to /src, the macOS-only flags removed, the dependency files dropped
  # and the object sent to /dev/null (warnings of an optimizing build stay).
  ninja -C build/gate -t commands 2>/dev/null |
    grep -E ' -c [^ ]+\.c$' |
    PF_ROOT=$ROOT awk '
      BEGIN { root = ENVIRON["PF_ROOT"] }
      {
        line = $0
        out = ""
        while ((i = index(line, root)) > 0) {
          out = out substr(line, 1, i - 1) "/src"
          line = substr(line, i + length(root))
        }
        print out line
      }' |
    sed -e 's# -arch [^ ]*##g' \
        -e 's# -isysroot [^ ]*##g' \
        -e 's# -mmacosx-version-min=[^ ]*##g' \
        -e 's# -MD##g' \
        -e 's# -MT [^ ]*##g' \
        -e 's# -MF [^ ]*##g' \
        -e 's# -D_DARWIN_C_SOURCE##g' \
        -e 's# -o [^ ]*# -o /dev/null#' \
        -e 's#^[^ ][^ ]* #cc #' > "$PF_WORK/linux-compile/cmds.txt" || true
  _pf_units=$(awk 'END { print NR + 0 }' "$PF_WORK/linux-compile/cmds.txt")
  if [ "$_pf_units" -eq 0 ]; then
    echo "[preflight] linux-compile: no compile commands came out of ninja -C build/gate -t commands" >&2
    return 1
  fi
  printf '%s\n' '#!/bin/sh' 'eval "$1"' > "$PF_WORK/linux-compile/run1.sh"
  if docker run --rm --pull never --network none -v "$ROOT:/src:ro" -w /src/build/gate "$PF_ACT_IMAGE" \
      sh -c "xargs -d '\\n' -P 4 -n 1 sh /src/$PF_WORK/linux-compile/run1.sh < /src/$PF_WORK/linux-compile/cmds.txt" \
      > "$PF_WORK/linux-compile/out.txt" 2>&1; then
    echo "[preflight] linux-compile: $_pf_units units compiled with the act image cc ($(docker run --rm --pull never "$PF_ACT_IMAGE" cc -dumpfullversion 2>/dev/null))"
    return 0
  fi
  grep -v '^WARNING: The requested image' "$PF_WORK/linux-compile/out.txt" | sed 's#/src/##g' | tail -n 40
  echo "[preflight] linux-compile: a unit failed under the Ubuntu cc (see above)" >&2
  return 1
}

# The retained roots a replay adds one entry to. A directory name decides
# nothing alone: only an entry that is absent from the snapshot taken before the
# replay and names the replay job is removed afterwards.
PF_REPLAY_ROOTS="build/gate/08-05-audio-private build/gate/09-11-phase09-private build/gate/07-qualification"

replay_snapshot() {
  for _pf_dir in $PF_REPLAY_ROOTS; do
    ls -1 "$_pf_dir" 2>/dev/null | sort > "$PF_WORK/before-$(basename "$_pf_dir").txt" || true
  done
}

# replay_cleanup RUN_ID SHA: remove only what this invocation created.
replay_cleanup() {
  rm -rf "build/evidence/combined/$PF_REPLAY_JOB" "build/evidence/$PF_REPLAY_JOB.json"
  rmdir build/evidence/combined build/evidence 2>/dev/null || true
  rm -rf "build/gate/08-05-$PF_REPLAY_JOB-$1-1-$2-"* "build/gate/09-hosted-$PF_REPLAY_JOB-$1-1-$2-"*
  for _pf_dir in $PF_REPLAY_ROOTS; do
    ls -1 "$_pf_dir" 2>/dev/null | sort | comm -13 "$PF_WORK/before-$(basename "$_pf_dir").txt" - |
      grep -F -- "$PF_REPLAY_JOB" |
      while IFS= read -r _pf_new; do
        rm -rf "${_pf_dir:?}/${_pf_new:?}"
      done || true
  done
}

# post-gate-replay: the exact ci.yml run lines after Gate, read from ci.yml
# itself, with GITHUB_JOB, GITHUB_RUN_ID (fresh), GITHUB_RUN_ATTEMPT and GITHUB_SHA.
check_post_gate_replay() {
  if [ "$PF_NO_REPLAY" -eq 1 ]; then
    echo "[preflight] post-gate-replay skipped: --no-replay given (valid only for a plan that changes no retained-evidence producer)"
    return 0
  fi
  for _pf_old in "build/evidence/combined/$PF_REPLAY_JOB" "build/evidence/$PF_REPLAY_JOB.json"; do
    if [ -e "$_pf_old" ]; then
      echo "[preflight] post-gate-replay: $_pf_old already exists and was not created by this run; remove it (it is ignored build output) and rerun" >&2
      return 1
    fi
  done
  fresh_work
  replay_snapshot
  awk -v job="$PF_REPLAY_JOB" -v first="$PF_REPLAY_FIRST" -v last="$PF_REPLAY_LAST" '
    /^  [A-Za-z0-9_-]+:[ \t]*$/ { injob = ($1 == job ":"); next }
    injob && /^      - name: / { cur = substr($0, 15); if (cur == first) on = 1; next }
    injob && on && /^        run: / { printf "%s\t%s\n", cur, substr($0, 14); if (cur == last) { on = 0; done = 1 }; next }
    END { exit done ? 0 : 1 }
  ' .github/workflows/ci.yml > "$PF_WORK/replay-steps.txt" || {
    echo "[preflight] post-gate-replay: could not read the steps '$PF_REPLAY_FIRST' to '$PF_REPLAY_LAST' of job $PF_REPLAY_JOB from .github/workflows/ci.yml; update the anchors of this script" >&2
    return 1
  }
  _pf_sha=$(git rev-parse HEAD)
  _pf_rid=$(date +%s)
  while [ -n "$(ls -d "build/gate/08-05-$PF_REPLAY_JOB-$_pf_rid-1-$_pf_sha-"* "build/gate/09-hosted-$PF_REPLAY_JOB-$_pf_rid-1-$_pf_sha-"* 2>/dev/null | head -n 1)" ]; do
    _pf_rid=$((_pf_rid + 1))
  done
  _pf_tab=$(printf '\t')
  _pf_n=0
  while IFS=$_pf_tab read -r _pf_sname _pf_scmd; do
    _pf_n=$((_pf_n + 1))
    if ! (
      export GITHUB_JOB="$PF_REPLAY_JOB" GITHUB_RUN_ID="$_pf_rid" GITHUB_RUN_ATTEMPT=1 GITHUB_SHA="$_pf_sha"
      sh -c "$_pf_scmd"
    ) > "$PF_WORK/replay-step-$_pf_n.log" 2>&1; then
      tail -n 20 "$PF_WORK/replay-step-$_pf_n.log" >&2
      echo "[preflight] post-gate-replay: step $_pf_n '$_pf_sname' failed (the kind of defect that only a hosted run showed, F-107)" >&2
      _pf_dirty=$(git status --porcelain --untracked-files=no -- . ':(exclude).planning' | awk 'END { print NR + 0 }')
      if [ "$_pf_dirty" -gt 0 ]; then
        echo "[preflight] post-gate-replay: $_pf_dirty tracked file(s) outside .planning/ differ from HEAD; the emit steps refuse hosted source inputs that differ from the selected clean commit (as a hosted checkout is clean). Commit them and rerun, or use --no-replay while the plan is mid-task" >&2
      fi
      replay_cleanup "$_pf_rid" "$_pf_sha"
      return 1
    fi
  done < "$PF_WORK/replay-steps.txt"
  replay_cleanup "$_pf_rid" "$_pf_sha"
  echo "[preflight] post-gate-replay: $_pf_n ci.yml steps replayed with run id $_pf_rid, outputs removed"
  return 0
}

step oracle-authority check_oracle_authority
step ctest-counts check_ctest_counts
step host-load check_host_load
step head-ci check_head_ci
step timeout-audit check_timeout_audit
step pii check_pii
step linux-compile check_linux_compile
step post-gate-replay check_post_gate_replay

rm -rf "$PF_WORK"
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

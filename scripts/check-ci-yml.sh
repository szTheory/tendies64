#!/bin/sh
# check-ci-yml.sh: static hardening checks for the CI workflow (VER-04, T-01-11 to T-01-13).
# Line-pattern awk, not a YAML parser, so it fails closed on anything it does not recognize.
#
# Usage: check-ci-yml.sh [--file PATH | --dir DIR] [--gate PATH] [--selftest]
#   --file PATH   one workflow file to check
#   --dir DIR     a workflow directory: every entry is checked (default: .github/workflows in
#                 the repository, so a second workflow file cannot escape the rules)
#   --gate PATH   script whose executable bit CI-GATE-EXEC checks (default scripts/gate.sh)
# Output: one line per violation, "RULE-ID path: message". Exit 0 clean, 1 violations, 2 tool error.
#
# Rules:
#   CI-RUN-LINE          every run line is exactly "run: ./scripts/gate.sh"; no block scalars;
#                        the run-line count equals the job count
#   CI-USES-PIN          every uses value, minus a trailing comment, ends in @ plus 40 lowercase hex
#   CI-RUNNER-LABEL      runs-on is exactly ubuntu-24.04 or macos-26
#   CI-LATEST-LABEL      no "-latest" runner label
#   CI-PR-TARGET         the privileged PR trigger never appears
#   CI-TRIGGER           the on: triggers are a subset of pull_request, push and workflow_dispatch;
#                        workflow_run, issue_comment, pull_request_review_comment and
#                        repository_dispatch never appear anywhere
#   CI-WORKFLOW-FILE     the workflow directory holds at least one .yml/.yaml file and nothing else
#   CI-PERMISSIONS-TOP   the top-level permissions value is the empty mapping
#   CI-PERMISSIONS-JOB   each job grants exactly contents: read
#   CI-PERSIST-CREDS     each checkout step sets persist-credentials: false
#   CI-FETCH-DEPTH       each checkout step sets fetch-depth: 0 (full-history rights scan)
#   CI-TIMEOUT           each job sets timeout-minutes from 1 to 10
#   CI-SECRETS           no secrets reference in any spelling (secrets.X, secrets['X'], secrets[...],
#                        toJSON(secrets)) and no token (github.token, github['token'], toJSON(github))
#   CI-UPLOAD-ARTIFACT   no upload-artifact or upload-pages-artifact step (GOV-07 CI artifact leg: the
#                        rights scan of an artifact staging directory is a deferred step, so the
#                        capability to publish workflow output stays closed until that step exists)
#   CI-EXPR-IN-RUN       no expression in a run line
#   CI-GATE-EXEC         the gate script is executable
#   CI-ZERO-JOBS         the workflow defines no jobs
set -eu
LC_ALL=C
export LC_ALL

SELF=$0
ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
ALL_RULES="CI-RUN-LINE CI-USES-PIN CI-RUNNER-LABEL CI-LATEST-LABEL CI-PR-TARGET CI-TRIGGER CI-WORKFLOW-FILE CI-PERMISSIONS-TOP CI-PERMISSIONS-JOB CI-PERSIST-CREDS CI-FETCH-DEPTH CI-TIMEOUT CI-SECRETS CI-UPLOAD-ARTIFACT CI-EXPR-IN-RUN CI-GATE-EXEC CI-ZERO-JOBS"

usage() {
  echo "usage: check-ci-yml.sh [--file PATH | --dir DIR] [--gate PATH] [--selftest]" >&2
}

# scan_yml FILE: print violations from the line-pattern checks.
scan_yml() {
  awk -v file="$1" '
    function viol(rule, msg) { printf "%s %s: line %d: %s\n", rule, file, NR, msg; nviol++ }
    function vjob(rule, msg) { printf "%s %s: job %s: %s\n", rule, file, job, msg; nviol++ }
    function closestep() {
      if (ischeckout) {
        if (!pc) { printf "CI-PERSIST-CREDS %s: job %s: checkout step lacks persist-credentials: false\n", file, job; nviol++ }
        if (!fd) { printf "CI-FETCH-DEPTH %s: job %s: checkout step lacks fetch-depth: 0\n", file, job; nviol++ }
      }
      ischeckout = 0; pc = 0; fd = 0
    }
    function closejob() {
      if (job != "") {
        closestep()
        if (!hascontents) vjob("CI-PERMISSIONS-JOB", "permissions must grant contents: read")
        if (!hastimeout) vjob("CI-TIMEOUT", "timeout-minutes is missing")
        if (!hasruns) vjob("CI-RUNNER-LABEL", "runs-on is missing")
      }
      job = ""
    }
    function trigger(t) {
      t = clean(t); gsub(/^["\047]|["\047]$/, "", t)
      if (t == "") return
      if (t != "pull_request" && t != "push" && t != "workflow_dispatch")
        viol("CI-TRIGGER", "trigger " t " is not one of pull_request, push, workflow_dispatch")
    }
    function clean(v) {
      sub(/[ \t]+#.*$/, "", v); sub(/^[ \t]+/, "", v); sub(/[ \t]+$/, "", v)
      return v
    }
    BEGIN { job = ""; injobs = 0; inon = 0 }
    { raw = $0; sub(/\r$/, "", raw) }
    raw ~ /^[ \t]*(#.*)?$/ { next }
    {
      line = raw
      match(line, /^ */); ind = RLENGTH
      body = substr(line, ind + 1)
      isitem = 0
      if (body ~ /^- /) { isitem = 1; sub(/^-[ \t]+/, "", body) }

      # Whole-line patterns.
      if (line ~ /pull_request_target/) viol("CI-PR-TARGET", "the privileged pull_request_target trigger is forbidden")
      if (line ~ /-latest/) viol("CI-LATEST-LABEL", "a -latest runner label is forbidden; use an explicit label")
      if (line ~ /(workflow_run|issue_comment|pull_request_review_comment|repository_dispatch)/) viol("CI-TRIGGER", "a privileged trigger name is forbidden")
      if (line ~ /(^|[^A-Za-z0-9_])secrets([^A-Za-z0-9_]|$)/ ||
          line ~ /github[ \t]*(\.[ \t]*token|\[[ \t]*["\047][ \t]*token[ \t]*["\047][ \t]*\])/ ||
          line ~ /[(,][ \t]*github[ \t]*[),]/) viol("CI-SECRETS", "secrets and the github token must not be referenced")
      if (line ~ /upload(-pages)?-artifact/) viol("CI-UPLOAD-ARTIFACT", "publishing workflow output is closed until an artifact staging scan step exists")
      if (line ~ /:[ \t]+[|>][-+0-9]*[ \t]*$/) viol("CI-RUN-LINE", "block scalars are forbidden")
      if (line ~ /(^|[^A-Za-z0-9_-])run[ \t]*:/) {
        nruns++
        if (body != "run: ./scripts/gate.sh") viol("CI-RUN-LINE", "a run line must be exactly: run: ./scripts/gate.sh")
        if (line ~ /\$\{\{/) viol("CI-EXPR-IN-RUN", "an expression in a run line is an injection path")
      }

      key = body; sub(/[ \t]*:.*/, "", key)
      val = ""
      p = index(body, ":")
      if (p > 0) val = clean(substr(body, p + 1))

      # Triggers: the on: mapping, list or scalar (quoted key spellings too).
      if (inon && ind > 0) {
        if (ind == 2 && isitem) trigger(body)
        else if (ind == 2) trigger(key)
        next
      }

      # Top level.
      if (ind == 0 && !isitem) {
        inon = 0
        k2 = key; gsub(/^["\047]|["\047]$/, "", k2)
        if (k2 == "on" || k2 == "true") {
          inon = 1
          if (val != "") {
            v2 = val; gsub(/^\[|\]$/, "", v2)
            nt = split(v2, tl, ",")
            for (ti = 1; ti <= nt; ti++) trigger(tl[ti])
          }
        }
        if (key == "jobs") { injobs = 1; sawjobs = 1 }
        else { closejob(); injobs = 0 }
        if (key == "permissions") { sawtop = 1; topok = (val == "{}") }
        next
      }

      # Job header: two-space key directly under jobs.
      if (injobs && ind == 2 && !isitem && body ~ /^[A-Za-z0-9_-]+:[ \t]*(#.*)?$/) {
        closejob()
        job = key; njobs++
        hascontents = 0; hastimeout = 0; hasruns = 0; inperm = 0
        next
      }

      if (injobs && job != "") {
        if (ind == 4 && !isitem) {
          inperm = (key == "permissions" && val == "")
          if (key == "timeout-minutes") {
            if (val ~ /^[0-9]+$/ && val + 0 >= 1 && val + 0 <= 10) hastimeout = 1
            else viol("CI-TIMEOUT", "timeout-minutes must be an integer from 1 to 10")
          }
          if (key == "runs-on") {
            hasruns = 1
            if (val != "ubuntu-24.04" && val != "macos-26") viol("CI-RUNNER-LABEL", "runs-on must be exactly ubuntu-24.04 or macos-26, found: " val)
          }
        } else if (ind == 6 && !isitem && inperm) {
          if (clean(body) == "contents: read") hascontents = 1
          else viol("CI-PERMISSIONS-JOB", "a job may grant only contents: read, found: " clean(body))
        }
        if (ind == 6 && isitem) closestep()
      }

      if (key == "uses") {
        n = split(val, parts, "@")
        sha = parts[n]
        if (n < 2 || length(sha) != 40 || sha !~ /^[0-9a-f]+$/) viol("CI-USES-PIN", "uses must end in @ plus a 40-character lowercase hex SHA: " val)
        if (val ~ /^actions\/checkout@/) ischeckout = 1
      }
      if (key == "persist-credentials") pc = (val == "false")
      if (key == "fetch-depth") fd = (val == "0")
    }
    END {
      closejob()
      if (!sawtop || !topok) { printf "CI-PERMISSIONS-TOP %s: the top-level permissions value must be the empty mapping {}\n", file; nviol++ }
      if (njobs == 0) { printf "CI-ZERO-JOBS %s: the workflow defines no jobs\n", file; nviol++ }
      else if (nruns != njobs) { printf "CI-RUN-LINE %s: run-line count %d does not equal job count %d\n", file, nruns, njobs; nviol++ }
    }
  ' "$1"
}

# ---- selftest -------------------------------------------------------------------------------

FAILS=0
CASES=0

# expect RULE DESCRIPTION FILE [GATE [--dir]]: the checker must exit 1 and print
# the rule id. With --dir, FILE is a workflow directory.
expect() {
  CASES=$((CASES + 1))
  _g=${4:-$ROOT/scripts/gate.sh}
  [ -n "$_g" ] || _g=$ROOT/scripts/gate.sh
  _how=--file
  [ "${5:-}" != --dir ] || _how=--dir
  _out=$(sh "$SELF" $_how "$3" --gate "$_g" 2>&1) && _rc=0 || _rc=$?
  case "$_out" in
    *"$1 "*)
      if [ "$_rc" -eq 1 ]; then echo "ok $1: $2"; else echo "FAIL $1: $2 (exit $_rc, expected 1)"; FAILS=$((FAILS + 1)); fi
      ;;
    *) echo "FAIL $1: $2 (rule id not printed; exit $_rc)"; FAILS=$((FAILS + 1)) ;;
  esac
  SEEN="$SEEN $1"
}

# mut NAME SED-EXPR: write a mutated copy of the real workflow to $W/NAME.yml (never in place).
mut() {
  sed "$2" "$REAL" > "$W/$1.yml"
}

selftest() {
  REAL=$ROOT/.github/workflows/ci.yml
  if [ ! -f "$REAL" ]; then echo "check-ci-yml: workflow not found: $REAL" >&2; return 1; fi
  W=$(mktemp -d "${TMPDIR:-/tmp}/t64ci.XXXXXX")
  trap 'rm -rf "$W"' EXIT HUP INT TERM
  SEEN=""

  cp "$REAL" "$W/clean.yml"
  CASES=$((CASES + 1))
  if _out=$(sh "$SELF" --file "$W/clean.yml" 2>&1); then echo "ok clean: the unmodified copy passes"
  else echo "FAIL clean: the unmodified copy was rejected: $_out"; FAILS=$((FAILS + 1)); fi

  mut run-extra 's|run: ./scripts/gate.sh|run: ./scripts/gate.sh \&\& echo extra|'
  expect CI-RUN-LINE "a run line with extra commands" "$W/run-extra.yml"
  awk '/run: .\/scripts\/gate.sh/ && !done { print "        run: |"; print "          ./scripts/gate.sh"; done = 1; next } { print }' "$REAL" > "$W/run-block.yml"
  expect CI-RUN-LINE "a block scalar" "$W/run-block.yml"
  awk '/- name: Gate/ && !done { print; getline; print; print "      - run: echo second"; done = 1; next } { print }' "$REAL" > "$W/run-count.yml"
  expect CI-RUN-LINE "a second run line" "$W/run-count.yml"

  mut uses-tag 's|actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1  # v7.0.1|actions/checkout@v4  # v4|'
  expect CI-USES-PIN "a tag instead of a SHA" "$W/uses-tag.yml"
  mut uses-short 's|@3d3c42e5aac5ba805825da76410c181273ba90b1|@3d3c42e5aac5ba805825da76410c181273ba90b|'
  expect CI-USES-PIN "a 39-character SHA" "$W/uses-short.yml"

  mut runner 's|ubuntu-24.04|ubuntu-22.04|'
  expect CI-RUNNER-LABEL "an unlisted runner label" "$W/runner.yml"
  mut latest 's|macos-26|macos-latest|'
  expect CI-LATEST-LABEL "a -latest label" "$W/latest.yml"
  mut prtarget 's|^  pull_request:|  pull_request_target:|'
  expect CI-PR-TARGET "the privileged PR trigger" "$W/prtarget.yml"
  mut permtop 's|^permissions: {}|permissions: write-all|'
  expect CI-PERMISSIONS-TOP "a non-empty top-level permissions value" "$W/permtop.yml"
  mut permjob 's|      contents: read|      contents: write|'
  expect CI-PERMISSIONS-JOB "a job that grants contents: write" "$W/permjob.yml"
  mut persist 's|persist-credentials: false|persist-credentials: true|'
  expect CI-PERSIST-CREDS "persisted credentials" "$W/persist.yml"
  mut fetch 's|fetch-depth: 0|fetch-depth: 1|'
  expect CI-FETCH-DEPTH "a shallow checkout" "$W/fetch.yml"
  grep -v 'timeout-minutes' "$REAL" > "$W/timeout.yml" || true
  expect CI-TIMEOUT "a job without a timeout" "$W/timeout.yml"
  awk '/timeout-minutes: 10/ && !done { print; print "    env:"; print "      TOKEN: ${{ secrets.TOKEN }}"; done = 1; next } { print }' "$REAL" > "$W/secrets.yml"
  expect CI-SECRETS "a secrets reference" "$W/secrets.yml"
  awk '/^    steps:/ && !done { print; print "      - uses: actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9  # v7.0.2"; done = 1; next } { print }' "$REAL" > "$W/upload.yml"
  expect CI-UPLOAD-ARTIFACT "an upload-artifact step" "$W/upload.yml"
  mut expr 's|run: ./scripts/gate.sh|run: ./scripts/gate.sh ${{ github.head_ref }}|'
  expect CI-EXPR-IN-RUN "an expression in a run line" "$W/expr.yml"

  # WR-15: every spelling of a secret or the token, in an env value.
  _n=0
  for _e in "\${{ secrets['TOKEN'] }}" "\${{ secrets[format('{0}', 'T')] }}" \
    "\${{ toJSON(secrets) }}" "\${{ github['token'] }}" "\${{ github.token }}" \
    "\${{ github [ \"token\" ] }}" "\${{ toJSON(github) }}"; do
    _n=$((_n + 1))
    awk -v e="$_e" '/timeout-minutes: 10/ && !done { print; print "    env:"; print "      V: " e; done = 1; next } { print }' "$REAL" > "$W/secret$_n.yml"
    expect CI-SECRETS "secret or token spelling $_n" "$W/secret$_n.yml"
  done
  # WR-15: privileged and unlisted triggers, in block and flow form.
  for _t in workflow_run issue_comment pull_request_review_comment repository_dispatch schedule; do
    sed "s|^  workflow_dispatch:|  $_t:|" "$REAL" > "$W/trig-$_t.yml"
    expect CI-TRIGGER "the $_t trigger" "$W/trig-$_t.yml"
  done
  awk '/^on:/ { print "on: [push, workflow_run]"; skip = 1; next } skip && /^[^ ]/ { skip = 0 } !skip { print }' "$REAL" > "$W/trig-flow.yml"
  expect CI-TRIGGER "a privileged trigger in flow form" "$W/trig-flow.yml"
  awk '/^on:/ { print "\"on\":"; next } { print }' "$REAL" | sed 's|^  push:|  - issue_comment|' > "$W/trig-list.yml"
  expect CI-TRIGGER "a quoted on key with a list item" "$W/trig-list.yml"
  awk '/^on:/ { print "on: issue_comment"; skip = 1; next } skip && /^[^ ]/ { skip = 0 } !skip { print }' "$REAL" > "$W/trig-scalar.yml"
  expect CI-TRIGGER "a scalar on value" "$W/trig-scalar.yml"

  # WR-15: directory mode checks every workflow file, not only ci.yml.
  mkdir "$W/wf-second" "$W/wf-other" "$W/wf-empty" "$W/wf-ok"
  cp "$REAL" "$W/wf-second/ci.yml"
  sed 's|^  pull_request:|  pull_request_target:|' "$REAL" > "$W/wf-second/release.yaml"
  expect CI-PR-TARGET "a second workflow file with the privileged trigger" "$W/wf-second" "" --dir
  cp "$REAL" "$W/wf-other/ci.yml"
  printf 'name: x\n' > "$W/wf-other/notes.txt"
  expect CI-WORKFLOW-FILE "a non-YAML file in the workflow directory" "$W/wf-other" "" --dir
  expect CI-WORKFLOW-FILE "a workflow directory with no workflow" "$W/wf-empty" "" --dir
  cp "$REAL" "$W/wf-ok/ci.yml"
  cp "$REAL" "$W/wf-ok/second.yml"
  CASES=$((CASES + 1))
  if _out=$(sh "$SELF" --dir "$W/wf-ok" 2>&1); then echo "ok dir-clean: two hardened workflow files pass"
  else echo "FAIL dir-clean: two hardened workflow files were rejected: $_out"; FAILS=$((FAILS + 1)); fi

  cp "$ROOT/scripts/gate.sh" "$W/gate-noexec.sh"
  chmod 644 "$W/gate-noexec.sh"
  expect CI-GATE-EXEC "a gate script without the executable bit" "$REAL" "$W/gate-noexec.sh"

  awk '/^jobs:/ { print; exit } { print }' "$REAL" > "$W/zero.yml"
  expect CI-ZERO-JOBS "a workflow with no jobs" "$W/zero.yml"

  for _r in $ALL_RULES; do
    case " $SEEN " in
      *" $_r "*) ;;
      *) echo "FAIL $_r: no seeded mutation asserts this rule"; FAILS=$((FAILS + 1)) ;;
    esac
  done

  echo "check-ci-yml selftest: cases=$CASES failures=$FAILS"
  if [ "$FAILS" -ne 0 ]; then return 1; fi
  return 0
}

# ---- main -----------------------------------------------------------------------------------

FILE=""
DIR=$ROOT/.github/workflows
GATE=$ROOT/scripts/gate.sh
MODE=check
while [ $# -gt 0 ]; do
  case "$1" in
    --file)
      if [ $# -lt 2 ]; then usage; exit 2; fi
      FILE=$2
      DIR=""
      shift 2
      ;;
    --dir)
      if [ $# -lt 2 ]; then usage; exit 2; fi
      DIR=$2
      FILE=""
      shift 2
      ;;
    --gate)
      if [ $# -lt 2 ]; then usage; exit 2; fi
      GATE=$2
      shift 2
      ;;
    --selftest)
      MODE=selftest
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [ "$MODE" = "selftest" ]; then
  if selftest; then exit 0; else exit 1; fi
fi

OUT=""
# add_out TEXT: appends lines to OUT.
add_out() {
  [ -n "$1" ] || return 0
  if [ -n "$OUT" ]; then OUT="$OUT
$1"; else OUT=$1; fi
}
if [ -n "$FILE" ]; then
  if [ ! -f "$FILE" ]; then
    echo "check-ci-yml: workflow not found: $FILE" >&2
    exit 2
  fi
  _o=$(scan_yml "$FILE") || { echo "check-ci-yml: tool error while scanning $FILE" >&2; exit 2; }
  add_out "$_o"
else
  [ -d "$DIR" ] || { echo "check-ci-yml: workflow directory not found: $DIR" >&2; exit 2; }
  _nwf=0
  for _f in "$DIR"/* "$DIR"/.[!.]*; do
    [ -e "$_f" ] || [ -L "$_f" ] || continue
    case "$_f" in
      *.yml | *.yaml)
        if [ -f "$_f" ] && [ ! -L "$_f" ]; then
          _nwf=$((_nwf + 1))
          _o=$(scan_yml "$_f") || { echo "check-ci-yml: tool error while scanning $_f" >&2; exit 2; }
          add_out "$_o"
          continue
        fi
        ;;
    esac
    add_out "CI-WORKFLOW-FILE $_f: only regular .yml or .yaml workflow files may live in the workflow directory"
  done
  if [ "$_nwf" -eq 0 ]; then
    add_out "CI-WORKFLOW-FILE $DIR: the workflow directory holds no workflow file"
  fi
fi
if [ ! -x "$GATE" ]; then
  add_out "CI-GATE-EXEC $GATE: the gate script is missing or not executable"
fi
if [ -n "$OUT" ]; then
  printf '%s\n' "$OUT"
  exit 1
fi
echo "check-ci-yml: ok"
exit 0

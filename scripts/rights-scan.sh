#!/bin/sh
# GOV-07 rights scan driver (POSIX sh). Enumerates bytes and hands them to
# t64-blobscan, which classifies them; the driver owns the git-level rules.
#
#   rights-scan.sh [--tree] [--history] [--range BASE..HEAD] [--staging DIR]
#                  [--allow TSV]... [--selftest]
#
# With no scope flag it scans the working tree and all reachable history
# (a superset of any PR range). --range limits history to BASE..HEAD.
# provenance/docs-assets.tsv is always added to the allowlist when present.
# The binary is ${T64_BUILD_DIR:-build/gate}/tools/rights/t64-blobscan.
#
# Rules owned here: R-SHALLOW, R-NOT-GIT. Every other rule id comes from
# t64-blobscan. Output is "RULE <id> <path> [oid]" and a final summary line.
# Exit 0 clean, 1 findings, 2 harness error.
# Every pipeline stage goes through a temp file so its status is checked
# (dash masks pipeline failures).
set -eu
LC_ALL=C
export LC_ALL

SCRIPT_ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
case $0 in
  /*) SELF=$0 ;;
  *) SELF=$(pwd -P)/$0 ;;
esac

BUILD=${T64_BUILD_DIR:-build/gate}
case $BUILD in
  /*) ;;
  *) BUILD=$SCRIPT_ROOT/$BUILD ;;
esac
BLOBSCAN=$BUILD/tools/rights/t64-blobscan

DO_TREE=0
DO_HISTORY=0
RANGE=""
STAGING=""
DO_SELFTEST=0
EXPLICIT=0
TMP=""
ALLOW_LIST=""
HARNESS=0
TREE_N="skipped"
HIST_N="skipped"

fatal() {
  echo "rights-scan: error: $*" >&2
  exit 2
}

cleanup() {
  if [ -n "$TMP" ] && [ -d "$TMP" ]; then
    rm -rf "$TMP"
  fi
}

usage() {
  echo "usage: rights-scan.sh [--tree] [--history] [--range BASE..HEAD] [--staging DIR] [--allow TSV]... [--selftest]" >&2
  exit 2
}

# run_blobscan MODE_ARGS...: runs t64-blobscan with the allowlists appended.
# Prints nothing; stdout and stderr are in $TMP/bs.out, status in BS_RC.
run_blobscan() {
  _bs_in=$1
  shift
  set -- "$BLOBSCAN" "$@"
  if [ -s "$ALLOW_LIST" ]; then
    while IFS= read -r _a; do
      set -- "$@" --allow "$_a"
    done < "$ALLOW_LIST"
  fi
  BS_RC=0
  "$@" < "$_bs_in" > "$TMP/bs.out" 2>&1 || BS_RC=$?
}

# absorb LABEL: copies $TMP/bs.out into the report (minus the summary line),
# records the scanned count in SCANNED_N and notes harness errors.
absorb() {
  if grep -v '^blobscan: scanned=' "$TMP/bs.out" >> "$TMP/report"; then :; fi
  SCANNED_N=$(sed -n 's/^blobscan: scanned=\([0-9][0-9]*\) .*/\1/p' "$TMP/bs.out")
  if [ "$BS_RC" -ge 2 ]; then
    HARNESS=1
  fi
  if [ "$BS_RC" -ne 0 ] && [ "$BS_RC" -ne 1 ] && [ "$BS_RC" -ne 2 ]; then
    echo "rights-scan: t64-blobscan exited $BS_RC" >> "$TMP/report"
  fi
}

scan_tree() {
  git ls-files -z --cached --others --exclude-standard > "$TMP/tree.z" ||
    fatal "git ls-files failed"
  run_blobscan "$TMP/tree.z" --files
  absorb tree
  TREE_N=${SCANNED_N:-0}
}

scan_history() {
  if [ -n "$RANGE" ]; then
    _spec=$RANGE
  else
    _spec=HEAD
  fi
  git rev-list --objects "$_spec" > "$TMP/revlist" || fatal "git rev-list failed"
  git -c core.quotepath=false log -m --raw --no-abbrev --no-renames --no-color \
    --no-ext-diff --ignore-submodules=none --format= "$_spec" > "$TMP/log.raw" ||
    fatal "git log failed"
  # Every (blob, path) pair that ever appeared: rev-list names each object once,
  # so renames and copies need the raw log to report every name.
  awk -F'\t' '/^:/ {
      split($1, m, " ")
      if (m[2] != "160000" && m[4] !~ /^0+$/) print m[4] " " $2
    }' "$TMP/log.raw" > "$TMP/extra.names" || fatal "awk failed"
  cat "$TMP/revlist" "$TMP/extra.names" > "$TMP/names.all" || fatal "cat failed"
  sort -u "$TMP/names.all" > "$TMP/names" || fatal "sort failed"
  awk '{print $1}' "$TMP/revlist" > "$TMP/oids.all" || fatal "awk failed"
  git cat-file --batch-check='%(objectname) %(objecttype)' < "$TMP/oids.all" \
    > "$TMP/types" || fatal "git cat-file --batch-check failed"
  awk '$2 == "blob" {print $1}' "$TMP/types" > "$TMP/oids" || fatal "awk failed"
  git cat-file --batch < "$TMP/oids" > "$TMP/stream" || fatal "git cat-file --batch failed"
  run_blobscan "$TMP/stream" --git-batch "$TMP/names"
  absorb history
  HIST_N=${SCANNED_N:-0}
}

scan_main() {
  : > "$TMP/report"
  ALLOW_LIST=$TMP/allow.list
  : > "$ALLOW_LIST"
  if [ -s "$TMP/allow.cli" ]; then
    cat "$TMP/allow.cli" >> "$ALLOW_LIST"
  fi

  if [ "$DO_TREE" -eq 1 ] || [ "$DO_HISTORY" -eq 1 ]; then
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
      echo "RULE R-NOT-GIT ." >> "$TMP/report"
      DO_TREE=0
      DO_HISTORY=0
    else
      _top=$(git rev-parse --show-toplevel) || fatal "no work tree"
      cd "$_top"
      if [ -f provenance/docs-assets.tsv ]; then
        echo "$_top/provenance/docs-assets.tsv" >> "$ALLOW_LIST"
      fi
      if [ "$DO_HISTORY" -eq 1 ] &&
        [ "$(git rev-parse --is-shallow-repository)" = "true" ]; then
        echo "RULE R-SHALLOW ." >> "$TMP/report"
        DO_HISTORY=0
      fi
    fi
  fi

  [ -x "$BLOBSCAN" ] || fatal "missing $BLOBSCAN (build first: cmake --build --preset gate)"
  [ "$DO_TREE" -eq 1 ] && scan_tree
  [ "$DO_HISTORY" -eq 1 ] && scan_history

  cat "$TMP/report"
  _f=$(grep -c '^RULE ' "$TMP/report" || true)
  echo "rights-scan: tree=$TREE_N history=$HIST_N findings=$_f"
  if [ "$HARNESS" -eq 1 ]; then
    exit 2
  fi
  if [ "$_f" -gt 0 ]; then
    exit 1
  fi
  exit 0
}

# ---------------------------------------------------------------- selftest

# Every protected-looking input is generated here at run time under a mktemp
# root outside the repository and removed on exit; nothing is committed.
g() {
  git -c user.name=t64-selftest -c user.email=t64-selftest@example.invalid \
    -c commit.gpgsign=false -c core.autocrlf=false "$@"
}

ST_CASES=0
ST_FAIL=0
NEED=""
NEED2=""
ABSENT=""

sha256_of() {
  if command -v sha256sum > /dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

# new_repo NAME: creates $ST/NAME as an empty git repository.
new_repo() {
  mkdir "$ST/$1"
  (cd "$ST/$1" && git init -q > /dev/null 2>&1)
}

commit_all() {
  (cd "$1" && g add -A && g commit -q -m "$2")
}

# case_run NAME RC RULE DIR ARGS...: runs the driver in DIR and checks the exit
# code, that RULE (or no RULE line when "-") is printed, and the optional
# NEED, NEED2 (must match) and ABSENT (must not be a RULE id) variables.
case_run() {
  _name=$1
  _rc=$2
  _rule=$3
  _dir=$4
  shift 4
  _out=$ST/out.$_name
  _got=0
  (cd "$_dir" && sh "$SELF" "$@") > "$_out" 2>&1 || _got=$?
  _why=""
  [ "$_got" -eq "$_rc" ] || _why="exit $_got, wanted $_rc"
  if [ -z "$_why" ]; then
    if [ "$_rule" = "-" ]; then
      if grep -q '^RULE ' "$_out"; then _why="unexpected RULE line"; fi
    else
      grep -q "^RULE $_rule " "$_out" || _why="no RULE $_rule line"
    fi
  fi
  if [ -z "$_why" ] && [ -n "$NEED" ]; then
    grep -q "$NEED" "$_out" || _why="output lacks $NEED"
  fi
  if [ -z "$_why" ] && [ -n "$NEED2" ]; then
    grep -q "$NEED2" "$_out" || _why="output lacks $NEED2"
  fi
  if [ -z "$_why" ] && [ -n "$ABSENT" ]; then
    if grep -q "^RULE $ABSENT " "$_out"; then _why="unexpected RULE $ABSENT"; fi
  fi
  NEED=""
  NEED2=""
  ABSENT=""
  ST_CASES=$((ST_CASES + 1))
  if [ -z "$_why" ]; then
    echo "ok $_name"
  else
    ST_FAIL=$((ST_FAIL + 1))
    echo "FAIL $_name: $_why"
    sed 's/^/    /' "$_out" | head -n 12
  fi
}

# direct_run NAME RC RULE INPUT ARGS...: runs t64-blobscan directly.
direct_run() {
  _name=$1
  _rc=$2
  _rule=$3
  _in=$4
  shift 4
  _out=$ST/out.$_name
  _got=0
  "$BLOBSCAN" "$@" < "$_in" > "$_out" 2>&1 || _got=$?
  _why=""
  [ "$_got" -eq "$_rc" ] || _why="exit $_got, wanted $_rc"
  if [ -z "$_why" ]; then
    grep -q "^RULE $_rule " "$_out" || _why="no RULE $_rule line"
  fi
  ST_CASES=$((ST_CASES + 1))
  if [ -z "$_why" ]; then
    echo "ok $_name"
  else
    ST_FAIL=$((ST_FAIL + 1))
    echo "FAIL $_name: $_why"
    sed 's/^/    /' "$_out" | head -n 12
  fi
}

selftest() {
  [ -x "$BLOBSCAN" ] || fatal "missing $BLOBSCAN (build first: cmake --build --preset gate)"
  ST=$(mktemp -d "${TMPDIR:-/tmp}/t64.XXXXXX") || fatal "mktemp failed"
  TMP=$ST
  trap cleanup EXIT
  trap 'exit 130' INT TERM HUP
  echo "selftest-root: $ST"
  unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE
  GIT_CONFIG_GLOBAL=/dev/null
  GIT_CONFIG_NOSYSTEM=1
  export GIT_CONFIG_GLOBAL GIT_CONFIG_NOSYSTEM
  ROM_Z64='\200\067\022\100'
  ROM_V64='\067\200\100\022'
  ROM_N64='\100\022\067\200'

  # clean: text only, tree and history both scanned.
  new_repo clean
  printf 'hello\n' > "$ST/clean/a.txt"
  commit_all "$ST/clean" init
  NEED='tree=[1-9]'
  NEED2='history=[1-9]'
  case_run clean 0 - "$ST/clean"

  # ROM magic in each byte order, hidden behind a .txt name (tree).
  new_repo rom-z64
  printf "${ROM_Z64}"'tail\n' > "$ST/rom-z64/x.txt"
  case_run rom-z64-tree 1 R-ROM-MAGIC "$ST/rom-z64" --tree
  new_repo rom-v64
  printf "${ROM_V64}"'tail\n' > "$ST/rom-v64/x.txt"
  case_run rom-v64-tree 1 R-ROM-MAGIC "$ST/rom-v64" --tree
  new_repo rom-n64
  printf "${ROM_N64}"'tail\n' > "$ST/rom-n64/x.txt"
  case_run rom-n64-tree 1 R-ROM-MAGIC "$ST/rom-n64" --tree

  # add-then-delete: gone from the tree, still in history.
  new_repo rom-del
  printf 'readme\n' > "$ST/rom-del/README.txt"
  printf "${ROM_Z64}"'tail\n' > "$ST/rom-del/data.txt"
  commit_all "$ST/rom-del" add
  (cd "$ST/rom-del" && g rm -q data.txt && g commit -q -m remove)
  case_run rom-add-then-delete 1 R-ROM-MAGIC "$ST/rom-del" --history
  case_run rom-deleted-tree-only 0 - "$ST/rom-del" --tree

  # shallow clone: the history scan would be vacuous, so it fails.
  new_repo deep
  printf 'one\n' > "$ST/deep/a.txt"
  commit_all "$ST/deep" one
  printf 'two\n' > "$ST/deep/a.txt"
  commit_all "$ST/deep" two
  git clone -q --depth 1 "file://$ST/deep" "$ST/shallow" > /dev/null 2>&1
  case_run shallow 1 R-SHALLOW "$ST/shallow"

  # empty input: nothing scanned is a failure, not a pass.
  : > "$ST/empty.in"
  direct_run zero-scanned 2 R-ZERO-SCANNED "$ST/empty.in" --files
  direct_run zero-scanned-batch 2 R-ZERO-SCANNED "$ST/empty.in" --git-batch "$ST/empty.in"

  # allowlisted by path and sha256 passes; a wrong digest does not.
  new_repo allow
  printf "${ROM_Z64}"'asset\n' > "$ST/allow/asset.bin"
  _sha=$(sha256_of "$ST/allow/asset.bin")
  printf '%s\t%s\t%s\n' asset.bin "$_sha" 'selftest' > "$ST/allow.tsv"
  NEED='tree=1'
  case_run allowlisted 0 - "$ST/allow" --tree --allow "$ST/allow.tsv"
  printf '%s\t%s\t%s\n' asset.bin \
    0000000000000000000000000000000000000000000000000000000000000000 'selftest' \
    > "$ST/allow-bad.tsv"
  case_run allowlist-wrong-sha 1 R-ROM-MAGIC "$ST/allow" --tree --allow "$ST/allow-bad.tsv"
  commit_all "$ST/allow" add
  case_run allowlisted-history 0 - "$ST/allow" --history --allow "$ST/allow.tsv"

  echo "selftest: cases=$ST_CASES failures=$ST_FAIL"
  [ "$ST_FAIL" -eq 0 ] || exit 1
  exit 0
}

# --------------------------------------------------------------------- main

ALLOW_CLI=""
while [ $# -gt 0 ]; do
  case $1 in
    --tree) DO_TREE=1; EXPLICIT=1 ;;
    --history) DO_HISTORY=1; EXPLICIT=1 ;;
    --range)
      [ $# -ge 2 ] || usage
      RANGE=$2
      DO_HISTORY=1
      EXPLICIT=1
      shift
      ;;
    --staging)
      [ $# -ge 2 ] || usage
      STAGING=$2
      EXPLICIT=1
      shift
      ;;
    --allow)
      [ $# -ge 2 ] || usage
      ALLOW_CLI="${ALLOW_CLI}$(cd "$(dirname "$2")" && pwd -P)/$(basename "$2")
"
      shift
      ;;
    --selftest) DO_SELFTEST=1 ;;
    *) usage ;;
  esac
  shift
done

if [ "$DO_SELFTEST" -eq 1 ]; then
  selftest
fi

if [ "$EXPLICIT" -eq 0 ]; then
  DO_TREE=1
  DO_HISTORY=1
fi

TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64.XXXXXX") || fatal "mktemp failed"
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
printf '%s' "$ALLOW_CLI" > "$TMP/allow.cli"
scan_main

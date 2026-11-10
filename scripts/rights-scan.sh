#!/bin/sh
# GOV-07 rights scan driver (POSIX sh). Enumerates bytes and hands them to
# t64-blobscan, which classifies them; the driver owns the git-level rules.
#
#   rights-scan.sh [--tree] [--history] [--range BASE..HEAD] [--staging DIR]
#                  [--allow TSV]... [--selftest]
#
# With no scope flag it scans the working tree and all history reachable from
# HEAD and every local branch, tag and remote-tracking ref (a superset of any
# PR range). --range limits history to BASE..HEAD (or one revision); it is
# validated and passed to git after --end-of-options.
# provenance/docs-assets.tsv is always added to the allowlist when present.
# fixtures/manifest.json is the second allowlist source (GOV-03): when the
# scanned repository has one, t64-manifest-check --emit-allowlist turns its
# authored-here/cleared commit-channel entries into extra rows. A manifest with
# any finding emits nothing and fails the scan (exit 2), so a bad entry can
# never whitelist protected bytes.
# The binaries are ${T64_BUILD_DIR:-build/gate}/tools/rights/t64-blobscan and
# ${T64_BUILD_DIR:-build/gate}/tools/manifest/t64-manifest-check.
#
# Rules owned here: R-SHALLOW, R-NOT-GIT, R-LFS-ATTR, R-GITLINK, and R-SYMLINK
# for a staging directory. Every other rule id comes from t64-blobscan.
# --staging DIR scans the files of an artifact staging directory (paths are
# relative to DIR for allowlist matching); no CI step calls it yet because
# Phase 1 uploads no artifacts, so the selftest is its proof. Output is "RULE <id> <path> [oid]" and a final summary line.
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
MANIFEST_CHECK=$BUILD/tools/manifest/t64-manifest-check
MANIFEST_SCHEMA=$SCRIPT_ROOT/fixtures/manifest.schema.json

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
STAGE_N=""
BS_CWD=""

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
  (
    if [ -n "$BS_CWD" ]; then cd "$BS_CWD" || exit 125; fi
    exec "$@" < "$_bs_in" > "$TMP/bs.out" 2>&1
  ) || BS_RC=$?
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

  # R-GITLINK: index entries of mode 160000.
  git -c core.quotepath=false ls-files -s > "$TMP/index.s" || fatal "git ls-files -s failed"
  awk -F'\t' '{ split($1, m, " "); if (m[1] == "160000") print "RULE R-GITLINK " $2 }' \
    "$TMP/index.s" >> "$TMP/report" || fatal "awk failed"

  # R-LFS-ATTR: any tracked or untracked .gitattributes that names the lfs filter.
  # -a: an attribute that marks the file itself binary must not hide it.
  _rc=0
  git -c core.quotepath=false grep -a -l -F -e 'filter=lfs' --untracked \
    -- ':(glob)**/.gitattributes' > "$TMP/lfs.files" || _rc=$?
  [ "$_rc" -le 1 ] || fatal "git grep failed"
  awk '{ print "RULE R-LFS-ATTR " $0 }' "$TMP/lfs.files" >> "$TMP/report" ||
    fatal "awk failed"
}

scan_staging() {
  [ -d "$STAGING" ] || fatal "staging directory not found: $STAGING"
  (cd "$STAGING" && find . -type l -print) > "$TMP/stage.links" ||
    fatal "find failed"
  awk '{ sub(/^\.\//, ""); print "RULE R-SYMLINK " $0 }' "$TMP/stage.links" \
    >> "$TMP/report" || fatal "awk failed"
  (cd "$STAGING" && find . ! -type d ! -type l -print0) > "$TMP/stage.z" ||
    fatal "find failed"
  BS_CWD=$STAGING
  run_blobscan "$TMP/stage.z" --files
  BS_CWD=""
  absorb staging
  STAGE_N=${SCANNED_N:-0}
}

# check_range: rejects a --range value that git could parse as an option or
# that does not name commits. Accepted: BASE..HEAD or one revision; no side
# may be empty, start with "-" or contain "..", and each side must resolve to
# a commit. The value is then passed after --end-of-options (git 2.24+).
check_range() {
  case $RANGE in
    *...*) fatal "--range must be BASE..HEAD or one revision: $RANGE" ;;
    *..*)
      _ra=${RANGE%%..*}
      _rb=${RANGE#*..}
      ;;
    *)
      _ra=$RANGE
      _rb=$RANGE
      ;;
  esac
  for _side in "$_ra" "$_rb"; do
    case $_side in
      "" | -* | *..*) fatal "invalid --range: $RANGE" ;;
    esac
    git rev-parse --verify -q "$_side^{commit}" > /dev/null 2>&1 ||
      fatal "--range side does not name a commit: $_side"
  done
}

# revs CMD...: runs CMD with the revision arguments of the history scope:
# the validated --range, or every local branch, tag and remote-tracking ref
# plus HEAD (a detached CI checkout), so history on other refs of a full
# clone is scanned too.
revs() {
  if [ -n "$RANGE" ]; then
    "$@" --end-of-options "$RANGE" --
  elif git rev-parse --verify -q HEAD > /dev/null 2>&1; then
    "$@" --branches --tags --remotes --end-of-options HEAD --
  else
    "$@" --branches --tags --remotes --
  fi
}

scan_history() {
  if [ -n "$RANGE" ]; then check_range; fi
  revs git rev-list --objects > "$TMP/revlist" || fatal "git rev-list failed"
  revs git -c core.quotepath=false log -m --raw --no-abbrev --no-renames --no-color \
    --no-ext-diff --ignore-submodules=none --format= > "$TMP/log.raw" ||
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

  # R-GITLINK: a gitlink is not an object in rev-list, but every gitlink that
  # ever entered a tree shows up as a mode-160000 entry in the raw log.
  awk -F'\t' '/^:/ { split($1, m, " ")
      if (m[1] == ":160000" || m[2] == "160000") print $2 }' "$TMP/log.raw" \
    > "$TMP/gitlinks.all" || fatal "awk failed"
  sort -u "$TMP/gitlinks.all" > "$TMP/gitlinks" || fatal "sort failed"
  awk '{ print "RULE R-GITLINK " $0 }' "$TMP/gitlinks" >> "$TMP/report" ||
    fatal "awk failed"

  # R-LFS-ATTR in history: every blob ever named .gitattributes.
  awk '{ i = index($0, " "); if (i == 0) next
      path = substr($0, i + 1); n = split(path, a, "/")
      if (a[n] == ".gitattributes") print $1 " " path }' "$TMP/names" \
    > "$TMP/attr.names" || fatal "awk failed"
  while IFS= read -r _line; do
    _oid=${_line%% *}
    _path=${_line#* }
    git cat-file blob "$_oid" > "$TMP/attr.blob" || fatal "git cat-file blob failed"
    if grep -a -q -F 'filter=lfs' "$TMP/attr.blob"; then
      echo "RULE R-LFS-ATTR $_path $_oid" >> "$TMP/report"
    fi
  done < "$TMP/attr.names"
}

scan_main() {
  : > "$TMP/report"
  ALLOW_LIST=$TMP/allow.list
  : > "$ALLOW_LIST"
  if [ -s "$TMP/allow.cli" ]; then
    cat "$TMP/allow.cli" >> "$ALLOW_LIST"
  fi

  if git rev-parse --git-dir > /dev/null 2>&1; then
    _top=$(git rev-parse --show-toplevel) || fatal "no work tree"
    cd "$_top"
    if [ -f provenance/docs-assets.tsv ]; then
      echo "$_top/provenance/docs-assets.tsv" >> "$ALLOW_LIST"
    fi
    if [ -f fixtures/manifest.json ]; then
      [ -x "$MANIFEST_CHECK" ] ||
        fatal "missing $MANIFEST_CHECK (build first: cmake --build --preset gate)"
      [ -f "$MANIFEST_SCHEMA" ] || fatal "missing $MANIFEST_SCHEMA"
      _mrc=0
      "$MANIFEST_CHECK" --schema "$MANIFEST_SCHEMA" --root "$_top" \
        --emit-allowlist "$TMP/manifest.allow" "$_top/fixtures/manifest.json" \
        > "$TMP/manifest.out" 2>&1 || _mrc=$?
      if [ "$_mrc" -ne 0 ]; then
        cat "$TMP/manifest.out" >&2
        fatal "fixtures/manifest.json has findings, so it cannot feed the allowlist"
      fi
      if [ -s "$TMP/manifest.allow" ]; then
        echo "$TMP/manifest.allow" >> "$ALLOW_LIST"
      fi
    fi
    if [ "$DO_HISTORY" -eq 1 ] &&
      [ "$(git rev-parse --is-shallow-repository)" = "true" ]; then
      echo "RULE R-SHALLOW ." >> "$TMP/report"
      DO_HISTORY=0
    fi
  elif [ "$DO_TREE" -eq 1 ] || [ "$DO_HISTORY" -eq 1 ]; then
    echo "RULE R-NOT-GIT ." >> "$TMP/report"
    DO_TREE=0
    DO_HISTORY=0
  fi

  [ -x "$BLOBSCAN" ] || fatal "missing $BLOBSCAN (build first: cmake --build --preset gate)"
  if [ "$DO_TREE" -eq 1 ]; then scan_tree; fi
  if [ "$DO_HISTORY" -eq 1 ]; then scan_history; fi
  if [ -n "$STAGING" ]; then scan_staging; fi

  cat "$TMP/report"
  _f=$(grep -c '^RULE ' "$TMP/report" || true)
  _stage=""
  if [ -n "$STAGING" ]; then _stage=" staging=$STAGE_N"; fi
  echo "rights-scan: tree=$TREE_N history=$HIST_N${_stage} findings=$_f"
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
NOTRE=""

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

# mk NAME: a repository holding one text file, so a scan has something to read.
mk() {
  new_repo "$1"
  printf 'readme\n' > "$ST/$1/README.txt"
}

# fill FILE N CHAR: writes N copies of CHAR (no newline).
fill() {
  awk -v n="$2" -v c="$3" 'BEGIN { for (i = 0; i < n; i++) printf "%s", c }' > "$1"
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
  if [ -z "$_why" ] && [ -n "$NOTRE" ]; then
    if grep -q "$NOTRE" "$_out"; then _why="output matches $NOTRE"; fi
  fi
  NEED=""
  NEED2=""
  ABSENT=""
  NOTRE=""
  ST_CASES=$((ST_CASES + 1))
  if [ -z "$_why" ]; then
    echo "ok $_name ($_rule)"
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
    echo "ok $_name ($_rule)"
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

  # History covers every branch, tag and remote-tracking ref, not only HEAD,
  # and --range is validated and never parsed as a git option (WR-18).
  new_repo refs
  printf 'readme\n' > "$ST/refs/README.txt"
  commit_all "$ST/refs" init
  (cd "$ST/refs" && g checkout -q -b side && printf "${ROM_Z64}"'tail\n' > d.txt &&
    g add d.txt && g commit -q -m side && g checkout -q -)
  case_run history-side-branch 1 R-ROM-MAGIC "$ST/refs" --history
  new_repo tagonly
  printf 'readme\n' > "$ST/tagonly/README.txt"
  commit_all "$ST/tagonly" init
  (cd "$ST/tagonly" && g checkout -q --detach && printf "${ROM_Z64}"'tail\n' > d.txt &&
    g add d.txt && g commit -q -m t && g tag v-seed && g checkout -q -)
  case_run history-tag-only 1 R-ROM-MAGIC "$ST/tagonly" --history
  case_run range-side-branch 1 R-ROM-MAGIC "$ST/refs" --range "$(cd "$ST/refs" && git rev-parse HEAD)..side"
  case_run range-leading-dash 2 - "$ST/refs" --range --all
  case_run range-option-side 2 - "$ST/refs" --range "HEAD..--output=$ST/refs/x"
  case_run range-unresolvable 2 - "$ST/refs" --range "nosuchref..HEAD"
  case_run range-empty-side 2 - "$ST/refs" --range "..HEAD"

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

  # ---- content classes, one seeded repository per case --------------------
  # utf-16 with a BOM: NUL near the start and invalid UTF-8.
  mk utf16
  printf '\377\376a\000b\000\n' > "$ST/utf16/t.txt"
  NEED='R-INVALID-UTF8'
  case_run utf16-bom 1 R-NUL-HEAD "$ST/utf16" --tree

  # NUL only after byte 8192: its own rule, and not the head rule.
  mk nul-late
  fill "$ST/nul-late/t.txt" 9002 a
  printf '\000' >> "$ST/nul-late/t.txt"
  ABSENT=R-NUL-HEAD
  case_run nul-late 1 R-NUL-LATE "$ST/nul-late" --tree
  commit_all "$ST/nul-late" add
  (cd "$ST/nul-late" && g rm -q t.txt && g commit -q -m rm)
  ABSENT=R-NUL-HEAD
  case_run nul-late-history 1 R-NUL-LATE "$ST/nul-late" --history

  # a NUL exactly at the last head byte is still the head rule.
  mk nul-edge
  fill "$ST/nul-edge/t.txt" 8191 a
  printf '\000' >> "$ST/nul-edge/t.txt"
  ABSENT=R-NUL-LATE
  case_run nul-head-edge 1 R-NUL-HEAD "$ST/nul-edge" --tree

  mk latin1
  printf 'caf\351\n' > "$ST/latin1/t.txt"
  ABSENT=R-NUL-HEAD
  case_run latin1 1 R-INVALID-UTF8 "$ST/latin1" --tree
  mk overlong
  printf '\300\200\n' > "$ST/overlong/t.txt"
  case_run overlong 1 R-INVALID-UTF8 "$ST/overlong" --tree

  # valid multi-byte characters across the 65536-byte read buffer edge pass,
  # in the tree and in history.
  mk straddle
  fill "$ST/straddle/a.txt" 65535 a
  printf '\342\202\254\n' >> "$ST/straddle/a.txt"
  fill "$ST/straddle/b.txt" 65534 a
  printf '\342\202\254\n' >> "$ST/straddle/b.txt"
  fill "$ST/straddle/c.txt" 65535 a
  printf '\360\237\230\200\n' >> "$ST/straddle/c.txt"
  fill "$ST/straddle/d.txt" 65534 a
  printf '\360\237\230\200\n' >> "$ST/straddle/d.txt"
  fill "$ST/straddle/e.txt" 65533 a
  printf '\360\237\230\200\n' >> "$ST/straddle/e.txt"
  commit_all "$ST/straddle" add
  NEED='history=[1-9]'
  case_run straddle 0 - "$ST/straddle"
  mk straddle-bad
  fill "$ST/straddle-bad/a.txt" 65535 a
  printf '\342\202' >> "$ST/straddle-bad/a.txt"
  case_run straddle-truncated 1 R-INVALID-UTF8 "$ST/straddle-bad" --tree
  mk straddle-bad2
  fill "$ST/straddle-bad2/a.txt" 65535 a
  printf '\342\202a\n' >> "$ST/straddle-bad2/a.txt"
  case_run straddle-broken 1 R-INVALID-UTF8 "$ST/straddle-bad2" --tree

  # a denied extension fails even for harmless text, and an allowlisted digest
  # does not waive it.
  mk ext
  printf 'plain\n' > "$ST/ext/x.sav"
  case_run denied-ext-text 1 R-DENIED-EXT "$ST/ext" --tree
  mk ext-upper
  printf 'plain\n' > "$ST/ext-upper/Y.N64"
  case_run denied-ext-upper 1 R-DENIED-EXT "$ST/ext-upper" --tree
  # Trailing whitespace, control bytes, dots or other non-alphanumeric bytes
  # after the extension, and any letter case, do not hide it (WR-11), in the
  # tree and in history, where git quotes a name with a control byte.
  mk ext-trail
  printf 'plain\n' > "$ST/ext-trail/a.sav "
  printf 'plain\n' > "$ST/ext-trail/b.sav
"
  NEED='R-DENIED-EXT a.sav '
  NEED2='R-DENIED-EXT b.sav?'
  case_run ext-trailing-space-newline 1 R-DENIED-EXT "$ST/ext-trail" --tree
  mk ext-trail2
  printf 'plain\n' > "$ST/ext-trail2/C.SAV."
  NEED='R-DENIED-EXT C.SAV.'
  case_run ext-trailing-dot-upper 1 R-DENIED-EXT "$ST/ext-trail2" --tree
  mk ext-trail3
  printf 'plain\n' > "$ST/ext-trail3/d.Eep	"
  case_run ext-trailing-tab-mixed 1 R-DENIED-EXT "$ST/ext-trail3" --tree
  commit_all "$ST/ext-trail" add
  (cd "$ST/ext-trail" && g rm -q -f -- "a.sav " "b.sav
" && g commit -q -m rm)
  NEED='R-DENIED-EXT a.sav '
  NEED2='R-DENIED-EXT "b.sav'
  case_run ext-trailing-history 1 R-DENIED-EXT "$ST/ext-trail" --history
  mk ext-near
  printf 'plain\n' > "$ST/ext-near/notes.save"
  printf 'plain\n' > "$ST/ext-near/x.sav1"
  printf 'plain\n' > "$ST/ext-near/sav"
  case_run ext-near-miss 0 - "$ST/ext-near" --tree
  _sha=$(sha256_of "$ST/ext/x.sav")
  printf '%s\t%s\t%s\n' x.sav "$_sha" 'selftest' > "$ST/ext.tsv"
  case_run allowlist-keeps-denied-ext 1 R-DENIED-EXT "$ST/ext" --tree --allow "$ST/ext.tsv"

  # opaque containers by magic.
  mk zip
  printf 'PK\003\004rest\n' > "$ST/zip/t.txt"
  case_run zip 1 R-CONTAINER "$ST/zip" --tree
  mk gzip
  printf '\037\213rest\n' > "$ST/gzip/t.txt"
  case_run gzip 1 R-CONTAINER "$ST/gzip" --tree
  mk xz
  printf '\375\067zXZ\000rest\n' > "$ST/xz/t.txt"
  case_run xz 1 R-CONTAINER "$ST/xz" --tree
  mk bzip2
  printf 'BZh91AY&SYrest\n' > "$ST/bzip2/t.txt"
  case_run bzip2 1 R-CONTAINER "$ST/bzip2" --tree
  mk zstd
  printf '\050\265\057\375rest\n' > "$ST/zstd/t.txt"
  case_run zstd 1 R-CONTAINER "$ST/zstd" --tree
  mk sevenz
  printf '7z\274\257\047\034rest\n' > "$ST/sevenz/t.txt"
  case_run 7z 1 R-CONTAINER "$ST/sevenz" --tree
  mk tar
  fill "$ST/tar/t.txt" 257 x
  printf 'ustar\n' >> "$ST/tar/t.txt"
  case_run tar-ustar 1 R-CONTAINER "$ST/tar" --tree
  mk bzh
  printf 'BZh is only a word here\nBZh91 no\n' > "$ST/bzh/t.txt"
  case_run bzh-text 0 - "$ST/bzh" --tree

  # Git LFS pointer files and attributes hide bytes behind valid text.
  mk lfsp
  printf 'version https://git-lfs.github.com/spec/v1\noid sha256:%s\nsize 5\n' \
    0000000000000000000000000000000000000000000000000000000000000000 > "$ST/lfsp/t.txt"
  case_run lfs-pointer 1 R-LFS-POINTER "$ST/lfsp" --tree
  mk lfsa
  printf '*.bin filter=lfs diff=lfs merge=lfs -text\n' > "$ST/lfsa/.gitattributes"
  case_run lfs-attr 1 R-LFS-ATTR "$ST/lfsa" --tree
  commit_all "$ST/lfsa" attr
  (cd "$ST/lfsa" && g rm -q -f .gitattributes && g commit -q -m rm)
  case_run lfs-attr-history 1 R-LFS-ATTR "$ST/lfsa" --history
  # An attributes file that marks every file binary (itself included) and
  # carries a NUL is still read for the lfs filter, in the tree and history.
  mk lfsb
  printf '* -diff binary\n\000\n*.bin filter=lfs\n' > "$ST/lfsb/.gitattributes"
  case_run lfs-attr-nodiff 1 R-LFS-ATTR "$ST/lfsb" --tree
  commit_all "$ST/lfsb" attr
  (cd "$ST/lfsb" && g rm -q -f .gitattributes && g commit -q -m rm)
  case_run lfs-attr-nodiff-history 1 R-LFS-ATTR "$ST/lfsb" --history

  # gitlinks are not objects, so rev-list never sees them.
  mk gl
  (cd "$ST/gl" && g add README.txt &&
    g update-index --add --cacheinfo 160000,1111111111111111111111111111111111111111,sub)
  NEED='R-GITLINK sub'
  case_run gitlink 1 R-GITLINK "$ST/gl" --tree
  (cd "$ST/gl" && g commit -q -m gitlink)
  (cd "$ST/gl" && g update-index --force-remove sub && g commit -q -m ungitlink)
  case_run gitlink-history 1 R-GITLINK "$ST/gl" --history
  case_run gitlink-gone-from-tree 0 - "$ST/gl" --tree

  # renames share one blob; every name is reported.
  mk ren
  printf "${ROM_Z64}"'tail\n' > "$ST/ren/a.txt"
  commit_all "$ST/ren" add
  (cd "$ST/ren" && g mv a.txt b.txt && g commit -q -m rename)
  NEED='R-ROM-MAGIC a.txt'
  NEED2='R-ROM-MAGIC b.txt'
  case_run rename 1 R-ROM-MAGIC "$ST/ren" --history

  # a tracked symlink in the tree.
  mk tlink
  ln -s README.txt "$ST/tlink/link"
  case_run tree-symlink 1 R-SYMLINK "$ST/tlink" --tree

  # ---- artifact staging directories ----------------------------------------
  mkdir "$ST/s-link"
  printf 'ok\n' > "$ST/s-link/ok.txt"
  ln -s ok.txt "$ST/s-link/lnk"
  case_run staging-symlink 1 R-SYMLINK "$ST" --staging "$ST/s-link"
  mkdir "$ST/s-rom"
  printf "${ROM_Z64}"'tail\n' > "$ST/s-rom/x.dat"
  case_run staging-rom-magic 1 R-ROM-MAGIC "$ST" --staging "$ST/s-rom"
  mkdir "$ST/s-nul"
  printf 'ab\000cd\n' > "$ST/s-nul/x.dat"
  case_run staging-nul 1 R-NUL-HEAD "$ST" --staging "$ST/s-nul"
  mkdir "$ST/s-clean"
  printf 'fine\n' > "$ST/s-clean/ok.txt"
  NEED='staging=[1-9]'
  case_run staging-clean 0 - "$ST" --staging "$ST/s-clean"
  mkdir "$ST/s-empty"
  case_run staging-empty 2 R-ZERO-SCANNED "$ST" --staging "$ST/s-empty"
  mkdir "$ST/s-fifo"
  printf 'fine\n' > "$ST/s-fifo/ok.txt"
  mkfifo "$ST/s-fifo/pipe"
  case_run staging-fifo 1 R-SPECIAL "$ST" --staging "$ST/s-fifo"

  # ---- advisories never fail the scan -----------------------------------------
  mk adv
  fill "$ST/adv/t.txt" 4031 a
  printf '\n' >> "$ST/adv/t.txt"
  NEED='^ADVISORY A-SIZE-4032 '
  case_run advisory-4032 0 - "$ST/adv" --tree
  mk lit
  awk 'BEGIN { printf "static const unsigned char t[] = {\n"
    for (i = 0; i < 300; i++) printf "0x41, "
    printf "0x42};\n" }' > "$ST/lit/t.c"
  NEED='^ADVISORY A-BYTE-LITERAL-RUN '
  case_run byte-literal-run 0 - "$ST/lit" --tree
  mk lit-short
  awk 'BEGIN { for (i = 0; i < 255; i++) printf "0x41, "
    printf "\nint x = 0x123, y = 0x4;\n" }' > "$ST/lit-short/t.c"
  NOTRE='^ADVISORY A-BYTE-LITERAL-RUN '
  case_run byte-literal-short 0 - "$ST/lit-short" --tree

  # The manifest is the only allowlist path for committed non-text bytes: a
  # cleared NUL-bearing fixture listed in fixtures/manifest.json passes, the
  # same bytes unlisted fail, and a manifest with a finding fails the scan.
  [ -x "$MANIFEST_CHECK" ] ||
    fatal "missing $MANIFEST_CHECK (build first: cmake --build --preset gate)"
  new_repo manifest-unlisted
  mkdir "$ST/manifest-unlisted/fixtures"
  printf 'cleared\000fixture\n' > "$ST/manifest-unlisted/fixtures/blob.bin"
  case_run manifest-unlisted 1 R-NUL-HEAD "$ST/manifest-unlisted" --tree
  new_repo manifest-ok
  mkdir "$ST/manifest-ok/fixtures"
  printf 'cleared\000fixture\n' > "$ST/manifest-ok/fixtures/blob.bin"
  _msha=$(sha256_of "$ST/manifest-ok/fixtures/blob.bin")
  _msize=$(wc -c < "$ST/manifest-ok/fixtures/blob.bin" | tr -d ' ')
  _mrev=0123456789abcdef0123456789abcdef01234567
  _mentry='{"id":"selftest-blob","source":{"repo":"https://github.com/example/fixture-source","revision":"'$_mrev'","path":"."},"build":{"recipe":"authored in place","toolchain":["none 1"]},"sha256":"SHA","size":SIZE,"profile":"selftest","source_rights":"authored-here","binary_rights":"cleared","distribution":["commit"],"rights_evidence":"selftest","notices":[],"expected":{"kind":"regression-pin","source":"selftest","revision":"'$_mrev'"},"path":"fixtures/blob.bin"}'
  printf '{"schema":"t64.manifest","schema_version":1,"entries":[%s]}\n' \
    "$(printf '%s' "$_mentry" | sed -e "s/SHA/$_msha/" -e "s/SIZE/$_msize/")" \
    > "$ST/manifest-ok/fixtures/manifest.json"
  case_run manifest-allowlisted 0 - "$ST/manifest-ok" --tree
  commit_all "$ST/manifest-ok" add
  case_run manifest-allowlisted-history 0 - "$ST/manifest-ok" --history
  # Same listing, different bytes: the checker reports M-DIGEST-MISMATCH and the
  # scan stops instead of trusting the manifest.
  printf 'tampered\000fixture\n' > "$ST/manifest-ok/fixtures/blob.bin"
  NEED='cannot feed the allowlist'
  case_run manifest-digest-mismatch 2 M-DIGEST-MISMATCH "$ST/manifest-ok" --tree
  # A manifest that is not valid at all also stops the scan.
  printf '{' > "$ST/manifest-ok/fixtures/manifest.json"
  NEED='cannot feed the allowlist'
  case_run manifest-invalid 2 M-JSON "$ST/manifest-ok" --tree

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
      STAGING=$(cd "$2" 2> /dev/null && pwd -P) || fatal "staging directory not found: $2"
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

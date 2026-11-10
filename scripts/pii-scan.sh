#!/bin/sh
# PII and private-path scan (AUTONOMY section 4, GOV-07 companion). POSIX sh.
#
#   pii-scan.sh [--tree] [--history] [--range BASE..HEAD] [--selftest]
#
# With no scope flag it scans the working tree (tracked files plus untracked,
# non-ignored files) and all reachable history: added lines of every diff,
# commit messages, and author and committer addresses.
#
# Rules (stable ids):
#   P-PRIVATE-PATH  a per-user home (/Users/<name>, /home/<name>, a drive-
#                   letter or WSL /mnt/<drive>/Users/<name> path), /root/<x>,
#                   /Volumes/<name>, a macOS per-user temp directory or a
#                   /tmp/claude-* session directory, each only when a name
#                   component follows the root (prose that merely names a
#                   directory, or a <user> placeholder, does not match). The
#                   CI runner home /home/runner is exempt.
#   P-EMAIL         an address that is not on the allowlist: local part
#                   "noreply", the GitHub noreply domain, example.com/org/net
#                   (and subdomains), the TLDs .invalid .test .example, and
#                   image names like icon@2x.png.
#   P-COMMIT-EMAIL  an author or committer address that is not on the allowlist.
#   P-WAIVER-MISMATCH  a history waiver whose commit was scanned did not
#                   match exactly its waived number of hits.
#   P-SHALLOW       a shallow history cannot be scanned in full.
#   P-ZERO-SCANNED  no files or no commits were scanned.
# History waivers: tools/rights/pii-history-waivers.tsv (in the scanned tree)
# may waive the diff hits of one immutable (commit, path) pair, for example a
# placeholder name in a committed document that predates a stricter pattern.
# Waivers never apply to the tree, to messages or to commit metadata.
# Output is "RULE <id> <where> <first 3 chars>..." so a match is never echoed
# in full, plus "pii-scan: tree_files=N commits=M findings=K".
# Exit 0 clean, 1 findings, 2 harness error. A real hit is an owner stop:
# report it redacted, never rewrite history.
set -eu
LC_ALL=C
export LC_ALL

case $0 in
  /*) SELF=$0 ;;
  *) SELF=$(pwd -P)/$0 ;;
esac

# Pattern bodies. A per-user root matches as soon as a user-name component
# follows it, whether a separator, a quote or the end of the line comes next
# (/Users/<name> names a person, not a directory). A bare directory name
# (/home, /Users, /root, /Volumes) or an angle-bracket placeholder does not.
PATH_RE='/Users/[A-Za-z0-9._-]+|/home/[A-Za-z0-9._-]+|/root/[A-Za-z0-9._-]+|/Volumes/[A-Za-z0-9._-]+|/mnt/[A-Za-z]/[Uu]sers/[A-Za-z0-9._-]+|/var/folders/[A-Za-z0-9_+-]+/|/private/var/[A-Za-z0-9._-]+/|/private/tmp/[A-Za-z0-9._-]+|/tmp/claude-[A-Za-z0-9._-]+|[A-Za-z]:[/\\]+[Uu]sers[/\\]+[A-Za-z0-9._-]+'
EMAIL_RE='[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z][A-Za-z]+'

# Shared awk library: the address allowlist.
AWK_LIB='
function email_ok(m,   at, i, lp, dom, n, p, tld) {
  at = 0
  for (i = length(m); i > 0; i--) {
    if (substr(m, i, 1) == "@") { at = i; break }
  }
  if (at == 0) return 0
  lp = tolower(substr(m, 1, at - 1))
  dom = tolower(substr(m, at + 1))
  if (lp == "noreply") return 1
  if (dom == "users.noreply.github.com") return 1
  n = split(dom, p, ".")
  tld = p[n]
  if (n >= 2 && p[n - 1] == "example" &&
      (tld == "com" || tld == "org" || tld == "net")) return 1
  if (tld == "invalid" || tld == "test" || tld == "example") return 1
  if (tld == "png" || tld == "jpg" || tld == "jpeg" || tld == "gif" ||
      tld == "svg" || tld == "webp" || tld == "ico") return 1
  return 0
}
'

DO_TREE=0
DO_HISTORY=0
RANGE=""
DO_SELFTEST=0
EXPLICIT=0
TMP=""
WAIVER_FILE=""
TREE_FILES="skipped"
COMMITS="skipped"
HARNESS=0

fatal() {
  echo "pii-scan: error: $*" >&2
  exit 2
}

cleanup() {
  if [ -n "$TMP" ] && [ -d "$TMP" ]; then
    rm -rf "$TMP"
  fi
}

usage() {
  echo "usage: pii-scan.sh [--tree] [--history] [--range BASE..HEAD] [--selftest]" >&2
  exit 2
}

# finish RULE TSV [WAIVERS]: reads "where<TAB>match" rows, drops exempt
# matches, and appends redacted RULE lines to the report. With WAIVERS (the
# validated history waivers, "commit:path<TAB>rule<TAB>count"), the hits of a
# waived diff location are dropped only when their number equals the waived
# count exactly; any other number reports every hit plus P-WAIVER-MISMATCH,
# so a waiver can never absorb a new match. A commit is immutable, so a waiver
# keyed by its full id and path cannot cover content added later.
finish() {
  _w=${3:-/dev/null}
  awk -v rule="$1" -v wfile="$_w" -v cfile="$TMP/commits.list" "$AWK_LIB"'
    BEGIN {
      FS = "\t"
      while ((getline l < wfile) > 0) {
        split(l, w, "\t")
        if (w[2] == rule) want[w[1]] = w[3] + 0
      }
      close(wfile)
      while ((getline l < cfile) > 0) scanned[l] = 1
      close(cfile)
    }
    {
      m = $2
      if (rule == "P-PRIVATE-PATH" && m == "/home/runner") next
      if (rule == "P-EMAIL" && email_ok(m)) next
      n++
      where[n] = $1
      line[n] = "RULE " rule " " $1 " " substr(m, 1, 3) "..."
      got[$1]++
    }
    END {
      for (i = 1; i <= n; i++) {
        k = where[i]
        if ((k in want) && got[k] == want[k]) continue
        print line[i]
      }
      for (k in want) {
        c = substr(k, 1, index(k, ":") - 1)
        if (!(c in scanned)) continue
        if (got[k] + 0 != want[k]) {
          print "RULE P-WAIVER-MISMATCH " k " " rule " waived=" want[k] " found=" (got[k] + 0)
        }
      }
    }' "$2" >> "$TMP/report" || fatal "awk failed"
}

# load_waivers FILE: validates the history waivers and writes the normalised
# "commit:path<TAB>rule<TAB>count" rows to $TMP/waivers. A row is
#   commit(40 lowercase hex)<TAB>path<TAB>rule<TAB>count<TAB>reason
# with rule P-PRIVATE-PATH or P-EMAIL, count a positive integer and a
# non-empty reason. Any malformed row is a harness error.
load_waivers() {
  : > "$TMP/waivers"
  [ -f "$1" ] || return 0
  _bad=$(awk -F'\t' -v out="$TMP/waivers" '
    /^#/ || /^$/ { next }
    {
      ok = (NF == 5 && length($1) == 40 && $1 !~ /[^0-9a-f]/ && $2 != "" &&
            ($3 == "P-PRIVATE-PATH" || $3 == "P-EMAIL") &&
            $4 ~ /^[1-9][0-9]*$/ && $5 ~ /[^ ]/)
      if (!ok) { print NR; exit }
      print $1 ":" $2 "\t" $3 "\t" $4 > out
    }' "$1") || fatal "awk failed"
  [ -z "$_bad" ] || fatal "malformed history waiver row $_bad in $WAIVER_FILE"
}

# grep_to_tsv RULE-NAME... is split in two: tree hits come from git grep as
# "path:line:match"; this turns them into "path:line<TAB>match".
tree_tsv() {
  awk '{
      if (match($0, /:[0-9]+:/)) {
        print substr($0, 1, RSTART + RLENGTH - 2) "\t" substr($0, RSTART + RLENGTH)
      }
    }' "$1" > "$2" || fatal "awk failed"
}

scan_tree() {
  git ls-files --cached --others --exclude-standard > "$TMP/files.list" ||
    fatal "git ls-files failed"
  TREE_FILES=$(($(wc -l < "$TMP/files.list")))
  if [ "$TREE_FILES" -eq 0 ]; then
    echo "RULE P-ZERO-SCANNED tree" >> "$TMP/report"
    HARNESS=1
    return 0
  fi
  # -a: a committed attribute (-diff, binary) must not make git grep treat a
  # file as binary and skip it; real binaries yield matches, never silence.
  _rc=0
  git grep -a -n -o -E --untracked -e "$PATH_RE" > "$TMP/tree.path" || _rc=$?
  [ "$_rc" -le 1 ] || fatal "git grep failed"
  tree_tsv "$TMP/tree.path" "$TMP/tree.path.tsv"
  finish P-PRIVATE-PATH "$TMP/tree.path.tsv"
  _rc=0
  git grep -a -n -o -E --untracked -e "$EMAIL_RE" > "$TMP/tree.email" || _rc=$?
  [ "$_rc" -le 1 ] || fatal "git grep failed"
  tree_tsv "$TMP/tree.email" "$TMP/tree.email.tsv"
  finish P-EMAIL "$TMP/tree.email.tsv"
}

# hist_hits RULE TEXT LOC RE [WAIVERS]: matches RE over TEXT (one candidate
# line per line) and maps each hit back to its location in LOC.
hist_hits() {
  _rc=0
  grep -a -n -o -E -e "$4" "$2" > "$TMP/hist.hits" || _rc=$?
  [ "$_rc" -le 1 ] || fatal "grep failed"
  awk 'NR == FNR { loc[FNR] = $0; next }
      { i = index($0, ":"); n = substr($0, 1, i - 1) + 0
        print loc[n] "\t" substr($0, i + 1) }' "$3" "$TMP/hist.hits" \
    > "$TMP/hist.tsv" || fatal "awk failed"
  finish "$1" "$TMP/hist.tsv" "${5:-}"
}

scan_history() {
  if [ -n "$RANGE" ]; then
    _spec=$RANGE
  else
    _spec=HEAD
  fi
  COMMITS=0
  if git rev-parse --verify -q "${_spec%%..*}" > /dev/null 2>&1 || [ -n "$RANGE" ]; then
    git rev-list "$_spec" > "$TMP/commits.list" 2> /dev/null || : > "$TMP/commits.list"
  else
    : > "$TMP/commits.list"
  fi
  COMMITS=$(($(wc -l < "$TMP/commits.list")))
  if [ "$COMMITS" -eq 0 ]; then
    echo "RULE P-ZERO-SCANNED history" >> "$TMP/report"
    HARNESS=1
    return 0
  fi

  # Added lines of every diff. Content lines start with ' ', '+' or '-', so a
  # line starting with "COMMIT " or "diff --git " is always a marker. Lines that
  # start with '+' are scanned in full, including "+++ " headers (a content
  # line that looks like a header must not escape the scan). --text forces a
  # content diff even where a committed attribute (-diff, binary) would print
  # "Binary files differ"; NUL bytes become spaces so no awk or grep drops the
  # rest of a line.
  git -c core.quotepath=false log -m -p --text --no-color --no-ext-diff \
    --no-textconv --format='COMMIT %H' "$_spec" > "$TMP/log.p.raw" ||
    fatal "git log -p failed"
  tr '\000' ' ' < "$TMP/log.p.raw" > "$TMP/log.p" || fatal "tr failed"
  awk -v text="$TMP/added.txt" -v loc="$TMP/added.loc" '
    /^COMMIT / { c = $2; f = ""; next }
    /^diff --git / { f = $3; sub(/^a\//, "", f); next }
    /^\+/ { print substr($0, 2) > text; print c ":" f > loc }
    END { close(text); close(loc) }' "$TMP/log.p" || fatal "awk failed"
  : >> "$TMP/added.txt"
  : >> "$TMP/added.loc"
  load_waivers "$WAIVER_FILE"
  hist_hits P-PRIVATE-PATH "$TMP/added.txt" "$TMP/added.loc" "$PATH_RE" "$TMP/waivers"
  hist_hits P-EMAIL "$TMP/added.txt" "$TMP/added.loc" "$EMAIL_RE" "$TMP/waivers"

  # Commit messages. A message is free text, so no in-band marker can be
  # trusted inside it: %w(0,2,2) indents every message line (blank lines
  # included) by two spaces, and only an unindented line is a record marker.
  git log --format='COMMIT %H%n%w(0,2,2)%B' "$_spec" > "$TMP/log.msg" ||
    fatal "git log failed"
  awk -v text="$TMP/msg.txt" -v loc="$TMP/msg.loc" '
    /^COMMIT / { c = $2; next }
    { if (substr($0, 1, 2) == "  ") $0 = substr($0, 3)
      print $0 > text; print "message:" c > loc }
    END { close(text); close(loc) }' "$TMP/log.msg" || fatal "awk failed"
  : >> "$TMP/msg.txt"
  : >> "$TMP/msg.loc"
  hist_hits P-PRIVATE-PATH "$TMP/msg.txt" "$TMP/msg.loc" "$PATH_RE"
  hist_hits P-EMAIL "$TMP/msg.txt" "$TMP/msg.loc" "$EMAIL_RE"

  # Author and committer addresses.
  git log --format='%h%x09%ae%x09%ce' "$_spec" > "$TMP/log.meta" ||
    fatal "git log failed"
  awk -F'\t' "$AWK_LIB"'
    {
      if ($2 != "" && !email_ok($2)) print "commit:" $1 "\t" $2
      if ($3 != "" && $3 != $2 && !email_ok($3)) print "commit:" $1 "\t" $3
    }' "$TMP/log.meta" > "$TMP/meta.tsv" || fatal "awk failed"
  finish P-COMMIT-EMAIL "$TMP/meta.tsv"
}

scan_main() {
  : > "$TMP/report"
  git rev-parse --git-dir > /dev/null 2>&1 || fatal "not a git repository"
  _top=$(git rev-parse --show-toplevel) || fatal "no work tree"
  cd "$_top"
  WAIVER_FILE=$_top/tools/rights/pii-history-waivers.tsv
  if [ "$DO_HISTORY" -eq 1 ] &&
    [ "$(git rev-parse --is-shallow-repository)" = "true" ]; then
    echo "RULE P-SHALLOW history" >> "$TMP/report"
    HARNESS=1
    DO_HISTORY=0
  fi
  if [ "$DO_TREE" -eq 1 ]; then scan_tree; fi
  if [ "$DO_HISTORY" -eq 1 ]; then scan_history; fi

  cat "$TMP/report"
  _f=$(grep -c '^RULE ' "$TMP/report" || true)
  echo "pii-scan: tree_files=$TREE_FILES commits=$COMMITS findings=$_f"
  if [ "$HARNESS" -eq 1 ]; then
    exit 2
  fi
  if [ "$_f" -gt 0 ]; then
    exit 1
  fi
  exit 0
}

# ---------------------------------------------------------------- selftest

# Every seeded value is assembled from fragments at run time under a mktemp
# root outside the repository, so no private path or address sits in this
# source and the scan cannot trip over its own controls.
g() {
  git -c user.name=t64-selftest -c user.email=t64-selftest@example.invalid \
    -c commit.gpgsign=false -c core.autocrlf=false "$@"
}

ST_CASES=0
ST_FAIL=0
NOTRE=""

new_repo() {
  mkdir "$ST/$1"
  (cd "$ST/$1" && git init -q > /dev/null 2>&1)
  printf 'readme\n' > "$ST/$1/README.txt"
}

commit_all() {
  (cd "$1" && g add -A && g commit -q -m "$2")
}

# case_run NAME RC RULE DIR ARGS...: RULE "-" means no RULE line at all.
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
  if [ -z "$_why" ] && [ -n "$NOTRE" ]; then
    if grep -q "$NOTRE" "$_out"; then _why="output echoes a seeded value"; fi
  fi
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

selftest() {
  ST=$(mktemp -d "${TMPDIR:-/tmp}/t64.XXXXXX") || fatal "mktemp failed"
  TMP=$ST
  trap cleanup EXIT
  trap 'exit 130' INT TERM HUP
  echo "selftest-root: $ST"
  unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE
  GIT_CONFIG_GLOBAL=/dev/null
  GIT_CONFIG_NOSYSTEM=1
  export GIT_CONFIG_GLOBAL GIT_CONFIG_NOSYSTEM

  P_MAC=$(printf '/%s/%s/%s/' Users jane work)
  P_LINUX=$(printf '/%s/%s/%s/' home jane work)
  P_RUNNER=$(printf '/%s/%s/%s/' home runner work)
  P_VARF=$(printf '/%s/%s/%s/T/x' var folders zz)
  P_PTMP=$(printf '/%s/%s/%s' private tmp claude-seed)
  P_PVAR=$(printf '/%s/%s/%s/x' private var tmpseed)
  P_WIN=$(printf '%s:\\%s\\%s\\x' C Users jane)
  E_REAL=$(printf '%s@%s.%s' someone realmail io)
  E_NOREPLY=$(printf '%s+%s@%s.%s.%s.%s' 12345 jane users noreply github com)
  E_EXAMPLE=$(printf '%s@%s.%s' a example com)

  new_repo clean
  commit_all "$ST/clean" init
  case_run clean 0 - "$ST/clean"
  if ! grep -q 'tree_files=[1-9] commits=1 ' "$ST/out.clean"; then
    ST_FAIL=$((ST_FAIL + 1))
    echo "FAIL clean-counts: scan counted nothing"
  fi

  # Untracked, non-ignored files count as tree.
  new_repo mac
  printf 'see %s\n' "$P_MAC" > "$ST/mac/n.txt"
  case_run tree-mac-path 1 P-PRIVATE-PATH "$ST/mac" --tree
  new_repo lin
  printf 'see %s\n' "$P_LINUX" > "$ST/lin/n.txt"
  case_run tree-linux-home 1 P-PRIVATE-PATH "$ST/lin" --tree
  new_repo run
  printf 'see %s\n' "$P_RUNNER" > "$ST/run/n.txt"
  case_run runner-home-allowed 0 - "$ST/run" --tree
  new_repo vf
  printf 'see %s\n' "$P_VARF" > "$ST/vf/n.txt"
  case_run tree-var-folders 1 P-PRIVATE-PATH "$ST/vf" --tree
  new_repo pt
  printf 'see %s\n' "$P_PTMP" > "$ST/pt/n.txt"
  case_run tree-private-tmp 1 P-PRIVATE-PATH "$ST/pt" --tree
  new_repo pv
  printf 'see %s\n' "$P_PVAR" > "$ST/pv/n.txt"
  case_run tree-private-var 1 P-PRIVATE-PATH "$ST/pv" --tree
  new_repo win
  printf 'see %s\n' "$P_WIN" > "$ST/win/n.txt"
  case_run tree-windows-path 1 P-PRIVATE-PATH "$ST/win" --tree

  # A user name that ends the token is still a person's home (WR-12), and
  # the other private roots: root's home, mounted volumes, WSL drive mounts
  # and agent session directories under /tmp.
  _u=testuser
  _i=0
  for _p in "cd $(printf '/%s/%s' Users "$_u")" \
    "HOME=$(printf '/%s/%s' home "$_u")" \
    "\"$(printf '/%s/%s' home "$_u")\"" \
    "$(printf '%s:\\%s\\%s' C Users "$_u")" \
    "$(printf '%s:/%s/%s' D users "$_u") end" \
    "$(printf '/%s/%s' root .cargo)" \
    "$(printf '/%s/%s/%s' Volumes "$_u" data)" \
    "$(printf '/%s/%s/%s/%s' mnt c users "$_u")" \
    "$(printf '/%s/%s/%s/%s/x' mnt d Users "$_u")" \
    "$(printf '/%s/%s-%s/x' tmp claude 501)"; do
    _i=$((_i + 1))
    new_repo "bare$_i"
    printf 'see %s\n' "$_p" > "$ST/bare$_i/n.txt"
    case_run "tree-bare-path-$_i" 1 P-PRIVATE-PATH "$ST/bare$_i" --tree
  done
  new_repo runbare
  printf 'cd %s\nHOME=%s;\n' "$(printf '/%s/%s' home runner)" \
    "$(printf '/%s/%s' home runner)" > "$ST/runbare/n.txt"
  case_run runner-home-bare-allowed 0 - "$ST/runbare" --tree

  # Prose that only names a directory, or a placeholder in angle brackets, is
  # not a private path.
  new_repo prose
  printf 'The home directory (/home), /Users and /private/tmp are named only.\n' > "$ST/prose/n.txt"
  printf 'Also /root, /Volumes, /mnt/c, /tmp, /home/ and /Users/<user>/x.\n' >> "$ST/prose/n.txt"
  case_run prose-allowed 0 - "$ST/prose" --tree

  new_repo em
  printf 'mail %s\n' "$E_REAL" > "$ST/em/n.txt"
  case_run tree-email 1 P-EMAIL "$ST/em" --tree
  new_repo ok
  printf 'a %s b %s\n' "$E_NOREPLY" "$E_EXAMPLE" > "$ST/ok/n.txt"
  case_run noreply-allowed 0 - "$ST/ok" --tree
  new_repo ex
  printf 'a %s\nb %s\n' "$(printf '%s@%s.%s' me host invalid)" \
    "$(printf '%s@%s.%s' noreply anything io)" > "$ST/ex/n.txt"
  case_run example-allowed 0 - "$ST/ex" --tree

  # History only: removed from the tree, still in a diff.
  new_repo hist
  printf 'see %s\n' "$P_MAC" > "$ST/hist/n.txt"
  commit_all "$ST/hist" add
  (cd "$ST/hist" && g rm -q -f n.txt && g commit -q -m remove)
  case_run history-only-path 1 P-PRIVATE-PATH "$ST/hist" --history
  case_run history-only-tree-clean 0 - "$ST/hist" --tree

  # History waivers: exact (commit, path, rule, count) only, never the tree.
  new_repo wv
  printf 'see %s\n' "$P_MAC" > "$ST/wv/n.txt"
  commit_all "$ST/wv" add
  _wc=$(cd "$ST/wv" && git rev-parse HEAD)
  mkdir -p "$ST/wv/tools/rights"
  _wf=$ST/wv/tools/rights/pii-history-waivers.tsv
  printf '# c\tp\tr\tn\twhy\n%s\tn.txt\tP-PRIVATE-PATH\t1\tselftest placeholder\n' "$_wc" > "$_wf"
  case_run waiver-history 0 - "$ST/wv" --history
  case_run waiver-not-tree 1 P-PRIVATE-PATH "$ST/wv" --tree
  printf '%s\tn.txt\tP-PRIVATE-PATH\t2\tselftest placeholder\n' "$_wc" > "$_wf"
  case_run waiver-count-mismatch 1 P-WAIVER-MISMATCH "$ST/wv" --history
  case_run waiver-count-mismatch-hits 1 P-PRIVATE-PATH "$ST/wv" --history
  printf '%s\tn.txt\tP-PRIVATE-PATH\t1\t\n' "$_wc" > "$_wf"
  case_run waiver-no-reason 2 - "$ST/wv" --history
  printf '%s\tn.txt\tP-PRIVATE-PATH\t1\tshort id\n' "$(printf '%s' "$_wc" | cut -c1-12)" > "$_wf"
  case_run waiver-short-commit 2 - "$ST/wv" --history
  printf '%s\tn.txt\tP-PRIVATE-PATH\t1\tselftest placeholder\n' "$_wc" > "$_wf"
  printf 'and %s\n' "$P_LINUX" >> "$ST/wv/n.txt"
  (cd "$ST/wv" && g add n.txt && g commit -q -m more)
  case_run waiver-new-commit 1 P-PRIVATE-PATH "$ST/wv" --history

  # A line that looks like a diff header must not escape the scan.
  new_repo hdr
  printf '++ %s\n' "$P_MAC" > "$ST/hdr/n.txt"
  commit_all "$ST/hdr" add
  (cd "$ST/hdr" && g rm -q -f n.txt && g commit -q -m remove)
  case_run history-header-lookalike 1 P-PRIVATE-PATH "$ST/hdr" --history

  # A committed attribute that marks files binary (-diff, binary) must not
  # hide their content from the tree or the history pass, and a NUL byte on
  # the line must not turn the match into a "binary file matches" message.
  new_repo attr
  printf '* -diff\n' > "$ST/attr/.gitattributes"
  printf 'see %s\n' "$P_MAC" > "$ST/attr/n.md"
  case_run attr-nodiff-tree 1 P-PRIVATE-PATH "$ST/attr" --tree
  commit_all "$ST/attr" add
  (cd "$ST/attr" && g rm -q -f n.md && g commit -q -m remove)
  case_run attr-nodiff-history 1 P-PRIVATE-PATH "$ST/attr" --history
  new_repo attrb
  printf '*.md binary\n' > "$ST/attrb/.gitattributes"
  printf 'nul\000 see %s\n' "$P_MAC" > "$ST/attrb/n.md"
  case_run attr-binary-nul-tree 1 P-PRIVATE-PATH "$ST/attrb" --tree
  commit_all "$ST/attrb" add
  (cd "$ST/attrb" && g rm -q -f n.md && g commit -q -m remove)
  case_run attr-binary-nul-history 1 P-PRIVATE-PATH "$ST/attrb" --history

  # Commit metadata.
  new_repo auth
  printf 'x\n' > "$ST/auth/a.txt"
  (cd "$ST/auth" && git add -A && git -c user.name=seed -c user.email="$E_REAL" \
    -c commit.gpgsign=false commit -q -m init)
  case_run commit-author-email 1 P-COMMIT-EMAIL "$ST/auth" --history
  new_repo msg
  printf 'x\n' > "$ST/msg/a.txt"
  (cd "$ST/msg" && g add -A && g commit -q -m "contact $E_REAL")
  case_run message-email 1 P-EMAIL "$ST/msg" --history
  # A body line that begins with the word a record marker used to start with
  # must still be scanned (free text has no reserved prefix).
  new_repo msgmark
  printf 'x\n' > "$ST/msgmark/a.txt"
  (cd "$ST/msgmark" && g add -A &&
    g commit -q -m "subject" -m "COMMIT contact $E_REAL" -m "COMMIT see $P_MAC")
  case_run message-marker-lookalike 1 P-EMAIL "$ST/msgmark" --history
  case_run message-marker-lookalike-path 1 P-PRIVATE-PATH "$ST/msgmark" --history

  # Redaction: nothing but three characters of a match is ever printed.
  new_repo red
  printf '%s %s\n' "$P_MAC" "$E_REAL" > "$ST/red/n.txt"
  commit_all "$ST/red" add
  NOTRE='jane\|realmail\|someone'
  case_run redaction 1 P-PRIVATE-PATH "$ST/red"
  NOTRE='jane\|realmail\|someone'
  case_run redaction-email 1 P-EMAIL "$ST/red"

  # Vacuous scans fail.
  mkdir "$ST/empty"
  (cd "$ST/empty" && git init -q > /dev/null 2>&1)
  case_run zero-scanned 2 P-ZERO-SCANNED "$ST/empty"
  new_repo shallow-src
  printf 'one\n' > "$ST/shallow-src/a.txt"
  commit_all "$ST/shallow-src" one
  printf 'two\n' > "$ST/shallow-src/a.txt"
  commit_all "$ST/shallow-src" two
  git clone -q --depth 1 "file://$ST/shallow-src" "$ST/shallow" > /dev/null 2>&1
  case_run shallow 2 P-SHALLOW "$ST/shallow"

  echo "selftest: cases=$ST_CASES failures=$ST_FAIL"
  [ "$ST_FAIL" -eq 0 ] || exit 1
  exit 0
}

# --------------------------------------------------------------------- main

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
scan_main

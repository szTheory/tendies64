#!/bin/sh
# API-07 evaluation guard (Phase 12, plan 12-10, decision DR-22, threats T-12-33 to T-12-35).
#
# The written fit evaluation of the sibling contract (docs/evidence/phase12-libfuturo-fit.md,
# API-07) is pinned to one commit, ends in the conclusion "no adapter now", claims no support
# and has no adapter code anywhere. Rules (stable ids), printed as "API07 <rule> where: text":
#   DOC         the evaluation document is missing
#   PIN-LINE    the document does not hold exactly one line starting "Pinned commit:" with
#               exactly one 40-hex value (lower-case hex, no longer hex run on that line)
#   PIN-RECORD  PIN-IMPACT.md of Phase 12 is missing, or its section "API-07 pin" has no single
#               "Pinned commit" row with exactly one 40-hex value
#   PIN-MATCH   the two values differ (the evaluation is pinned to another commit than the
#               record that plan 12-01 took)
#   CONCLUSION  the section "## Conclusion" is missing or lacks the words "no adapter now"
#   CLAIM       a line holds the phrase "<name> support" (the sibling project's name, any letter
#               case) with no word "no" before it on the same line: the evaluation must never
#               claim that such support exists
#   ADAPTER     a tracked file under core, runner, host, tools or tests names the sibling
#               project (case-insensitive): no adapter code, no build target, no test of it
# The project name appears in this script only as the TOKEN below; scripts/ is outside the
# scanned roots, and the CTest registration comment does not spell it.
#
# Usage:
#   check-api07-evaluation.sh                 scan the repository containing this script
#   check-api07-evaluation.sh --root DIR      scan DIR instead (a git tree: tracked files only;
#                                             another tree: every file)
#   check-api07-evaluation.sh --selftest      seeded trees, one per rule, and clean controls
#   check-api07-evaluation.sh --with-selftest selftest first, then the scan (the CTest name)
# Output: findings, then "api07-evaluation: files=N findings=M". Exit 0 clean, 1 findings, 2
# usage error or a scan that covered no code file. POSIX sh and awk; git only for a git tree.
set -eu
LC_ALL=C
export LC_ALL

TOKEN=libfuturo
DOC=docs/evidence/phase12-libfuturo-fit.md
PIN_SUFFIX=12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md
CODE_ROOTS='core runner host tools tests'
ALL_RULES='DOC PIN-LINE PIN-RECORD PIN-MATCH CONCLUSION CLAIM ADAPTER'

SELF=$0
HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$(cd "$HERE/.." && pwd -P)

usage() {
  echo "usage: check-api07-evaluation.sh [--root DIR | --selftest | --with-selftest]" >&2
  exit 2
}

# sha_of_line: one line on stdin; prints "<runs of exactly 40 hex digits> <that run or -> <1 when a
# longer hex run is present, else 0>". A hex run is a maximal run of [0-9a-f].
sha_of_line() {
  awk '{
    s = $0; c = 0; bad = 0; sha = "-"
    while (match(s, /[0-9a-f]+/)) {
      len = RLENGTH; tok = substr(s, RSTART, len)
      if (len == 40) { c++; sha = tok } else if (len > 40) { bad = 1 }
      s = substr(s, RSTART + RLENGTH)
    }
    print c, sha, bad
  }'
}

# scan DIR: print the findings and the summary line; return 0 clean, 1 findings, 2 no code file.
scan() {
  _dir=$1
  _rep=$(mktemp "${TMPDIR:-/tmp}/t64api07.XXXXXX")
  _roots=""
  for _r in $CODE_ROOTS; do
    if [ -d "$_dir/$_r" ]; then
      _roots="$_roots $_r"
    fi
  done
  if [ -z "$_roots" ]; then
    rm -f "$_rep"
    echo "api07-evaluation: no code root to scan under $_dir" >&2
    return 2
  fi
  # shellcheck disable=SC2086
  _files=$(cd "$_dir" && find $_roots -type f 2>/dev/null | wc -l | tr -d ' ')
  if [ "$_files" -eq 0 ]; then
    rm -f "$_rep"
    echo "api07-evaluation: no file to scan under the code roots of $_dir" >&2
    return 2
  fi

  _doc="$_dir/$DOC"
  _docsha=""
  if [ ! -f "$_doc" ]; then
    echo "API07 DOC $DOC: the evaluation document is missing" >> "$_rep"
  else
    _pl=$(grep -c '^Pinned commit:' "$_doc" || true)
    if [ "$_pl" -ne 1 ]; then
      echo "API07 PIN-LINE $DOC: $_pl lines start with \"Pinned commit:\", expected exactly 1" >> "$_rep"
    else
      set -- $(grep '^Pinned commit:' "$_doc" | sha_of_line)
      if [ "$1" -ne 1 ] || [ "$3" -ne 0 ]; then
        echo "API07 PIN-LINE $DOC: the pin line must hold exactly one 40-hex value" >> "$_rep"
      else
        _docsha=$2
      fi
    fi
    # Conclusion section.
    if ! grep -q -x '## Conclusion' "$_doc"; then
      echo "API07 CONCLUSION $DOC: the section \"## Conclusion\" is missing" >> "$_rep"
    elif ! awk '/^## /{ f = ($0 == "## Conclusion") } f' "$_doc" | grep -q -F 'no adapter now'; then
      echo "API07 CONCLUSION $DOC: the Conclusion section lacks \"no adapter now\"" >> "$_rep"
    fi
    # A claim of support: the phrase with no word "no" before it on its line.
    awk -v where="$DOC" -v tok="$TOKEN" '
      {
        l = tolower($0); phrase = tok " support"; off = 0
        while ((i = index(substr(l, off + 1), phrase)) > 0) {
          pos = off + i
          pre = substr(l, 1, pos - 1)
          if (pre !~ /(^|[^a-z])no([^a-z]|$)/) {
            printf "API07 CLAIM %s:%d: a line claims support without a preceding \"no\"\n", where, NR
            break
          }
          off = pos + length(phrase) - 1
        }
      }' "$_doc" >> "$_rep"
  fi

  # The record of plan 12-01: the "Pinned commit" row of the section "API-07 pin".
  _pi=$(find "$_dir/.planning" -path "*/$PIN_SUFFIX" -type f 2>/dev/null | head -n 1)
  _recsha=""
  if [ -z "$_pi" ]; then
    echo "API07 PIN-RECORD .planning: PIN-IMPACT.md of Phase 12 was not found" >> "$_rep"
  else
    _rows=$(awk '/^## /{ f = ($0 ~ /^## API-07 pin/) } f && /^\| Pinned commit \|/' "$_pi")
    _nrows=$(printf '%s\n' "$_rows" | grep -c . || true)
    if [ "$_nrows" -ne 1 ]; then
      echo "API07 PIN-RECORD PIN-IMPACT.md: the section \"API-07 pin\" holds $_nrows \"Pinned commit\" rows, expected 1" >> "$_rep"
    else
      set -- $(printf '%s\n' "$_rows" | sha_of_line)
      if [ "$1" -ne 1 ] || [ "$3" -ne 0 ]; then
        echo "API07 PIN-RECORD PIN-IMPACT.md: the pin row must hold exactly one 40-hex value" >> "$_rep"
      else
        _recsha=$2
      fi
    fi
  fi
  if [ -n "$_docsha" ] && [ -n "$_recsha" ] && [ "$_docsha" != "$_recsha" ]; then
    echo "API07 PIN-MATCH $DOC: the pinned commit differs from the one of PIN-IMPACT.md" >> "$_rep"
  fi

  # No adapter code: the project name in a tracked file of a code root.
  _tmp=$(mktemp "${TMPDIR:-/tmp}/t64api07g.XXXXXX")
  _grc=0
  if [ -e "$_dir/.git" ]; then
    # shellcheck disable=SC2086
    git -C "$_dir" grep -n -I -i -e "$TOKEN" -- $_roots > "$_tmp" 2>/dev/null || _grc=$?
  else
    # shellcheck disable=SC2086
    (cd "$_dir" && grep -rn -I -i -e "$TOKEN" $_roots) > "$_tmp" 2>/dev/null || _grc=$?
  fi
  if [ "$_grc" -gt 1 ]; then
    rm -f "$_rep" "$_tmp"
    echo "api07-evaluation: grep failed (status $_grc)" >&2
    return 2
  fi
  cut -c1-160 "$_tmp" | sed 's/^/API07 ADAPTER /' >> "$_rep"
  rm -f "$_tmp"

  _n=$(grep -c '^API07 ' "$_rep" || true)
  cat "$_rep"
  rm -f "$_rep"
  echo "api07-evaluation: files=$_files findings=$_n"
  [ "$_n" -eq 0 ]
}

# ----- selftest ---------------------------------------------------------------------------
SELF_FAIL=0
SEEN_RULES=""
SELF_SHA=4177a43e67312e2db88ab21cba13aa7350063105
OTHER_SHA=0123456789abcdef0123456789abcdef01234567
GOOD_PIN="Pinned commit: $SELF_SHA"
GOOD_ROW="| Pinned commit | \`$SELF_SHA\` (object type commit) |"
GOOD_CONCL="The answer is no adapter now."
GOOD_EXTRA="A plain note."

# mk_tree NAME PIN_LINE PIN_ROW CONCLUSION EXTRA: a clean tree with the given variations.
mk_tree() {
  _t="$SELF_TMP/$1"
  _pd="$_t/.planning/phases/${PIN_SUFFIX%/*}"
  mkdir -p "$_t/docs/evidence" "$_pd"
  printf '# Evaluation\n\n%s\n\n## Notes\n\n%s\n\n## Conclusion\n\n%s\n' "$2" "$5" "$4" > "$_t/$DOC"
  printf '## API-07 pin: LibFuturo\n\n| Item | Value |\n|------|-------|\n%s\n| Other | none |\n\n## Decision record\n\n| Pinned commit | %s |\n' "$3" "$OTHER_SHA" > "$_pd/PIN-IMPACT.md"
  for _r in $CODE_ROOTS; do
    mkdir -p "$_t/$_r"
    printf '/* clean */\n' > "$_t/$_r/clean.c"
  done
}

# run_seed NAME RULE: scan the tree and require a finding of RULE (or a clean scan for "clean").
run_seed() {
  _name=$1
  _want=$2
  _t="$SELF_TMP/$_name"
  _rc=0
  scan "$_t" > "$SELF_TMP/$_name.out" 2> "$SELF_TMP/$_name.err" || _rc=$?
  if [ "$_want" = clean ]; then
    if [ "$_rc" -eq 0 ]; then
      echo "selftest ok: $_name is accepted"
    else
      echo "selftest FAIL: $_name should be accepted (exit $_rc)"
      SELF_FAIL=$((SELF_FAIL + 1))
    fi
  elif [ "$_rc" -eq 1 ] && grep -q "^API07 $_want " "$SELF_TMP/$_name.out"; then
    echo "selftest ok: $_name reports $_want"
    SEEN_RULES="$SEEN_RULES $_want"
  else
    echo "selftest FAIL: $_name should report $_want (exit $_rc)"
    SELF_FAIL=$((SELF_FAIL + 1))
  fi
}

selftest() {
  SELF_TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64api07-self.XXXXXX")
  trap 'rm -rf "$SELF_TMP"' EXIT
  _tok="Libfuturo"

  mk_tree clean-baseline "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed clean-baseline clean

  # DOC: no document.
  mk_tree seed-doc-missing "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  rm -f "$SELF_TMP/seed-doc-missing/$DOC"
  run_seed seed-doc-missing DOC

  # PIN-LINE: no line, a short value, a long value, two values, two lines, a wrong prefix.
  mk_tree seed-pin-absent "A line without the pin." "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-pin-absent PIN-LINE
  mk_tree seed-pin-short "Pinned commit: $(printf '%s' "$SELF_SHA" | cut -c1-39)" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-pin-short PIN-LINE
  mk_tree seed-pin-long "Pinned commit: ${SELF_SHA}a" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-pin-long PIN-LINE
  mk_tree seed-pin-two-values "Pinned commit: $SELF_SHA and $OTHER_SHA" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-pin-two-values PIN-LINE
  mk_tree seed-pin-two-lines "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_PIN"
  run_seed seed-pin-two-lines PIN-LINE
  mk_tree seed-pin-upper "Pinned commit: $(printf '%s' "$SELF_SHA" | tr 'a-f' 'A-F')" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-pin-upper PIN-LINE

  # PIN-RECORD: no row, two rows, a row without a 40-hex value, no file at all.
  mk_tree seed-record-no-row "$GOOD_PIN" "| Item | none |" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-record-no-row PIN-RECORD
  mk_tree seed-record-two-rows "$GOOD_PIN" "$GOOD_ROW
$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-record-two-rows PIN-RECORD
  mk_tree seed-record-no-sha "$GOOD_PIN" "| Pinned commit | not recorded |" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-record-no-sha PIN-RECORD
  mk_tree seed-record-missing "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  rm -rf "$SELF_TMP/seed-record-missing/.planning"
  run_seed seed-record-missing PIN-RECORD

  # PIN-MATCH: the document and the record name different commits.
  mk_tree seed-mismatch "Pinned commit: $OTHER_SHA" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  run_seed seed-mismatch PIN-MATCH

  # CONCLUSION: no section, the words only outside the section, a changed phrase.
  mk_tree seed-concl-missing "$GOOD_PIN" "$GOOD_ROW" "x" "$GOOD_EXTRA"
  sed 's/^## Conclusion$/## Result/' "$SELF_TMP/seed-concl-missing/$DOC" > "$SELF_TMP/concl.tmp"
  cp "$SELF_TMP/concl.tmp" "$SELF_TMP/seed-concl-missing/$DOC"
  run_seed seed-concl-missing CONCLUSION
  mk_tree seed-concl-elsewhere "$GOOD_PIN" "$GOOD_ROW" "An adapter later." "the words no adapter now sit in another section"
  run_seed seed-concl-elsewhere CONCLUSION
  mk_tree seed-concl-changed "$GOOD_PIN" "$GOOD_ROW" "An adapter is planned." "$GOOD_EXTRA"
  run_seed seed-concl-changed CONCLUSION

  # CLAIM: the phrase with no "no" before it; clean controls with "no" before it.
  mk_tree seed-claim-plain "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "$_tok support is complete."
  run_seed seed-claim-plain CLAIM
  mk_tree seed-claim-lower "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "A claim of libfuturo support follows."
  run_seed seed-claim-lower CLAIM
  mk_tree seed-claim-not "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "Not $_tok support."
  run_seed seed-claim-not CLAIM
  mk_tree clean-claim-second "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "No $_tok support, and later $_tok support"
  run_seed clean-claim-second clean
  mk_tree clean-claim-no "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "There is no claim that $_tok support exists."
  run_seed clean-claim-no clean
  mk_tree clean-claim-no-first "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "no $_tok support exists."
  run_seed clean-claim-no-first clean
  mk_tree seed-claim-note-on "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "A note on $_tok support."
  # "note on" holds the letters n-o only inside words, so the line is still a claim.
  run_seed seed-claim-note-on CLAIM

  # ADAPTER: the name in a file under each code root, in any case; outside the roots is clean.
  for _code in $CODE_ROOTS; do
    mk_tree "seed-adapter-$_code" "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
    printf '/* bridge to LIBFUTURO */\n' > "$SELF_TMP/seed-adapter-$_code/$_code/adapter.c"
    run_seed "seed-adapter-$_code" ADAPTER
  done
  mk_tree clean-token-in-docs "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
  mkdir -p "$SELF_TMP/clean-token-in-docs/docs/notes"
  printf 'about the sibling project %s\n' "$_tok" > "$SELF_TMP/clean-token-in-docs/docs/notes/n.md"
  run_seed clean-token-in-docs clean

  # A git tree judges tracked files only: an untracked build output is ignored.
  if command -v git > /dev/null 2>&1; then
    mk_tree git-untracked "$GOOD_PIN" "$GOOD_ROW" "$GOOD_CONCL" "$GOOD_EXTRA"
    git -C "$SELF_TMP/git-untracked" init -q 2> /dev/null
    git -C "$SELF_TMP/git-untracked" add -A 2> /dev/null
    printf '/* %s */\n' "$_tok" > "$SELF_TMP/git-untracked/tools/build-output.c"
    run_seed git-untracked clean
    git -C "$SELF_TMP/git-untracked" add tools/build-output.c 2> /dev/null
    run_seed git-untracked ADAPTER
  else
    echo "selftest note: git is absent, the tracked-file seeds are skipped"
  fi

  # A scan that covers no code file is an error, never a pass.
  mkdir -p "$SELF_TMP/empty/docs"
  _rc=0
  scan "$SELF_TMP/empty" > /dev/null 2>&1 || _rc=$?
  if [ "$_rc" -eq 2 ]; then
    echo "selftest ok: an empty scan is an error"
  else
    echo "selftest FAIL: an empty scan should exit 2 (exit $_rc)"
    SELF_FAIL=$((SELF_FAIL + 1))
  fi

  # Every rule has at least one seed that fails on it.
  for _rule in $ALL_RULES; do
    case " $SEEN_RULES " in
      *" $_rule "*) ;;
      *)
        echo "selftest FAIL: no seeded case asserts the rule $_rule"
        SELF_FAIL=$((SELF_FAIL + 1))
        ;;
    esac
  done
  echo "api07-evaluation selftest: failures=$SELF_FAIL"
  [ "$SELF_FAIL" -eq 0 ]
}

case "${1:-}" in
  "") scan "$ROOT" ;;
  --selftest)
    [ $# -eq 1 ] || usage
    selftest
    ;;
  --with-selftest)
    [ $# -eq 1 ] || usage
    selftest
    scan "$ROOT"
    ;;
  --root)
    [ $# -eq 2 ] || usage
    [ -d "$2" ] || { echo "api07-evaluation: not a directory: $2" >&2; exit 2; }
    scan "$2"
    ;;
  *) usage ;;
esac

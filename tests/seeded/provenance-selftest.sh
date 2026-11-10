#!/bin/sh
# Proves every rule id of t64-provenance-check on temporary trees (GOV-02). POSIX sh.
#
#   provenance-selftest.sh --bin PATH
#
# Each case builds a small tree in a temporary directory with printf, writes
# provenance/copies.json from shell variables, lists the files NUL-separated
# the way scripts/check-provenance.sh does, runs the checker, and compares the
# set of rule ids it printed with the set the case expects. A passing case
# expects no rule id, exit 0, an "ok" line and errors=0. Every file that needs a
# license-identifier or copyright-text tag gets it from fragments below, so this
# script never carries a header-form tag itself.
# Each case prints "ok <case> <rule ids>" or "FAIL <case>: <reason>". The run
# exits 1 when any case failed or a rule id was never exercised, 2 on usage.
set -eu
LC_ALL=C
export LC_ALL

BIN=""
while [ $# -gt 0 ]; do
  case "$1" in
    --bin)
      [ $# -ge 2 ] || { echo "usage: provenance-selftest.sh --bin PATH" >&2; exit 2; }
      BIN=$2
      shift 2
      ;;
    *) echo "usage: provenance-selftest.sh --bin PATH" >&2; exit 2 ;;
  esac
done
[ -n "$BIN" ] || { echo "usage: provenance-selftest.sh --bin PATH" >&2; exit 2; }
[ -x "$BIN" ] || { echo "provenance-selftest: not executable: $BIN" >&2; exit 2; }

T=$(mktemp -d "${TMPDIR:-/tmp}/t64provst.XXXXXX")
cleanup() { rm -rf "$T"; }
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM HUP

# The tag names, assembled from fragments.
W=SPDX
LIC="${W}-License-Identifier:"
CRT="${W}-FileCopyrightText:"
AT=@

ALL_RULES="P-JSON P-FIELD P-SHA P-OWNER-MISSING P-OWNER-EMAIL P-SPDX-NOT-ALLOWED P-UNRECORDED-COPY P-PATH-MISSING P-PATH P-DUP-PATH P-LICENSE-TEXT P-LICENSES-EXTRA P-ARCHIVED-OWNER P-HEADER P-MODS-MISSING P-ZERO-SCANNED"

CASES=0
FAILS=0
SEEN=" "
NAME=""
ROOT=""

# mk REL: writes stdin to ROOT/REL, creating parents.
mk() {
  mkdir -p "$ROOT/$(dirname "$1")"
  cat > "$ROOT/$1"
}

# copied_file REL ID: a file that carries a license tag for ID and an upstream holder.
copied_file() {
  {
    printf '// %s %s\n' "$LIC" "$2"
    printf '// %s 2020 Upstream Author\n' "$CRT"
    printf '\nint copied(void) { return 1; }\n'
  } | mk "$1"
}

# prose_lines N: N lines of plain text.
prose_lines() {
  _i=1
  while [ "$_i" -le "$1" ]; do
    printf 'plain prose line %s\n' "$_i"
    _i=$((_i + 1))
  done
}

defaults() {
  E_ID=lib-copied
  E_PATHS='["lib/copied.c"]'
  E_UPSTREAM='"upstream":{"repo":"https://example.org/up","path":"a/b.c","revision":"0123456789abcdef0123456789abcdef01234567"},'
  E_RETRIEVED=2026-11-09
  E_SPDX=MIT
  E_LT=LICENSES/MIT.txt
  E_NOTICE='"notice":"Copyright (c) 2020 Upstream Author",'
  E_MODE=verbatim
  E_MODS='"modifications":"",'
  E_OWNER='"maintenance_owner":"szTheory",'
  E_STATUS=active
  E_EXTRA=''
  E_EXPOSURE='[]'
  TOPEXP='[]'
  COPIES_RAW=''
  RAW_JSON=''
  NO_COPIES=0
  LIST_FILES=''
  LIST_EXTRA=''
  LIST_EMPTY=0
}

entry_json() {
  printf '{%s"id":"%s","paths":%s,%s"retrieved":"%s","spdx":"%s","license_text":"%s",%s"mode":"%s",%s"verification_test":"t.sh",%s"upstream_status":"%s","exposure":%s}' \
    "$E_EXTRA" "$E_ID" "$E_PATHS" "$E_UPSTREAM" "$E_RETRIEVED" "$E_SPDX" "$E_LT" \
    "$E_NOTICE" "$E_MODE" "$E_MODS" "$E_OWNER" "$E_STATUS" "$E_EXPOSURE"
}

write_copies() {
  [ "$NO_COPIES" = 1 ] && return 0
  if [ -n "$RAW_JSON" ]; then
    printf '%s' "$RAW_JSON" > "$ROOT/provenance/copies.json"
    return 0
  fi
  if [ "$COPIES_RAW" = "-" ]; then
    _body=''
  elif [ -n "$COPIES_RAW" ]; then
    _body=$COPIES_RAW
  else
    _body=$(entry_json)
  fi
  printf '{"schema":"t64.copies","schema_version":1,"copies":[%s],"exposure":%s}\n' \
    "$_body" "$TOPEXP" > "$ROOT/provenance/copies.json"
}

# base_tree NAME: a clean tree: own code, an Apache-header file, one recorded MIT copy.
base_tree() {
  NAME=$1
  ROOT=$T/$1
  mkdir -p "$ROOT/provenance" "$ROOT/LICENSES"
  defaults
  printf 'int own(void) { return 0; }\n' | mk src/own.c
  {
    printf '// %s Apache-2.0\n' "$LIC"
    printf '// %s 2026 Tendies64 contributors\n' "$CRT"
  } | mk src/apache.c
  copied_file lib/copied.c MIT
  printf 'MIT license text stand-in for the selftest\n' | mk LICENSES/MIT.txt
}

fail() {
  FAILS=$((FAILS + 1))
  echo "FAIL $NAME: $1"
  printf '%s\n' "$OUT" | head -n 6 | sed 's/^/    /'
}

# check EXPECT [NEEDLE]: EXPECT is "ok" or space-separated rule ids.
check() {
  CASES=$((CASES + 1))
  write_copies
  if [ "$LIST_EMPTY" = 1 ]; then
    : > "$T/$NAME.list"
  elif [ -n "$LIST_FILES" ]; then
    printf '%s\n' $LIST_FILES | tr '\n' '\000' > "$T/$NAME.list"
  else
    (cd "$ROOT" && find . \( -type f -o -type l \) | sed 's|^\./||' | sort) | tr '\n' '\000' > "$T/$NAME.list"
  fi
  if [ -n "$LIST_EXTRA" ]; then
    printf '%s\000' "$LIST_EXTRA" >> "$T/$NAME.list"
  fi
  RC=0
  OUT=$("$BIN" --root "$ROOT" --copies "$ROOT/provenance/copies.json" --files-from "$T/$NAME.list" 2>&1) || RC=$?
  GOT=$(printf '%s\n' "$OUT" | awk '$1 == "RULE" { print $2 }' | sort -u | tr '\n' ' ')
  GOT=${GOT% }
  if [ "$1" = ok ]; then
    if [ "$RC" -ne 0 ]; then fail "expected exit 0, got $RC"; return 0; fi
    if [ -n "$GOT" ]; then fail "expected no rule id, got: $GOT"; return 0; fi
    if ! printf '%s\n' "$OUT" | grep -q '^ok '; then fail "no ok line"; return 0; fi
    if ! printf '%s\n' "$OUT" | grep -q 'errors=0$'; then fail "summary is not errors=0"; return 0; fi
    if [ $# -ge 2 ] && ! printf '%s\n' "$OUT" | grep -q "$2"; then
      fail "output lacks \"$2\""
      return 0
    fi
    echo "ok $NAME ok"
    return 0
  fi
  WANT=$(printf '%s\n' $1 | sort -u | tr '\n' ' ')
  WANT=${WANT% }
  if [ "$RC" -ne 1 ]; then fail "expected exit 1, got $RC"; return 0; fi
  if [ "$GOT" != "$WANT" ]; then fail "expected rule ids [$WANT], got [$GOT]"; return 0; fi
  if printf '%s\n' "$OUT" | grep -q '^ok '; then fail "printed an ok line next to findings"; return 0; fi
  for _r in $WANT; do SEEN="$SEEN$_r "; done
  echo "ok $NAME $WANT"
}

# ---- passing trees ---------------------------------------------------------

base_tree clean
check ok "copied_looking=1 entries=1 errors=0"

base_tree apache-own-header
printf '// %s Apache-2.0\n' "$LIC" | mk src/ap2.c
check ok "copied_looking=1 entries=1"

base_tree own-holder-year
printf '// %s Copyright (c) 2026 Tendies64 contributors\n' "$CRT" | mk src/own2.c
check ok "copied_looking=1 entries=1"

base_tree late-spdx
{ prose_lines 39; printf '// %s GPL-3.0-only\n' "$LIC"; } | mk docs/late.txt
check ok "copied_looking=1 entries=1"

base_tree line21-spdx
{ prose_lines 20; printf '// %s MIT\n' "$LIC"; } | mk docs/l21.txt
check ok "copied_looking=1 entries=1"

base_tree midline-spdx
printf 'first line\nsecond line\nA sentence that quotes %s MIT in its middle.\n' "$LIC" | mk docs/mid.md
check ok "copied_looking=1 entries=1"

base_tree list-missing-file
LIST_EXTRA=src/deleted-but-still-listed.c
check ok "copied_looking=1 entries=1"

base_tree third-party-recorded
rm "$ROOT/lib/copied.c"
copied_file third_party/up/x.c MIT
E_PATHS='["third_party/up/x.c"]'
check ok "copied_looking=1 entries=1"

base_tree adapted-with-mods
E_MODE=adapted
E_MODS='"modifications":"renamed two functions",'
check ok "entries=1 errors=0"

base_tree archived-owner-ok
E_STATUS=archived
E_OWNER='"maintenance_owner":"Tendies64",'
check ok "entries=1 errors=0"

base_tree owner-role-ok
E_OWNER='"maintenance_owner":"Tendies64 maintainers",'
check ok "entries=1 errors=0"

base_tree exposure-valid
E_EXPOSURE='[{"date":"2026-11-09","who":"szTheory","source":"a public document","license":"MIT","action":"noted"}]'
TOPEXP=$E_EXPOSURE
check ok "entries=1 errors=0"

for _id in ISC BSD-3-Clause Unlicense; do
  base_tree "spdx-ok-$_id"
  rm "$ROOT/LICENSES/MIT.txt"
  printf 'license text stand-in\n' | mk "LICENSES/$_id.txt"
  copied_file lib/copied.c "$_id"
  E_SPDX=$_id
  E_LT=LICENSES/$_id.txt
  check ok "copied_looking=1 entries=1"
done

# ---- unrecorded copies -----------------------------------------------------

base_tree unrecorded-spdx
printf '// %s MIT\n' "$LIC" | mk lib/other.c
check P-UNRECORDED-COPY

base_tree unrecorded-third-party
printf 'plain text with no header\n' | mk third_party/x/y.txt
check P-UNRECORDED-COPY

base_tree foreign-copyright
printf '// %s 2020 Someone Else\n' "$CRT" | mk lib/fc.c
check P-UNRECORDED-COPY

base_tree holder-prefix-spoof
printf '// %s 2026 Tendies64 contributors and others\n' "$CRT" | mk lib/spoof.c
check P-UNRECORDED-COPY

base_tree line20-spdx
{ prose_lines 19; printf '// %s MIT\n' "$LIC"; } | mk docs/l20.txt
check P-UNRECORDED-COPY

_n=0
for _lead in '<!--' '/*' '//' '--' '*' '#' ';' '' '/**' '///' '##' '%' ';;' '---' \
  'REM' 'rem' '::' '/*!' '<!---' '%%' '#!' '// *'; do
  _n=$((_n + 1))
  base_tree "leader-$_n"
  printf '  %s %s ISC\n' "$_lead" "$LIC" | mk lib/lead.txt
  check P-UNRECORDED-COPY
done

# A UTF-8 byte-order mark before the comment leader on line 1 (WR-13).
base_tree bom-line1
printf '\357\273\277// %s GPL-2.0-only\nint x;\n' "$LIC" | mk lib/bom.c
check P-UNRECORDED-COPY
base_tree bom-copyright
printf '\357\273\277# %s 2020 Someone Else\n' "$CRT" | mk lib/bomc.sh
check P-UNRECORDED-COPY
# A one-line block comment around the tag.
base_tree block-oneline
printf '/** %s GPL-2.0-only */\nint y;\n' "$LIC" | mk lib/blk.c
check P-UNRECORDED-COPY
# A word before the tag is prose, not a header (the policy keeps that rule).
base_tree rem-word-prose
printf 'REMARK %s MIT\n' "$LIC" | mk docs/remark.txt
check ok "copied_looking=1 entries=1"

# ---- record fields ---------------------------------------------------------

base_tree short-sha
E_UPSTREAM='"upstream":{"repo":"https://example.org/up","path":"a/b.c","revision":"abc123"},'
check P-SHA

base_tree long-sha
E_UPSTREAM='"upstream":{"repo":"https://example.org/up","path":"a/b.c","revision":"0123456789abcdef0123456789abcdef012345678"},'
check P-SHA

base_tree missing-sha
E_UPSTREAM='"upstream":{"repo":"https://example.org/up","path":"a/b.c"},'
check P-SHA

base_tree owner-missing
E_OWNER=''
check P-OWNER-MISSING

base_tree owner-blank
E_OWNER='"maintenance_owner":"  ",'
check P-OWNER-MISSING

base_tree owner-email
E_OWNER="\"maintenance_owner\":\"dev${AT}example.com\","
check P-OWNER-EMAIL

base_tree owner-bad-shape
E_OWNER='"maintenance_owner":"a/b",'
check P-FIELD

base_tree archived-owner
E_STATUS=archived
check P-ARCHIVED-OWNER

base_tree adapted-no-mods
E_MODE=adapted
check P-MODS-MISSING

base_tree unknown-field
E_EXTRA='"surprise":1,'
check P-FIELD

base_tree missing-field
E_NOTICE=''
check P-FIELD

base_tree bad-date
E_RETRIEVED=2026-13-40
check P-FIELD

base_tree bad-mode
E_MODE=copied
check P-FIELD

base_tree bad-status
E_STATUS=dormant
check P-FIELD

base_tree exposure-bad-item
E_EXPOSURE='[{"date":"2026-11-09"}]'
check P-FIELD

base_tree exposure-bad-date
E_EXPOSURE='[{"date":"yesterday","who":"szTheory","source":"a public document","license":"MIT","action":"noted"}]'
check P-FIELD

base_tree top-not-object
RAW_JSON='[]'
check "P-FIELD P-LICENSES-EXTRA P-UNRECORDED-COPY"

base_tree schema-wrong
RAW_JSON='{"schema":"other","schema_version":1,"copies":[],"exposure":[]}'
check "P-FIELD P-LICENSES-EXTRA P-UNRECORDED-COPY"

base_tree duplicate-id
copied_file lib/second.c MIT
_first=$(entry_json)
E_PATHS='["lib/second.c"]'
COPIES_RAW="$_first,$(entry_json)"
check P-FIELD

# ---- licenses --------------------------------------------------------------

# spdx_case NAME ID: a record whose license id and text agree but are not allowlisted.
spdx_case() {
  base_tree "$1"
  rm "$ROOT/LICENSES/MIT.txt"
  printf 'license text stand-in\n' | mk "LICENSES/$2.txt"
  copied_file lib/copied.c "$2"
  E_SPDX=$2
  E_LT=LICENSES/$2.txt
  check P-SPDX-NOT-ALLOWED
}
spdx_case gpl GPL-3.0-only
spdx_case apache-copy Apache-2.0
spdx_case bsd2 BSD-2-Clause
spdx_case mit0 MIT-0
spdx_case zero-bsd 0BSD
spdx_case zlib Zlib
spdx_case mpl MPL-2.0

base_tree license-text-missing
rm "$ROOT/LICENSES/MIT.txt"
check P-LICENSE-TEXT

base_tree license-text-empty
: > "$ROOT/LICENSES/MIT.txt"
check P-LICENSE-TEXT

base_tree license-text-symlink
rm "$ROOT/LICENSES/MIT.txt"
ln -s ../src/own.c "$ROOT/LICENSES/MIT.txt"
check P-LICENSE-TEXT

base_tree license-text-wrong-name
E_LT=LICENSES/mit.txt
check "P-LICENSE-TEXT P-LICENSES-EXTRA"

base_tree licenses-extra
printf 'a stray license text\n' | mk LICENSES/ISC.txt
check P-LICENSES-EXTRA

base_tree header-mismatch
copied_file lib/copied.c ISC
check P-HEADER

base_tree header-late
{ prose_lines 24; printf '// %s MIT\n' "$LIC"; } | mk lib/copied.c
check P-HEADER

# ---- paths -----------------------------------------------------------------

base_tree path-missing
rm "$ROOT/lib/copied.c"
E_PATHS='["lib/nothere.c"]'
check P-PATH-MISSING

base_tree path-traversal
rm "$ROOT/lib/copied.c"
E_PATHS='["../outside.c"]'
check P-PATH

base_tree path-absolute
rm "$ROOT/lib/copied.c"
E_PATHS='["/etc/hosts"]'
check P-PATH

base_tree path-dot-segment
rm "$ROOT/lib/copied.c"
E_PATHS='["lib/./copied.c"]'
check P-PATH

base_tree path-symlink
rm "$ROOT/lib/copied.c"
ln -s ../src/own.c "$ROOT/lib/link.c"
E_PATHS='["lib/link.c"]'
check P-PATH

base_tree path-symlink-dir
ln -s lib "$ROOT/linkdir"
E_PATHS='["linkdir/copied.c"]'
check "P-PATH P-UNRECORDED-COPY"

base_tree list-path-traversal
LIST_EXTRA='../escape.c'
check P-PATH

base_tree dup-path
_first=$(entry_json)
E_ID=lib-copied-2
COPIES_RAW="$_first,$(entry_json)"
check P-DUP-PATH

base_tree dup-path-in-entry
E_PATHS='["lib/copied.c","lib/copied.c"]'
check P-DUP-PATH

# ---- documents and counts --------------------------------------------------

base_tree bad-json
RAW_JSON='{"schema":"t64.copies",'
check P-JSON

base_tree json-duplicate-key
RAW_JSON='{"schema":"t64.copies","schema":"t64.copies"}'
check P-JSON

base_tree json-empty
RAW_JSON=' '
check P-JSON

base_tree json-file-missing
NO_COPIES=1
check P-JSON

base_tree zero-files
COPIES_RAW='-'
LIST_FILES=ghost/a.c
check P-ZERO-SCANNED

base_tree zero-files-empty-list
COPIES_RAW='-'
LIST_EMPTY=1
check P-ZERO-SCANNED

# ---- coverage of the rule list ---------------------------------------------

for _r in $ALL_RULES; do
  case "$SEEN" in
    *" $_r "*) ;;
    *)
      FAILS=$((FAILS + 1))
      echo "FAIL coverage: rule $_r was never exercised by a failing case"
      ;;
  esac
done

echo "provenance-selftest: cases=$CASES failures=$FAILS rules=$(printf '%s\n' $ALL_RULES | wc -l | tr -d ' ')"
if [ "$FAILS" -ne 0 ]; then
  exit 1
fi

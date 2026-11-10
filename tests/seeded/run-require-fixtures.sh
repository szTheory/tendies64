#!/bin/sh
# Proves --require-fixtures in both modes, --emit-allowlist and the schema-subset
# refusal of t64-manifest-check on a temporary root (GOV-03, GOV-05). POSIX sh.
#
#   run-require-fixtures.sh --bin PATH
#
# The temporary root holds a copy of the real schema and a manifest with one
# cleared, commit-channel entry whose plain-text fixture is generated here with
# printf; nothing protected-looking is created and the root is removed on exit.
# Each case prints "ok <name>" or "FAIL <name>: <reason>"; the run exits 1 when
# any case failed and 2 on a usage error.
set -eu
LC_ALL=C
export LC_ALL

BIN=""
while [ $# -gt 0 ]; do
  case "$1" in
    --bin)
      [ $# -ge 2 ] || { echo "usage: run-require-fixtures.sh --bin PATH" >&2; exit 2; }
      BIN=$2
      shift 2
      ;;
    *) echo "usage: run-require-fixtures.sh --bin PATH" >&2; exit 2 ;;
  esac
done
[ -n "$BIN" ] || { echo "usage: run-require-fixtures.sh --bin PATH" >&2; exit 2; }
[ -x "$BIN" ] || { echo "run-require-fixtures: not executable: $BIN" >&2; exit 2; }

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd -P)
SCHEMA=$SCRIPT_DIR/../../fixtures/manifest.schema.json
[ -f "$SCHEMA" ] || { echo "run-require-fixtures: missing $SCHEMA" >&2; exit 2; }

T=$(mktemp -d "${TMPDIR:-/tmp}/t64.XXXXXX")
cleanup() { rm -rf "$T"; }
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM HUP

sha256_of() {
  if command -v sha256sum > /dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

mkdir "$T/fixtures"
cp "$SCHEMA" "$T/fixtures/manifest.schema.json"
printf 'plain text fixture\n' > "$T/fixtures/plain.txt"
SHA=$(sha256_of "$T/fixtures/plain.txt")
SIZE=$(wc -c < "$T/fixtures/plain.txt" | tr -d ' ')
REV=0123456789abcdef0123456789abcdef01234567
cat > "$T/fixtures/manifest.json" <<EOF
{"schema":"t64.manifest","schema_version":1,"entries":[{"id":"temp-cleared","source":{"repo":"https://github.com/example/fixture-source","revision":"$REV","path":"."},"build":{"recipe":"authored in place","toolchain":["none 1"]},"sha256":"$SHA","size":$SIZE,"profile":"temp-profile","source_rights":"authored-here","binary_rights":"cleared","distribution":["commit"],"rights_evidence":"temporary root","notices":[],"expected":{"kind":"regression-pin","source":"temporary root","revision":"$REV"},"path":"fixtures/plain.txt"}]}
EOF
MAN=$T/fixtures/manifest.json

CASES=0
FAILS=0
# run NAME WANT_RC WANT_TEXT ARGS...: runs the tool, then checks the exit code
# and that the output contains WANT_TEXT.
run() {
  _name=$1
  _rc=$2
  _text=$3
  shift 3
  _got=0
  "$BIN" "$@" > "$T/out" 2>&1 < /dev/null || _got=$?
  CASES=$((CASES + 1))
  _why=""
  if [ "$_got" -ne "$_rc" ]; then
    _why="exit $_got, wanted $_rc"
  elif ! grep -Fq -- "$_text" "$T/out"; then
    _why="output lacks: $_text"
  fi
  if [ -z "$_why" ]; then
    echo "ok $_name"
  else
    FAILS=$((FAILS + 1))
    echo "FAIL $_name: $_why"
    sed 's/^/  | /' "$T/out"
  fi
}

run present-no-flag 0 'present=1' --root "$T" "$MAN"
mv "$T/fixtures/plain.txt" "$T/plain.keep"
run absent-no-flag 0 'skipped=1' --root "$T" "$MAN"
run absent-require-fixtures 1 'M-FIXTURE-ABSENT' --root "$T" --require-fixtures "$MAN"
mv "$T/plain.keep" "$T/fixtures/plain.txt"
printf 'tampered\n' >> "$T/fixtures/plain.txt"
run tampered 1 'M-DIGEST-MISMATCH' --root "$T" "$MAN"
printf 'plain text fixture\n' > "$T/fixtures/plain.txt"

# --emit-allowlist writes exactly one row for the present case.
rm -f "$T/allow.tsv"
_got=0
"$BIN" --root "$T" --emit-allowlist "$T/allow.tsv" "$MAN" > "$T/out" 2>&1 || _got=$?
CASES=$((CASES + 1))
_want=$(printf 'fixtures/plain.txt\t%s\tmanifest:temp-cleared' "$SHA")
_rows=$(wc -l < "$T/allow.tsv" | tr -d ' ')
_row=$(sed -n '1p' "$T/allow.tsv")
if [ "$_got" -eq 0 ] && [ "$_rows" = 1 ] && [ "$_row" = "$_want" ]; then
  echo "ok emit-allowlist"
else
  FAILS=$((FAILS + 1))
  echo "FAIL emit-allowlist: exit $_got, rows $_rows"
fi

# A manifest with a finding must not feed the allowlist.
printf 'tampered\n' >> "$T/fixtures/plain.txt"
rm -f "$T/allow.tsv"
_got=0
"$BIN" --root "$T" --emit-allowlist "$T/allow.tsv" "$MAN" > "$T/out" 2>&1 || _got=$?
CASES=$((CASES + 1))
if [ "$_got" -eq 1 ] && [ -f "$T/allow.tsv" ] && [ ! -s "$T/allow.tsv" ]; then
  echo "ok emit-allowlist-empty-on-findings"
else
  FAILS=$((FAILS + 1))
  echo "FAIL emit-allowlist-empty-on-findings: exit $_got"
fi
printf 'plain text fixture\n' > "$T/fixtures/plain.txt"

# A symlinked fixture is refused (M-PATH), never followed.
rm -f "$T/fixtures/plain.txt"
ln -s "$T/fixtures/manifest.json" "$T/fixtures/plain.txt"
run symlink-refused 1 'M-PATH' --root "$T" "$MAN"
rm -f "$T/fixtures/plain.txt"
printf 'plain text fixture\n' > "$T/fixtures/plain.txt"

# Same-size tampering fails on the digest alone; a wrong size fails on the size alone.
printf 'PLAIN text fixture\n' > "$T/fixtures/plain.txt"
run digest-only 1 'sha256 does not match' --root "$T" "$MAN"
printf 'plain text fixture\n' > "$T/fixtures/plain.txt"
sed "s/\"size\":$SIZE/\"size\":$((SIZE + 1))/" "$MAN" > "$T/fixtures/size.json"
run size-only 1 'size does not match' --root "$T" "$T/fixtures/size.json"

# Pending bytes must not be committed, even when no path is named.
sed -e 's/"binary_rights":"cleared"/"binary_rights":"pending"/' -e 's/,"path":"fixtures\/plain.txt"//' \
  "$MAN" > "$T/fixtures/pending.json"
run commit-without-rights 1 'M-BYTES-NOT-PERMITTED' --root "$T" "$T/fixtures/pending.json"

# A symlinked parent directory is refused too, not only a symlinked file.
mkdir "$T/real"
printf 'plain text fixture\n' > "$T/real/plain.txt"
ln -s "$T/real" "$T/fixtures/sub"
sed 's#"path":"fixtures/plain.txt"#"path":"fixtures/sub/plain.txt"#' "$MAN" > "$T/fixtures/sub.json"
run symlink-parent-refused 1 'M-PATH' --root "$T" "$T/fixtures/sub.json"
rm -f "$T/fixtures/sub"

# A schema keyword outside the declared subset is a harness error (exit 2).
sed 's/"type": "object",/"type": "object", "format": "x",/' "$SCHEMA" > "$T/format.schema.json"
run schema-keyword 2 'M-SCHEMA-KEYWORD' --schema "$T/format.schema.json" --root "$T" "$MAN"
# So are a keyword value outside the subset and a pattern that is not anchored.
sed 's/"additionalProperties": false,/"additionalProperties": true,/' "$SCHEMA" > "$T/open.schema.json"
run schema-open-additional 2 'M-SCHEMA-KEYWORD' --schema "$T/open.schema.json" --root "$T" "$MAN"
sed 's/"\^\[0-9a-f\]{40}\$"/"[0-9a-f]{40}"/' "$SCHEMA" > "$T/loose.schema.json"
run schema-unanchored-pattern 2 'M-SCHEMA-KEYWORD' --schema "$T/loose.schema.json" --root "$T" "$MAN"

echo "run-require-fixtures: cases=$CASES failures=$FAILS"
[ "$FAILS" -eq 0 ] || exit 1
exit 0

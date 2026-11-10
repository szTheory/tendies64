#!/bin/sh
# GOV-02 per-copy provenance gate driver (POSIX sh). Lists every file git knows
# about (tracked plus untracked and not ignored) and hands the list and
# provenance/copies.json to t64-provenance-check, which owns every rule id.
#
#   check-provenance.sh
#
# The binary is ${T64_BUILD_DIR:-build/gate}/tools/provenance/t64-provenance-check.
# Output is the checker's own: "RULE <id> <path>: message", the summary line
# "provenance: files=N copied_looking=C entries=E errors=X" and "ok <file>".
# Exit 0 clean, 1 findings, 2 harness error (no git, no binary).
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
BUILD=${T64_BUILD_DIR:-build/gate}
case $BUILD in
  /*) ;;
  *) BUILD=$ROOT/$BUILD ;;
esac
BIN=$BUILD/tools/provenance/t64-provenance-check

if [ $# -ne 0 ]; then
  echo "usage: check-provenance.sh" >&2
  exit 2
fi
if [ ! -x "$BIN" ]; then
  echo "check-provenance: error: $BIN is not built (run the gate or cmake --build)" >&2
  exit 2
fi

TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64prov.XXXXXX")
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM HUP

cd "$ROOT"
# The list goes through a file so that git's exit status is checked.
if ! git ls-files -z --cached --others --exclude-standard > "$TMP/list"; then
  echo "check-provenance: error: git ls-files failed (not a git checkout?)" >&2
  exit 2
fi

rc=0
"$BIN" --root . --copies provenance/copies.json --files-from "$TMP/list" || rc=$?
exit "$rc"

#!/bin/sh
# Tendies64 gate: the single local command (GOV-05). POSIX sh, no bashisms.
# Steps: banner, sh-n, configure, build, ctest, summary. Later plans add steps.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
cd "$ROOT"

GATE_T0=$(date +%s)
GATE_STEPS=""

# step NAME CMD...: run CMD, print "[gate] NAME ok (Ns)" or "[gate] NAME FAILED".
# The command's output is never piped, so its exit status is the step status.
step() {
  _name=$1
  shift
  _t0=$(date +%s)
  if "$@"; then
    :
  else
    echo "[gate] $_name FAILED"
    exit 1
  fi
  _dt=$(($(date +%s) - _t0))
  echo "[gate] $_name ok (${_dt}s)"
  GATE_STEPS="${GATE_STEPS}${_name} ${_dt}s
"
}

banner() {
  echo "[gate] host: $(uname -s -m)"
  echo "[gate] cc: $(${CC:-cc} --version 2>&1 | head -n 1 || true)"
  echo "[gate] cmake: $(cmake --version 2>&1 | head -n 1 || true)"
  echo "[gate] ninja: $(ninja --version 2>&1 | head -n 1 || true)"
  echo "[gate] git: $(git --version 2>&1 | head -n 1 || true)"
  echo "[gate] awk: $( (awk --version 2>&1 || awk -W version 2>&1 || true) | head -n 1)"
  return 0
}

sh_n() {
  _list=$(find scripts tools tests -type f -name '*.sh' 2>/dev/null | sort) || _list=""
  if [ -z "$_list" ]; then
    echo "[gate] sh-n: no shell scripts found (zero files is a failure)" >&2
    return 1
  fi
  _n=0
  _ifs=$IFS
  IFS='
'
  set -f
  for _f in $_list; do
    if ! sh -n "$_f"; then
      IFS=$_ifs
      set +f
      echo "[gate] sh-n: syntax error in $_f" >&2
      return 1
    fi
    _n=$((_n + 1))
  done
  IFS=$_ifs
  set +f
  echo "[gate] sh-n: checked $_n script(s)"
}

banner
step sh-n sh_n
step configure cmake --preset gate
step build cmake --build --preset gate
step ctest ctest --preset gate

GATE_TOTAL=$(($(date +%s) - GATE_T0))
echo "[gate] summary:"
printf '%s' "$GATE_STEPS" | while IFS= read -r _line; do
  echo "[gate]   $_line"
done
echo "[gate] total ${GATE_TOTAL}s"
echo "[gate] PASS"

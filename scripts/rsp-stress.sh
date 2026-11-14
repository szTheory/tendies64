#!/bin/sh
set -eu

if [ "$#" -ne 3 ] || [ "$1" != "--authored" ] || \
   [ "$2" != "--family" ] || [ "$3" != "vmulf-through-vrsq32" ]; then
  echo "usage: scripts/rsp-stress.sh --authored --family vmulf-through-vrsq32" >&2
  exit 2
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd -P)
BUILD_DIR="$ROOT/build/gate"
FUZZ_BINARY="$BUILD_DIR/core/test_rsp_fuzz"

if [ ! -x "$FUZZ_BINARY" ]; then
  echo "missing built native fuzz target: build/gate/core/test_rsp_fuzz" >&2
  echo "configure gate and build test_rsp_fuzz before running authored stress" >&2
  exit 2
fi

exec python3 -B - "$ROOT" "$FUZZ_BINARY" <<'PY'
import json
import pathlib
import resource
import subprocess
import sys
import time

root = pathlib.Path(sys.argv[1])
binary = pathlib.Path(sys.argv[2])
build = root / "build" / "gate"
log_path = build / "05-08-rsp-stress.log"
record_path = build / "05-08-rsp-stress-resource.json"
seed = 0x05F8C0DE
iterations = 100_000
process_timeout_seconds = 600

start = time.monotonic()
timed_out = False
cleanup_sent = False
return_code = 0
with log_path.open("w", encoding="utf-8") as output:
    process = subprocess.Popen(
        [str(binary), "--stress", hex(seed), str(iterations)],
        cwd=root,
        stdout=output,
        stderr=subprocess.STDOUT,
        close_fds=True,
    )
    try:
        return_code = process.wait(timeout=process_timeout_seconds)
    except subprocess.TimeoutExpired:
        timed_out = True
        cleanup_sent = True
        process.terminate()
        try:
            return_code = process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            return_code = process.wait()
elapsed = time.monotonic() - start
usage = resource.getrusage(resource.RUSAGE_CHILDREN)
record = {
    "family": "vmulf-through-vrsq32",
    "seed": f"0x{seed:08x}",
    "iterations_per_opcode": iterations,
    "process_timeout_seconds": process_timeout_seconds,
    "timed_out": timed_out,
    "cleanup_sent": cleanup_sent,
    "process_reaped": process.poll() is not None,
    "exit_code": return_code,
    "elapsed_seconds": elapsed,
    "max_rss_native": usage.ru_maxrss,
}
record_path.write_text(json.dumps(record, sort_keys=True) + "\n", encoding="utf-8")
if timed_out:
    print("authored RSP stress timed out; owned process was stopped and reaped", file=sys.stderr)
    raise SystemExit(124)
if return_code != 0:
    print(f"authored RSP stress failed with exit code {return_code}", file=sys.stderr)
    raise SystemExit(return_code if return_code > 0 else 1)
print(
    f"PASS: authored stress seed=0x{seed:08x} iterations_per_opcode={iterations} "
    f"elapsed_seconds={elapsed:.3f} max_rss_native={usage.ru_maxrss} "
    "timeout_seconds=600 process_reaped=true"
)
PY

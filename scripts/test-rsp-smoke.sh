#!/bin/sh
set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

sh scripts/build-rsp-smoke.sh --build
sh scripts/build-rsp-smoke.sh --selftest
python3 tests/test_rsp_smoke.py

#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

CC=${CC:-cc}
PACKER=build/fixtures/t64_rom_pack

mkdir -p build/fixtures
"$CC" -std=c17 -Wall -Wextra -Werror -pedantic \
    tools/fixture/t64_rom_pack.c -o "$PACKER"

case "${1:-}" in
    --selftest)
        exec "$PACKER" --selftest
        ;;
    *)
        printf '%s\n' "usage: sh scripts/build-boot-fixtures.sh --selftest" >&2
        exit 2
        ;;
esac

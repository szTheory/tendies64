#!/bin/sh
set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

PACKER=build/private-cpu-diagnostic/t64_rom_pack
BOOT=build/fixtures/ipl3_compat.z64
AS=${T64_MIPS_AS:-build/tools/install/bin/mips64-elf-as}
LD=${T64_MIPS_LD:-build/tools/install/bin/mips64-elf-ld}
OBJCOPY=${T64_MIPS_OBJCOPY:-build/tools/install/bin/mips64-elf-objcopy}
OBJDUMP=${T64_MIPS_OBJDUMP:-build/tools/install/bin/mips64-elf-objdump}
EXPECTED_ASSET_SHA256=f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068

die()
{
    printf 'error: %s\n' "$*" >&2
    exit 1
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 || die "required host command is missing: $1"
}

sha256_file()
{
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    elif command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        die 'shasum or sha256sum is required'
    fi
}

compile_packer()
{
    require_command "${CC:-cc}"
    mkdir -p build/private-cpu-diagnostic
    "${CC:-cc}" -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow \
        -Wstrict-prototypes -pedantic -Itools/common \
        tools/fixture/t64_rom_pack.c tools/common/t64_sha256.c -o "$PACKER"
}

verify_preuse_qualification()
{
    require_command python3
    qualification_root=${T64_CPU_DIAGNOSTIC_QUALIFICATION_ROOT:-build/private-cpu-diagnostic}
    [ -d "$qualification_root" ] && [ ! -L "$qualification_root" ] || {
        printf '%s\n' 'pre-use qualification refused: private qualification root is missing' >&2
        exit 4
    }
    python3 scripts/verify-cpu-diagnostic-qualification.py \
        "$qualification_root/qualification.json" "$ROOT" 2>&1 || exit 4
}

run_selftest()
{
    compile_packer
    "$PACKER" --selftest
    printf '%s\n' 'PASS: CPU-profile pack bounds are offline and authored-only'
}

verify_boot_asset()
{
    [ -f "$BOOT" ] && [ ! -L "$BOOT" ] || \
        die 'the pinned cleared IPL3 asset must already exist at build/fixtures/ipl3_compat.z64'
    [ "$(wc -c < "$BOOT" | tr -d '[:space:]')" = 4096 ] || \
        die 'the pinned cleared IPL3 asset has the wrong size'
    [ "$(sha256_file "$BOOT")" = "$EXPECTED_ASSET_SHA256" ] || \
        die 'the pinned cleared IPL3 asset SHA-256 does not match'
}

verify_target_tools()
{
    for tool in "$AS" "$LD" "$OBJCOPY" "$OBJDUMP"; do
        [ -x "$tool" ] || die "audited target tool is unavailable: $tool"
        version=$("$tool" --version 2>&1 | sed -n '1p')
        case "$version" in *'GNU Binutils) 2.45'*) ;; *) die "unexpected target tool version: $version" ;; esac
    done
    "$AS" --version 2>&1 | grep -F 'mips64-elf' >/dev/null || \
        die 'assembler target is not mips64-elf'
    "$LD" -V 2>&1 | grep -F 'elf32ebmip' >/dev/null || \
        die 'linker lacks the required big-endian 32-bit MIPS emulation'
    "$OBJCOPY" --info 2>&1 | grep -F 'elf32-bigmips' >/dev/null || \
        die 'objcopy lacks the required big-endian MIPS format'
    "$OBJDUMP" --info 2>&1 | grep -F 'elf32-bigmips' >/dev/null || \
        die 'objdump lacks the required big-endian MIPS format'
}

run_qualify()
{
    verify_preuse_qualification
    compile_packer
    "$PACKER" --selftest >/dev/null || die 'packer selftest failed'
    verify_boot_asset
    verify_target_tools
    temp=$(mktemp -d build/private-cpu-diagnostic/cpu-profile.XXXXXX)
    trap 'rm -rf "$temp"' EXIT HUP INT TERM
    "$AS" -32 -mips3 -EB --defsym=VARIANT=0 \
        -o "$temp/hello.o" fixtures/hello/hello.S
    "$LD" -m elf32ebmip -EB -T fixtures/hello/hello.ld \
        -o "$temp/hello.elf" "$temp/hello.o"
    "$OBJCOPY" -O binary "$temp/hello.elf" "$temp/hello.payload.bin"
    entry=$("$OBJDUMP" -f "$temp/hello.elf" |
        awk '$1 == "start" && $2 == "address" { print $3 }')
    [ "$entry" = 0x80000400 ] || die "unexpected authored guest entry: $entry"
    "$PACKER" --pack --cpu-profile --boot "$BOOT" \
        --payload "$temp/hello.payload.bin" --output "$temp/hello.z64" \
        --entry "$entry" --clock 0x0000000f --crc2 0x00000000
    image_bytes=$(wc -c < "$temp/hello.z64" | tr -d '[:space:]')
    payload_bytes=$(wc -c < "$temp/hello.payload.bin" | tr -d '[:space:]')
    [ "$image_bytes" = "$((payload_bytes + 4096))" ] || \
        die 'CPU-profile image length does not equal payload plus 0x1000'
    printf 'PASS: authored CPU-profile flat image (%s bytes, entry %s)\n' \
        "$image_bytes" "$entry"
}

case "${1:-}" in
    --selftest) run_selftest ;;
    --qualify) run_qualify ;;
    *) printf '%s\n' 'usage: sh scripts/build-cpu-diagnostic.sh --selftest|--qualify' >&2; exit 2 ;;
esac

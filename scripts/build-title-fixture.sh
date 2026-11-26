#!/bin/sh
# Builds the two Phase 12 title fixture ROMs from fixtures/title/title.S.
# Provenance: a copy of scripts/build-composite-fixture.sh (plan 12-03), itself a
# copy of scripts/build-save-fixtures.sh and the earlier pad, admit and audio
# builders (same pinned asset, payload, Binutils and packer source hashes, same
# prerequisite and layout checks, same manifest identity check under --qualify)
# with its own source list, stems, completion symbol and metadata schema. The
# earlier builders and scripts/build-boot-fixtures.sh are not modified.
# Distributed under the repository license (Apache-2.0).
set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

BOOT=build/fixtures/ipl3_compat.z64
PAYLOAD=build/fixtures/ipl3_compat.payload.bin
TOOL_DIR=build/tools/install/bin
STAMP=build/tools/qualified-host-tools.json
PACKER=build/fixtures/t64_rom_pack
AS=${T64_MIPS_AS:-$TOOL_DIR/mips64-elf-as}
LD=${T64_MIPS_LD:-$TOOL_DIR/mips64-elf-ld}
OBJCOPY=${T64_MIPS_OBJCOPY:-$TOOL_DIR/mips64-elf-objcopy}
OBJDUMP=${T64_MIPS_OBJDUMP:-$TOOL_DIR/mips64-elf-objdump}

ASSET_SHA256=f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068
PAYLOAD_SHA256=f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5
BINUTILS_SHA256=c50c0e7f9cb188980e2cc97e4537626b1672441815587f1eab69d2a1bfbef5d2

# The title stem and its one-defsym refused-frame control (the same source).
TITLE_STEMS='phase12-title-s32k phase12-title-refused-s32k'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
sha256_stream() {
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 | awk '{print $1}'
    elif command -v sha256sum >/dev/null 2>&1; then
        sha256sum | awk '{print $1}'
    else
        die 'shasum or sha256sum is required'
    fi
}
sha256_file() { sha256_stream < "$1"; }
file_size() { wc -c < "$1" | tr -d '[:space:]'; }
tool_version() { "$1" --version 2>&1 | sed -n '1p'; }

verify_prerequisites() {
    [ -f "$BOOT" ] && [ ! -L "$BOOT" ] || die 'pinned openIPL3 asset is absent or not a regular file'
    [ -f "$PAYLOAD" ] && [ ! -L "$PAYLOAD" ] || die 'pinned openIPL3 payload is absent or not a regular file'
    [ "$(file_size "$BOOT")" = 4096 ] || die 'pinned openIPL3 asset has the wrong size'
    [ "$(sha256_file "$BOOT")" = "$ASSET_SHA256" ] || die 'pinned openIPL3 asset hash mismatch'
    [ "$(file_size "$PAYLOAD")" = 4032 ] || die 'pinned openIPL3 payload has the wrong size'
    [ "$(sha256_file "$PAYLOAD")" = "$PAYLOAD_SHA256" ] || die 'pinned openIPL3 payload hash mismatch'
    [ -f "$STAMP" ] || die 'qualified host tool receipt is absent'
    for tool in "$AS" "$LD" "$OBJCOPY" "$OBJDUMP"; do
        [ -x "$tool" ] || die "qualified host tool is unavailable: $tool"
    done
    jq -e --arg as "$(sha256_file "$AS")" --arg ld "$(sha256_file "$LD")" \
        --arg oc "$(sha256_file "$OBJCOPY")" --arg od "$(sha256_file "$OBJDUMP")" \
        --arg binutils "$BINUTILS_SHA256" \
        '.source.sha256 == $binutils and .tools.as.sha256 == $as and
         .tools.ld.sha256 == $ld and .tools.objcopy.sha256 == $oc and
         .tools.objdump.sha256 == $od' "$STAMP" >/dev/null || \
        die 'qualified host tool hashes differ from the saved receipt'
    [ "$(tool_version "$AS")" = 'GNU assembler (GNU Binutils) 2.45' ] || die 'unexpected MIPS assembler version'
    [ "$(tool_version "$LD")" = 'GNU ld (GNU Binutils) 2.45' ] || die 'unexpected MIPS linker version'
    [ "$(tool_version "$OBJCOPY")" = 'GNU objcopy (GNU Binutils) 2.45' ] || die 'unexpected MIPS objcopy version'
    [ "$(tool_version "$OBJDUMP")" = 'GNU objdump (GNU Binutils) 2.45' ] || die 'unexpected MIPS objdump version'
    "$AS" --version 2>&1 | grep -F 'mips64-elf' >/dev/null || die 'assembler target is not mips64-elf'
    "$LD" -V 2>&1 | grep -F 'elf32ebmip' >/dev/null || die 'linker lacks elf32ebmip'
    "$OBJCOPY" --info 2>&1 | grep -F 'elf32-bigmips' >/dev/null || die 'objcopy lacks big-endian MIPS output'
    "$OBJDUMP" --info 2>&1 | grep -F 'elf32-bigmips' >/dev/null || die 'objdump lacks big-endian MIPS support'
    [ "$(sha256_file tools/fixture/t64_rom_pack.c)" = \
        d91119ded68875c08342a088050c820ab47d885ea9f05b8e835de8fe0a6c2a93 ] || \
        die 'ROM packer source differs from its qualified source pin'
    [ "$(sha256_file tools/common/t64_sha256.c)" = \
        de587155f2c10e3a5c8cc9814f9c14cbe485bc20554ea725bbe73b914e053940 ] || \
        die 'ROM packer hash source differs from its qualified source pin'
    [ -x "$PACKER" ] || die 'qualified ROM packer executable is unavailable'
}

# The oracle, the spec, the expectations and the input scripts are not inputs
# of the ROM bytes, so they are left out and can change without moving the
# manifest rows.
source_content_sha256() {
    cat fixtures/title/title.S fixtures/title/title.ld \
        tools/fixture/t64_rom_pack.c tools/common/t64_sha256.c \
        tools/common/t64_sha256.h scripts/build-title-fixture.sh | sha256_stream
}

linked_symbol() {
    symbol=$1
    elf=$2
    hex=$("$OBJDUMP" -t "$elf" | awk -v sym="$symbol" '$NF == sym { print $1 }')
    [ -n "$hex" ] || die "linked fixture is missing symbol $symbol"
    printf '0x%s\n' "$hex"
}

build_variant() {
    output_dir=$1
    stem=$2
    source=$3
    script=$4
    defsyms=$5
    object="$output_dir/$stem.o"
    elf="$output_dir/$stem.elf"
    flat="$output_dir/$stem.payload.bin"
    rom="$output_dir/$stem.z64"
    set -- -32 -mips3 -EB
    for defsym in $defsyms; do
        set -- "$@" --defsym "$defsym=1"
    done
    "$AS" "$@" -o "$object" "$source"
    "$LD" -m elf32ebmip -EB -T "$script" -o "$elf" "$object"
    "$OBJCOPY" -O binary "$elf" "$flat"
    [ -s "$flat" ] || die "linked $stem payload is empty"
    entry=$("$OBJDUMP" -f "$elf" | awk '$1 == "start" && $2 == "address" { print $3 }')
    clock=$(linked_symbol __t64_rom_clock "$elf")
    crc2=$(linked_symbol __t64_rom_crc2 "$elf")
    [ "$entry" = 0x80000400 ] || die "unexpected linked $stem entry: $entry"
    [ "$clock" = 0x0000000f ] || die "unexpected linked $stem clock: $clock"
    [ "$crc2" = 0x00000000 ] || die "unexpected linked $stem CRC2 policy: $crc2"
    completion=$(linked_symbol t64_title_completion "$elf")
    "$OBJDUMP" -d "$elf" | awk -v pc="${completion#0x}:" '$1 == pc { print $3; exit }' \
        | grep -x break >/dev/null || die "linked $stem completion is not a break"
    printf '%s\n' "$completion" > "$output_dir/$stem.completion"
    "$PACKER" --pack --boot "$BOOT" --payload "$flat" --output "$rom" \
        --entry "$entry" --clock "$clock" --crc2 "$crc2"
    packed_payload="$output_dir/$stem.packed-payload.bin"
    embedded_ipl3="$output_dir/$stem.embedded-ipl3.bin"
    dd if="$rom" of="$packed_payload" bs=1 skip=4096 \
        count="$(file_size "$flat")" 2>/dev/null
    dd if="$rom" of="$embedded_ipl3" bs=1 skip=64 count=4032 2>/dev/null
    cmp "$flat" "$packed_payload" >/dev/null || die "packer changed $stem payload bytes"
    cmp "$PAYLOAD" "$embedded_ipl3" >/dev/null || die "packer changed pinned IPL3 bytes in $stem"
    rm -f "$packed_payload" "$embedded_ipl3"
}

write_metadata() {
    output_dir=$1
    stem=$2
    source_hash=$(source_content_sha256)
    jq -n --arg source_hash "$source_hash" \
        --arg rom_sha "$(sha256_file "$output_dir/$stem.z64")" \
        --arg payload_sha "$(sha256_file "$output_dir/$stem.payload.bin")" \
        --arg elf_sha "$(sha256_file "$output_dir/$stem.elf")" \
        --arg as_sha "$(sha256_file "$AS")" --arg ld_sha "$(sha256_file "$LD")" \
        --arg objcopy_sha "$(sha256_file "$OBJCOPY")" \
        --arg objdump_sha "$(sha256_file "$OBJDUMP")" \
        --arg asset_sha "$ASSET_SHA256" --arg payload_ipl3_sha "$PAYLOAD_SHA256" \
        --argjson rom_size "$(file_size "$output_dir/$stem.z64")" \
        --argjson payload_size "$(file_size "$output_dir/$stem.payload.bin")" \
        --arg completion "$(cat "$output_dir/$stem.completion")" \
        '{schema:"t64.phase12-title-fixture-build.v1",source_content_sha256:$source_hash,
          rom_sha256:$rom_sha,rom_size:$rom_size,payload_sha256:$payload_sha,
          payload_size:$payload_size,elf_sha256:$elf_sha,entry_pc:"0x80000400",completion_pc:$completion,
          openipl3_asset_sha256:$asset_sha,openipl3_payload_sha256:$payload_ipl3_sha,
          tool_sha256:{as:$as_sha,ld:$ld_sha,objcopy:$objcopy_sha,objdump:$objdump_sha}}' \
        > "$output_dir/$stem.build.json"
}

# A hash proves identity, not rights: the built ROM identity of each stem must
# equal its manifest row (id, SHA-256 and size).
check_manifest_identities() {
    for stem in $TITLE_STEMS; do
        expected=$(jq -r --arg id "$stem" \
            '.entries[] | select(.id == $id) | "\(.sha256) \(.size)"' \
            fixtures/manifest.json)
        [ -n "$expected" ] || die "manifest row is absent for $stem"
        actual="$(sha256_file "build/fixtures/$stem.z64") $(file_size "build/fixtures/$stem.z64")"
        [ "$actual" = "$expected" ] || \
            die "ROM identity of $stem differs from its manifest row"
    done
}

run_build() {
    staging="build/fixtures/.phase12-title-stage.$$"
    cleanup() { rm -rf "$staging"; }
    mkdir -p "$staging"
    trap cleanup EXIT HUP INT TERM
    build_variant "$staging" phase12-title-s32k \
        fixtures/title/title.S fixtures/title/title.ld ''
    build_variant "$staging" phase12-title-refused-s32k \
        fixtures/title/title.S fixtures/title/title.ld T64_TITLE_REFUSED
    for stem in $TITLE_STEMS; do
        write_metadata "$staging" "$stem"
        mv "$staging/$stem.z64" "build/fixtures/$stem.z64"
        mv "$staging/$stem.elf" "build/fixtures/$stem.elf"
        mv "$staging/$stem.payload.bin" "build/fixtures/$stem.payload.bin"
        mv "$staging/$stem.build.json" "build/fixtures/$stem.json"
    done
    trap - EXIT HUP INT TERM
    cleanup
}

verify_prerequisites
case "${1:-}" in
    --build)
        run_build
        ;;
    --qualify)
        for stem in $TITLE_STEMS; do
            [ -s "build/fixtures/$stem.z64" ] || die 'generated Phase 12 title fixtures are absent'
        done
        check_manifest_identities
        printf '%s\n' 'qualified Phase 12 title fixture prerequisites and generated payloads'
        ;;
    *)
        printf '%s\n' 'usage: sh scripts/build-title-fixture.sh --build|--qualify' >&2
        exit 2
        ;;
esac

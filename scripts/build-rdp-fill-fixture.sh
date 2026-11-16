#!/bin/sh
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
CC=${CC:-cc}

ASSET_SHA256=f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068
PAYLOAD_SHA256=f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5
BINUTILS_SHA256=c50c0e7f9cb188980e2cc97e4537626b1672441815587f1eab69d2a1bfbef5d2

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

    AS_VERSION=$(tool_version "$AS")
    LD_VERSION=$(tool_version "$LD")
    OBJCOPY_VERSION=$(tool_version "$OBJCOPY")
    OBJDUMP_VERSION=$(tool_version "$OBJDUMP")
    [ "$AS_VERSION" = 'GNU assembler (GNU Binutils) 2.45' ] || die "unexpected assembler version: $AS_VERSION"
    [ "$LD_VERSION" = 'GNU ld (GNU Binutils) 2.45' ] || die "unexpected linker version: $LD_VERSION"
    [ "$OBJCOPY_VERSION" = 'GNU objcopy (GNU Binutils) 2.45' ] || die "unexpected objcopy version: $OBJCOPY_VERSION"
    [ "$OBJDUMP_VERSION" = 'GNU objdump (GNU Binutils) 2.45' ] || die "unexpected objdump version: $OBJDUMP_VERSION"
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
}

source_content_sha256() {
    cat fixtures/rdp/fill.S fixtures/rdp/fill.ld \
        tools/fixture/t64_rom_pack.c tools/common/t64_sha256.c \
        tools/common/t64_sha256.h scripts/build-rdp-fill-fixture.sh | sha256_stream
}

linked_entry() {
    "$OBJDUMP" -f "$1" | awk '$1 == "start" && $2 == "address" { print $3 }'
}
linked_symbol() {
    symbol=$1
    elf=$2
    hex=$("$OBJDUMP" -t "$elf" | awk -v sym="$symbol" '$NF == sym { print $1 }')
    [ -n "$hex" ] || die "linked fixture is missing symbol $symbol"
    printf '0x%s\n' "$hex"
}

build_to() {
    output_dir=$1
    mkdir -p "$output_dir"
    object="$output_dir/rdp-fill.o"
    elf="$output_dir/rdp-fill.elf"
    flat="$output_dir/rdp-fill.payload.bin"
    rom="$output_dir/rdp-fill.z64"
    "$AS" -32 -mips3 -EB -o "$object" fixtures/rdp/fill.S
    "$LD" -m elf32ebmip -EB -T fixtures/rdp/fill.ld -o "$elf" "$object"
    "$OBJCOPY" -O binary "$elf" "$flat"
    [ -s "$flat" ] || die 'linked fill payload is empty'
    entry=$(linked_entry "$elf")
    clock=$(linked_symbol __t64_rom_clock "$elf")
    crc2=$(linked_symbol __t64_rom_crc2 "$elf")
    [ "$entry" = 0x80000400 ] || die "unexpected linked fill entry: $entry"
    [ "$clock" = 0x0000000f ] || die "unexpected linked fill clock: $clock"
    [ "$crc2" = 0x00000000 ] || die "unexpected linked fill CRC2 policy: $crc2"
    "$PACKER" --pack --boot "$BOOT" --payload "$flat" --output "$rom" \
        --entry "$entry" --clock "$clock" --crc2 "$crc2"

    packed_payload="$output_dir/rdp-fill.packed-payload.bin"
    embedded_ipl3="$output_dir/rdp-fill.embedded-ipl3.bin"
    dd if="$rom" of="$packed_payload" bs=1 skip=4096 count="$(file_size "$flat")" 2>/dev/null
    dd if="$rom" of="$embedded_ipl3" bs=1 skip=64 count=4032 2>/dev/null
    cmp "$flat" "$packed_payload" >/dev/null || die 'ROM packer changed authored fill payload bytes'
    cmp "$PAYLOAD" "$embedded_ipl3" >/dev/null || die 'ROM packer changed the pinned IPL3 payload bytes'
    rm -f "$packed_payload" "$embedded_ipl3"
}

write_metadata() {
    output_dir=$1
    source_hash=$(source_content_sha256)
    rom="$output_dir/rdp-fill.z64"
    flat="$output_dir/rdp-fill.payload.bin"
    jq -n --arg source_hash "$source_hash" \
        --arg rom_sha "$(sha256_file "$rom")" --arg payload_sha "$(sha256_file "$flat")" \
        --arg as_version "$AS_VERSION" --arg ld_version "$LD_VERSION" \
        --arg objcopy_version "$OBJCOPY_VERSION" --arg objdump_version "$OBJDUMP_VERSION" \
        --arg as_sha "$(sha256_file "$AS")" --arg ld_sha "$(sha256_file "$LD")" \
        --arg objcopy_sha "$(sha256_file "$OBJCOPY")" --arg objdump_sha "$(sha256_file "$OBJDUMP")" \
        --argjson rom_size "$(file_size "$rom")" --argjson payload_size "$(file_size "$flat")" \
        --arg asset_sha "$ASSET_SHA256" --arg embedded_sha "$PAYLOAD_SHA256" \
        '{schema:"t64.rdp-fill-build-metadata",schema_version:1,
          fixture_id:"rdp-vi-fill-diagnostic",source_content_sha256:$source_hash,
          authored_payload:{size_bytes:$payload_size,sha256:$payload_sha},
          whole_rom:{size_bytes:$rom_size,sha256:$rom_sha},
          embedded_ipl3:{source_asset_size_bytes:4096,source_asset_sha256:$asset_sha,
            source_offset:"0x40",size_bytes:4032,sha256:$embedded_sha,
            destination_offset:"0x40",rights_scope:"separately recorded Unlicense asset"},
          recipe:{assembler:"mips64-elf-as -32 -mips3 -EB",
            linker:"mips64-elf-ld -m elf32ebmip -EB",linker_script:"fixtures/rdp/fill.ld",
            rom_packer:"tools/fixture/t64_rom_pack.c",
            command:"sh scripts/build-rdp-fill-fixture.sh --build"},
          host_tools:{as:{version:$as_version,sha256:$as_sha},ld:{version:$ld_version,sha256:$ld_sha},
            objcopy:{version:$objcopy_version,sha256:$objcopy_sha},
            objdump:{version:$objdump_version,sha256:$objdump_sha}},
          metadata_scope:"Host tool hashes identify this host build and are excluded from guest/profile identity."}' \
        > "$output_dir/rdp-fill.build.json"
}

install_output() {
    source=$1
    destination=$2
    temporary="${destination}.tmp.$$"
    cp "$source" "$temporary"
    mv "$temporary" "$destination"
}

run_build() {
    staging=$(mktemp -d build/fixtures/rdp-fill-build.XXXXXX)
    cleanup() { rm -rf "$staging"; }
    trap cleanup EXIT HUP INT TERM
    "$CC" -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow \
        -Wstrict-prototypes -pedantic -Itools/common \
        tools/fixture/t64_rom_pack.c tools/common/t64_sha256.c -o "$PACKER"
    build_to "$staging"
    write_metadata "$staging"
    mkdir -p build/fixtures
    install_output "$staging/rdp-fill.z64" build/fixtures/rdp-fill.z64
    install_output "$staging/rdp-fill.payload.bin" build/fixtures/rdp-fill.payload.bin
    install_output "$staging/rdp-fill.build.json" build/fixtures/rdp-fill.build.json
    trap - EXIT HUP INT TERM
    cleanup
}

verify_source_revision() {
    revision=$1
    case "$revision" in *[!0-9a-f]*|'') die 'RDP fixture source revision is not lowercase hexadecimal' ;; esac
    [ "${#revision}" -eq 40 ] || die 'RDP fixture source revision is not a 40-character commit'
    for file in fixtures/rdp/fill.S fixtures/rdp/fill.ld \
        tools/fixture/t64_rom_pack.c tools/common/t64_sha256.c \
        tools/common/t64_sha256.h scripts/build-rdp-fill-fixture.sh; do
        git cat-file -e "$revision:$file" 2>/dev/null || die "source revision lacks $file"
        committed_hash=$(git show "$revision:$file" | sha256_stream)
        current_hash=$(sha256_file "$file")
        [ "$committed_hash" = "$current_hash" ] || die "committed source differs for $file"
    done
}

verify_manifest() {
    rom=build/fixtures/rdp-fill.z64
    flat=build/fixtures/rdp-fill.payload.bin
    rom_sha=$(sha256_file "$rom")
    flat_sha=$(sha256_file "$flat")
    rom_size=$(file_size "$rom")
    flat_size=$(file_size "$flat")
    source_hash=$(source_content_sha256)
    revision=$(jq -er '[.entries[] | select(.id == "rdp-fill")][0].source.revision' fixtures/manifest.json) || \
        die 'RDP fixture source revision is missing from the manifest'
    payload_revision=$(jq -er '[.entries[] | select(.id == "rdp-fill-payload")][0].source.revision' fixtures/manifest.json) || \
        die 'RDP payload source revision is missing from the manifest'
    [ "$payload_revision" = "$revision" ] || die 'RDP payload and ROM source revisions differ'
    verify_source_revision "$revision"
    jq -e --arg rom "$rom_sha" --arg flat "$flat_sha" \
        --arg source_hash "$source_hash" --arg revision "$revision" \
        --argjson rom_size "$rom_size" --argjson flat_size "$flat_size" \
        --arg asset "$ASSET_SHA256" --arg payload "$PAYLOAD_SHA256" \
        '([.entries[] | select(.id == "rdp-fill")][0] |
            (.source.path == "fixtures/rdp/fill.S" and .sha256 == $rom and .size == $rom_size and
            .source.revision == $revision and (.build.recipe | contains($source_hash)) and
            (.build.recipe | contains($flat)) and
            .expected.revision == ("source-content-sha256:" + $source_hash) and
            .source_rights == "authored-here" and .binary_rights == "cleared" and
            .distribution == ["build-only"])) and
         ([.entries[] | select(.id == "rdp-fill-payload")][0] |
            (.source.path == "fixtures/rdp/fill.S" and .sha256 == $flat and .size == $flat_size and
            .source.revision == $revision and (.build.recipe | contains($source_hash)) and
            .source_rights == "authored-here" and .binary_rights == "cleared" and
            .distribution == ["build-only"])) and
         ([.entries[] | select(.id == "openipl3-compat-asset")][0] |
            .sha256 == $asset and .size == 4096) and
         ([.entries[] | select(.id == "openipl3-compat-payload")][0] |
            .sha256 == $payload and .size == 4032)' \
        fixtures/manifest.json >/dev/null || die 'manifest does not bind RDP source, generated bytes and rights'
    if [ -x build/gate/tools/manifest/t64-manifest-check ]; then
        build/gate/tools/manifest/t64-manifest-check --schema fixtures/manifest.schema.json \
            --root "$ROOT" fixtures/manifest.json >/dev/null || die 'manifest checker rejected fixtures/manifest.json'
    fi
}

verify_prerequisites
case "${1:-}" in
    --build)
        run_build
        ;;
    --qualify)
        run_build
        verify_manifest
        printf '%s\n' 'qualified authored RDP fill source, tool receipt, IPL3 composition, and manifest'
        ;;
    *)
        printf '%s\n' 'usage: sh scripts/build-rdp-fill-fixture.sh --build|--qualify' >&2
        exit 2
        ;;
esac

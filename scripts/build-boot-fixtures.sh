#!/bin/sh
set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

BOOT=build/fixtures/ipl3_compat.z64
PAYLOAD=build/fixtures/ipl3_compat.payload.bin
TOOL_DIR=build/tools/install/bin
BINUTILS_URL=https://ftp.gnu.org/gnu/binutils/binutils-2.45.tar.xz
BINUTILS_SHA256=c50c0e7f9cb188980e2cc97e4537626b1672441815587f1eab69d2a1bfbef5d2
AS=${T64_MIPS_AS:-$TOOL_DIR/mips64-elf-as}
LD=${T64_MIPS_LD:-$TOOL_DIR/mips64-elf-ld}
OBJCOPY=${T64_MIPS_OBJCOPY:-$TOOL_DIR/mips64-elf-objcopy}
OBJDUMP=${T64_MIPS_OBJDUMP:-$TOOL_DIR/mips64-elf-objdump}
CC=${CC:-cc}
PACKER=build/fixtures/t64_rom_pack
HOST_TOOL_STAMP=build/tools/qualified-host-tools.json

EXPECTED_ASSET_SHA256=f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068
EXPECTED_ASSET_MD5=71ce6a00936394ad364fbe584234fd82
EXPECTED_PAYLOAD_SHA256=f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5

die()
{
    printf 'error: %s\n' "$*" >&2
    exit 1
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 || die "required host command is missing: $1"
}

sha256_stream()
{
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 | awk '{print $1}'
    elif command -v sha256sum >/dev/null 2>&1; then
        sha256sum | awk '{print $1}'
    else
        die 'shasum or sha256sum is required'
    fi
}

sha256_file()
{
    file=$1
    sha256_stream < "$file"
}

md5_file()
{
    file=$1
    if command -v md5 >/dev/null 2>&1; then
        md5 -q "$file"
    elif command -v md5sum >/dev/null 2>&1; then
        md5sum "$file" | awk '{print $1}'
    else
        die 'md5 or md5sum is required for the pinned asset check'
    fi
}

file_size()
{
    wc -c < "$1" | tr -d '[:space:]'
}

tool_version()
{
    "$1" --version 2>&1 | sed -n '1p'
}

download_boot_asset()
{
    if [ -e "$BOOT" ]; then
        return
    fi
    require_command curl
    mkdir -p build/fixtures
    tmp="${BOOT}.download.$$"
    trap 'rm -f "$tmp"' EXIT HUP INT TERM
    curl --fail --location --silent --show-error --proto '=https' --tlsv1.2 \
        --output "$tmp" \
        https://raw.githubusercontent.com/n64brew/libdragon/98154dafd5ac50e74ee09a5de1035d24a53f54f0/boot/bin/ipl3_compat.z64 \
        || die 'failed to fetch the exact pinned IPL3 source artifact'
    [ "$(file_size "$tmp")" = 4096 ] || die 'downloaded IPL3 has the wrong size'
    [ "$(sha256_file "$tmp")" = "$EXPECTED_ASSET_SHA256" ] || \
        die 'downloaded IPL3 does not match its pinned SHA-256'
    mv "$tmp" "$BOOT"
    trap - EXIT HUP INT TERM
}

verify_contract()
{
    require_command jq
    jq -e --arg sha "$EXPECTED_ASSET_SHA256" \
        --arg md5 "$EXPECTED_ASSET_MD5" \
        --arg payload "$EXPECTED_PAYLOAD_SHA256" \
        '.schema == "t64.open-ipl3-boot-contract" and
         .schema_version == 1 and
         .asset.size_bytes == 4096 and
         .asset.sha256 == $sha and
         .asset.md5 == $md5 and
         .asset.copied_payload.source_offset == "0x40" and
         .asset.copied_payload.length_bytes == 4032 and
         .asset.copied_payload.end_offset_exclusive == "0x1000" and
         .asset.copied_payload.sha256 == $payload and
         .asset.copied_payload.destination_virtual_address == "0xA4000040" and
         .asset.copied_payload.linked_entry_pc == "0xA4000040" and
         .toolchain.name == "GNU Binutils" and .toolchain.version == "2.45" and
         .toolchain.source_sha256 == "c50c0e7f9cb188980e2cc97e4537626b1672441815587f1eab69d2a1bfbef5d2" and
         .toolchain.host_prerequisites.system_zlib.header == "zlib.h" and
         .toolchain.host_prerequisites.system_zlib.link_library == "libz" and
         .toolchain.host_prerequisites.system_zlib.scope == "host toolchain build only" and
         .toolchain.host_prerequisites.system_zlib.linux_ci_package == "zlib1g-dev"' \
        fixtures/boot-contract.json >/dev/null || \
        die 'boot-contract.json does not match the qualified source and toolchain pins'
}

verify_boot_asset()
{
    download_boot_asset
    [ -f "$BOOT" ] && [ ! -L "$BOOT" ] || die 'pinned IPL3 source asset is not a regular file'
    [ "$(file_size "$BOOT")" = 4096 ] || die 'pinned IPL3 source asset has the wrong size'
    [ "$(sha256_file "$BOOT")" = "$EXPECTED_ASSET_SHA256" ] || \
        die 'pinned IPL3 source asset SHA-256 mismatch'
    [ "$(md5_file "$BOOT")" = "$EXPECTED_ASSET_MD5" ] || \
        die 'pinned IPL3 source asset MD5 mismatch'
    verify_contract
}

verify_tool_versions()
{
    local_as=$(tool_version "$AS")
    local_ld=$(tool_version "$LD")
    local_objcopy=$(tool_version "$OBJCOPY")
    local_objdump=$(tool_version "$OBJDUMP")
    case "$local_as" in *'GNU Binutils) 2.45'*) ;; *) die "wrong assembler version: $local_as" ;; esac
    case "$local_ld" in *'GNU Binutils) 2.45'*) ;; *) die "wrong linker version: $local_ld" ;; esac
    case "$local_objcopy" in *'GNU Binutils) 2.45'*) ;; *) die "wrong objcopy version: $local_objcopy" ;; esac
    case "$local_objdump" in *'GNU Binutils) 2.45'*) ;; *) die "wrong objdump version: $local_objdump" ;; esac
    "$AS" --version 2>&1 | grep -F 'mips64-elf' >/dev/null || \
        die 'assembler is not configured for mips64-elf'
    "$LD" -V 2>&1 | grep -F 'elf32ebmip' >/dev/null || \
        die 'linker lacks the required big-endian 32-bit MIPS emulation'
    "$OBJCOPY" --info 2>&1 | grep -F 'elf32-bigmips' >/dev/null || \
        die 'objcopy lacks the required big-endian MIPS format'
    "$OBJDUMP" --info 2>&1 | grep -F 'elf32-bigmips' >/dev/null || \
        die 'objdump lacks the required big-endian MIPS format'
    AS_VERSION=$local_as
    LD_VERSION=$local_ld
    OBJCOPY_VERSION=$local_objcopy
    OBJDUMP_VERSION=$local_objdump
}

safe_extract_binutils()
{
    archive=$1
    temporary=$2
    require_command tar
    tar -tJf "$archive" | awk '
        BEGIN { count = 0 }
        {
            if ($0 == "" || substr($0, 1, 1) == "/" ||
                $0 !~ /^binutils-2[.]45\// || length($0) > 512) exit 1
            n = split($0, part, "/")
            for (i = 1; i <= n; i++) if (part[i] == ".." || part[i] == ".") exit 1
            count++
            if (count > 150000) exit 1
        }
        END { if (count == 0) exit 1 }
    ' || die 'pinned Binutils archive has an unsafe or unexpected path inventory'
    mkdir -p "$temporary"
    tar -xJf "$archive" -C "$temporary"
    [ -d "$temporary/binutils-2.45" ] || die 'Binutils archive has no expected source root'
    file_count=$(find "$temporary/binutils-2.45" -type f | wc -l | tr -d '[:space:]')
    source_kib=$(du -sk "$temporary/binutils-2.45" | awk '{print $1}')
    [ "$file_count" -le 150000 ] && [ "$source_kib" -le 524288 ] || \
        die 'extracted Binutils source exceeds the bounded file inventory'
}

build_pinned_binutils()
{
    require_command curl
    require_command make
    require_command tar
    require_command awk
    require_command "$CC"
    if ! printf '%s\n' '#include <zlib.h>' | "$CC" -E -x c - >/dev/null 2>&1; then
        die 'pinned Binutils source build requires system zlib development headers (zlib.h); install the host zlib development package, such as zlib1g-dev on Ubuntu'
    fi
    mkdir -p build/tools/downloads build/tools/source build/tools/build \
        build/tools/install
    archive=build/tools/downloads/binutils-2.45.tar.xz
    if [ -e "$archive" ]; then
        [ "$(sha256_file "$archive")" = "$BINUTILS_SHA256" ] || \
            die 'existing Binutils archive does not match the pinned source hash'
    else
        download="${archive}.download.$$"
        trap 'rm -f "$download"' EXIT HUP INT TERM
        curl --fail --location --silent --show-error --proto '=https' --tlsv1.2 \
            --output "$download" "$BINUTILS_URL" || die 'failed to fetch pinned Binutils source'
        [ "$(sha256_file "$download")" = "$BINUTILS_SHA256" ] || \
            die 'downloaded Binutils source does not match its official release hash'
        mv "$download" "$archive"
        trap - EXIT HUP INT TERM
    fi

    extraction="build/tools/source/extract.$$"
    safe_extract_binutils "$archive" "$extraction"
    source="build/tools/source/binutils-2.45-$BINUTILS_SHA256"
    [ ! -e "$source" ] || die 'qualified Binutils source directory already exists; refusing to replace it'
    mv "$extraction/binutils-2.45" "$source"
    rmdir "$extraction"
    log=build/tools/binutils-2.45-build.log
    build="build/tools/build/binutils-2.45-$BINUTILS_SHA256"
    mkdir -p "$build"
    if (
        cd "$build"
        "$ROOT/$source/configure" \
            --target=mips64-elf \
            --prefix="$ROOT/build/tools/install" \
            --disable-nls \
            --disable-werror \
            --disable-gdb \
            --disable-gprof \
            --disable-gold \
            --with-system-zlib
        make -j2 all-binutils all-gas all-ld && \
            make install-binutils install-gas install-ld
    ) >"$log" 2>&1; then
        :
    else
        build_status=$?
        echo "error: pinned Binutils source build failed (exit $build_status); last 80 sanitized log lines:" >&2
        tail -n 80 "$log" | sed -E \
            "s|$ROOT|<checkout>|g; s|/home/[^/[:space:]]+|<home>|g; s|/Users/[^/[:space:]]+|<home>|g" >&2
        die "pinned Binutils source build failed (exit $build_status)"
    fi
}

ensure_tools()
{
    paths="$AS $LD $OBJCOPY $OBJDUMP"
    present=0
    total=0
    for path in $paths; do
        total=$((total + 1))
        [ -x "$path" ] && present=$((present + 1))
    done
    if [ "$present" = 0 ]; then
        [ "$AS" = "$TOOL_DIR/mips64-elf-as" ] &&
            [ "$LD" = "$TOOL_DIR/mips64-elf-ld" ] &&
            [ "$OBJCOPY" = "$TOOL_DIR/mips64-elf-objcopy" ] &&
            [ "$OBJDUMP" = "$TOOL_DIR/mips64-elf-objdump" ] || \
            die 'tool overrides must name a complete qualified toolset'
        build_pinned_binutils
    elif [ "$present" != "$total" ]; then
        die 'qualified Binutils installation is incomplete; refusing mixed tool versions'
    fi
    [ -x "$AS" ] && [ -x "$LD" ] && [ -x "$OBJCOPY" ] && [ -x "$OBJDUMP" ] || \
        die 'qualified Binutils tools are unavailable'
}

verify_tool_hashes()
{
    AS_SHA256=$(sha256_file "$AS")
    LD_SHA256=$(sha256_file "$LD")
    OBJCOPY_SHA256=$(sha256_file "$OBJCOPY")
    OBJDUMP_SHA256=$(sha256_file "$OBJDUMP")
    HOST_OS=$(uname -s)
    HOST_ARCH=$(uname -m)

    if [ -f "$HOST_TOOL_STAMP" ]; then
        jq -e --arg os "$HOST_OS" --arg arch "$HOST_ARCH" \
            --arg as "$AS_SHA256" --arg ld "$LD_SHA256" \
            --arg objcopy "$OBJCOPY_SHA256" --arg objdump "$OBJDUMP_SHA256" \
            '.host.os == $os and .host.architecture == $arch and
             .tools.as.sha256 == $as and .tools.ld.sha256 == $ld and
             .tools.objcopy.sha256 == $objcopy and .tools.objdump.sha256 == $objdump' \
            "$HOST_TOOL_STAMP" >/dev/null || die 'qualified host tool binary hash drift'
    else
        mkdir -p build/tools
        jq -n --arg os "$HOST_OS" --arg arch "$HOST_ARCH" \
            --arg as "$AS_SHA256" --arg ld "$LD_SHA256" \
            --arg objcopy "$OBJCOPY_SHA256" --arg objdump "$OBJDUMP_SHA256" \
            '{schema:"t64.qualified-host-tools",schema_version:1,
              source:{name:"GNU Binutils",version:"2.45",sha256:"c50c0e7f9cb188980e2cc97e4537626b1672441815587f1eab69d2a1bfbef5d2"},
              host:{os:$os,architecture:$arch},
              tools:{as:{version:"2.45",sha256:$as},ld:{version:"2.45",sha256:$ld},
                     objcopy:{version:"2.45",sha256:$objcopy},objdump:{version:"2.45",sha256:$objdump}}}' \
            > "$HOST_TOOL_STAMP"
    fi
}

verify_tools()
{
    ensure_tools
    verify_tool_versions
    verify_tool_hashes
}

verify_inputs()
{
    require_command awk
    require_command dd
    require_command find
    require_command jq
    verify_tools
    verify_boot_asset
}

compile_packer()
{
    require_command "$CC"
    "$CC" -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow \
        -Wstrict-prototypes -pedantic -Itools/common \
        tools/fixture/t64_rom_pack.c tools/common/t64_sha256.c -o "$PACKER"
}

linked_entry()
{
    "$OBJDUMP" -f "$1" | awk '$1 == "start" && $2 == "address" { print $3 }'
}

linked_symbol()
{
    symbol=$1
    elf=$2
    hex=$("$OBJDUMP" -t "$elf" | awk -v sym="$symbol" '$NF == sym { print $1 }')
    [ -n "$hex" ] || die "linked fixture is missing symbol $symbol"
    printf '0x%s\n' "$hex"
}

build_set()
{
    output_dir=$1
    mkdir -p "$output_dir"
    for spec in 0:hello 1:hello-empty-break 2:hello-wrong-text \
        3:hello-empty-loop 4:hello-incomplete-loop 5:hello-unsupported \
        6:hello-output-overflow; do
        variant=${spec%%:*}
        name=${spec#*:}
        object="$output_dir/$name.o"
        elf="$output_dir/$name.elf"
        flat="$output_dir/$name.payload.bin"
        rom="$output_dir/$name.z64"
        "$AS" -32 -mips3 -EB --defsym="VARIANT=$variant" \
            -o "$object" fixtures/hello/hello.S
        "$LD" -m elf32ebmip -EB -T fixtures/hello/hello.ld \
            -o "$elf" "$object"
        "$OBJCOPY" -O binary "$elf" "$flat"
        [ -s "$flat" ] || die "linked flat payload is empty for $name"
        entry=$(linked_entry "$elf")
        clock=$(linked_symbol __t64_rom_clock "$elf")
        crc2=$(linked_symbol __t64_rom_crc2 "$elf")
        [ "$entry" = 0x80000400 ] || die "unexpected linked entry for $name: $entry"
        [ "$clock" = 0x0000000f ] || die "unexpected linked clock for $name: $clock"
        [ "$crc2" = 0x00000000 ] || die "unexpected linked CRC2 policy for $name: $crc2"
        "$PACKER" --pack --boot "$BOOT" --payload "$flat" --output "$rom" \
            --entry "$entry" --clock "$clock" --crc2 "$crc2"
        packed_payload="$output_dir/$name.packed-payload.bin"
        embedded_ipl3="$output_dir/$name.embedded-ipl3.bin"
        dd if="$rom" of="$packed_payload" bs=1 skip=4096 \
            count="$(file_size "$flat")" 2>/dev/null
        dd if="$rom" of="$embedded_ipl3" bs=1 skip=64 count=4032 2>/dev/null
        cmp "$flat" "$packed_payload" >/dev/null || \
            die "packer changed the flat payload bytes for $name"
        cmp "$PAYLOAD" "$embedded_ipl3" >/dev/null || \
            die "packer changed the admitted IPL3 payload bytes for $name"
        rm -f "$packed_payload" "$embedded_ipl3"
    done
}

source_content_sha256()
{
    cat fixtures/hello/hello.S fixtures/hello/hello.ld \
        tools/fixture/t64_rom_pack.c scripts/build-boot-fixtures.sh | sha256_stream
}

write_build_metadata()
{
    output_dir=$1
    source_hash=$(source_content_sha256)
    compiler_version=$(tool_version "$CC")
    roms='[]'
    for name in hello hello-empty-break hello-wrong-text hello-empty-loop \
        hello-incomplete-loop hello-unsupported hello-output-overflow; do
        rom="$output_dir/$name.z64"
        rom_hash=$(sha256_file "$rom")
        rom_size=$(file_size "$rom")
        roms=$(printf '%s' "$roms" | jq --arg id "$name" \
            --arg sha "$rom_hash" --argjson size "$rom_size" \
            '. + [{id:$id,sha256:$sha,size_bytes:$size}]')
    done

    expected="$output_dir/hello.expected.txt"
    input="$output_dir/hello.input.json"
    printf '%s\n' 'Tendies64 hello via open IPL3' > "$expected"
    printf '%s\n' '{"schema":"t64.input-schedule","schema_version":1,"events":[]}' > "$input"
    jq -n --arg os "$HOST_OS" --arg arch "$HOST_ARCH" \
        --arg source_hash "$source_hash" \
        --arg compiler_version "$compiler_version" \
        --arg asset_sha "$EXPECTED_ASSET_SHA256" \
        --arg payload_sha "$EXPECTED_PAYLOAD_SHA256" \
        --arg asset_md5 "$EXPECTED_ASSET_MD5" \
        --arg transcript_sha "$(sha256_file "$expected")" \
        --arg transcript_hex "$(od -An -tx1 "$expected" | tr -d '[:space:]')" \
        --arg input_sha "$(sha256_file "$input")" \
        --arg as_version "$AS_VERSION" --arg as_sha "$AS_SHA256" \
        --arg ld_version "$LD_VERSION" --arg ld_sha "$LD_SHA256" \
        --arg objcopy_version "$OBJCOPY_VERSION" --arg objcopy_sha "$OBJCOPY_SHA256" \
        --arg objdump_version "$OBJDUMP_VERSION" --arg objdump_sha "$OBJDUMP_SHA256" \
        --argjson asset_bytes 4096 --argjson payload_bytes 4032 \
        --argjson roms "$roms" \
        '{schema:"t64.fixture-build-metadata",schema_version:1,
          host:{os:$os,architecture:$arch,c_compiler:$compiler_version},
          source_content_sha256:$source_hash,
          inputs:{ipl3_source_asset:{size_bytes:$asset_bytes,sha256:$asset_sha,md5:$asset_md5},
                  embedded_ipl3_payload:{source_offset:"0x40",size_bytes:$payload_bytes,sha256:$payload_sha}},
          header_transformations:[
            {offset:"0x00-0x3f",field:"base ROM header",value:"authored from zero; no upstream header bytes copied"},
            {offset:"0x00",field:"N64 z64 magic",value:"0x80371240"},
            {offset:"0x04",field:"NTSC clock",value:"0x0000000f"},
            {offset:"0x08",field:"flat payload linked entry",value:"0x80000400"},
            {offset:"0x0c",field:"release version",value:"0x00000000"},
            {offset:"0x10",field:"loader_compat checked flat payload byte length",value:"per-ROM actual nonzero size"},
            {offset:"0x14",field:"CRC2 linker field",value:"0x00000000; no CIC CRC claim"},
            {offset:"0x18-0x1f",field:"reserved header bytes",value:"zero"},
            {offset:"0x20",field:"authored cartridge title",value:"TENDIES64 HELLO"},
            {offset:"0x3a",field:"cartridge region",value:"E"}],
          expected_transcript:{source:"independent literal in scripts/build-boot-fixtures.sh",length_bytes:30,sha256:$transcript_sha,hex:$transcript_hex},
          empty_input_schedule:{sha256:$input_sha,format:"t64.input-schedule v1; zero events"},
          toolchain_source:{name:"GNU Binutils",version:"2.45",sha256:"c50c0e7f9cb188980e2cc97e4537626b1672441815587f1eab69d2a1bfbef5d2",configure_flags:["--target=mips64-elf","--disable-nls","--disable-werror","--disable-gdb","--disable-gprof","--disable-gold","--with-system-zlib"]},
          host_tools:{as:{version:$as_version,sha256:$as_sha},ld:{version:$ld_version,sha256:$ld_sha},objcopy:{version:$objcopy_version,sha256:$objcopy_sha},objdump:{version:$objdump_version,sha256:$objdump_sha}},
          roms:$roms,
          metadata_scope:"Host tool hashes identify this host build and are excluded from guest/profile identity."}' \
        > "$output_dir/hello.build.json"
}

install_file()
{
    source=$1
    destination=$2
    if [ -e "$destination" ] || [ -L "$destination" ]; then
        [ -f "$destination" ] && [ ! -L "$destination" ] || \
            die "refusing to replace a non-regular generated output: $destination"
    fi
    mv -f "$source" "$destination"
}

assert_manifest_entry()
{
    entry_id=$1
    artifact=$2
    sha=$(sha256_file "$artifact")
    size=$(file_size "$artifact")
    jq -e --arg id "$entry_id" --arg sha "$sha" --argjson size "$size" \
        '.entries | any(.id == $id and .sha256 == $sha and .size == $size and
          .source_rights == "authored-here" and .binary_rights == "cleared" and
          .distribution == ["build-only"])' fixtures/manifest.json >/dev/null || \
        die "fixture manifest identity or rights mismatch for $entry_id"
}

verify_manifest()
{
    asset_sha=$(sha256_file "$BOOT")
    payload_sha=$(sha256_file "$PAYLOAD")
    hello_sha=$(sha256_file build/fixtures/hello.z64)
    hello_size=$(file_size build/fixtures/hello.z64)
    source_hash=$(source_content_sha256)
    source_revision=$(jq -er '.entries[] | select(.id == "hello-openipl3") | .source.revision' fixtures/manifest.json) || \
        die 'hello source revision is missing from fixture manifest'
    case "$source_revision" in *[!0-9a-f]*|'') die 'hello source revision is not a lowercase 40-hex commit' ;; esac
    [ "${#source_revision}" -eq 40 ] || die 'hello source revision is not a 40-hex commit'
    for file in fixtures/hello/hello.S fixtures/hello/hello.ld \
        tools/fixture/t64_rom_pack.c scripts/build-boot-fixtures.sh; do
        git cat-file -e "$source_revision:$file" 2>/dev/null || \
            die "manifest source revision does not contain $file"
        committed_sha=$(git show "$source_revision:$file" | sha256_stream)
        current_sha=$(sha256_file "$file")
        [ "$committed_sha" = "$current_sha" ] || \
            die "manifest source revision content differs for $file"
    done

    jq -e --arg asset "$asset_sha" --arg payload "$payload_sha" \
        --arg hello "$hello_sha" --arg source_hash "$source_hash" \
        --arg revision "$source_revision" --argjson size "$hello_size" \
        '([.entries[] | select(.id == "openipl3-compat-asset")][0] |
            .sha256 == $asset and .size == 4096 and .source.revision == "98154dafd5ac50e74ee09a5de1035d24a53f54f0") and
         ([.entries[] | select(.id == "openipl3-compat-payload")][0] |
            .sha256 == $payload and .size == 4032 and .source.revision == "98154dafd5ac50e74ee09a5de1035d24a53f54f0") and
         ([.entries[] | select(.id == "hello-openipl3")][0] |
            .sha256 == $hello and .size == $size and .source.revision == $revision and
            (.build.recipe | contains($source_hash)) and
            .expected.revision == ("source-content-sha256:" + $source_hash))' \
        fixtures/manifest.json >/dev/null || \
        die 'fixture manifest does not bind the source, payload, generated ROM and source-content pins'

    assert_manifest_entry hello-openipl3 build/fixtures/hello.z64
    for spec in hello-empty-break:hello-empty-break hello-wrong-text:hello-wrong-text \
        hello-empty-loop:hello-empty-loop hello-incomplete-loop:hello-incomplete-loop \
        hello-unsupported:hello-unsupported hello-output-overflow:hello-output-overflow; do
        id=${spec%%:*}
        name=${spec#*:}
        assert_manifest_entry "$id" "build/fixtures/$name.z64"
    done
    jq -e --arg payload "$payload_sha" \
        '[.entries[] | select(.id == "openipl3-compat-payload")][0] |
         .sha256 == $payload and .size == 4032 and .binary_rights == "cleared" and
         .distribution == ["build-only"]' fixtures/manifest.json >/dev/null || \
        die 'fixture manifest does not bind the extracted open IPL3 payload'

    if [ -x build/gate/tools/manifest/t64-manifest-check ]; then
        build/gate/tools/manifest/t64-manifest-check --schema fixtures/manifest.schema.json \
            --root "$ROOT" fixtures/manifest.json >/dev/null || \
            die 't64-manifest-check rejected fixtures/manifest.json'
    fi
}

expect_rejection()
{
    label=$1
    shift
    if "$@" >"$CONTROL_LOG" 2>&1; then
        die "negative control was accepted: $label"
    fi
    printf 'PASS: %s\n' "$label"
}

run_selftest()
{
    require_command mktemp
    require_command cmp
    require_command od
    tmp=$(mktemp -d build/fixtures/t64-rom-selftest.XXXXXX)
    CONTROL_LOG="$tmp/control.log"
    cleanup()
    {
        rm -rf "$tmp"
    }
    trap cleanup EXIT HUP INT TERM

    "$PACKER" --selftest
    dd if="$BOOT" of="$tmp/truncated-boot.bin" bs=1 count=100 2>/dev/null
    cp "$BOOT" "$tmp/wrong-boot.bin"
    printf '\001' | dd of="$tmp/wrong-boot.bin" bs=1 seek=100 conv=notrunc 2>/dev/null
    dd if=/dev/zero of="$tmp/oversized-payload.bin" bs=1048576 count=1 2>/dev/null
    build_set "$tmp/first"

    expect_rejection 'truncated boot asset' "$PACKER" --pack \
        --boot "$tmp/truncated-boot.bin" --payload "$tmp/first/hello.payload.bin" \
        --output "$tmp/truncated.z64" --entry 0x80000400 --clock 0x0000000f --crc2 0x00000000
    expect_rejection 'wrong boot asset hash' "$PACKER" --pack \
        --boot "$tmp/wrong-boot.bin" --payload "$tmp/first/hello.payload.bin" \
        --output "$tmp/wrong-boot.z64" --entry 0x80000400 --clock 0x0000000f --crc2 0x00000000
    : > "$tmp/empty.bin"
    expect_rejection 'zero-length flat payload' "$PACKER" --pack \
        --boot "$BOOT" --payload "$tmp/empty.bin" --output "$tmp/empty.z64" \
        --entry 0x80000400 --clock 0x0000000f --crc2 0x00000000
    expect_rejection 'payload beyond one MiB cartridge ceiling' "$PACKER" --pack \
        --boot "$BOOT" --payload "$tmp/oversized-payload.bin" --output "$tmp/oversized.z64" \
        --entry 0x80000400 --clock 0x0000000f --crc2 0x00000000
    expect_rejection 'unaligned flat payload entry' "$PACKER" --pack \
        --boot "$BOOT" --payload "$tmp/first/hello.payload.bin" --output "$tmp/unaligned.z64" \
        --entry 0x80000401 --clock 0x0000000f --crc2 0x00000000
    expect_rejection 'incorrect cartridge clock' "$PACKER" --pack \
        --boot "$BOOT" --payload "$tmp/first/hello.payload.bin" --output "$tmp/clock.z64" \
        --entry 0x80000400 --clock 0x00000010 --crc2 0x00000000
    expect_rejection 'incorrect CRC2 field' "$PACKER" --pack \
        --boot "$BOOT" --payload "$tmp/first/hello.payload.bin" --output "$tmp/crc2.z64" \
        --entry 0x80000400 --clock 0x0000000f --crc2 0x00000001
    if T64_MIPS_AS=/usr/bin/true sh scripts/build-boot-fixtures.sh --qualify \
        >"$CONTROL_LOG" 2>&1; then
        die 'wrong assembler tool was accepted'
    fi
    printf 'PASS: wrong assembler tool\n'

    build_set "$tmp/second"
    for name in hello hello-empty-break hello-wrong-text hello-empty-loop \
        hello-incomplete-loop hello-unsupported hello-output-overflow; do
        cmp "$tmp/first/$name.z64" "$tmp/second/$name.z64" >/dev/null || \
            die "two clean builds produced different bytes for $name"
        [ "$(sha256_file "$tmp/first/$name.z64")" = \
          "$(sha256_file "$tmp/second/$name.z64")" ] || \
            die "two clean builds produced different hashes for $name"
    done
    printf 'PASS: two clean builds produced identical bytes and hashes for all seven variants\n'

    final="$tmp/final"
    mkdir -p "$final"
    dd if="$BOOT" of="$final/ipl3_compat.payload.bin" bs=1 skip=64 count=4032 2>/dev/null
    [ "$(sha256_file "$final/ipl3_compat.payload.bin")" = "$EXPECTED_PAYLOAD_SHA256" ] || \
        die 'final selftest payload extraction identity mismatch'
    PAYLOAD="$final/ipl3_compat.payload.bin"
    build_set "$final"
    write_build_metadata "$final"
    install_generated_outputs "$final"
    PAYLOAD=build/fixtures/ipl3_compat.payload.bin
    verify_manifest
    printf 'PASS: source, tool, boot, payload and manifest identities\n'
}

install_generated_outputs()
{
    staging=$1
    mkdir -p build/fixtures
    for name in hello hello-empty-break hello-wrong-text hello-empty-loop \
        hello-incomplete-loop hello-unsupported hello-output-overflow; do
        install_file "$staging/$name.z64" "build/fixtures/$name.z64"
    done
    install_file "$staging/hello.payload.bin" build/fixtures/hello.payload.bin
    install_file "$staging/ipl3_compat.payload.bin" build/fixtures/ipl3_compat.payload.bin
    install_file "$staging/hello.expected.txt" build/fixtures/hello.expected.txt
    install_file "$staging/hello.input.json" build/fixtures/hello.input.json
    install_file "$staging/hello.build.json" build/fixtures/hello.build.json
}

run_build()
{
    staging=$(mktemp -d build/fixtures/t64-rom-build.XXXXXX)
    cleanup()
    {
        rm -rf "$staging"
    }
    trap cleanup EXIT HUP INT TERM
    dd if="$BOOT" of="$staging/ipl3_compat.payload.bin" bs=1 skip=64 count=4032 2>/dev/null
    [ "$(sha256_file "$staging/ipl3_compat.payload.bin")" = "$EXPECTED_PAYLOAD_SHA256" ] || \
        die 'build payload extraction identity mismatch'
    PAYLOAD="$staging/ipl3_compat.payload.bin"
    build_set "$staging"
    write_build_metadata "$staging"
    install_generated_outputs "$staging"
    PAYLOAD=build/fixtures/ipl3_compat.payload.bin
    trap - EXIT HUP INT TERM
    rm -rf "$staging"
}

run_qualify()
{
    compile_packer
    "$PACKER" --selftest >/dev/null || die 'packer selftest failed during qualification'
    run_build
    verify_manifest
    printf '%s\n' 'qualified authored fixture source, tools, boot identities and manifest'
}

verify_inputs

case "${1:-}" in
    --qualify)
        run_qualify
        ;;
    --build)
        compile_packer
        run_build
        ;;
    --selftest)
        compile_packer
        run_selftest
        ;;
    *)
        printf '%s\n' 'usage: sh scripts/build-boot-fixtures.sh --qualify|--build|--selftest' >&2
        exit 2
        ;;
esac

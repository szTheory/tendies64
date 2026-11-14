#!/bin/sh
set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

SOURCE_REVISION=3b3e10c786f46002bc6a65b5193cf01633303828
BOOT=build/fixtures/ipl3_compat.z64
PAYLOAD=build/fixtures/ipl3_compat.payload.bin
TOOL_DIR=build/tools/install/bin
STAMP=build/tools/qualified-host-tools.json
OUT_DIR=build/fixtures/rsp-smoke
PACKER=build/fixtures/t64_rom_pack
AS=${T64_MIPS_AS:-$TOOL_DIR/mips64-elf-as}
LD=${T64_MIPS_LD:-$TOOL_DIR/mips64-elf-ld}
OBJCOPY=${T64_MIPS_OBJCOPY:-$TOOL_DIR/mips64-elf-objcopy}
OBJDUMP=${T64_MIPS_OBJDUMP:-$TOOL_DIR/mips64-elf-objdump}
ASSET_SHA256=f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068
ASSET_MD5=71ce6a00936394ad364fbe584234fd82
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

verify_inputs() {
  [ -f "$BOOT" ] && [ ! -L "$BOOT" ] || die 'pinned openIPL3 asset is absent or not a regular file'
  [ -f "$PAYLOAD" ] && [ ! -L "$PAYLOAD" ] || die 'pinned openIPL3 payload is absent or not a regular file'
  [ "$(file_size "$BOOT")" = 4096 ] || die 'openIPL3 asset size differs from the admitted contract'
  [ "$(sha256_file "$BOOT")" = "$ASSET_SHA256" ] || die 'openIPL3 asset SHA-256 differs from the admitted contract'
  [ "$(sha256_file "$PAYLOAD")" = "$PAYLOAD_SHA256" ] || die 'openIPL3 payload SHA-256 differs from the admitted contract'
  [ "$(tool_version "$AS")" = 'GNU assembler (GNU Binutils) 2.45' ] || die 'assembler version differs from the admitted toolchain'
  [ "$(tool_version "$LD")" = 'GNU ld (GNU Binutils) 2.45' ] || die 'linker version differs from the admitted toolchain'
  [ "$(tool_version "$OBJCOPY")" = 'GNU objcopy (GNU Binutils) 2.45' ] || die 'objcopy version differs from the admitted toolchain'
  [ "$(tool_version "$OBJDUMP")" = 'GNU objdump (GNU Binutils) 2.45' ] || die 'objdump version differs from the admitted toolchain'
  [ -f "$STAMP" ] || die 'qualified host tool hash receipt is absent'
  jq -e --arg as "$(sha256_file "$AS")" --arg ld "$(sha256_file "$LD")" \
      --arg oc "$(sha256_file "$OBJCOPY")" --arg od "$(sha256_file "$OBJDUMP")" \
      --arg binutils "$BINUTILS_SHA256" \
      '.source.sha256 == $binutils and .tools.as.sha256 == $as and
       .tools.ld.sha256 == $ld and .tools.objcopy.sha256 == $oc and
       .tools.objdump.sha256 == $od' "$STAMP" >/dev/null ||
    die 'qualified host tool hashes differ from the saved receipt'
  git cat-file -e "$SOURCE_REVISION:fixtures/rsp/rsp-smoke.S" 2>/dev/null ||
    die 'pinned authored source revision does not contain rsp-smoke.S'
  git cat-file -e "$SOURCE_REVISION:fixtures/rsp/rsp-smoke.ld" 2>/dev/null ||
    die 'pinned authored source revision does not contain rsp-smoke.ld'
  [ "$(git show "$SOURCE_REVISION:fixtures/rsp/rsp-smoke.S" | sha256_stream)" = "$(sha256_file fixtures/rsp/rsp-smoke.S)" ] ||
    die 'working assembly differs from its committed source revision'
  [ "$(git show "$SOURCE_REVISION:fixtures/rsp/rsp-smoke.ld" | sha256_stream)" = "$(sha256_file fixtures/rsp/rsp-smoke.ld)" ] ||
    die 'working linker script differs from its committed source revision'
  "$AS" --version 2>&1 | grep -F 'mips64-elf' >/dev/null || die 'assembler target is not mips64-elf'
  "$LD" -V 2>&1 | grep -F 'elf32ebmip' >/dev/null || die 'linker lacks elf32ebmip'
  "$OBJCOPY" --info 2>&1 | grep -F 'elf32-bigmips' >/dev/null || die 'objcopy lacks big-endian MIPS output'
  "$OBJDUMP" --info 2>&1 | grep -F 'elf32-bigmips' >/dev/null || die 'objdump lacks big-endian MIPS support'
}

verify_composition() {
  composed_rom=$1
  composed_flat=$2
  embedded="$OUT_DIR/composition.ipl3.bin"
  payload="$OUT_DIR/composition.app.bin"
  dd if="$composed_rom" of="$embedded" bs=1 skip=64 count=4032 2>/dev/null
  dd if="$composed_rom" of="$payload" bs=1 skip=4096 count="$(file_size "$composed_flat")" 2>/dev/null
  cmp "$PAYLOAD" "$embedded" >/dev/null || die 'embedded openIPL3 payload differs from the admitted bytes'
  cmp "$composed_flat" "$payload" >/dev/null || die 'ROM application bytes differ from the linked authored payload'
  rm -f "$embedded" "$payload"
}

verify_vector_memory_encodings() {
  python3 - "$1" "$2" <<'PY'
import pathlib
import sys

payload = pathlib.Path(sys.argv[1]).read_bytes()
program_vma = int(sys.argv[2], 16)
program_offset = program_vma - 0x80000400
vector_anchors = (
    (0x50, 0xc8c42000, 0x32, 6, 4, 0),
    (0x5c, 0xe8c42000, 0x3a, 6, 4, 0),
    (0x64, 0xe8c42000, 0x3a, 6, 4, 0),
    (0x70, 0xe805200a, 0x3a, 0, 5, 0x0a),
    (0x78, 0xe806200b, 0x3a, 0, 6, 0x0b),
    (0x80, 0xe807200c, 0x3a, 0, 7, 0x0c),
    (0x90, 0xe808200d, 0x3a, 0, 8, 0x0d),
    (0x98, 0xe808200e, 0x3a, 0, 8, 0x0e),
)
for relative, expected, operation, base, vector, raw_offset in vector_anchors:
    at = program_offset + relative
    word = int.from_bytes(payload[at:at + 4], "big")
    fields = ((word >> 26) & 0x3f, (word >> 21) & 0x1f,
              (word >> 16) & 0x1f, (word >> 11) & 0x1f,
              (word >> 7) & 0x0f, word & 0x7f)
    if word != expected or fields != (operation, base, vector, 4, 0, raw_offset):
        raise SystemExit(
            f"vector-memory encoding at guest+0x{relative:x} "
            f"was 0x{word:08x}, fields={fields}; expected literal 0x{expected:08x}")
scalar_anchors = ((0x24, 0x48810900, 1), (0x2c, 0x48821100, 2))
for relative, expected, vector_register in scalar_anchors:
    at = program_offset + relative
    word = int.from_bytes(payload[at:at + 4], "big")
    fields = ((word >> 26) & 0x3f, (word >> 21) & 0x1f,
              (word >> 16) & 0x1f, (word >> 11) & 0x1f,
              (word >> 7) & 0x0f, word & 0x7f)
    expected_fields = (0x12, 4, vector_register, vector_register, 2, 0)
    if word != expected or fields != expected_fields:
        raise SystemExit(
            f"MTC2 lane-1 encoding at guest+0x{relative:x} was "
            f"0x{word:08x}, fields={fields}; expected 0x{expected:08x} "
            f"for rs=MTC2, v{vector_register}[1], element=2")
PY
}

install_identity_matches() {
  install_path=$1
  install_expected_sha=$2
  install_expected_size=$3
  [ -f "$install_path" ] && [ ! -L "$install_path" ] || return 1
  install_actual_sha=$(sha256_file "$install_path")
  install_actual_size=$(file_size "$install_path")
  [ "$install_actual_sha" = "$install_expected_sha" ] &&
    [ "$install_actual_size" = "$install_expected_size" ]
}

require_install_identity() {
  install_identity_matches "$1" "$2" "$3" ||
    die 'installed ROM differs from the independently checked candidate identity'
}

install_guard_selftest() {
  install_guard_dir="$OUT_DIR/install-guard.$$"
  [ ! -e "$install_guard_dir" ] || die 'install-guard selftest path already exists'
  mkdir "$install_guard_dir"
  trap 'rm -f "$install_guard_dir/candidate" "$install_guard_dir/installed"; rmdir "$install_guard_dir" 2>/dev/null || true' EXIT HUP INT TERM
  printf '%s' 'authored-candidate-bytes' > "$install_guard_dir/candidate"
  cp "$install_guard_dir/candidate" "$install_guard_dir/installed"
  install_guard_sha=$(sha256_file "$install_guard_dir/candidate")
  install_guard_size=$(file_size "$install_guard_dir/candidate")
  install_identity_matches "$install_guard_dir/installed" \
    "$install_guard_sha" "$install_guard_size" ||
    die 'install-guard positive control did not match'
  printf '%s' 'X' | dd of="$install_guard_dir/installed" bs=1 seek=0 conv=notrunc 2>/dev/null
  if install_identity_matches "$install_guard_dir/installed" \
      "$install_guard_sha" "$install_guard_size"; then
    die 'install-guard mutation was accepted'
  fi
  rm -f "$install_guard_dir/candidate" "$install_guard_dir/installed"
  rmdir "$install_guard_dir"
  trap - EXIT HUP INT TERM
}

build_variant() {
  build_variant_number=$1
  build_variant_name=$2
  build_variant_object="$OUT_DIR/$build_variant_name.o"
  build_variant_elf="$OUT_DIR/$build_variant_name.elf"
  build_variant_flat="$OUT_DIR/$build_variant_name.payload.bin"
  build_variant_rom="$OUT_DIR/$build_variant_name.z64"
  build_variant_tmp_rom="$build_variant_rom.tmp.$$"
  "$AS" -32 -mips3 -EB --defsym="VARIANT=$build_variant_number" -o "$build_variant_object" fixtures/rsp/rsp-smoke.S
  "$LD" -m elf32ebmip -EB -T fixtures/rsp/rsp-smoke.ld -o "$build_variant_elf" "$build_variant_object"
  "$OBJCOPY" -O binary "$build_variant_elf" "$build_variant_flat"
  [ -s "$build_variant_flat" ] || die "linked payload is empty for $build_variant_name"
  build_variant_entry=$("$OBJDUMP" -f "$build_variant_elf" | awk '$1 == "start" && $2 == "address" { print $3 }')
  build_variant_program_vma=$("$OBJDUMP" -t "$build_variant_elf" | awk '$NF == "rsp_program" { print $1 }')
  build_variant_clock=$("$OBJDUMP" -t "$build_variant_elf" | awk '$NF == "__t64_rom_clock" { print $1 }')
  build_variant_crc2=$("$OBJDUMP" -t "$build_variant_elf" | awk '$NF == "__t64_rom_crc2" { print $1 }')
  [ "$build_variant_entry" = 0x80000400 ] || die "unexpected entry point for $build_variant_name"
  [ -n "$build_variant_program_vma" ] || die "RSP program symbol is absent for $build_variant_name"
  [ "$build_variant_clock" = 0000000f ] || die "unexpected ROM clock for $build_variant_name"
  [ "$build_variant_crc2" = 00000000 ] || die "unexpected CRC2 policy for $build_variant_name"
  verify_vector_memory_encodings "$build_variant_flat" "$build_variant_program_vma"
  [ ! -e "$build_variant_tmp_rom" ] || die "temporary ROM path already exists for $build_variant_name"
  "$PACKER" --pack --boot "$BOOT" --payload "$build_variant_flat" --output "$build_variant_tmp_rom" \
      --entry 0x80000400 --clock 0x0000000f --crc2 0x00000000
  verify_composition "$build_variant_tmp_rom" "$build_variant_flat"
  build_variant_candidate_sha=$(sha256_file "$build_variant_tmp_rom")
  build_variant_candidate_size=$(file_size "$build_variant_tmp_rom")
  mv "$build_variant_tmp_rom" "$build_variant_rom"
  require_install_identity "$build_variant_rom" "$build_variant_candidate_sha" "$build_variant_candidate_size"
  printf '%s %s %s %s\n' "$build_variant_name" "$build_variant_number" \
    "$build_variant_candidate_sha" "$build_variant_candidate_size"
}

variant_classification() {
  case $1 in
    0) printf '%s' pass ;;
    1|5) printf '%s' guest-fail ;;
    2) printf '%s' refused ;;
    3) printf '%s' missing-break ;;
    4) printf '%s' completion-delay-mismatch ;;
    *) die "unknown authored variant $1" ;;
  esac
}

variant_purpose() {
  case $1 in
    0) printf '%s' authenticated-baseline ;;
    1) printf '%s' wrong-result ;;
    2) printf '%s' unsupported-opcode ;;
    3) printf '%s' missing-break ;;
    4) printf '%s' altered-terminal-delay ;;
    5) printf '%s' same-bank-wrap ;;
    *) die "unknown authored variant $1" ;;
  esac
}

variant_manifest_id() {
  case $1 in
    1) printf '%s' t64-authored-rsp-smoke-wrong-result ;;
    2) printf '%s' t64-authored-rsp-smoke-unsupported-opcode ;;
    3) printf '%s' t64-authored-rsp-smoke-missing-break ;;
    4) printf '%s' t64-authored-rsp-smoke-altered-delay ;;
    5) printf '%s' t64-authored-rsp-smoke-same-bank-wrap ;;
    *) die "unknown authored variant manifest id $1" ;;
  esac
}

write_build_receipt() {
  receipt_variant=$1
  receipt_name=$2
  receipt_class=$(variant_classification "$receipt_variant")
  receipt_purpose=$(variant_purpose "$receipt_variant")
  receipt_rom="$OUT_DIR/$receipt_name.z64"
  receipt_path="$OUT_DIR/$receipt_name.build.json"
  receipt_source_sha=$(sha256_file fixtures/rsp/rsp-smoke.S)
  receipt_linker_sha=$(sha256_file fixtures/rsp/rsp-smoke.ld)
  receipt_recipe_sha=$(sha256_file scripts/build-rsp-smoke.sh)
  receipt_rom_sha=$(sha256_file "$receipt_rom")
  receipt_rom_size=$(file_size "$receipt_rom")
  jq -n --arg revision "$SOURCE_REVISION" --arg source "$receipt_source_sha" \
      --arg linker "$receipt_linker_sha" --arg recipe "$receipt_recipe_sha" \
      --arg boot "$ASSET_SHA256" --arg payload "$PAYLOAD_SHA256" \
      --arg rom "$receipt_rom_sha" --argjson size "$receipt_rom_size" \
      --arg as "$(sha256_file "$AS")" --arg ld "$(sha256_file "$LD")" \
      --arg objcopy "$(sha256_file "$OBJCOPY")" \
      --arg objdump "$(sha256_file "$OBJDUMP")" \
      --argjson variant "$receipt_variant" --arg classification "$receipt_class" \
      --arg purpose "$receipt_purpose" \
      '{schema:"t64.rsp-smoke-build",schema_version:1,
        source:{repository:"https://github.com/szTheory/tendies64",revision:$revision,
          assembly_sha256:$source,linker_sha256:$linker},
        recipe:{path:"scripts/build-rsp-smoke.sh",sha256:$recipe},
        toolchain:{name:"GNU Binutils",version:"2.45",assembler_sha256:$as,
          linker_sha256:$ld,objcopy_sha256:$objcopy,objdump_sha256:$objdump},
        boot:{asset_sha256:$boot,payload_sha256:$payload},
        fixture:{variant:$variant,sha256:$rom,size_bytes:$size,
          profile:"ntsc-cart-4m-openipl3-rsp-functional-v1",model:6,state:12,
          expected:{classification:$classification,purpose:$purpose}}}' \
      > "$receipt_path"
}

verify_build_receipt() {
  verify_name=$1
  verify_variant=$2
  verify_path="$OUT_DIR/$verify_name.build.json"
  verify_rom="$OUT_DIR/$verify_name.z64"
  verify_class=$(variant_classification "$verify_variant")
  verify_purpose=$(variant_purpose "$verify_variant")
  [ -f "$verify_rom" ] || die "authored ROM is absent: $verify_name"
  [ -f "$verify_path" ] || die "authored build receipt is absent: $verify_name"
  verify_composition "$verify_rom" "$OUT_DIR/$verify_name.payload.bin"
  jq -e --arg revision "$SOURCE_REVISION" \
      --arg source "$(sha256_file fixtures/rsp/rsp-smoke.S)" \
      --arg linker "$(sha256_file fixtures/rsp/rsp-smoke.ld)" \
      --arg recipe "$(sha256_file scripts/build-rsp-smoke.sh)" \
      --arg boot "$ASSET_SHA256" --arg payload "$PAYLOAD_SHA256" \
      --arg as "$(sha256_file "$AS")" --arg ld "$(sha256_file "$LD")" \
      --arg objcopy "$(sha256_file "$OBJCOPY")" \
      --arg objdump "$(sha256_file "$OBJDUMP")" \
      --argjson variant "$verify_variant" --arg classification "$verify_class" \
      --arg purpose "$verify_purpose" \
      '.schema == "t64.rsp-smoke-build" and .schema_version == 1 and
       .source.revision == $revision and
       .source.assembly_sha256 == $source and .source.linker_sha256 == $linker and
       .recipe.path == "scripts/build-rsp-smoke.sh" and .recipe.sha256 == $recipe and
       .boot.asset_sha256 == $boot and .boot.payload_sha256 == $payload and
       .toolchain.assembler_sha256 == $as and .toolchain.linker_sha256 == $ld and
       .toolchain.objcopy_sha256 == $objcopy and .toolchain.objdump_sha256 == $objdump and
       .fixture.variant == $variant and
       .fixture.expected.classification == $classification and
       .fixture.expected.purpose == $purpose and
       .fixture.profile == "ntsc-cart-4m-openipl3-rsp-functional-v1" and
       .fixture.model == 6 and .fixture.state == 12' \
      "$verify_path" >/dev/null || die "authored smoke receipt differs from this source/profile: $verify_name"
  verify_receipt_sha=$(jq -er '.fixture.sha256' "$verify_path")
  verify_receipt_size=$(jq -er '.fixture.size_bytes' "$verify_path")
  require_install_identity "$verify_rom" "$verify_receipt_sha" "$verify_receipt_size"
}

verify_manifest() {
  manifest_sha=$(jq -er '.fixture.sha256' "$OUT_DIR/rsp-smoke.build.json")
  manifest_size=$(jq -er '.fixture.size_bytes' "$OUT_DIR/rsp-smoke.build.json")
  jq -e --arg revision "$SOURCE_REVISION" --arg sha "$manifest_sha" --argjson size "$manifest_size" \
    '[.entries[] | select(.id == "t64-authored-rsp-smoke")] as $rows |
     ($rows | length) == 1 and
     $rows[0].source.revision == $revision and
     $rows[0].sha256 == $sha and $rows[0].size == $size and
     $rows[0].profile == "ntsc-cart-4m-openipl3-rsp-functional-v1" and
     $rows[0].source_rights == "authored-here" and
     $rows[0].binary_rights == "cleared" and
     ($rows[0].distribution | index("local-only")) != null and
     ($rows[0].rights_evidence | contains("Unlicense"))' \
    fixtures/manifest.json >/dev/null || die 'authored ROM manifest differs from the committed source/build/rights contract'

  for manifest_variant in 1 2 3 4 5; do
    manifest_name="rsp-smoke-variant$manifest_variant"
    manifest_id=$(variant_manifest_id "$manifest_variant")
    manifest_variant_sha=$(jq -er '.fixture.sha256' "$OUT_DIR/$manifest_name.build.json")
    manifest_variant_size=$(jq -er '.fixture.size_bytes' "$OUT_DIR/$manifest_name.build.json")
    manifest_class=$(variant_classification "$manifest_variant")
    jq -e --arg id "$manifest_id" --arg revision "$SOURCE_REVISION" \
        --arg sha "$manifest_variant_sha" --argjson size "$manifest_variant_size" \
        --arg classification "$manifest_class" \
        '[.entries[] | select(.id == $id)] as $rows |
         ($rows | length) == 1 and
         $rows[0].source.revision == $revision and
         $rows[0].sha256 == $sha and $rows[0].size == $size and
         $rows[0].profile == "ntsc-cart-4m-openipl3-rsp-functional-v1" and
         $rows[0].source_rights == "authored-here" and
         $rows[0].binary_rights == "cleared" and
         ($rows[0].distribution | index("local-only")) != null and
         ($rows[0].rights_evidence | contains("Unlicense")) and
         $rows[0].expected.kind == "self-checking" and
         ($rows[0].expected.source | contains($classification))' \
        fixtures/manifest.json >/dev/null ||
      die "authored smoke variant manifest differs from its checked identity: $manifest_variant"
  done
}

verify_inputs

case "${1:-}" in
  --build)
    mkdir -p "$OUT_DIR"
    # The legacy fixture qualification checks the shared packer and boot
    # contract; verify_inputs above forbids fetching or substituting either.
    sh scripts/build-boot-fixtures.sh --selftest >/dev/null
    "$PACKER" --selftest >/dev/null || die 'ROM packer selftest failed'
    build_variant 0 rsp-smoke
    write_build_receipt 0 rsp-smoke
    for build_variant_number in 1 2 3 4 5; do
      build_variant "$build_variant_number" "rsp-smoke-variant$build_variant_number"
      write_build_receipt "$build_variant_number" "rsp-smoke-variant$build_variant_number"
    done
    verify_manifest
    ;;
  --selftest)
    [ -f "$PACKER" ] || die 'qualified ROM packer is absent'
    "$PACKER" --selftest >/dev/null || die 'ROM packer selftest failed'
    install_guard_selftest
    for selftest_variant in 0 1 2 3 4 5; do
      if [ "$selftest_variant" -eq 0 ]; then
        selftest_name=rsp-smoke
      else
        selftest_name="rsp-smoke-variant$selftest_variant"
      fi
      verify_build_receipt "$selftest_name" "$selftest_variant"
    done
    verify_manifest
    printf '%s\n' 'PASS: six source, toolchain, boot, composition and install identities match authored RSP receipts'
    ;;
  *)
    printf '%s\n' 'usage: sh scripts/build-rsp-smoke.sh --build|--selftest' >&2
    exit 2
    ;;
esac

#!/bin/sh
set -eu
umask 077

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
cd "$ROOT"

PRIVATE_ROOT=build/private-cpu-diagnostic
PINNED_SOURCE=$PRIVATE_ROOT/nemu64-test
OVERLAY_SOURCE=$PRIVATE_ROOT/overlay-checkout-8aa0921
SOURCE_PROJECTION=$PRIVATE_ROOT/source-projection.json
SOURCE_AUDIT=$PRIVATE_ROOT/source-audit-full.json
SOURCE_CLOSURE=$PRIVATE_ROOT/source-closure-8aa0921-park-fix.json
SOURCE_REVISION=9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c
PACKER=build/private-cpu-diagnostic/t64_rom_pack
QUALIFIED_CC=
QUALIFIED_CC_SHA=
BOOT=build/fixtures/ipl3_compat.z64
AS=
AS_SHA=
LD=
LD_SHA=
OBJCOPY=
OBJCOPY_SHA=
OBJDUMP=
OBJDUMP_SHA=
EXPECTED_ASSET_SHA256=f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068
TOOLCHAIN_STAGE=$PRIVATE_ROOT/toolchain-install-inputs
TOOLCHAIN_PREFIX=$PRIVATE_ROOT/toolchain-prefix
TOOLCHAIN_HOME=$PRIVATE_ROOT/toolchain-home
TOOLCHAIN_TMP=$PRIVATE_ROOT/toolchain-tmp
CARGO_HOME=$PRIVATE_ROOT/cargo-home-attempt-03
CARGO_TARGET_DIR=$PRIVATE_ROOT/cargo-target-attempt-03
UNIT_GRAPH=$PRIVATE_ROOT/unit-graph-attempt-03.json
UNIT_GRAPH_LOG=$PRIVATE_ROOT/unit-graph-attempt-03.stderr.log
UNIT_GRAPH_REVIEW=$PRIVATE_ROOT/unit-graph-review-8aa0921.json
RUSTC_ARCHIVE=$PRIVATE_ROOT/root-host-tool-audit/rustc-nightly-aarch64-apple-darwin.tar.xz
CARGO_ARCHIVE=$PRIVATE_ROOT/root-host-tool-audit/cargo-nightly-aarch64-apple-darwin.tar.xz
HOST_STD_ARCHIVE=$PRIVATE_ROOT/root-host-tool-audit/rust-std-nightly-aarch64-apple-darwin.tar.xz
RUST_SRC_ARCHIVE=$PRIVATE_ROOT/root-rust-src-audit/rust-src-nightly.tar.xz

die()
{
    printf 'error: %s\n' "$*" >&2
    exit 1
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 || die "required host command is missing: $1"
}

verify_private_directory()
{
    candidate=$1
    case "$candidate" in /*) ;; *) candidate=$ROOT/$candidate ;; esac
    case "$candidate" in
        "$ROOT"/*) relative=${candidate#"$ROOT"/} ;;
        *) die "private directory escapes the canonical repository: $candidate" ;;
    esac
    cursor=$ROOT
    rest=$relative
    while [ -n "$rest" ]; do
        component=${rest%%/*}
        if [ "$component" = "$rest" ]; then rest=; else rest=${rest#*/}; fi
        cursor=$cursor/$component
        [ ! -L "$cursor" ] || die "private directory ancestor is linked: $cursor"
        [ -d "$cursor" ] || die "required private directory is missing: $cursor"
    done
    [ "$cursor" = "$candidate" ] || die "private directory path is not canonical: $candidate"
    if [ "$candidate" = "$ROOT/$PRIVATE_ROOT" ]; then
        python3 -B -c 'import pathlib,stat,sys; p=pathlib.Path(sys.argv[1]); raise SystemExit(0 if stat.S_IMODE(p.stat().st_mode) & 0o077 == 0 else 1)' "$candidate" || \
            die "private directory is accessible to group or other users: $candidate"
    fi
}

prepare_source()
{
    verify_private_directory "$PRIVATE_ROOT"
    verify_private_directory "$PINNED_SOURCE"
    [ -f "$SOURCE_PROJECTION" ] && [ ! -L "$SOURCE_PROJECTION" ] || \
        die 'the sealed private source projection is missing or linked'
    [ -f "$SOURCE_AUDIT" ] && [ ! -L "$SOURCE_AUDIT" ] || \
        die 'the complete private source audit is missing or linked'
    source_head=$(git -C "$PINNED_SOURCE" rev-parse HEAD 2>/dev/null) || \
        die 'pinned source checkout revision cannot be read'
    [ "$source_head" = "$SOURCE_REVISION" ] || \
        die 'pinned source checkout does not match the reviewed revision'
    source_status=$(git -C "$PINNED_SOURCE" status --porcelain=v1 --untracked-files=all 2>/dev/null) || \
        die 'pinned source checkout status cannot be read'
    [ -z "$source_status" ] || die 'pinned source checkout has local changes'

    if [ -e "$OVERLAY_SOURCE" ] || [ -L "$OVERLAY_SOURCE" ]; then
        [ -d "$OVERLAY_SOURCE" ] && [ ! -L "$OVERLAY_SOURCE" ] || \
            die 'existing authored overlay path is linked or not a directory'
    else
        git clone --quiet --no-hardlinks "$PINNED_SOURCE" "$OVERLAY_SOURCE" || \
            die 'could not create a private pinned-source overlay checkout'
        PYTHONDONTWRITEBYTECODE=1 python3 -B scripts/apply-cpu-diagnostic-overlay.py \
            --apply "$OVERLAY_SOURCE" tests/cpu-diagnostic/overlay.patch \
            --source-projection "$SOURCE_PROJECTION" --source-audit "$SOURCE_AUDIT" || \
            die 'authored source overlay did not apply to its exact pinned ranges'
    fi

    PYTHONDONTWRITEBYTECODE=1 python3 -B scripts/verify-cpu-diagnostic-source-closure.py \
        --source-root "$OVERLAY_SOURCE" --pinned-source "$PINNED_SOURCE" \
        --overlay tests/cpu-diagnostic/overlay.patch \
        --source-projection "$SOURCE_PROJECTION" --source-audit "$SOURCE_AUDIT" \
        --output "$SOURCE_CLOSURE" || die 'applied source closure did not pass prebuild review'
    printf '%s\n' 'PASS: pinned source overlay and complete prebuild module closure are sealed privately'
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
    compiler=$QUALIFIED_CC
    require_command "$compiler"
    [ -f "$compiler" ] && [ ! -L "$compiler" ] || \
        die 'qualified host compiler path is not a regular executable'
    [ "$(sha256_file "$compiler")" = "$QUALIFIED_CC_SHA" ] || \
        die 'qualified host compiler SHA-256 changed before invocation'
    mkdir -p build/private-cpu-diagnostic
    "$compiler" -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow \
        -Wstrict-prototypes -pedantic -Itools/common \
        tools/fixture/t64_rom_pack.c tools/common/t64_sha256.c -o "$PACKER"
}

verify_archive()
{
    archive=$1
    expected=$2
    [ -f "$archive" ] && [ ! -L "$archive" ] || \
        die "audited Rust component archive is missing or linked: $archive"
    [ "$(sha256_file "$archive")" = "$expected" ] || \
        die "audited Rust component archive SHA-256 changed: $archive"
}

install_toolchain()
{
    verify_preuse_qualification
    verify_private_directory "$PRIVATE_ROOT"
    verify_archive "$RUSTC_ARCHIVE" bd38aa1f0248f66a5682366b6deea6247f3d6c4bdd4024195a4bf37a17e2b45a
    verify_archive "$CARGO_ARCHIVE" 2aef38157428f63fe9bde388f27baf79336fcffc0c6e4683035e43e6b0ca26df
    verify_archive "$HOST_STD_ARCHIVE" 1fa77a688ee001c791401416019ee58c4470825d753030321247e8752c0a3239
    verify_archive "$RUST_SRC_ARCHIVE" b8bb98533e65cb2468548b20b3575518d01b44e7738045a12de2dbb71a0d6cc2
    for path in "$TOOLCHAIN_STAGE" "$TOOLCHAIN_PREFIX" "$TOOLCHAIN_HOME" "$TOOLCHAIN_TMP"; do
        [ ! -e "$path" ] && [ ! -L "$path" ] || \
            die "refusing to reuse an existing Rust installation path: $path"
    done
    umask 077
    mkdir -m 700 "$TOOLCHAIN_STAGE" "$TOOLCHAIN_PREFIX" "$TOOLCHAIN_HOME" "$TOOLCHAIN_TMP"
    mkdir -m 700 "$TOOLCHAIN_STAGE/rustc" "$TOOLCHAIN_STAGE/cargo" \
        "$TOOLCHAIN_STAGE/rust-std" "$TOOLCHAIN_STAGE/rust-src"
    /usr/bin/tar -xf "$RUSTC_ARCHIVE" -C "$TOOLCHAIN_STAGE/rustc"
    /usr/bin/tar -xf "$HOST_STD_ARCHIVE" -C "$TOOLCHAIN_STAGE/rust-std"
    /usr/bin/tar -xf "$RUST_SRC_ARCHIVE" -C "$TOOLCHAIN_STAGE/rust-src"
    /usr/bin/tar -xf "$CARGO_ARCHIVE" -C "$TOOLCHAIN_STAGE/cargo"

    for component in rustc rust-std rust-src cargo; do
        case "$component" in
            rustc) root="$TOOLCHAIN_STAGE/rustc/rustc-nightly-aarch64-apple-darwin" ;;
            rust-std) root="$TOOLCHAIN_STAGE/rust-std/rust-std-nightly-aarch64-apple-darwin" ;;
            rust-src) root="$TOOLCHAIN_STAGE/rust-src/rust-src-nightly" ;;
            cargo) root="$TOOLCHAIN_STAGE/cargo/cargo-nightly-aarch64-apple-darwin" ;;
        esac
        [ -d "$root" ] && [ ! -L "$root" ] && \
            [ -f "$root/install.sh" ] && [ ! -L "$root/install.sh" ] || \
            die "audited component extraction is incomplete: $component"
        env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME="$TOOLCHAIN_HOME" \
            TMPDIR="$TOOLCHAIN_TMP" LC_ALL=C LANG=C \
            /bin/bash "$root/install.sh" --prefix="$ROOT/$TOOLCHAIN_PREFIX" --disable-ldconfig || \
            die "isolated Rust component installation failed: $component"
    done

    verify_installed_toolchain
    printf '%s\n' 'PASS: exact Rust nightly components installed under the fresh private prefix'
}

verify_installed_toolchain()
{
    verify_preuse_qualification
    verify_private_directory "$PRIVATE_ROOT"
    verify_archive "$RUSTC_ARCHIVE" bd38aa1f0248f66a5682366b6deea6247f3d6c4bdd4024195a4bf37a17e2b45a
    verify_archive "$CARGO_ARCHIVE" 2aef38157428f63fe9bde388f27baf79336fcffc0c6e4683035e43e6b0ca26df
    verify_archive "$HOST_STD_ARCHIVE" 1fa77a688ee001c791401416019ee58c4470825d753030321247e8752c0a3239
    verify_archive "$RUST_SRC_ARCHIVE" b8bb98533e65cb2468548b20b3575518d01b44e7738045a12de2dbb71a0d6cc2
    [ -d "$TOOLCHAIN_PREFIX" ] && [ ! -L "$TOOLCHAIN_PREFIX" ] || \
        die 'dedicated Rust toolchain prefix is missing or linked'
    rustc_bin="$ROOT/$TOOLCHAIN_PREFIX/bin/rustc"
    cargo_bin="$ROOT/$TOOLCHAIN_PREFIX/bin/cargo"
    [ -f "$rustc_bin" ] && [ ! -L "$rustc_bin" ] && [ -x "$rustc_bin" ] || \
        die 'installed rustc is missing or not a regular executable'
    [ -f "$cargo_bin" ] && [ ! -L "$cargo_bin" ] && [ -x "$cargo_bin" ] || \
        die 'installed cargo is missing or not a regular executable'
    [ "$(sha256_file "$rustc_bin")" = 61975b277dd3eedb80256c48ceb3dfade0dc9a9cfcc67ff3a52b5d9e4b700cfd ] || \
        die 'installed rustc bytes differ from the audited component archive'
    [ "$(sha256_file "$cargo_bin")" = c43ed4e7561c2edeb6cdb1d938094a4964f7a2147ec96ea1c7a23dcb00aa93d1 ] || \
        die 'installed cargo bytes differ from the audited component archive'
    cargo_component="$TOOLCHAIN_STAGE/cargo/cargo-nightly-aarch64-apple-darwin"
    [ -f "$cargo_component/version" ] && [ ! -L "$cargo_component/version" ] || \
        die 'audited Cargo component version metadata is missing or linked'
    [ "$(cat "$cargo_component/version")" = '0.99.0-nightly (59800466c 2026-07-07)' ] || \
        die 'audited Cargo package version metadata differs from the dated component'
    env -i PATH="$ROOT/$TOOLCHAIN_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
        HOME="$TOOLCHAIN_HOME" TMPDIR="$TOOLCHAIN_TMP" LC_ALL=C LANG=C \
        "$rustc_bin" --version --verbose > "$PRIVATE_ROOT/rustc-version.txt"
    env -i PATH="$ROOT/$TOOLCHAIN_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
        HOME="$TOOLCHAIN_HOME" TMPDIR="$TOOLCHAIN_TMP" LC_ALL=C LANG=C \
        "$cargo_bin" --version --verbose > "$PRIVATE_ROOT/cargo-version.txt"
    grep -Fx 'release: 1.99.0-nightly' "$PRIVATE_ROOT/rustc-version.txt" >/dev/null || \
        die 'installed rustc reports an unexpected release'
    grep -Fx 'commit-hash: d0babd8b6b05ef9bb65d42f928cef4129d64cf65' "$PRIVATE_ROOT/rustc-version.txt" >/dev/null || \
        die 'installed rustc reports an unexpected source commit'
    grep -Fx 'host: aarch64-apple-darwin' "$PRIVATE_ROOT/rustc-version.txt" >/dev/null || \
        die 'installed rustc reports an unexpected host target'
    grep -Fx 'cargo 1.99.0-nightly (59800466c 2026-07-07)' "$PRIVATE_ROOT/cargo-version.txt" >/dev/null || \
        die 'installed cargo reports an unexpected CLI release'
    grep -Fx 'release: 1.99.0-nightly' "$PRIVATE_ROOT/cargo-version.txt" >/dev/null || \
        die 'installed cargo reports an unexpected CLI release metadata'
    grep -Fx 'commit-hash: 59800466c5c41c444d264b1010b4d57e85a7117f' "$PRIVATE_ROOT/cargo-version.txt" >/dev/null || \
        die 'installed cargo reports an unexpected source commit'
    grep -Fx 'host: aarch64-apple-darwin' "$PRIVATE_ROOT/cargo-version.txt" >/dev/null || \
        die 'installed cargo reports an unexpected host target'
    sysroot=$(env -i PATH="$ROOT/$TOOLCHAIN_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
        HOME="$TOOLCHAIN_HOME" TMPDIR="$TOOLCHAIN_TMP" LC_ALL=C LANG=C \
        "$rustc_bin" --print sysroot)
    [ "$sysroot" = "$ROOT/$TOOLCHAIN_PREFIX" ] || \
        die 'installed rustc sysroot escaped the dedicated private prefix'
    [ -f "$sysroot/lib/rustlib/src/rust/library/Cargo.lock" ] && \
        [ ! -L "$sysroot/lib/rustlib/src/rust/library/Cargo.lock" ] || \
        die 'installed rust-src Cargo.lock is missing or linked'
    sysroot_lock_sha=$(sha256_file "$sysroot/lib/rustlib/src/rust/library/Cargo.lock")
    [ "$sysroot_lock_sha" = 9e87d1ac04edbf5fa61e27cb21984a83566573a007767713868965fba70acb6d ] || \
        die 'installed rust-src Cargo.lock differs from the audited sysroot lock'
    target_libdir=$(env -i PATH="$ROOT/$TOOLCHAIN_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
        HOME="$TOOLCHAIN_HOME" TMPDIR="$TOOLCHAIN_TMP" LC_ALL=C LANG=C \
        "$rustc_bin" --print target-libdir --target aarch64-apple-darwin)
    case "$target_libdir" in "$sysroot"/*) ;; *) die 'host standard library resolved outside the dedicated sysroot' ;; esac
    [ -d "$target_libdir" ] && [ ! -L "$target_libdir" ] || \
        die 'installed host standard library target directory is missing or linked'
    T64_TOOLCHAIN_PREFIX="$ROOT/$TOOLCHAIN_PREFIX" \
    T64_PRIVATE_ROOT="$ROOT/$PRIVATE_ROOT" \
    T64_RUSTC_SHA="$(sha256_file "$rustc_bin")" \
    T64_CARGO_SHA="$(sha256_file "$cargo_bin")" \
    T64_SYSROOT_LOCK_SHA="$sysroot_lock_sha" \
    T64_TARGET_LIBDIR="$target_libdir" \
        python3 -B - <<'PY'
import json
import os
import pathlib

private = pathlib.Path(os.environ["T64_PRIVATE_ROOT"])
report = {
    "schema": "t64.cpu-toolchain-install.v1",
    "status": "PASS_EXACT_COMPONENTS_INSTALLED",
    "channel": "nightly-2026-07-16",
    "host_target": "aarch64-apple-darwin",
    "private_prefix": os.environ["T64_TOOLCHAIN_PREFIX"],
    "components": {
        "rustc": {
            "archive_sha256": "bd38aa1f0248f66a5682366b6deea6247f3d6c4bdd4024195a4bf37a17e2b45a",
            "binary_sha256": os.environ["T64_RUSTC_SHA"],
            "version_output": (private / "rustc-version.txt").read_text(),
        },
        "cargo": {
            "archive_sha256": "2aef38157428f63fe9bde388f27baf79336fcffc0c6e4683035e43e6b0ca26df",
            "binary_sha256": os.environ["T64_CARGO_SHA"],
            "package_version": "0.99.0-nightly (59800466c 2026-07-07)",
            "cli_version_output": (private / "cargo-version.txt").read_text(),
            "source_explanation": [
                "https://github.com/rust-lang/cargo/blob/59800466c5c41c444d264b1010b4d57e85a7117f/Cargo.toml#L147-L149",
                "https://github.com/rust-lang/cargo/blob/59800466c5c41c444d264b1010b4d57e85a7117f/src/cargo/version.rs#L61-L81",
                "https://github.com/rust-lang/cargo/blob/59800466c5c41c444d264b1010b4d57e85a7117f/src/bin/cargo/cli.rs#L207-L215",
            ],
        },
        "host-rust-std": {
            "archive_sha256": "1fa77a688ee001c791401416019ee58c4470825d753030321247e8752c0a3239",
            "target": "aarch64-apple-darwin",
            "target_libdir": os.environ["T64_TARGET_LIBDIR"],
        },
        "rust-src": {
            "archive_sha256": "b8bb98533e65cb2468548b20b3575518d01b44e7738045a12de2dbb71a0d6cc2",
            "library_cargo_lock_sha256": os.environ["T64_SYSROOT_LOCK_SHA"],
        },
    },
    "effects": "Component hashes and version strings verified; no Cargo graph, project hook, build, upstream runner, or guest executed.",
}
destination = private / "toolchain-install.json"
temporary = private / f"toolchain-install.json.tmp.{os.getpid()}"
with temporary.open("x", encoding="utf-8") as stream:
    json.dump(report, stream, sort_keys=True, indent=2)
    stream.write("\n")
os.chmod(temporary, 0o600)
os.replace(temporary, destination)
PY
    printf '%s\n' 'PASS: exact installed Rust components, separate Cargo package/CLI versions, and sysroot lock verified'
}

run_unit_graph()
{
    verify_preuse_qualification
    verify_installed_toolchain
    verify_private_directory "$PRIVATE_ROOT"
    vendor_report="$PRIVATE_ROOT/vendor-manifest-complete.json"
    [ -f "$vendor_report" ] && [ ! -L "$vendor_report" ] || \
        die 'complete project/sysroot offline vendor report is missing or linked'
    [ ! -e "$UNIT_GRAPH_REVIEW" ] && [ ! -L "$UNIT_GRAPH_REVIEW" ] || \
        die 'refusing to overwrite a prior private Cargo unit graph review'
    target_spec="$OVERLAY_SOURCE/mips-nintendo64-none.json"
    [ -f "$target_spec" ] && [ ! -L "$target_spec" ] || \
        die 'reviewed custom target specification is missing or linked'
    [ "$(sha256_file "$target_spec")" = b9e3122e9c7c2acd64e201fa82ce332dcd2c880c01aa589db6ea251ce90a6111 ] || \
        die 'custom target specification differs from the reviewed bytes'
    project_lock="$OVERLAY_SOURCE/Cargo.lock"
    [ -f "$project_lock" ] && [ ! -L "$project_lock" ] || \
        die 'exact packaged Cargo.lock is missing or linked'
    project_lock_before=$(sha256_file "$project_lock")
    sysroot_lock="$TOOLCHAIN_PREFIX/lib/rustlib/src/rust/library/Cargo.lock"
    sysroot_lock_before=$(sha256_file "$sysroot_lock")
    [ "$sysroot_lock_before" = 9e87d1ac04edbf5fa61e27cb21984a83566573a007767713868965fba70acb6d ] || \
        die 'sysroot Cargo.lock changed after its pre-use audit'
    python3 -B - "$vendor_report" "$project_lock_before" "$sysroot_lock_before" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
if (
    report.get("schema") != "t64.cpu-offline-vendor-union.v1"
    or report.get("status") != "PASS_PROJECT_AND_SYSROOT_RESOLUTION_SOURCES_MATERIALIZED"
    or report.get("project_lock_sha256") != sys.argv[2]
    or report.get("sysroot_lock_sha256") != sys.argv[3]
    or len(report.get("project_packages", [])) != 14
    or len(report.get("sysroot_registry_packages", [])) != 30
):
    raise SystemExit("private offline vendor receipt does not match both exact Cargo locks")
PY
    if [ -e "$UNIT_GRAPH" ] || [ -e "$UNIT_GRAPH_LOG" ] || [ -L "$UNIT_GRAPH" ] || [ -L "$UNIT_GRAPH_LOG" ]; then
        [ -f "$UNIT_GRAPH" ] && [ ! -L "$UNIT_GRAPH" ] && \
            [ -f "$UNIT_GRAPH_LOG" ] && [ ! -L "$UNIT_GRAPH_LOG" ] || \
            die 'prior Cargo unit-graph output is incomplete or linked'
        [ ! -s "$UNIT_GRAPH_LOG" ] || die 'prior Cargo unit-graph stderr is nonempty'
    else
        [ ! -e "$CARGO_HOME" ] && [ ! -L "$CARGO_HOME" ] || \
            die 'refusing to reuse an existing Cargo home without a completed graph'
        [ ! -e "$CARGO_TARGET_DIR" ] && [ ! -L "$CARGO_TARGET_DIR" ] || \
            die 'refusing to reuse an existing private Cargo target directory without a completed graph'
        python3 -B - "$ROOT" "$CARGO_HOME" "$ROOT/$PRIVATE_ROOT/cargo-vendor-complete" <<'PY'
import json
import pathlib
import sys

repository = pathlib.Path(sys.argv[1]).resolve(strict=True)
cargo_home = pathlib.Path(sys.argv[2])
vendor = pathlib.Path(sys.argv[3]).resolve(strict=True)
config = cargo_home / "config.toml"
if cargo_home.exists() or cargo_home.is_symlink():
    raise SystemExit("refusing to reuse an existing private Cargo home")
cargo_home.mkdir(mode=0o700)
text = (
    "[source.crates-io]\n"
    'replace-with = "t64-reviewed-vendor"\n\n'
    "[source.t64-reviewed-vendor]\n"
    f"directory = {json.dumps(str(vendor))}\n\n"
    "[net]\n"
    "offline = true\n"
)
with config.open("x", encoding="utf-8") as stream:
    stream.write(text)
config.chmod(0o600)
PY
        [ -f "$CARGO_HOME/config.toml" ] && [ ! -L "$CARGO_HOME/config.toml" ] || \
            die 'private offline Cargo source configuration was not created'
        rustc_bin="$ROOT/$TOOLCHAIN_PREFIX/bin/rustc"
        cargo_bin="$ROOT/$TOOLCHAIN_PREFIX/bin/cargo"
        umask 077
        (cd "$OVERLAY_SOURCE" && \
            env -i PATH="$ROOT/$TOOLCHAIN_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
                HOME="$ROOT/$TOOLCHAIN_HOME" TMPDIR="$ROOT/$TOOLCHAIN_TMP" \
                CARGO_HOME="$ROOT/$CARGO_HOME" CARGO_TARGET_DIR="$ROOT/$CARGO_TARGET_DIR" \
                RUSTC="$rustc_bin" LC_ALL=C LANG=C \
                "$cargo_bin" --config 'profile.release.panic="abort"' \
                    build --release --target mips-nintendo64-none.json \
                    --unit-graph -Z unstable-options --offline --locked --frozen \
                > "$ROOT/$UNIT_GRAPH" 2> "$ROOT/$UNIT_GRAPH_LOG") || \
            die 'offline locked Cargo unit-graph command refused the reviewed graph'
    fi
    project_lock_after=$(sha256_file "$project_lock")
    sysroot_lock_after=$(sha256_file "$sysroot_lock")
    [ "$project_lock_before" = "$project_lock_after" ] || \
        die 'Cargo unit graph changed the packaged project Cargo.lock'
    [ "$sysroot_lock_before" = "$sysroot_lock_after" ] || \
        die 'Cargo unit graph changed the sysroot Cargo.lock resolution input'
    [ -s "$UNIT_GRAPH" ] && [ ! -L "$UNIT_GRAPH" ] && \
        [ ! -L "$UNIT_GRAPH_LOG" ] || die 'Cargo unit graph output is missing or linked'
    PYTHONDONTWRITEBYTECODE=1 python3 -B scripts/verify-cpu-diagnostic-unit-graph.py \
        --graph "$ROOT/$UNIT_GRAPH" \
        --project-manifest "$ROOT/$OVERLAY_SOURCE/Cargo.toml" \
        --project-lock "$ROOT/$OVERLAY_SOURCE/Cargo.lock" \
        --sysroot-lock "$ROOT/$sysroot_lock" \
        --target-spec "$ROOT/$target_spec" \
        --cargo-config "$ROOT/$OVERLAY_SOURCE/.cargo/config.toml" \
        --cargo-home-config "$ROOT/$CARGO_HOME/config.toml" \
        --vendor-report "$ROOT/$vendor_report" \
        --project-review "$ROOT/$PRIVATE_ROOT/package-registry-audit/qualification-source-review.json" \
        --rust-src-audit "$ROOT/$PRIVATE_ROOT/root-rust-src-audit/audit.json" \
        --qualification "$ROOT/$PRIVATE_ROOT/qualification.json" \
        --toolchain-install "$ROOT/$PRIVATE_ROOT/toolchain-install.json" \
        --source-projection "$ROOT/$SOURCE_PROJECTION" \
        --source-audit "$ROOT/$SOURCE_AUDIT" \
        --source-closure "$ROOT/$SOURCE_CLOSURE" \
        --output "$ROOT/$UNIT_GRAPH_REVIEW" || die 'frozen offline Cargo unit graph differs from the reviewed source/tool closure'
    printf '%s\n' 'PASS: frozen offline Cargo unit graph and exact reviewed package/hook closure verified; nothing was built or run'
}

verify_preuse_qualification()
{
    require_command python3
    qualification_root=${T64_CPU_DIAGNOSTIC_QUALIFICATION_ROOT:-build/private-cpu-diagnostic}
    [ -d "$qualification_root" ] && [ ! -L "$qualification_root" ] || {
        printf '%s\n' 'pre-use qualification refused: private qualification root is missing' >&2
        exit 4
    }
    tools_json=$(PYTHONDONTWRITEBYTECODE=1 python3 -B \
        scripts/verify-cpu-diagnostic-qualification.py --tool-paths-json \
        "$qualification_root/qualification.json" "$ROOT" 2>&1) || {
        printf '%s\n' "$tools_json" >&2
        exit 4
    }
    QUALIFIED_CC=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["host_c_compiler"])')
    QUALIFIED_CC_SHA=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["host_c_compiler_sha256"])')
    AS=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["target_binutils"]["as"])')
    AS_SHA=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["target_binutils_sha256"]["as"])')
    LD=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["target_binutils"]["ld"])')
    LD_SHA=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["target_binutils_sha256"]["ld"])')
    OBJCOPY=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["target_binutils"]["objcopy"])')
    OBJCOPY_SHA=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["target_binutils_sha256"]["objcopy"])')
    OBJDUMP=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["target_binutils"]["objdump"])')
    OBJDUMP_SHA=$(printf '%s' "$tools_json" | python3 -c \
        'import json,sys; print(json.load(sys.stdin)["target_binutils_sha256"]["objdump"])')
    [ -x "$QUALIFIED_CC" ] && [ -x "$AS" ] && [ -x "$LD" ] && \
        [ -x "$OBJCOPY" ] && [ -x "$OBJDUMP" ] || {
        printf '%s\n' 'pre-use qualification refused: reviewed host compiler is unavailable' >&2
        exit 4
    }
    printf '%s\n' 'pre-use qualification accepted'
}

run_selftest()
{
    verify_preuse_qualification
    verify_boot_asset
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
    for pair in "$AS:$AS_SHA" "$LD:$LD_SHA" "$OBJCOPY:$OBJCOPY_SHA" "$OBJDUMP:$OBJDUMP_SHA"; do
        tool=${pair%%:*}
        expected=${pair#*:}
        [ -f "$tool" ] && [ ! -L "$tool" ] && [ -x "$tool" ] || \
            die "audited target tool path is not a regular executable: $tool"
        [ "$(sha256_file "$tool")" = "$expected" ] || \
            die "audited target tool SHA-256 changed before invocation: $tool"
    done
    for tool in "$AS" "$LD" "$OBJCOPY" "$OBJDUMP"; do
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
    verify_boot_asset
    verify_target_tools
    compile_packer
    "$PACKER" --selftest >/dev/null || die 'packer selftest failed'
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
    --prepare-source) prepare_source ;;
    --selftest) run_selftest ;;
    --qualify) run_qualify ;;
    --install-toolchain) install_toolchain ;;
    --verify-toolchain) verify_installed_toolchain ;;
    --unit-graph) run_unit_graph ;;
    *) printf '%s\n' 'usage: sh scripts/build-cpu-diagnostic.sh --prepare-source|--selftest|--qualify|--install-toolchain|--verify-toolchain|--unit-graph' >&2; exit 2 ;;
esac

#!/usr/bin/env python3
"""Build the privately qualified, source-derived RSP diagnostic component."""

from __future__ import annotations

import hashlib
import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-rsp-diagnostic"
CPU_PRIVATE = ROOT / "build/private-cpu-diagnostic"
PREFIX = CPU_PRIVATE / "toolchain-prefix"
SOURCE = PRIVATE / "overlay-checkout-rsp-v1"
PROFILE_ID = "ntsc-cart-4m-openipl3-rsp-functional-v1"
MAX_TIMEOUT = 600


def digest(path: pathlib.Path) -> str:
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"required regular file is missing or linked: {path}")
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read_json(path: pathlib.Path) -> object:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError(f"required private JSON is missing, linked, or oversized: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: pathlib.Path, value: object) -> None:
    if path.exists() or path.is_symlink():
        raise ValueError(f"refusing to overwrite private build evidence: {path}")
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        stream.write("\n")
    path.chmod(0o600)


def run(command: list[str], work: pathlib.Path, label: str, *, cwd: pathlib.Path = ROOT,
        env: dict[str, str] | None = None, timeout: int = 120) -> dict[str, object]:
    start = time.monotonic()
    stdout_path = work / f"{label}.stdout.log"
    stderr_path = work / f"{label}.stderr.log"
    with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
        process = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                   stdout=stdout, stderr=stderr, start_new_session=True)
        timed_out = False
        deadline = start + timeout
        while os.waitid(os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                timed_out = True
                break
            time.sleep(min(0.05, remaining))
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass
        exit_code = process.wait(timeout=5)
        group_empty = False
        cleanup_deadline = time.monotonic() + 2
        while time.monotonic() < cleanup_deadline:
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                group_empty = True
                break
            except PermissionError:
                pass
            time.sleep(0.02)
    command_receipt = {
        "argv": command,
        "cwd": str(cwd),
        "environment": env,
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "deadline_seconds": timeout,
        "exit": exit_code,
        "timed_out": timed_out,
        "process_group_empty": group_empty,
        "elapsed_seconds": round(time.monotonic() - start, 3),
        "stdout_log": stdout_path.name,
        "stderr_log": stderr_path.name,
    }
    write_json(work / f"{label}.command.json", command_receipt)
    if timed_out or exit_code != 0 or not group_empty:
        raise ValueError(f"{label} failed; exact command and bounded private logs were retained")
    return command_receipt


def qualify() -> None:
    if os.environ.get("CI", "").lower() in ("true", "1") or os.environ.get("T64_REQUIRE_FIXTURES") == "1":
        raise ValueError("qualified RSP diagnostic builds are local-only")
    os.umask(0o077)
    if PRIVATE.is_symlink() or not PRIVATE.is_dir() or PRIVATE.stat().st_mode & 0o077:
        raise ValueError("private RSP output root must exist with owner-only access")
    qualification_path = PRIVATE / "qualification.json"
    work = pathlib.Path(tempfile.mkdtemp(prefix="rsp-build-attempt-", dir=PRIVATE))
    work.chmod(0o700)
    qualification_sha = digest(qualification_path)
    verify_command = [sys.executable, "-B", "scripts/verify-rsp-diagnostic-qualification.py", "--verify", str(qualification_path)]
    run(verify_command, work, "preuse", timeout=120)
    qualification = read_json(qualification_path)
    if qualification.get("schema") != "t64.rsp-qualification.v1" or qualification.get("status") != "qualified" or qualification.get("profile_id") != PROFILE_ID:
        raise ValueError("RSP qualification receipt does not match the intended component")

    tool_receipt = read_json(CPU_PRIVATE / "qualification.json")
    tool_rows = {row["name"]: row for row in tool_receipt["tools"]["target_binutils"]}
    objcopy = tool_rows["objcopy"]
    cc = tool_receipt["tools"]["host_c_compiler"]
    if digest(pathlib.Path(objcopy["path"])) != objcopy["sha256"] or digest(pathlib.Path(cc["path"])) != cc["sha256"]:
        raise ValueError("accepted target objcopy or host compiler changed before use")
    runtime_rows = qualification["tools"]["runtime_files"]
    for row in runtime_rows:
        if digest(ROOT / row["path"]) != row["sha256"]:
            raise ValueError("accepted Rust linker runtime library changed before use")

    project_lock = SOURCE / "Cargo.lock"
    sysroot_lock = PREFIX / "lib/rustlib/src/rust/library/Cargo.lock"
    lock_before = [digest(project_lock), digest(sysroot_lock)]
    if lock_before != [qualification["locks"]["project_cargo_lock_sha256"], qualification["locks"]["sysroot_cargo_lock_sha256"]]:
        raise ValueError("locked resolution inputs changed before actual RSP build")
    cargo = PREFIX / "bin/cargo"
    rustc = PREFIX / "bin/rustc"
    cargo_config = CPU_PRIVATE / "cargo-home-attempt-03/config.toml"
    config_text = cargo_config.read_text(encoding="utf-8")
    if "offline = true" not in config_text or str(CPU_PRIVATE / "cargo-vendor-complete") not in config_text:
        raise ValueError("accepted Cargo home no longer points only at the reviewed offline vendor tree")

    target_dir = PRIVATE / "cargo-target-rsp-attempt-01"
    if target_dir.exists() or target_dir.is_symlink():
        raise ValueError("refusing to reuse an unbound RSP Cargo target directory")
    env = {
        "PATH": str(PREFIX / "bin") + ":/usr/bin:/bin:/usr/sbin:/sbin",
        "HOME": str(CPU_PRIVATE / "toolchain-home"),
        "TMPDIR": str(CPU_PRIVATE / "toolchain-tmp"),
        "CARGO_HOME": str(CPU_PRIVATE / "cargo-home-attempt-03"),
        "CARGO_TARGET_DIR": str(target_dir),
        "RUSTC": str(rustc),
        "LC_ALL": "C",
        "LANG": "C",
        "DYLD_LIBRARY_PATH": str(PREFIX / "lib"),
    }
    write_json(work / "attempt-preuse.json", {
        "schema": "t64.rsp-build-attempt.v1",
        "attempt_id": work.name,
        "status": "starting-actual-build",
        "qualification_sha256": qualification_sha,
        "preuse_closure_sha256": qualification["closure"]["sha256"],
        "source_revision": qualification["source_revision"],
        "profile_id": PROFILE_ID,
        "core_build_sha256": qualification["build_identity"]["core_build_sha256"],
        "core_model": qualification["build_identity"]["core_model"],
        "core_state_schema": qualification["build_identity"]["core_state_schema"],
        "project_lock_before": lock_before[0],
        "sysroot_lock_before": lock_before[1],
        "target_dir": str(target_dir),
        "environment": env,
    })
    cargo_command = [str(cargo), "--config", 'profile.release.panic="abort"', "build", "--jobs", "4", "--release", "--target", "mips-nintendo64-none.json", "--offline", "--locked", "--frozen"]
    try:
        run(cargo_command, work, "cargo", cwd=SOURCE, env=env, timeout=MAX_TIMEOUT)
    except ValueError as error:
        write_json(work / "attempt-result.json", {"schema": "t64.rsp-build-attempt-result.v1", "status": "actual-build-failed", "reached_stage": "cargo", "error": str(error), "qualification_sha256": qualification_sha})
        raise
    lock_after = [digest(project_lock), digest(sysroot_lock)]
    if lock_before != lock_after:
        raise ValueError("actual RSP build modified a locked resolution")

    elf = target_dir / "mips-nintendo64-none/release/n64-systemtest"
    if not elf.is_file() or elf.is_symlink():
        raise ValueError("cargo exited successfully without producing the expected RSP ELF")
    elf_sha = digest(elf)
    tools = {row["name"]: row for row in tool_receipt["tools"]["target_binutils"]}
    objdump = tools["objdump"]
    run([objdump["path"], "-h", str(elf)], work, "elf-sections", timeout=60)
    run([objdump["path"], "-t", str(elf)], work, "elf-symbols", timeout=60)
    payload = work / "rsp.payload.bin"
    run([objcopy["path"], "-O", "binary", str(elf), str(payload)], work, "objcopy", timeout=60)
    if digest(pathlib.Path(objcopy["path"])) != objcopy["sha256"]:
        raise ValueError("accepted objcopy changed during RSP payload extraction")
    if payload.stat().st_size > 4 * 1024 * 1024:
        raise ValueError("RSP payload exceeds the sealed four-MiB profile")
    packer = work / "t64-rom-pack-rsp"
    run([cc["path"], "-std=c17", "-Wall", "-Wextra", "-Werror", "-Wconversion", "-Wshadow", "-Wstrict-prototypes", "-pedantic", "-Itools/common", "tools/fixture/t64_rom_pack.c", "tools/common/t64_sha256.c", "-o", str(packer)], work, "packer-build", timeout=60)
    rom = work / "rsp.z64"
    boot = ROOT / "build/fixtures/ipl3_compat.z64"
    run([str(packer), "--pack", "--boot", str(boot), "--payload", str(payload), "--output", str(rom), "--entry", "0x80000400", "--clock", "0x0000000f", "--crc2", "0x00000000"], work, "rom-pack", timeout=60)
    composition = work / "composition.json"
    run([sys.executable, "-B", "scripts/verify-rsp-diagnostic-build.py", "--inspect", "--elf", str(elf), "--payload", str(payload), "--rom", str(rom), "--boot", str(boot), "--qualification", str(qualification_path), "--sections-log", str(work / "elf-sections.stdout.log"), "--symbols-log", str(work / "elf-symbols.stdout.log"), "--report", str(composition)], work, "composition", timeout=60)
    outputs = {"elf": {"path": str(elf.relative_to(ROOT)), "sha256": elf_sha, "size": elf.stat().st_size},
               "payload": {"path": str(payload.relative_to(ROOT)), "sha256": digest(payload), "size": payload.stat().st_size},
               "rom": {"path": str(rom.relative_to(ROOT)), "sha256": digest(rom), "size": rom.stat().st_size},
               "composition": {"path": str(composition.relative_to(ROOT)), "sha256": digest(composition)}}
    build_receipt = {"schema": "t64.rsp-qualified-build.v1", "schema_version": 1, "status": "qualified-build-composed", "component": "rsp", "profile_id": PROFILE_ID, "source_revision": qualification["source_revision"], "qualification_sha256": qualification_sha, "preuse_closure_sha256": qualification["closure"]["sha256"], "core_build_sha256": qualification["build_identity"]["core_build_sha256"], "attempt_id": work.name, "attempt_dir": str(work.relative_to(ROOT)), "cargo_argv": cargo_command, "cargo_deadline_seconds": MAX_TIMEOUT, "locks_unchanged": True, "artifacts": outputs, "guest_rom_executed": False}
    write_json(PRIVATE / "qualified-build.json", build_receipt)
    write_json(work / "attempt-result.json", {"schema": "t64.rsp-build-attempt-result.v1", "status": "qualified-build-composed", "reached_stage": "composition", "artifacts": outputs, "qualification_sha256": qualification_sha})
    print("PASS: actual RSP source build and ROM composition inspected; guest ROM was not executed")


def seal_existing_attempt(attempt_name: str) -> None:
    """Seal a completed actual build after a separately retained composition inspection."""
    os.umask(0o077)
    if PRIVATE.is_symlink() or not PRIVATE.is_dir() or PRIVATE.stat().st_mode & 0o077:
        raise ValueError("private RSP output root must exist with owner-only access")
    if pathlib.Path(attempt_name).name != attempt_name or not attempt_name.startswith("rsp-build-attempt-"):
        raise ValueError("existing build attempt name is malformed")
    work = PRIVATE / attempt_name
    if work.is_symlink() or not work.is_dir() or work.stat().st_mode & 0o077:
        raise ValueError("existing build attempt is missing or not owner-only")
    result_path = work / "attempt-result.json"
    receipt_path = PRIVATE / "qualified-build.json"
    if result_path.exists() or receipt_path.exists():
        raise ValueError("refusing to overwrite existing RSP attempt or build receipts")
    preuse = read_json(work / "attempt-preuse.json")
    cargo = read_json(work / "cargo.command.json")
    require_keys = (preuse.get("schema") == "t64.rsp-build-attempt.v1" and preuse.get("attempt_id") == attempt_name
                   and cargo.get("exit") == 0 and cargo.get("timed_out") is False and cargo.get("process_group_empty") is True
                   and cargo.get("deadline_seconds") == MAX_TIMEOUT)
    if not require_keys:
        raise ValueError("existing attempt does not contain a completed bounded cargo build")
    argv = cargo.get("argv")
    if not isinstance(argv, list) or argv[-9:] != ["build", "--jobs", "4", "--release", "--target", "mips-nintendo64-none.json", "--offline", "--locked", "--frozen"]:
        raise ValueError("existing cargo command differs from the approved offline resource profile")
    qualification = read_json(PRIVATE / "qualification.json")
    if digest(PRIVATE / "qualification.json") != preuse.get("qualification_sha256"):
        raise ValueError("pre-use qualification changed during the actual build")
    # Keep prior reports immutable; inspection must authenticate today's bytes.
    composition_path = work / "composition-recovered.json"
    target = PRIVATE / "cargo-target-rsp-attempt-01/mips-nintendo64-none/release/n64-systemtest"
    payload, rom = work / "rsp.payload.bin", work / "rsp.z64"
    locks = [SOURCE / "Cargo.lock", PREFIX / "lib/rustlib/src/rust/library/Cargo.lock"]
    if [digest(path) for path in locks] != [preuse.get("project_lock_before"), preuse.get("sysroot_lock_before")]:
        raise ValueError("locked resolution changed since the actual build began")
    inspection = run(
        [sys.executable, "-B", "scripts/verify-rsp-diagnostic-build.py", "--inspect",
         "--elf", str(target), "--payload", str(payload), "--rom", str(rom),
         "--boot", str(ROOT / "build/fixtures/ipl3_compat.z64"),
         "--qualification", str(PRIVATE / "qualification.json"),
         "--sections-log", str(work / "elf-sections.stdout.log"),
         "--symbols-log", str(work / "elf-symbols.stdout.log"),
         "--report", str(composition_path)], work, "composition-recovered", timeout=60)
    composition = read_json(composition_path)
    if composition.get("schema") != "t64.rsp-build-composition.v1" or composition.get("status") != "PASS_POSTBUILD_COMPOSITION" or composition.get("qualification_sha256") != preuse.get("qualification_sha256"):
        raise ValueError("existing actual output composition is not accepted")
    artifacts = {"elf": target, "payload": payload, "rom": rom, "composition": composition_path}
    encoded = {name: {"path": str(path.relative_to(ROOT)), "sha256": digest(path), "size": path.stat().st_size}
               for name, path in artifacts.items()}
    write_json(work / "postbuild-inspection.json", inspection)
    build_receipt = {"schema": "t64.rsp-qualified-build.v1", "schema_version": 1, "status": "qualified-build-composed", "component": "rsp", "profile_id": PROFILE_ID, "source_revision": qualification["source_revision"], "qualification_sha256": preuse["qualification_sha256"], "preuse_closure_sha256": preuse["preuse_closure_sha256"], "core_build_sha256": preuse["core_build_sha256"], "attempt_id": attempt_name, "attempt_dir": str(work.relative_to(ROOT)), "cargo_argv": argv, "cargo_deadline_seconds": MAX_TIMEOUT, "cargo_exit": 0, "cargo_timed_out": False, "cargo_process_group_empty": True, "locks_unchanged": True, "artifacts": encoded, "guest_rom_executed": False, "composition_inspection_recovered_after_setup_failure": True}
    write_json(receipt_path, build_receipt)
    write_json(result_path, {"schema": "t64.rsp-build-attempt-result.v1", "status": "qualified-build-composed", "reached_stage": "composition", "artifacts": encoded, "qualification_sha256": preuse["qualification_sha256"], "initial_composition_setup_failure_preserved": True})
    print("PASS: existing actual RSP build and composition sealed without rebuilding")


if __name__ == "__main__":
    try:
        if sys.argv[1:] == ["--qualify-build"]:
            qualify()
        elif len(sys.argv) == 3 and sys.argv[1] == "--seal-existing-attempt":
            seal_existing_attempt(sys.argv[2])
        else:
            print("usage: build-qualified-rsp-diagnostic.py --qualify-build | --seal-existing-attempt ATTEMPT", file=sys.stderr)
            raise SystemExit(2)
    except (OSError, ValueError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        print(f"refused: {error}", file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""Seal a local Tier B/T1 BASE context from the accepted source projection."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import re
import signal
import stat
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-base-diagnostic"
CPU_PRIVATE = ROOT / "build/private-cpu-diagnostic"
RSP_PRIVATE = ROOT / "build/private-rsp-diagnostic"
CPU_ADAPTER_RELATIVE = "build/gate/runner/t64-cpu-diagnostic"
BASE_ADAPTER = ROOT / "build/gate/runner/t64-base-diagnostic"
PROJECTION = ROOT / "build/private-cpu-diagnostic/source-projection.json"
RSP_PROJECTION = RSP_PRIVATE / "rsp-source-projection.json"
RSP_QUALIFICATION = RSP_PRIVATE / "qualification.json"
RSP_OVERLAY = RSP_PRIVATE / "rsp-overlay-preparation.json"
CPU_SOURCE = CPU_PRIVATE / "overlay-checkout-8aa0921"
BASE_SOURCE = CPU_PRIVATE / "nemu64-test"
BASE_IMAGE = PRIVATE / "base.z64"
BASE_COMPOSITION = PRIVATE / "combined-image.json"
HEX64 = re.compile(r"^[0-9a-f]{64}$")
HEX40 = re.compile(r"^[0-9a-f]{40}$")
PROFILE_ID = "ntsc-cart-4m-openipl3-rdram-b-t1-v1"


class Refusal(ValueError):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def retain_execution_snapshot(repository_root: pathlib.Path,
                               private_root: pathlib.Path,
                               context_bytes: bytes,
                               binding_bytes: bytes) -> pathlib.Path:
    repository_root = repository_root.resolve(strict=True)
    private_root = private_root.absolute()
    if private_root.is_symlink():
        refuse("retained execution snapshot root is linked")
    private_root = private_root.resolve(strict=True)
    expected_private = repository_root / "build/private-base-diagnostic"
    if private_root != expected_private:
        refuse("retained execution snapshot root is outside the fixed private root")
    if private_root.is_symlink() or not private_root.is_dir():
        refuse("retained execution snapshot root is missing or linked")
    private_info = private_root.stat(follow_symlinks=False)
    if (stat.S_IMODE(private_info.st_mode) != 0o700 or
            private_info.st_uid != os.geteuid()):
        refuse("retained execution snapshot root is not owner-only")

    try:
        context = json.loads(context_bytes)
        binding = json.loads(binding_bytes)
    except (UnicodeError, json.JSONDecodeError) as exc:
        refuse(f"retained context pair is invalid JSON: {exc}")
    context_sha256 = digest(context_bytes)
    if (not isinstance(context, dict) or canonical(context) != context_bytes or
            not isinstance(binding, dict) or canonical(binding) != binding_bytes or
            set(binding) != {
                "context_sha256", "inventory_sha256", "schema", "schema_version",
                "source_authority_sha256", "source_projection_sha256",
            } or binding.get("context_sha256") != context_sha256 or
            binding.get("schema") != "t64.base-qualification-binding" or
            binding.get("schema_version") != 1):
        refuse("retained context pair is not a canonical bound pair")
    bindings = context.get("bindings")
    executable_rows = bindings.get("executable") if isinstance(bindings, dict) else None
    if (not isinstance(executable_rows, list) or len(executable_rows) != 1 or
            not isinstance(executable_rows[0], dict) or
            set(executable_rows[0]) != {"path", "sha256"} or
            not isinstance(executable_rows[0].get("path"), str) or
            not isinstance(executable_rows[0].get("sha256"), str) or
            HEX64.fullmatch(executable_rows[0]["sha256"]) is None):
        refuse("retained BASE executable binding is malformed")

    relative = pathlib.PurePosixPath(executable_rows[0]["path"])
    if (relative.is_absolute() or "\\" in executable_rows[0]["path"] or
            any(part in {"", ".", ".."} for part in relative.parts)):
        refuse("retained BASE executable path escapes the repository")
    executable_path = repository_root.joinpath(*relative.parts)
    cursor = repository_root
    for part in relative.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse("retained BASE executable path traverses a symlink")

    def read_stable_file(path: pathlib.Path, label: str, maximum: int,
                         owner_only: bool = False) -> bytes:
        try:
            before = path.lstat()
        except OSError as exc:
            refuse(f"{label} cannot be inspected: {exc}")
        if (not stat.S_ISREG(before.st_mode) or before.st_size <= 0 or
                before.st_size > maximum or before.st_mode & 0o022 or
                before.st_uid != os.geteuid() or
                (owner_only and stat.S_IMODE(before.st_mode) != 0o600)):
            refuse(f"{label} is not a bounded owned regular file")
        try:
            descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
        except OSError as exc:
            refuse(f"{label} cannot be opened without following links: {exc}")
        try:
            opened = os.fstat(descriptor)
            if ((opened.st_dev, opened.st_ino, opened.st_size) !=
                    (before.st_dev, before.st_ino, before.st_size) or
                    not stat.S_ISREG(opened.st_mode)):
                refuse(f"{label} changed while opening")
            pieces = []
            total = 0
            while True:
                block = os.read(descriptor, 1024 * 1024)
                if not block:
                    break
                total += len(block)
                if total > maximum:
                    refuse(f"{label} exceeds its retention size bound")
                pieces.append(block)
            after = os.fstat(descriptor)
            after_path = path.lstat()
            if ((after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) !=
                    (opened.st_dev, opened.st_ino, opened.st_size, opened.st_mtime_ns) or
                    (after_path.st_dev, after_path.st_ino, after_path.st_size) !=
                    (opened.st_dev, opened.st_ino, opened.st_size) or
                    total != before.st_size):
                refuse(f"{label} changed while being copied")
            return b"".join(pieces)
        finally:
            os.close(descriptor)

    executable_bytes = read_stable_file(
        executable_path, "qualified BASE executable", 512 * 1024 * 1024
    )
    executable_info = executable_path.stat(follow_symlinks=False)
    if not executable_info.st_mode & 0o111:
        refuse("qualified BASE executable is not executable")
    executable_sha256 = digest(executable_bytes)
    if executable_sha256 != executable_rows[0]["sha256"]:
        refuse("qualified BASE executable changed before retention")

    snapshots = private_root / "execution-snapshots"
    created_snapshots = False
    if snapshots.exists() or snapshots.is_symlink():
        if snapshots.is_symlink() or not snapshots.is_dir():
            refuse("retained execution snapshot collection is linked or invalid")
        info = snapshots.stat(follow_symlinks=False)
        if stat.S_IMODE(info.st_mode) != 0o700 or info.st_uid != os.geteuid():
            refuse("retained execution snapshot collection is not owner-only")
    else:
        os.mkdir(snapshots, 0o700)
        created_snapshots = True
    snapshot = snapshots / context_sha256
    expected_files = {
        "binding.json": binding_bytes,
        "context.json": context_bytes,
        "executable": executable_bytes,
    }
    if snapshot.exists() or snapshot.is_symlink():
        if snapshot.is_symlink() or not snapshot.is_dir():
            refuse("retained execution snapshot is linked or invalid")
        info = snapshot.stat(follow_symlinks=False)
        if stat.S_IMODE(info.st_mode) != 0o700 or info.st_uid != os.geteuid():
            refuse("retained execution snapshot is not owner-only")
        if {path.name for path in snapshot.iterdir()} != set(expected_files):
            refuse("existing retained execution snapshot is incomplete")
        for name, expected in expected_files.items():
            path = snapshot / name
            actual = read_stable_file(
                path, f"retained {name}", 512 * 1024 * 1024, owner_only=True
            )
            mode = stat.S_IMODE(path.stat(follow_symlinks=False).st_mode)
            if mode != 0o600 or actual != expected:
                refuse("immutable retained execution snapshot differs from its bound pair")
        if read_stable_file(
                executable_path, "qualified BASE executable", 512 * 1024 * 1024
        ) != executable_bytes:
            refuse("qualified BASE executable changed after retention")
        return snapshot

    os.mkdir(snapshot, 0o700)
    for name, data in expected_files.items():
        path = snapshot / name
        descriptor = os.open(
            path, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
            getattr(os, "O_NOFOLLOW", 0), 0o600
        )
        try:
            with os.fdopen(descriptor, "wb") as output:
                if output.write(data) != len(data):
                    refuse("retained execution snapshot copy was incomplete")
                output.flush()
                os.fsync(output.fileno())
        except Exception:
            path.unlink(missing_ok=True)
            raise
    directory_fd = os.open(snapshot, os.O_RDONLY)
    try:
        os.fsync(directory_fd)
    finally:
        os.close(directory_fd)
    parent_fd = os.open(snapshots, os.O_RDONLY)
    try:
        os.fsync(parent_fd)
    finally:
        os.close(parent_fd)
    if created_snapshots:
        private_fd = os.open(private_root, os.O_RDONLY)
        try:
            os.fsync(private_fd)
        finally:
            os.close(private_fd)
    for name, expected in expected_files.items():
        actual = read_stable_file(
            snapshot / name, f"retained {name}", 512 * 1024 * 1024,
            owner_only=True,
        )
        if actual != expected:
            refuse("retained execution snapshot copy differs from its source")
    source_after = read_stable_file(
        executable_path, "qualified BASE executable", 512 * 1024 * 1024
    )
    if source_after != executable_bytes or digest(source_after) != executable_sha256:
        refuse("qualified BASE executable changed after retention")
    return snapshot


def file_digest(path: pathlib.Path) -> str:
    if path.is_symlink() or not path.is_file():
        refuse("qualified BASE input is missing or linked")
    return digest(path.read_bytes())


def repo_relative(path: pathlib.Path) -> str:
    path = path.absolute()
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        refuse("qualified BASE input is outside the repository")


def private_write(path: pathlib.Path, data: bytes) -> None:
    if path.exists() or path.is_symlink():
        if (path.is_symlink() or not path.is_file() or
                stat.S_IMODE(path.stat(follow_symlinks=False).st_mode) != 0o600 or
                path.read_bytes() != data):
            refuse("immutable BASE context file already exists with different bytes")
        return
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                         getattr(os, "O_NOFOLLOW", 0), 0o600)
    with os.fdopen(descriptor, "wb") as output:
        output.write(data)
        output.flush()
        os.fsync(output.fileno())


def _replace_private(path: pathlib.Path, data: bytes) -> None:
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=PRIVATE)
    temporary = pathlib.Path(temporary_name)
    try:
        os.fchmod(descriptor, 0o600)
        with os.fdopen(descriptor, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
        directory_fd = os.open(PRIVATE, os.O_RDONLY)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
    finally:
        temporary.unlink(missing_ok=True)


def install_context_pair(context_bytes: bytes, binding_bytes: bytes) -> None:
    context_path = PRIVATE / "context.json"
    binding_path = PRIVATE / "binding.json"
    old_context = context_path.read_bytes() if context_path.is_file() and not context_path.is_symlink() else None
    old_binding = binding_path.read_bytes() if binding_path.is_file() and not binding_path.is_symlink() else None
    if old_context == context_bytes and old_binding == binding_bytes:
        return
    if old_context is not None or old_binding is not None:
        history = PRIVATE / "context-history"
        history.mkdir(mode=0o700, exist_ok=True)
        if history.is_symlink() or not history.is_dir():
            refuse("private immutable-context history is linked or invalid")
        os.chmod(history, 0o700)
        for label, previous in (("context", old_context), ("binding", old_binding)):
            if previous is not None:
                digest_prefix = digest(previous)[:16]
                private_write(history / f"{label}-{digest_prefix}.json", previous)
    if old_context is None:
        private_write(context_path, context_bytes)
    else:
        _replace_private(context_path, context_bytes)
    if old_binding is None:
        private_write(binding_path, binding_bytes)
    else:
        _replace_private(binding_path, binding_bytes)


def load_module(path: pathlib.Path, name: str):
    if path.is_symlink() or not path.is_file():
        refuse(f"{name} is missing or linked")
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        refuse(f"{name} cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        sys.modules.pop(name, None)
        raise
    return module


def add_binding(bindings: dict[str, list[dict[str, str]]], role: str,
                path: pathlib.Path, seen: set[str]) -> None:
    try:
        relative = repo_relative(path)
    except Refusal:
        refuse(f"{role} binding cannot be represented inside the repository context")
    if relative in seen:
        refuse(f"{role} binding duplicates an earlier qualified context path")
    seen.add(relative)
    bindings[role].append({"path": relative, "sha256": file_digest(path)})


def row_path(row: object, label: str) -> pathlib.Path:
    if not isinstance(row, dict) or not isinstance(row.get("path"), str):
        refuse(f"{label} path record is malformed")
    path = pathlib.Path(row["path"])
    if not path.is_absolute():
        path = ROOT / path
    path = path.absolute()
    file_digest(path)
    return path


def private_artifact(root: pathlib.Path, row: object, label: str) -> tuple[pathlib.Path, bytes, str]:
    if not isinstance(row, dict) or set(row) != {"path", "sha256"} or not isinstance(row.get("path"), str):
        refuse(f"{label} artifact record is malformed")
    relative = pathlib.PurePosixPath(row["path"])
    if relative.is_absolute() or any(part in {"", ".", ".."} for part in relative.parts):
        refuse(f"{label} artifact path escapes its private root")
    path = root.joinpath(*relative.parts)
    if path.is_symlink() or not path.is_file():
        refuse(f"{label} artifact is missing or linked")
    data = path.read_bytes()
    expected = row.get("sha256")
    if digest(data) != expected:
        refuse(f"{label} artifact differs from its accepted build seal")
    return path, data, expected


def accepted_cpu_image() -> tuple[dict, dict, dict, pathlib.Path, bytes, pathlib.Path]:
    qualification_path = CPU_PRIVATE / "qualification.json"
    qualified_path = CPU_PRIVATE / "qualified-build.json"
    qualification_bytes = qualification_path.read_bytes()
    qualified_bytes = qualified_path.read_bytes()
    qualification = json.loads(qualification_bytes)
    qualified = json.loads(qualified_bytes)
    verifier = subprocess.run(
        [sys.executable, "-B", "scripts/verify-cpu-diagnostic-qualification.py",
         str(qualification_path), str(ROOT)], cwd=ROOT,
        env={**os.environ, "CI": "", "T64_REQUIRE_FIXTURES": "0",
             "PYTHONDONTWRITEBYTECODE": "1"}, stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120, check=False,
    )
    if verifier.returncode != 0:
        refuse("accepted CPU source/tool/asset qualification no longer passes")
    if (qualified.get("schema") != "t64.cpu-qualified-build" or
            qualified.get("schema_version") not in (1, 2) or
            qualified.get("status") != "qualified" or
            qualified.get("adapter", {}).get("path") != CPU_ADAPTER_RELATIVE):
        refuse("accepted CPU image build seal is malformed")
    artifacts = qualified.get("artifacts")
    expected_artifact_keys = {"base_rom", "composition", "inventory", "qualification",
                              "source_closure", "source_projection"}
    if not isinstance(artifacts, dict) or set(artifacts) != expected_artifact_keys:
        refuse("accepted CPU image artifact set is incomplete")
    resolved = {key: private_artifact(CPU_PRIVATE, artifacts[key], key)
                for key in expected_artifact_keys}
    if (resolved["qualification"][1] != qualification_bytes or
            resolved["source_projection"][0] != PROJECTION):
        refuse("CPU image receipts do not reference the accepted source/tool records")
    image_path, image_bytes, image_sha = resolved["base_rom"]
    composition = json.loads(resolved["composition"][1])
    source_closure = json.loads(resolved["source_closure"][1])
    projection = json.loads(resolved["source_projection"][1])
    selector = qualified.get("selector")
    if (not isinstance(selector, dict) or
            selector.get("encoding") != "registration-index-u16be:value-index-u16be" or
            selector.get("width") != 4 or type(selector.get("rom_offset")) is not int or
            selector["rom_offset"] < 0 or selector["rom_offset"] + 4 > len(image_bytes) or
            image_bytes[selector["rom_offset"]:selector["rom_offset"] + 4] != bytes(4)):
        refuse("accepted CPU selector composition is malformed")
    inventory = json.loads(resolved["inventory"][1])
    inventory_identity = inventory.get("identity") if isinstance(inventory, dict) else None
    source = qualification.get("source")
    source_projection = source.get("projection") if isinstance(source, dict) else None
    if (not isinstance(inventory_identity, dict) or
            not isinstance(source_projection, dict) or
            set(source_projection) != {"path", "sha256", "projection_sha256"} or
            source_projection.get("path") != "build/private-cpu-diagnostic/source-projection.json" or
            source_projection.get("sha256") != digest(resolved["source_projection"][1]) or
            source_projection.get("projection_sha256") != projection.get("projection_sha256") or
            composition.get("schema") != "t64.cpu-build-composition" or
            composition.get("schema_version") != 1 or
            composition.get("status") != "PASS_POSTBUILD_COMPOSITION" or
            composition.get("guest_execution_verified") is not False or
            composition.get("rom_sha256") != image_sha or
            composition.get("qualification_sha256") != resolved["qualification"][2] or
            composition.get("source_closure_sha256") != resolved["source_closure"][2] or
            not isinstance(composition.get("layout"), dict) or
            composition["layout"].get("selector_rom_offset") != selector["rom_offset"] or
            composition["layout"].get("selector_bytes") != selector["width"] or
            source_closure.get("status") != "PASS_PREBUILD_SOURCE" or
            source_closure.get("source_projection_sha256") != projection.get("projection_sha256") or
            source_closure.get("source_revision") != projection.get("source_revision") or
            inventory_identity.get("base_rom_sha256") != image_sha or
            inventory_identity.get("tool_receipt_sha256") != resolved["qualification"][2] or
            inventory_identity.get("source_revision") != qualification.get("source_revision") or
            source.get("overlay_sha256") != inventory_identity.get("overlay_sha256")):
        refuse("accepted CPU source image and post-build composition do not match")
    asset_closure = row_path(qualification["assets"]["closure_report"], "asset closure")
    if (asset_closure != resolved["source_closure"][0] or
            file_digest(asset_closure) != resolved["source_closure"][2]):
        refuse("accepted source and asset closure roles do not name the same reviewed report")

    return (qualification, qualified, inventory, image_path, image_bytes,
            resolved["composition"][0])


def accepted_rsp_inputs(cpu_projection_bytes: bytes,
                        source_revision: str) -> tuple[dict, dict, dict, pathlib.Path]:
    for path in (RSP_PROJECTION, RSP_QUALIFICATION, RSP_OVERLAY):
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 32 * 1024 * 1024:
            refuse("accepted RSP source inputs are missing, linked, or oversized")
    qualification = json.loads(RSP_QUALIFICATION.read_bytes())
    if (not isinstance(qualification, dict) or
            qualification.get("schema") != "t64.rsp-qualification.v1" or
            qualification.get("status") != "qualified" or
            qualification.get("source_revision") != source_revision):
        refuse("accepted RSP qualification targets a different source revision")
    closure_ref = qualification.get("closure")
    closure_path = row_path(closure_ref, "RSP source closure")
    if closure_path != RSP_PRIVATE / "rsp-preuse-closure.json":
        refuse("RSP qualification does not use the fixed reviewed source closure")
    closure_bytes = closure_path.read_bytes()
    if digest(closure_bytes) != closure_ref.get("sha256"):
        refuse("accepted RSP source closure differs from its qualification")
    closure = json.loads(closure_bytes)
    projection_bytes = RSP_PROJECTION.read_bytes()
    rsp_projection = json.loads(projection_bytes)
    overlay = json.loads(RSP_OVERLAY.read_bytes())
    membership = qualification.get("membership")
    inputs = rsp_projection.get("inputs") if isinstance(rsp_projection, dict) else None
    closure_membership = closure.get("membership") if isinstance(closure, dict) else None
    closure_overlay = closure.get("overlay") if isinstance(closure, dict) else None
    if (not isinstance(membership, dict) or
            membership.get("path") != RSP_PROJECTION.relative_to(ROOT).as_posix() or
            membership.get("sha256") != digest(projection_bytes) or
            not isinstance(rsp_projection, dict) or
            rsp_projection.get("source_revision") != source_revision or
            not isinstance(inputs, dict) or
            inputs.get("cpu_source_projection_sha256") !=
                digest(cpu_projection_bytes) or
            not isinstance(closure, dict) or
            closure.get("status") != "PASS_PREBUILD_RSP" or
            not isinstance(closure_membership, dict) or
            closure_membership.get("path") != membership.get("path") or
            closure_membership.get("sha256") != membership.get("sha256") or
            not isinstance(overlay, dict) or
            overlay.get("schema") != "t64.rsp-overlay-preparation.v1" or
            overlay.get("source_revision") != source_revision or
            not isinstance(closure_overlay, dict) or
            closure_overlay.get("checkout") != overlay.get("checkout") or
            closure_overlay.get("sha256") != overlay.get("overlay_sha256")):
        refuse("RSP projection, overlay, and source closure do not form one accepted input set")
    verifier = subprocess.run(
        [sys.executable, "-B", "scripts/verify-rsp-diagnostic-qualification.py",
         "--verify-source-closure", str(RSP_QUALIFICATION)],
        cwd=ROOT, env={**os.environ, "CI": "", "T64_REQUIRE_FIXTURES": "0",
                       "PYTHONDONTWRITEBYTECODE": "1"},
        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=120, check=False,
    )
    if verifier.returncode != 0:
        refuse("accepted RSP source/tool/asset closure no longer passes")
    if (digest(RSP_PROJECTION.read_bytes()) != membership["sha256"] or
            digest(RSP_QUALIFICATION.read_bytes()) != digest(canonical(qualification)) or
            digest(closure_path.read_bytes()) != closure_ref["sha256"] or
            digest(RSP_OVERLAY.read_bytes()) != digest(canonical(overlay))):
        refuse("RSP source inputs changed during their pre-use verification")
    overlay_root_text = overlay.get("checkout")
    if not isinstance(overlay_root_text, str):
        refuse("accepted RSP overlay checkout path is malformed")
    overlay_root = ROOT / pathlib.PurePosixPath(overlay_root_text)
    try:
        overlay_root.relative_to(RSP_PRIVATE)
    except ValueError:
        refuse("accepted RSP overlay checkout escapes its private root")
    if overlay_root.is_symlink() or not overlay_root.is_dir():
        refuse("accepted RSP overlay checkout is missing or linked")
    return rsp_projection, qualification, closure, overlay_root


def run_private_command(command: list[str], work: pathlib.Path, label: str,
                        *, cwd: pathlib.Path = ROOT,
                        env: dict[str, str] | None = None,
                        timeout: int = 60) -> bytes:
    started = time.monotonic()
    stdout_path, stderr_path = work / f"{label}.stdout.log", work / f"{label}.stderr.log"
    out_fd = os.open(stdout_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                     getattr(os, "O_NOFOLLOW", 0), 0o600)
    try:
        err_fd = os.open(stderr_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                         getattr(os, "O_NOFOLLOW", 0), 0o600)
    except BaseException:
        os.close(out_fd)
        raise
    with os.fdopen(out_fd, "wb") as out, os.fdopen(err_fd, "wb") as err:
        process = subprocess.Popen(
            command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
            stdout=out, stderr=err, start_new_session=True,
        )
        timed_out = False
        deadline = started + timeout
        try:
            while os.waitid(os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
                if time.monotonic() >= deadline:
                    timed_out = True
                    break
                time.sleep(min(0.01, max(0.0, deadline - time.monotonic())))
            timed_out = timed_out or time.monotonic() >= deadline
        finally:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except (ProcessLookupError, PermissionError):
                pass
            finally:
                exit_code = process.wait(timeout=1)
        group_empty = False
        cleanup_deadline = time.monotonic() + 1
        while time.monotonic() < cleanup_deadline:
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                group_empty = True
                break
            except PermissionError:
                pass
            time.sleep(0.01)
    private_write(work / f"{label}.command.json", canonical({
        "argv": command, "cwd": str(cwd), "exit": exit_code,
        "timed_out": timed_out, "process_group_empty": group_empty,
        "seconds": round(time.monotonic() - started, 3),
    }))
    if timed_out or exit_code != 0 or not group_empty:
        refuse(f"combined BASE {label} failed; inspect the private build attempt")
    return stdout_path.read_bytes()


def build_combined_image(cpu_qualification: dict, projection: dict,
                         projection_bytes: bytes, source_closure: dict,
                         source_audit: dict, rsp_projection: dict,
                         rsp_qualification: dict, rsp_closure: dict,
                         rsp_overlay_root: pathlib.Path,
                         source_closure_sha256: str) -> tuple[pathlib.Path, pathlib.Path]:
    overlay_builder = load_module(
        ROOT / "scripts/apply-base-diagnostic-overlay.py", "t64_base_overlay")
    projection = overlay_builder.checked_projection(projection)
    cpu_revision = projection["source_revision"]
    if rsp_projection.get("source_revision") != cpu_revision:
        refuse("CPU and RSP source projections do not share a revision")
    work = pathlib.Path(tempfile.mkdtemp(prefix="combined-build-", dir=PRIVATE))
    os.chmod(work, 0o700)
    combined_source = work / "source"
    selected, overlay = overlay_builder.apply_combined_overlay(
        CPU_SOURCE, rsp_overlay_root, source_closure, rsp_closure,
        projection, rsp_projection, source_audit, BASE_SOURCE,
        combined_source)

    cpu_modules = source_closure.get("module_files")
    rsp_files = rsp_closure.get("overlay", {}).get("files")
    if not isinstance(cpu_modules, list) or not isinstance(rsp_files, list):
        refuse("accepted CPU/RSP source module closures are incomplete")
    cpu_file_hashes = {
        row.get("path"): row.get("sha256") for row in cpu_modules
        if isinstance(row, dict) and isinstance(row.get("path"), str)
    }
    rsp_file_hashes = {
        row.get("path"): row.get("sha256") for row in rsp_files
        if isinstance(row, dict) and isinstance(row.get("path"), str)
    }
    for member in selected:
        source_path = str(member["source_file"])
        expected = str(member["source_file_sha256"])
        known = rsp_file_hashes.get(source_path) if member["group"] == "rsp" else cpu_file_hashes.get(source_path)
        if known != expected:
            refuse("selected BASE member source is absent from its accepted component closure")

    tools = cpu_qualification["tools"]
    compiler = tools["host_c_compiler"]
    objcopy = next((row for row in tools["target_binutils"]
                    if row.get("name") == "objcopy"), None)
    objdump = next((row for row in tools["target_binutils"]
                    if row.get("name") == "objdump"), None)
    if not isinstance(objcopy, dict) or not isinstance(objdump, dict):
        refuse("accepted BASE source closure has no audited object tools")
    for row in (compiler, objcopy, objdump):
        path = pathlib.Path(str(row.get("path", "")))
        if file_digest(path) != row.get("sha256"):
            refuse("an accepted CPU/RSP build tool changed")

    prefix = CPU_PRIVATE / "toolchain-prefix"
    runtime = (
        (prefix / "lib/rustlib/aarch64-apple-darwin/bin/rust-lld",
         "6f44b61e91d6d7b6ba80bb75391587bb4fa832b248281bd67d519516cde43f98"),
        (prefix / "lib/libLLVM.dylib",
         "216146d4f374dea0eb0df654049025f82454aaa147af6d8388e5b37ff54084e1"),
    )
    for path, expected in runtime:
        if file_digest(path) != expected:
            refuse("audited Rust compiler runtime changed before the combined build")

    project_lock = combined_source / "Cargo.lock"
    sysroot_lock = prefix / "lib/rustlib/src/rust/library/Cargo.lock"
    lock_hashes = [file_digest(project_lock), file_digest(sysroot_lock)]
    env = {
        "PATH": str(prefix / "bin") + ":/usr/bin:/bin:/usr/sbin:/sbin",
        "HOME": os.environ.get("HOME", ""),
        "TMPDIR": str(work / "tmp"),
        "CARGO_HOME": str(CPU_PRIVATE / "cargo-home-attempt-03"),
        "CARGO_TARGET_DIR": str(work / "cargo-target"),
        "RUSTC": str(prefix / "bin/rustc"), "LC_ALL": "C", "LANG": "C",
        "DYLD_LIBRARY_PATH": str(prefix / "lib"),
        "PYTHONDONTWRITEBYTECODE": "1",
    }
    for private_dir in (pathlib.Path(env["TMPDIR"]),):
        private_dir.mkdir(mode=0o700, parents=True, exist_ok=False)
    run_private_command(
        ["sh", "scripts/build-cpu-diagnostic.sh", "--verify-toolchain"],
        work, "toolchain", timeout=120,
    )
    cargo = prefix / "bin/cargo"
    run_private_command(
        [str(cargo), "--config", 'profile.release.panic="abort"', "build",
         "--jobs", "4", "--release", "--target", "mips-nintendo64-none.json",
         "--offline", "--locked", "--frozen"], work, "cargo",
        cwd=combined_source, env=env, timeout=600,
    )
    if lock_hashes != [file_digest(project_lock), file_digest(sysroot_lock)]:
        refuse("combined BASE build changed a locked resolution")
    elf = pathlib.Path(env["CARGO_TARGET_DIR"]) / "mips-nintendo64-none/release/n64-systemtest"
    if elf.is_symlink() or not elf.is_file():
        refuse("combined BASE Cargo build produced no bounded target ELF")
    elf_bytes = elf.read_bytes()
    payload = work / "base.payload.bin"
    rom = work / "base.z64"
    run_private_command(
        [str(objcopy["path"]), "-O", "binary", str(elf), str(payload)],
        work, "objcopy", timeout=60,
    )
    if file_digest(pathlib.Path(objcopy["path"])) != objcopy["sha256"]:
        refuse("accepted objcopy changed during the combined build")

    packer = work / "packer"
    run_private_command(
        [str(compiler["path"]), "-std=c17", "-Wall", "-Wextra", "-Werror",
         "-Wconversion", "-Wshadow", "-Wstrict-prototypes", "-pedantic",
         "-Itools/common", "tools/fixture/t64_rom_pack.c",
         "tools/common/t64_sha256.c", "-o", str(packer)],
        work, "packer-build", timeout=60,
    )
    run_private_command([str(packer), "--selftest"], work, "packer-controls")
    boot = ROOT / "build/fixtures/ipl3_compat.z64"
    run_private_command(
        [str(packer), "--pack", "--cpu-profile", "--boot", str(boot),
         "--payload", str(payload), "--output", str(rom), "--entry",
         "0x80000400", "--clock", "0x0000000f", "--crc2", "0x00000000"],
        work, "pack", timeout=60,
    )
    objdump_text = run_private_command(
        [str(objdump["path"]), "-f", "-h", "-t", str(elf)],
        work, "objdump", timeout=60,
    ).decode("utf-8", errors="strict")
    cpu_build_verifier = load_module(
        ROOT / "scripts/verify-cpu-diagnostic-build.py", "t64_base_cpu_composition")
    payload_bytes, rom_bytes = payload.read_bytes(), rom.read_bytes()
    boot_bytes = boot.read_bytes()
    font = combined_source / "src/graphics/system_font.rs"
    if font.is_symlink() or not font.is_file():
        refuse("combined BASE asset exclusion baseline is missing")
    font_bytes = font.read_bytes()
    layout = cpu_build_verifier.verify_layout(elf_bytes, payload_bytes, objdump_text)
    cpu_build_verifier.verify_composition(
        elf_bytes, payload_bytes, rom_bytes, boot_bytes, font_bytes)
    if lock_hashes != [file_digest(project_lock), file_digest(sysroot_lock)]:
        refuse("combined BASE locks changed during composition inspection")

    selector_offset = layout.get("selector_rom_offset")
    if (type(selector_offset) is not int or selector_offset < 0 or
            selector_offset + 4 > len(rom_bytes) or
            rom_bytes[selector_offset:selector_offset + 4] != bytes(4)):
        refuse("combined BASE ROM does not contain one zeroed four-byte selector")
    selected_image_rows = []
    for member in selected:
        registration_index = int(member["registration_index"])
        value_index = int(member["value_index"])
        selector_bytes = overlay_builder.selector_value(registration_index, value_index)
        selected_image_rows.append({
            "group": member["group"], "lane": member["lane"],
            "level": member["level"],
            "registration_index": registration_index,
            "value_index": value_index,
            "selector_hex": selector_bytes.hex(),
            "selected_rom_sha256": overlay_builder.selected_rom_sha256(
                rom_bytes, selector_offset, registration_index, value_index),
        })
    composition = {
        "schema": "t64.base-combined-image",
        "schema_version": 1,
        "status": "PASS_POSTBUILD_COMPOSITION",
        "guest_execution_verified": False,
        "source_revision": cpu_revision,
        "base_rom": {"path": "build/private-base-diagnostic/base.z64",
                     "sha256": digest(rom_bytes)},
        "selector": {
            "encoding": "registration-index-u16be:value-index-u16be",
            "width": 4, "rom_offset": selector_offset,
        },
        "source_bindings": {
            "cpu_source_projection_sha256": digest(projection_bytes),
            "rsp_source_projection_sha256": digest(RSP_PROJECTION.read_bytes()),
            "cpu_source_closure_sha256": source_closure_sha256,
            "rsp_source_closure_sha256": rsp_qualification["closure"]["sha256"],
            "combined_overlay_sha256": digest(canonical(overlay)),
            "boot_sha256": digest(boot_bytes),
        },
        "combined_overlay": overlay,
        "members": selected_image_rows,
    }
    private_write(BASE_IMAGE, rom_bytes)
    private_write(BASE_COMPOSITION, canonical(composition))
    return BASE_IMAGE, BASE_COMPOSITION


def qualify() -> None:
    os.umask(0o077)
    if os.environ.get("CI", "").lower() in {"1", "true", "yes"} or os.environ.get("T64_REQUIRE_FIXTURES") == "1":
        refuse("qualified BASE context creation is local-only")
    if not PRIVATE.exists():
        PRIVATE.mkdir(mode=0o700, parents=True)
    if PRIVATE.is_symlink() or not PRIVATE.is_dir():
        refuse("private BASE root is missing or linked")
    os.chmod(PRIVATE, 0o700)
    if stat.S_IMODE(PRIVATE.stat(follow_symlinks=False).st_mode) != 0o700:
        refuse("private BASE root permissions are not owner-only")

    (qualification, qualified, cpu_inventory, _cpu_image_path, _cpu_image_bytes,
     _cpu_composition_path) = accepted_cpu_image()
    qualification_path = CPU_PRIVATE / "qualification.json"
    qualified_path = CPU_PRIVATE / "qualified-build.json"
    cpu_runner = load_module(ROOT / "scripts/run-cpu-diagnostic.py", "t64_cpu_image_selector")
    projection_bytes = PROJECTION.read_bytes()
    projection = json.loads(projection_bytes)
    if (projection.get("schema") != "t64.cpu-source-projection" or
            projection.get("schema_version") != 2 or
            not HEX40.fullmatch(str(projection.get("source_revision", "")))):
        refuse("accepted CPU source projection is malformed")
    revision = projection["source_revision"]
    rsp_projection, rsp_qualification, rsp_closure, rsp_overlay_root = accepted_rsp_inputs(
        projection_bytes, revision)

    derive = subprocess.run(
        [sys.executable, "-B", "scripts/derive-base-diagnostic-source.py", "--derive"],
        cwd=ROOT, env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=120, check=False,
    )
    if derive.returncode != 0:
        refuse("source projection did not produce the private BASE denominator")
    authority_path = PRIVATE / "source-authority.json"
    inventory_path = PRIVATE / "inventory.json"
    authority_bytes = authority_path.read_bytes()
    inventory_bytes = inventory_path.read_bytes()
    source_closure_data = (CPU_PRIVATE / pathlib.PurePosixPath(
        qualified["artifacts"]["source_closure"]["path"])).read_bytes()
    source_closure_sha256 = digest(source_closure_data)
    if source_closure_sha256 != qualified["artifacts"]["source_closure"]["sha256"]:
        refuse("accepted CPU source closure differs from its build seal")
    source_closure = json.loads(source_closure_data)
    source_audit_path = CPU_PRIVATE / "source-audit-full.json"
    if source_audit_path.is_symlink() or not source_audit_path.is_file():
        refuse("accepted CPU source audit is missing or linked")
    source_audit = json.loads(source_audit_path.read_bytes())
    source_closure_copy = PRIVATE / f"source-image-source-closure-{digest(source_closure_data)[:16]}.json"
    private_write(source_closure_copy, source_closure_data)
    base_members = json.loads(authority_bytes).get("members")
    cpu_members = cpu_runner.required_members(cpu_inventory)
    first_base = next((member for member in base_members
                       if member.get("required") is True and
                       member.get("lane") == "required-base" and
                       member.get("execution") in {"once", "values"}), None)
    if (first_base is None or not cpu_members or
            first_base.get("index") != cpu_members[0].get("registration_index") or
            first_base.get("value_index") != cpu_members[0].get("value_index")):
        refuse("first required BASE member is absent from the accepted source-image selector")
    authority_sha = digest(authority_bytes)
    for path in (authority_path, inventory_path):
        if path.is_symlink() or stat.S_IMODE(path.stat(follow_symlinks=False).st_mode) != 0o600:
            refuse("derived BASE authority or inventory is not owner-only")

    image_path, composition_path = build_combined_image(
        qualification, projection, projection_bytes, source_closure,
        source_audit, rsp_projection, rsp_qualification, rsp_closure,
        rsp_overlay_root, source_closure_sha256,
    )

    adapter_sha = file_digest(BASE_ADAPTER)
    identity_run = subprocess.run(
        [str(BASE_ADAPTER), "--runtime-identity-json"], cwd=ROOT,
        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=10, check=False,
    )
    if identity_run.returncode != 0 or file_digest(BASE_ADAPTER) != adapter_sha:
        refuse("Tier B/T1 BASE adapter identity could not be authenticated")
    try:
        identity = json.loads(identity_run.stdout)
    except (UnicodeError, json.JSONDecodeError):
        refuse("Tier B/T1 BASE adapter identity is malformed")
    if (not isinstance(identity, dict) or
            set(identity) != {"core_build_identity", "profile_id", "profile_sha256", "source_revision"} or
            identity.get("profile_id") != PROFILE_ID or
            not HEX40.fullmatch(str(identity.get("source_revision", ""))) or
            not HEX64.fullmatch(str(identity.get("profile_sha256", ""))) or
            not re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", str(identity.get("core_build_identity", "")))):
        refuse("BASE adapter is not the pinned Tier B/T1 target")

    identity_bytes = canonical(identity)
    identity_path = PRIVATE / f"runtime-identity-{digest(identity_bytes)[:16]}.json"
    private_write(identity_path, identity_bytes)
    adapter_inventory = subprocess.run(
        [str(BASE_ADAPTER), "--inventory", str(authority_path), authority_sha,
         str(inventory_path)], cwd=ROOT, stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10, check=False,
    )
    if adapter_inventory.returncode != 0 or file_digest(BASE_ADAPTER) != adapter_sha:
        refuse("C BASE inventory authority rejected the derived denominator")

    bindings = {name: [] for name in (
        "asset", "boot", "build", "compiler", "composition", "executable",
        "image", "lock", "profile", "runtime", "source", "tool", "verifier",
        "source_verifier", "rsp_qualification", "rsp_source_closure",
        "rsp_overlay",
    )}
    seen: set[str] = set()
    add_binding(bindings, "executable", BASE_ADAPTER, seen)
    add_binding(bindings, "profile", identity_path, seen)
    add_binding(bindings, "asset", row_path(qualification["assets"]["closure_report"], "asset closure"), seen)
    add_binding(bindings, "boot", ROOT / "core/t64_machine.c", seen)
    add_binding(bindings, "boot", ROOT / "runner/t64_cold_boot.c", seen)
    add_binding(bindings, "build", ROOT / "scripts/run-base-diagnostic.py", seen)
    add_binding(bindings, "build", ROOT / "scripts/run-cpu-diagnostic.py", seen)
    add_binding(bindings, "build", ROOT / "scripts/build-qualified-base-diagnostic.py", seen)
    add_binding(bindings, "build", ROOT / "scripts/derive-base-diagnostic-source.py", seen)
    add_binding(bindings, "build", ROOT / "scripts/apply-base-diagnostic-overlay.py", seen)
    add_binding(bindings, "build", ROOT / "scripts/prepare-rsp-diagnostic.py", seen)
    add_binding(bindings, "build", ROOT / "scripts/verify-rsp-diagnostic-qualification.py", seen)
    add_binding(bindings, "build", ROOT / "scripts/build-cpu-diagnostic.sh", seen)
    add_binding(bindings, "build", ROOT / "scripts/build-qualified-cpu-diagnostic.py", seen)
    add_binding(bindings, "build", ROOT / "runner/t64_base_diagnostic.c", seen)
    add_binding(bindings, "build", ROOT / "runner/CMakeLists.txt", seen)
    add_binding(bindings, "build", ROOT / "CMakePresets.json", seen)
    add_binding(bindings, "build", qualified_path, seen)
    add_binding(bindings, "build", CPU_PRIVATE / pathlib.PurePosixPath(
        qualified["artifacts"]["inventory"]["path"]), seen)
    # The source verifier checks the externally located host compiler's exact
    # receipt; the immutable BASE context binds the in-repository build policy.
    add_binding(bindings, "compiler", ROOT / "CMakeLists.txt", seen)
    for row in qualification["tools"]["target_binutils"]:
        add_binding(bindings, "tool", row_path(row, "target tool"), seen)
    add_binding(bindings, "lock", row_path(qualification["project"]["lockfile"], "project lock"), seen)
    add_binding(bindings, "lock", row_path(qualification["project"]["manifest"], "project manifest"), seen)
    for row in qualification["toolchain"]["archives"]:
        path = row_path(row["archive"], "runtime archive")
        if repo_relative(path) not in seen:
            add_binding(bindings, "runtime", path, seen)
    for path in (
        row_path(qualification["toolchain"]["manifest"], "toolchain manifest"),
        qualification_path,
        source_closure_copy,
    ):
        if repo_relative(path) not in seen:
            add_binding(bindings, "runtime", path, seen)
    artifacts = qualified.get("artifacts")
    if not isinstance(artifacts, dict):
        refuse("accepted CPU image artifacts are missing")
    add_binding(bindings, "image", image_path, seen)
    add_binding(bindings, "composition", composition_path, seen)
    add_binding(bindings, "rsp_qualification", RSP_QUALIFICATION, seen)
    add_binding(bindings, "rsp_source_closure", row_path(
        rsp_qualification["closure"], "RSP source closure"), seen)
    add_binding(bindings, "rsp_overlay", RSP_OVERLAY, seen)
    add_binding(bindings, "verifier", ROOT / "scripts/verify-base-diagnostic-qualification.py", seen)
    add_binding(bindings, "source_verifier", ROOT / "scripts/verify-cpu-diagnostic-qualification.py", seen)

    reviewed = projection.get("reviewed_source_file_sha256")
    if not isinstance(reviewed, dict):
        refuse("accepted projection has no source file partition")
    source_root = CPU_PRIVATE / "nemu64-test"
    for relative in sorted(reviewed):
        source_path = source_root / pathlib.PurePosixPath(relative)
        if file_digest(source_path) != reviewed[relative]:
            refuse("accepted source checkout differs from its projection")
        add_binding(bindings, "source", source_path, seen)
    testlist = source_root / "src/tests/testlist.rs"
    if "src/tests/testlist.rs" not in reviewed:
        add_binding(bindings, "source", testlist, seen)

    context = {
        "bindings": bindings,
        "inventory": {"path": "build/private-base-diagnostic/inventory.json",
                      "sha256": digest(inventory_bytes)},
        "profile": "tier-b-t1",
        "schema": "t64.base-qualification-context",
        "schema_version": 2,
        "source_authority": {"path": "build/private-base-diagnostic/source-authority.json",
                             "sha256": authority_sha},
        "source_projection": {"path": "build/private-cpu-diagnostic/source-projection.json",
                              "sha256": file_digest(PROJECTION)},
        "rsp_source_projection": {
            "path": "build/private-rsp-diagnostic/rsp-source-projection.json",
            "sha256": file_digest(RSP_PROJECTION),
        },
        "source_revision": revision,
    }
    context_bytes = canonical(context)
    binding = {
        "context_sha256": digest(context_bytes),
        "inventory_sha256": context["inventory"]["sha256"],
        "schema": "t64.base-qualification-binding",
        "schema_version": 1,
        "source_authority_sha256": authority_sha,
        "source_projection_sha256": context["source_projection"]["sha256"],
    }
    binding_bytes = canonical(binding)
    retain_execution_snapshot(ROOT, PRIVATE, context_bytes, binding_bytes)
    install_context_pair(context_bytes, binding_bytes)


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] != "--qualify-build":
        print("usage: build-qualified-base-diagnostic.py --qualify-build", file=sys.stderr)
        return 2
    try:
        qualify()
    except (OSError, Refusal, subprocess.SubprocessError, KeyError, TypeError, ValueError) as exc:
        print(f"BASE qualification refused: {exc}", file=sys.stderr)
        return 4
    print("PASS: private Tier B/T1 BASE context qualified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

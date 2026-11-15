#!/usr/bin/env python3
"""Seal a local Tier B/T1 BASE context from the accepted source projection."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import re
import stat
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-base-diagnostic"
CPU_PRIVATE = ROOT / "build/private-cpu-diagnostic"
CPU_ADAPTER_RELATIVE = "build/gate/runner/t64-cpu-diagnostic"
BASE_ADAPTER = ROOT / "build/gate/runner/t64-base-diagnostic"
PROJECTION = ROOT / "build/private-cpu-diagnostic/source-projection.json"
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

    (qualification, qualified, cpu_inventory, image_path, image_bytes,
     composition_path) = accepted_cpu_image()
    qualification_path = CPU_PRIVATE / "qualification.json"
    qualified_path = CPU_PRIVATE / "qualified-build.json"
    cpu_runner = load_module(ROOT / "scripts/run-cpu-diagnostic.py", "t64_cpu_image_selector")
    projection = json.loads(PROJECTION.read_bytes())
    if (projection.get("schema") != "t64.cpu-source-projection" or
            projection.get("schema_version") != 2 or
            not HEX40.fullmatch(str(projection.get("source_revision", "")))):
        refuse("accepted CPU source projection is malformed")
    revision = projection["source_revision"]

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
        "source_verifier",
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
        "schema_version": 1,
        "source_authority": {"path": "build/private-base-diagnostic/source-authority.json",
                             "sha256": authority_sha},
        "source_projection": {"path": "build/private-cpu-diagnostic/source-projection.json",
                              "sha256": file_digest(PROJECTION)},
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

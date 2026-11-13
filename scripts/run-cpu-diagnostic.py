#!/usr/bin/env python3
"""Run the private CPU value set through the production local adapter."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import selectors
import signal
import stat
import subprocess
import sys
import time
from dataclasses import dataclass

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-cpu-diagnostic"
MAX_INVENTORY = 1_048_576
MAX_METADATA = 16 * 1024 * 1024
MAX_ROM = 0x00401000
MAX_ADAPTER_BYTES = 512 * 1024 * 1024
MAX_GUEST_OUTPUT = 16 * 1024
MAX_GROUP_REPORT = 64 * 1024 * 1024
TICK_LIMIT = 10_000_000
RUN_TIMEOUT = 12
GROUPS = (
    "address_error_exception", "arithmetic", "cache", "cop0", "cop1",
    "cop_unusable", "endian_re", "exception_instructions", "jumps",
    "overflow_exception", "privilege", "tlb", "tlb64", "traps",
)
HEX40 = re.compile(r"[0-9a-f]{40}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
SELECTOR = re.compile(r"r([0-9]{4})v([0-9]{5})\Z")
ADAPTER_REPO_PATH = "build/gate/runner/t64-cpu-diagnostic"


class Refusal(ValueError):
    pass


@dataclass
class Inputs:
    private_root: pathlib.Path
    adapter: pathlib.Path
    adapter_path: str
    adapter_sha256: str
    qualified_build_path: str
    qualified_build_sha256: str
    inventory_path: str
    inventory_sha256: str
    inventory_bytes: bytes
    inventory: dict
    base_bytes: bytes
    selector_offset: int


def fail(message: str) -> None:
    raise Refusal(message)


def no_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail("metadata contains a duplicate JSON key")
        result[key] = value
    return result


def parse_json(data: bytes, label: str):
    try:
        return json.loads(data, object_pairs_hook=no_duplicate_keys)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        fail(f"{label} is malformed JSON: {error}")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def digest_regular_file(path: pathlib.Path, maximum: int = MAX_ADAPTER_BYTES) -> str:
    path = pathlib.Path(path)
    before = path.lstat()
    if path.is_symlink() or not stat.S_ISREG(before.st_mode) or before.st_size <= 0 or before.st_size > maximum:
        fail("adapter is not a bounded regular file")
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
    hasher = hashlib.sha256()
    try:
        opened = os.fstat(descriptor)
        if (opened.st_dev, opened.st_ino, opened.st_size) != (before.st_dev, before.st_ino, before.st_size):
            fail("adapter changed while opening")
        remaining = opened.st_size
        while remaining:
            block = os.read(descriptor, min(remaining, 1024 * 1024))
            if not block:
                fail("adapter ended while hashing")
            hasher.update(block)
            remaining -= len(block)
        after = os.fstat(descriptor)
        current = path.lstat()
        if (
            (opened.st_dev, opened.st_ino, opened.st_size, opened.st_mtime_ns, opened.st_ctime_ns)
            != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns)
            or (after.st_dev, after.st_ino) != (current.st_dev, current.st_ino)
            or stat.S_ISLNK(current.st_mode)
        ):
            fail("adapter changed while hashing")
    finally:
        os.close(descriptor)
    return hasher.hexdigest()


def repository_file(relative_text: object, *, executable: bool = False) -> pathlib.Path:
    if not isinstance(relative_text, str):
        fail("adapter repository path is malformed")
    relative = pathlib.PurePosixPath(relative_text)
    if relative.is_absolute() or not relative.parts or any(part in ("", ".", "..") for part in relative.parts):
        fail("adapter repository path is not canonical and relative")
    cursor = ROOT
    for part in relative.parts:
        cursor = cursor / part
        try:
            info = cursor.lstat()
        except OSError:
            fail("sealed adapter path is missing")
        if stat.S_ISLNK(info.st_mode):
            fail("sealed adapter path traverses a symlink")
    if not stat.S_ISREG(info.st_mode) or (executable and not os.access(cursor, os.X_OK)):
        fail("sealed adapter path is not an executable regular file")
    return cursor


def check_adapter_path(adapter: pathlib.Path, relative_path: str, expected_sha256: str) -> pathlib.Path:
    if relative_path != ADAPTER_REPO_PATH:
        fail("qualified build selects a different diagnostic adapter path")
    expected_path = repository_file(relative_path, executable=True)
    provided_path = pathlib.Path(os.path.abspath(adapter))
    if provided_path != expected_path:
        fail("requested adapter path differs from the qualified production adapter")
    if digest_regular_file(expected_path) != expected_sha256:
        fail("production adapter digest differs from the qualified build seal")
    return expected_path


def check_adapter(context: Inputs) -> None:
    seal_path = private_file(context.private_root / context.qualified_build_path, context.private_root)
    seal_bytes = read_bounded(seal_path, context.private_root, 1_048_576)
    if digest(seal_bytes) != context.qualified_build_sha256:
        fail("qualified build seal changed after input loading")
    seal = closed_object(
        parse_json(seal_bytes, "qualified build seal"),
        {"adapter", "artifacts", "schema", "schema_version", "selector", "status"},
        "qualified build seal",
    )
    adapter_record = closed_object(seal["adapter"], {"path", "sha256"}, "sealed adapter")
    if adapter_record != {"path": context.adapter_path, "sha256": context.adapter_sha256}:
        fail("qualified build adapter binding changed after input loading")
    adapter_path = repository_file(context.adapter_path, executable=True)
    if pathlib.Path(os.path.abspath(context.adapter)) != adapter_path:
        fail("loaded adapter path differs from the sealed path")
    if digest_regular_file(adapter_path) != context.adapter_sha256:
        fail("production adapter changed after input loading")


def check_hash(value: object, label: str, length: int = 64) -> str:
    pattern = HEX40 if length == 40 else HEX64
    if not isinstance(value, str) or pattern.fullmatch(value) is None:
        fail(f"{label} is not a lowercase SHA-256 identity")
    return value


def private_directory(path: pathlib.Path, boundary: pathlib.Path) -> pathlib.Path:
    path = pathlib.Path(os.path.abspath(path))
    try:
        relative = path.relative_to(boundary)
    except ValueError:
        fail("private path escapes the qualified diagnostic root")
    cursor = boundary
    if cursor.is_symlink():
        fail("private root traverses a symlink")
    for part in relative.parts:
        if part in ("", ".", ".."):
            fail("private path contains an unsafe component")
        cursor = cursor / part
        if cursor.is_symlink():
            fail("private path traverses a symlink")
    if not cursor.is_dir():
        fail("private directory is missing")
    mode = stat.S_IMODE(cursor.stat().st_mode)
    if cursor.stat().st_uid != os.geteuid() or mode & 0o077:
        fail("private directory is not owner-only")
    return cursor


def private_file(path: pathlib.Path, boundary: pathlib.Path, *, exists: bool = True) -> pathlib.Path:
    path = pathlib.Path(os.path.abspath(path))
    try:
        relative = path.relative_to(boundary)
    except ValueError:
        fail("private file escapes the qualified diagnostic root")
    if not relative.parts or any(part in ("", ".", "..") for part in relative.parts):
        fail("private file path contains an unsafe component")
    parent = private_directory(path.parent, boundary)
    candidate = parent / path.name
    if candidate.is_symlink():
        fail("private file is a symlink")
    if exists:
        info = candidate.stat()
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid() or stat.S_IMODE(info.st_mode) & 0o077:
            fail("private artifact is not a bounded owner-only regular file")
    elif candidate.exists():
        fail("refusing to overwrite an existing private output")
    return candidate


def read_bounded(path: pathlib.Path, boundary: pathlib.Path, maximum: int) -> bytes:
    path = private_file(path, boundary)
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    descriptor = os.open(path, flags)
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_size <= 0 or info.st_size > maximum:
            fail("private input is empty or exceeds its byte limit")
        chunks = []
        remaining = info.st_size
        while remaining:
            block = os.read(descriptor, min(remaining, 1024 * 1024))
            if not block:
                fail("private input changed or ended early")
            chunks.append(block)
            remaining -= len(block)
        if os.read(descriptor, 1):
            fail("private input changed while it was read")
        return b"".join(chunks)
    finally:
        os.close(descriptor)


def write_exclusive(path: pathlib.Path, boundary: pathlib.Path, data: bytes) -> None:
    path = private_file(path, boundary, exists=False)
    descriptor = os.open(
        path,
        os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0),
        0o600,
    )
    try:
        view = memoryview(data)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                fail("private output write failed")
            view = view[written:]
        os.fsync(descriptor)
    except BaseException:
        os.close(descriptor)
        try:
            path.unlink()
        except OSError:
            pass
        raise
    os.close(descriptor)


def replace_private(path: pathlib.Path, boundary: pathlib.Path, data: bytes) -> None:
    path = pathlib.Path(os.path.abspath(path))
    parent = private_directory(path.parent, boundary)
    if path.exists() or path.is_symlink():
        info = path.lstat()
        if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid():
            fail("current-run pointer is not an owned regular file")
    temporary = parent / f".{path.name}.{os.getpid()}.{time.time_ns()}.tmp"
    write_exclusive(temporary, boundary, data)
    os.replace(temporary, path)


def run_bounded(command, *, cwd: pathlib.Path, env: dict[str, str], timeout: int, maximum: int) -> tuple[int, bytes, bytes]:
    try:
        process = subprocess.Popen(
            [str(item) for item in command], cwd=cwd, env=env,
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            start_new_session=True,
        )
    except OSError as error:
        fail(f"bounded subprocess could not start: {error}")
    output = {process.stdout: bytearray(), process.stderr: bytearray()}
    deadline = time.monotonic() + timeout
    with selectors.DefaultSelector() as selector:
        for stream in output:
            os.set_blocking(stream.fileno(), False)
            selector.register(stream, selectors.EVENT_READ)
        while selector.get_map():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()
                fail("bounded subprocess timed out")
            for key, _ in selector.select(min(remaining, 0.25)):
                try:
                    block = os.read(key.fd, 65536)
                except BlockingIOError:
                    continue
                if not block:
                    selector.unregister(key.fileobj)
                    continue
                captured = output[key.fileobj]
                if len(captured) + len(block) > maximum:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait()
                    fail("bounded subprocess exceeded its capture limit")
                captured.extend(block)
    remaining = deadline - time.monotonic()
    try:
        code = process.wait(timeout=max(remaining, 0.0))
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait()
        fail("bounded subprocess timed out")
    return code, bytes(output[process.stdout]), bytes(output[process.stderr])


def invoke_adapter(context: Inputs, *arguments: str, timeout: int = 10,
                   maximum: int = 4096, controls: bool = False) -> tuple[int, bytes, bytes]:
    check_adapter(context)
    try:
        return run_bounded(
            [context.adapter, *arguments], cwd=ROOT, env=make_env(controls=controls),
            timeout=timeout, maximum=maximum,
        )
    finally:
        check_adapter(context)


def make_env(*, controls: bool = False) -> dict[str, str]:
    env = dict(os.environ)
    env["CI"] = ""
    env["T64_REQUIRE_FIXTURES"] = "0"
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    if controls:
        env["T64_CPU_DIAGNOSTIC_CONTROL_TEST"] = "1"
    return env


def refuse_public_execution() -> None:
    if os.environ.get("CI", "").lower() in {"true", "1"} or os.environ.get("T64_REQUIRE_FIXTURES") == "1":
        fail("private CPU diagnostics are refused in CI or required-fixture mode")


def closed_object(value, keys: set[str], label: str) -> dict:
    if not isinstance(value, dict) or set(value) != keys:
        fail(f"{label} fields do not match the closed schema")
    return value


def artifact_path(private_root: pathlib.Path, value: object, label: str) -> tuple[pathlib.Path, bytes, str]:
    row = closed_object(value, {"path", "sha256"}, label)
    relative_text = row["path"]
    if not isinstance(relative_text, str):
        fail(f"{label} path is malformed")
    relative = pathlib.PurePosixPath(relative_text)
    if relative.is_absolute() or not relative.parts or any(part in ("", ".", "..") for part in relative.parts):
        fail(f"{label} path escapes the private root")
    path = private_root.joinpath(*relative.parts)
    sha = check_hash(row["sha256"], label + " digest")
    limit = {
        "inventory": MAX_INVENTORY,
        "qualification": 1_048_576,
        "base_rom": MAX_ROM,
        "composition": 1_048_576,
        "source_closure": MAX_METADATA,
        "source_projection": MAX_METADATA,
    }[label]
    data = read_bounded(path, private_root, limit)
    if digest(data) != sha:
        fail(f"{label} changed after qualification")
    return path, data, sha


def check_projection(projection: dict, inventory: dict) -> None:
    source_rows = projection.get("registrations")
    inventory_rows = inventory.get("registrations")
    if not isinstance(source_rows, list) or not isinstance(inventory_rows, list) or len(source_rows) != len(inventory_rows):
        fail("sealed source membership differs from the execution inventory")
    fields = ("execution", "group", "index", "lane", "level", "name", "reason")
    for source, registered in zip(source_rows, inventory_rows, strict=True):
        if not isinstance(source, dict) or not isinstance(registered, dict):
            fail("sealed source membership contains a malformed registration")
        if any(source.get(key) != registered.get(key) for key in fields):
            fail("execution inventory changed a sealed source registration")
        source_values = source.get("values")
        inventory_values = registered.get("values")
        if not isinstance(source_values, list) or not isinstance(inventory_values, list) or len(source_values) != len(inventory_values):
            fail("execution inventory changed a sealed source value count")
        for source_value, inventory_value in zip(source_values, inventory_values, strict=True):
            if not isinstance(source_value, dict) or not isinstance(inventory_value, dict):
                fail("sealed source membership contains a malformed value")
            if any(source_value.get(key) != inventory_value.get(key) for key in ("id", "index", "selector")):
                fail("execution inventory changed a sealed source value")


def adapter_json(context: Inputs, *arguments: str, maximum: int = 4096) -> dict:
    code, stdout, _ = invoke_adapter(context, *arguments, timeout=10, maximum=maximum)
    if code != 0:
        fail("production diagnostic adapter refused a qualified control")
    return parse_json(stdout, "production adapter response")


def load_qualified_inputs(adapter: pathlib.Path) -> Inputs:
    private_root = private_directory(PRIVATE, ROOT)
    try:
        ignored = subprocess.run(
            ["git", "check-ignore", "--quiet", "--no-index", "--", "build/private-cpu-diagnostic"],
            cwd=ROOT, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, timeout=10, check=False,
        )
    except (OSError, subprocess.SubprocessError):
        fail("private output root could not be verified as ignored")
    if ignored.returncode != 0:
        fail("private diagnostic root is not ignored by Git")
    qualified_path = private_file(private_root / "qualified-build.json", private_root)
    qualified_bytes = read_bounded(qualified_path, private_root, 1_048_576)
    qualified = closed_object(
        parse_json(qualified_bytes, "qualified build seal"),
        {"adapter", "artifacts", "schema", "schema_version", "selector", "status"},
        "qualified build seal",
    )
    if qualified["schema"] != "t64.cpu-qualified-build" or qualified["schema_version"] != 1 or qualified["status"] != "qualified":
        fail("qualified build seal is not accepted")
    adapter_record = closed_object(qualified["adapter"], {"path", "sha256"}, "sealed adapter")
    adapter_sha = check_hash(adapter_record["sha256"], "sealed adapter digest")
    adapter_path = adapter_record["path"]
    if not isinstance(adapter_path, str) or adapter_path != ADAPTER_REPO_PATH:
        fail("qualified build selects a different diagnostic adapter path")
    adapter = check_adapter_path(adapter, adapter_path, adapter_sha)
    artifacts = closed_object(
        qualified["artifacts"],
        {"base_rom", "composition", "inventory", "qualification", "source_closure", "source_projection"},
        "qualified build artifacts",
    )
    resolved = {
        key: artifact_path(private_root, artifacts[key], key)
        for key in artifacts
    }
    selector = closed_object(
        qualified["selector"], {"encoding", "rom_offset", "width"}, "selector seal",
    )
    if selector != {
        "encoding": "registration-index-u16be:value-index-u16be",
        "rom_offset": selector["rom_offset"], "width": 4,
    } or not isinstance(selector["rom_offset"], int) or isinstance(selector["rom_offset"], bool):
        fail("selector encoding or offset differs from the reviewed contract")
    base_path, base_bytes, base_sha = resolved["base_rom"]
    composition = parse_json(resolved["composition"][1], "base composition report")
    if (
        not isinstance(composition, dict)
        or composition.get("schema") != "t64.cpu-build-composition"
        or composition.get("schema_version") != 1
        or composition.get("status") != "PASS_POSTBUILD_COMPOSITION"
        or composition.get("guest_execution_verified") is not False
        or composition.get("rom_sha256") != base_sha
        or composition.get("qualification_sha256") != resolved["qualification"][2]
        or composition.get("source_closure_sha256") != resolved["source_closure"][2]
        or not isinstance(composition.get("layout"), dict)
        or composition["layout"].get("selector_rom_offset") != selector["rom_offset"]
        or composition["layout"].get("selector_bytes") != selector["width"]
        or selector["rom_offset"] + selector["width"] > len(base_bytes)
        or base_bytes[selector["rom_offset"]:selector["rom_offset"] + 4] != bytes(4)
    ):
        fail("base composition and selector seal do not match the qualified image")

    qualification_path = resolved["qualification"][0]
    command = [
        sys.executable, "-B", "scripts/verify-cpu-diagnostic-qualification.py",
        str(qualification_path), str(ROOT),
    ]
    code, _, _ = run_bounded(command, cwd=ROOT, env=make_env(), timeout=120, maximum=4096)
    if code != 0:
        fail("existing pre-use source, tool, asset, or rights qualification no longer passes")
    qualification = parse_json(resolved["qualification"][1], "qualification receipt")
    source = qualification.get("source", {})
    projection = parse_json(resolved["source_projection"][1], "source membership projection")
    source_projection = source.get("projection") if isinstance(source, dict) else None
    if (
        not isinstance(source, dict)
        or source.get("overlay_sha256") is None
        or not isinstance(source_projection, dict)
        or set(source_projection) != {"path", "sha256", "projection_sha256"}
        or source_projection.get("path") != str(resolved["source_projection"][0].relative_to(ROOT))
        or source_projection.get("sha256") != resolved["source_projection"][2]
        or source_projection.get("projection_sha256") != projection.get("projection_sha256")
    ):
        fail("qualification receipt does not bind the sealed source projection")
    closure = parse_json(resolved["source_closure"][1], "source closure")
    inventory = parse_json(resolved["inventory"][1], "execution inventory")
    inventory_identity = inventory.get("identity") if isinstance(inventory, dict) else None
    if (
        not isinstance(closure, dict)
        or not isinstance(inventory_identity, dict)
        or closure.get("status") != "PASS_PREBUILD_SOURCE"
        or closure.get("source_projection_sha256") != projection.get("projection_sha256")
        or closure.get("source_revision") != projection.get("source_revision")
        or source.get("overlay_sha256") != inventory_identity.get("overlay_sha256")
        or qualification.get("source_revision") != inventory_identity.get("source_revision")
        or inventory_identity.get("tool_receipt_sha256") != resolved["qualification"][2]
        or inventory_identity.get("base_rom_sha256") != base_sha
    ):
        fail("source, tool, overlay, or base-ROM identities drifted")
    check_projection(projection, inventory)
    context = Inputs(
        private_root=private_root,
        adapter=adapter,
        adapter_path=adapter_path,
        adapter_sha256=adapter_sha,
        qualified_build_path="qualified-build.json",
        qualified_build_sha256=digest(qualified_bytes),
        inventory_path=str(resolved["inventory"][0].relative_to(private_root)),
        inventory_sha256=resolved["inventory"][2],
        inventory_bytes=resolved["inventory"][1],
        inventory=inventory,
        base_bytes=base_bytes,
        selector_offset=selector["rom_offset"],
    )
    code, _, _ = invoke_adapter(context, "--validate-inventory", str(resolved["inventory"][0]))
    if code != 0:
        fail("production inventory validator refused the sealed inventory")
    runtime = adapter_json(context, "--runtime-identity-json")
    if set(runtime) != {"core_build_sha256", "profile_sha256", "source_revision"}:
        fail("runtime identity response does not match its closed schema")
    if (
        runtime["core_build_sha256"] != inventory_identity.get("core_build_sha256")
        or runtime["profile_sha256"] != inventory_identity.get("profile_sha256")
    ):
        fail("running adapter core or profile differs from the qualified inventory")
    if (
        digest(qualified_bytes) != digest(read_bounded(qualified_path, private_root, 1_048_576))
        or any(data != read_bounded(path, private_root, max(len(data), 1)) for path, data, _ in resolved.values())
    ):
        fail("qualified inputs changed during verification")
    check_adapter(context)
    return context


def required_members(inventory: dict):
    members = []
    for registration in inventory["registrations"]:
        if registration["lane"] != "required-base":
            continue
        for value in registration["values"]:
            match = SELECTOR.fullmatch(value["selector"])
            if match is None or int(match.group(1)) != registration["index"] or int(match.group(2)) != value["index"]:
                fail("required inventory selector does not bind its exact member")
            members.append({
                "registration_index": registration["index"],
                "value_index": value["index"],
                "selector": value["selector"],
                "selected_rom_sha256": value["selected_rom_sha256"],
            })
    if not members:
        fail("execution inventory has no required CPU values")
    return members


def create_run_directory(private_root: pathlib.Path) -> pathlib.Path:
    private_directory(private_root, ROOT)
    name = f"run-{time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())}-{time.time_ns() % 1_000_000_000:09d}"
    path = private_root / name
    os.mkdir(path, 0o700)
    os.mkdir(path / "batches", 0o700)
    os.mkdir(path / "logs", 0o700)
    return path


def create_manifest(context: Inputs, run_directory: pathlib.Path) -> dict:
    check_adapter(context)
    manifest = {
        "adapter_path": context.adapter_path,
        "adapter_sha256": context.adapter_sha256,
        "batch_directory": "batches",
        "inventory_path": context.inventory_path,
        "inventory_sha256": context.inventory_sha256,
        "qualified_build_path": context.qualified_build_path,
        "qualified_build_sha256": context.qualified_build_sha256,
        "schema": "t64.cpu-batches",
        "schema_version": 1,
    }
    data = (json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n").encode()
    write_exclusive(run_directory / "batches.json", context.private_root, data)
    return manifest


def write_pointer(context: Inputs, run_directory: pathlib.Path) -> None:
    check_adapter(context)
    pointer = {
        "adapter_path": context.adapter_path,
        "adapter_sha256": context.adapter_sha256,
        "inventory_sha256": context.inventory_sha256,
        "qualified_build_sha256": context.qualified_build_sha256,
        "run_directory": run_directory.name,
        "schema": "t64.cpu-current-batch-run",
        "schema_version": 1,
    }
    data = (json.dumps(pointer, sort_keys=True, separators=(",", ":")) + "\n").encode()
    replace_private(context.private_root / "current-batch-run.json", context.private_root, data)


def write_variant(path: pathlib.Path, context: Inputs, member: dict) -> None:
    registration = member["registration_index"]
    value = member["value_index"]
    offset = context.selector_offset
    if (
        not isinstance(registration, int) or isinstance(registration, bool)
        or not isinstance(value, int) or isinstance(value, bool)
        or registration < 0 or registration > 0xFFFF or value < 0 or value > 0xFFFF
        or len(context.base_bytes) > MAX_ROM or offset + 4 > len(context.base_bytes)
    ):
        fail("qualified base image exceeds the local adapter bound")
    selector_bytes = registration.to_bytes(2, "big") + value.to_bytes(2, "big")
    descriptor = os.open(
        private_file(path, context.private_root, exists=False),
        os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0),
        0o600,
    )
    hasher = hashlib.sha256()
    try:
        for segment in (
            memoryview(context.base_bytes)[:offset],
            memoryview(selector_bytes),
            memoryview(context.base_bytes)[offset + 4:],
        ):
            hasher.update(segment)
            view = segment
            while view:
                written = os.write(descriptor, view)
                if written <= 0:
                    fail("selector variant write failed")
                view = view[written:]
        os.fsync(descriptor)
        if hasher.hexdigest() != member["selected_rom_sha256"]:
            fail("selector-only variant differs from the sealed inventory hash")
    except BaseException:
        os.close(descriptor)
        try:
            path.unlink()
        except OSError:
            pass
        raise
    os.close(descriptor)


def read_batch_if_present(path: pathlib.Path, private_root: pathlib.Path) -> bytes | None:
    if not path.exists() and not path.is_symlink():
        return None
    return read_bounded(path, private_root, 4096)


def append_status_record(path: pathlib.Path, private_root: pathlib.Path, record: bytes) -> None:
    path = private_file(path, private_root)
    descriptor = os.open(path, os.O_WRONLY | os.O_APPEND | getattr(os, "O_NOFOLLOW", 0))
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid():
            fail("value-status output is not a private regular file")
        view = memoryview(record)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                fail("value-status append failed")
            view = view[written:]
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def run_one_member(context: Inputs, run_directory: pathlib.Path, member: dict, *,
                   fake_guest_command: list[str] | None = None,
                   control_mode: bool = False) -> int:
    run_directory = private_directory(run_directory, context.private_root)
    batches = private_directory(run_directory / "batches", context.private_root)
    logs = private_directory(run_directory / "logs", context.private_root)
    status_path = private_file(run_directory / "value-status.jsonl", context.private_root)
    registration = member["registration_index"]
    value = member["value_index"]
    selector = member["selector"]
    base_name = f"r{registration:04d}v{value:05d}"
    variant = run_directory / f"variant-{base_name}.z64"
    batch = batches / f"batch-{base_name}.json"
    log_path = logs / f"member-{base_name}.json"
    code = 4
    stdout = b""
    stderr = b""
    try:
        check_adapter(context)
        write_variant(variant, context, member)
        arguments = [
            "--run-local", "--local-only",
            str(context.private_root / context.inventory_path), str(variant),
            str(len(context.base_bytes)), member["selected_rom_sha256"],
            str(TICK_LIMIT), str(registration), str(value), str(batch),
        ]
        try:
            if fake_guest_command is None:
                code, stdout, stderr = invoke_adapter(
                    context, *arguments, timeout=RUN_TIMEOUT, maximum=MAX_GUEST_OUTPUT,
                )
            else:
                check_adapter(context)
                try:
                    code, stdout, stderr = run_bounded(
                        [*fake_guest_command, *arguments], cwd=ROOT,
                        env=make_env(controls=control_mode),
                        timeout=RUN_TIMEOUT, maximum=MAX_GUEST_OUTPUT,
                    )
                finally:
                    check_adapter(context)
        except Refusal as error:
            code = 4
            stderr = str(error).encode()
        canonical_batch = read_batch_if_present(batch, context.private_root)
        if canonical_batch is not None:
            append_status_record(status_path, context.private_root, canonical_batch)
    except Refusal as error:
        stderr = (stderr + (b"\n" if stderr else b"") + str(error).encode())[:MAX_GUEST_OUTPUT]
        code = 4
    finally:
        if variant.exists() and not variant.is_symlink():
            variant.unlink()
    log = {
        "returncode": code,
        "selector": selector,
        "stderr": stderr.decode("utf-8", "replace"),
        "stdout": stdout.decode("utf-8", "replace"),
    }
    if not log_path.exists():
        write_exclusive(
            log_path, context.private_root,
            (json.dumps(log, sort_keys=True, separators=(",", ":")) + "\n").encode(),
        )
    return code


def run_all(context: Inputs, *, fake_guest_command: list[str] | None = None,
            control_mode: bool = False) -> tuple[pathlib.Path, int]:
    run_directory = create_run_directory(context.private_root)
    batches = run_directory / "batches"
    logs = run_directory / "logs"
    status_path = run_directory / "value-status.jsonl"
    descriptor = os.open(
        private_file(status_path, context.private_root, exists=False),
        os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0), 0o600,
    )
    os.close(descriptor)
    create_manifest(context, run_directory)
    write_pointer(context, run_directory)
    members = required_members(context.inventory)
    fatal = False
    for member in members:
        try:
            code = run_one_member(
                context, run_directory, member, fake_guest_command=fake_guest_command,
                control_mode=control_mode,
            )
        except Refusal:
            fatal = True
            break
        if code not in (0, 2, 3):
            fatal = True
            break
    return run_directory, (4 if fatal else 0)


def first_required_member(context: Inputs) -> dict:
    return required_members(context.inventory)[0]


def load_run(context: Inputs, explicit_run: str | None) -> tuple[pathlib.Path, dict]:
    if explicit_run is None:
        pointer_path = private_file(context.private_root / "current-batch-run.json", context.private_root)
        pointer = parse_json(read_bounded(pointer_path, context.private_root, 4096), "current run pointer")
        pointer = closed_object(
            pointer,
            {"adapter_path", "adapter_sha256", "inventory_sha256", "qualified_build_sha256", "run_directory", "schema", "schema_version"},
            "current run pointer",
        )
        if pointer["schema"] != "t64.cpu-current-batch-run" or pointer["schema_version"] != 1:
            fail("current run pointer schema is unsupported")
        run_name = pointer["run_directory"]
        if not isinstance(run_name, str) or not run_name.startswith("run-") or "/" in run_name:
            fail("current run pointer path is invalid")
        if (
            pointer["adapter_path"] != context.adapter_path
            or pointer["adapter_sha256"] != context.adapter_sha256
            or pointer["inventory_sha256"] != context.inventory_sha256
            or pointer["qualified_build_sha256"] != context.qualified_build_sha256
        ):
            fail("current run pointer identities differ from the qualified build")
        run_directory = private_directory(context.private_root / run_name, context.private_root)
    else:
        relative = pathlib.PurePosixPath(explicit_run)
        if relative.is_absolute() or len(relative.parts) != 1 or not relative.name.startswith("run-"):
            fail("explicit run path must name one private run directory")
        run_directory = private_directory(context.private_root / relative.name, context.private_root)
    manifest_path = private_file(run_directory / "batches.json", context.private_root)
    manifest = parse_json(read_bounded(manifest_path, context.private_root, 4096), "batch run manifest")
    manifest = closed_object(
        manifest,
        {"adapter_path", "adapter_sha256", "batch_directory", "inventory_path", "inventory_sha256", "qualified_build_path", "qualified_build_sha256", "schema", "schema_version"},
        "batch run manifest",
    )
    if (
        manifest["schema"] != "t64.cpu-batches" or manifest["schema_version"] != 1
        or manifest["adapter_path"] != context.adapter_path
        or manifest["adapter_sha256"] != context.adapter_sha256
        or manifest["qualified_build_path"] != context.qualified_build_path
        or manifest["qualified_build_sha256"] != context.qualified_build_sha256
        or manifest["inventory_path"] != context.inventory_path
        or manifest["inventory_sha256"] != context.inventory_sha256
        or manifest["batch_directory"] != "batches"
    ):
        fail("batch run manifest does not bind the current qualified inventory")
    return run_directory, manifest


def verify_complete(context: Inputs, *, explicit_run: str | None = None) -> int:
    run_directory, manifest = load_run(context, explicit_run)
    inventory_path = context.private_root / manifest["inventory_path"]
    batch_directory = run_directory / manifest["batch_directory"]
    private_directory(batch_directory, context.private_root)
    output_path = run_directory / "group-status.json"
    # The adapter is the sole parser and completeness calculator. Persist its
    # canonical bytes unchanged, including its honest incomplete report.
    code, stdout, _ = invoke_adapter(
        context, "--verify-complete-json", str(inventory_path), str(batch_directory),
        timeout=60, maximum=MAX_GROUP_REPORT,
    )
    if code not in (0, 3):
        detail = stdout.decode("utf-8", "replace").strip()
        fail("production shared aggregator refused the batch set: " + detail[:256])
    if not stdout.startswith(b"{") or not stdout.endswith(b"}\n"):
        fail("production shared aggregator did not return a canonical group report")
    write_exclusive(output_path, context.private_root, stdout)
    return code


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--run-all", action="store_true")
    mode.add_argument("--verify-complete", action="store_true")
    parser.add_argument("--adapter", required=True)
    parser.add_argument("--run-dir")
    args = parser.parse_args(argv)
    try:
        refuse_public_execution()
        context = load_qualified_inputs(pathlib.Path(args.adapter))
        if args.run_all:
            run_directory, code = run_all(context)
            print(f"run_directory={run_directory.name}")
            if code == 0:
                print("run-all finished; use --verify-complete to classify the canonical batch set")
            return code
        code = verify_complete(context, explicit_run=args.run_dir)
        print("classification=complete" if code == 0 else "classification=incomplete")
        return code
    except Refusal as error:
        print(f"cpu diagnostic refused: {error}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())

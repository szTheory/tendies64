#!/usr/bin/env python3
"""Run one locally qualified BASE member and retain its original outcome."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import secrets
import stat
import subprocess
import sys
import time
from types import SimpleNamespace

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-base-diagnostic"
CPU_RUNNER = ROOT / "scripts/run-cpu-diagnostic.py"
RUNNER_RELATIVE = "scripts/run-base-diagnostic.py"
MAX_METADATA = 64 * 1024 * 1024


class Refusal(ValueError):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def digest_file(path: pathlib.Path, maximum: int = 512 * 1024 * 1024) -> str:
    before = path.lstat()
    if path.is_symlink() or not stat.S_ISREG(before.st_mode) or before.st_size <= 0 or before.st_size > maximum:
        refuse("qualified adapter is not a bounded regular file")
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) |
                         getattr(os, "O_NONBLOCK", 0))
    hasher = hashlib.sha256()
    total = 0
    try:
        opened = os.fstat(descriptor)
        if (not stat.S_ISREG(opened.st_mode) or opened.st_mode & 0o022 or
                (opened.st_dev, opened.st_ino, opened.st_size) != (before.st_dev, before.st_ino, before.st_size)):
            refuse("qualified adapter changed while opening")
        while True:
            block = os.read(descriptor, 1024 * 1024)
            if not block:
                break
            total += len(block)
            if total > maximum:
                refuse("qualified input grew past its size bound")
            hasher.update(block)
        after = os.fstat(descriptor)
        if (total != opened.st_size or
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns) !=
                (opened.st_size, opened.st_mtime_ns, opened.st_ctime_ns)):
            refuse("qualified input changed while hashing")
    finally:
        os.close(descriptor)
    return hasher.hexdigest()


def canonical(value) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode()


def _private_bytes(path: pathlib.Path, maximum: int = MAX_METADATA,
                   *, empty: bool = False) -> bytes:
    before = path.lstat()
    if (not stat.S_ISREG(before.st_mode) or stat.S_IMODE(before.st_mode) != 0o600 or
            before.st_size > maximum or before.st_size < (0 if empty else 1)):
        refuse("private input is not a bounded owner-only regular file")
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) |
                         getattr(os, "O_NONBLOCK", 0))
    with os.fdopen(descriptor, "rb") as stream:
        opened = os.fstat(stream.fileno())
        if (not stat.S_ISREG(opened.st_mode) or stat.S_IMODE(opened.st_mode) != 0o600 or
                (opened.st_dev, opened.st_ino, opened.st_size) !=
                (before.st_dev, before.st_ino, before.st_size)):
            refuse("private input changed while opening")
        data = stream.read(maximum + 1)
        after = os.fstat(stream.fileno())
        if (len(data) != opened.st_size or len(data) > maximum or
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns) !=
                (opened.st_size, opened.st_mtime_ns, opened.st_ctime_ns)):
            refuse("private input changed while reading")
    return data


def _canonical_object(data: bytes) -> dict:
    value = json.loads(data)
    if not isinstance(value, dict) or canonical(value) != data:
        refuse("private metadata is not a canonical JSON object")
    return value


def _bound_bytes(context: dict, relative: str, role: str,
                  maximum: int = MAX_METADATA) -> bytes:
    posix = pathlib.PurePosixPath(relative)
    if posix.is_absolute() or not relative or "\\" in relative or any(
            item in (".", "..") for item in posix.parts):
        refuse("bound path escapes the repository")
    cursor = ROOT
    for item in posix.parts:
        cursor = cursor / item
        if cursor.is_symlink():
            refuse("bound path traverses a symlink")
    data = _private_bytes(cursor, maximum)
    sha = hashlib.sha256(data).hexdigest()
    if not any(row.get("path") == relative and row.get("sha256") == sha
               for row in context["bindings"].get(role, [])):
        refuse("input differs from its independently verified role binding")
    return data


def _load_module(path: pathlib.Path, name: str):
    if path.is_symlink() or not path.is_file():
        refuse(f"{name} is missing or linked")
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        refuse(f"{name} cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    try:
        spec.loader.exec_module(module)
    except Exception as exc:
        sys.modules.pop(name, None)
        refuse(f"{name} failed to load: {exc}")
    return module


def _safe_adapter(context: dict, repository_root: pathlib.Path,
                  adapter_path: pathlib.Path) -> pathlib.Path:
    adapter_path = adapter_path.absolute()
    try:
        relative = adapter_path.resolve(strict=True).relative_to(repository_root)
    except (OSError, ValueError):
        refuse("BASE adapter is outside the qualified repository")
    repo_relative = relative.as_posix()
    rows = context["bindings"].get("executable", [])
    binding = next((row for row in rows if row.get("path") == repo_relative), None)
    if binding is None:
        refuse("selected BASE adapter is not in the qualified executable closure")
    if adapter_path.is_symlink() or not os.access(adapter_path, os.X_OK):
        refuse("selected BASE adapter is linked or not executable")
    if digest_file(adapter_path) != binding["sha256"]:
        refuse("selected BASE adapter bytes differ from their binding")
    return adapter_path


def _verify_runner_binding(context: dict) -> None:
    rows = context["bindings"].get("build", [])
    for relative in (RUNNER_RELATIVE, "scripts/run-cpu-diagnostic.py"):
        expected = digest_file(ROOT / relative)
        if not any(row.get("path") == relative and row.get("sha256") == expected
                   for row in rows):
            refuse("normal BASE dispatch helper is absent from the immutable build closure")


def _verify_runtime_profile(context: dict, adapter: pathlib.Path) -> None:
    profile_rows = context["bindings"].get("profile", [])
    row = next((item for item in profile_rows
                if isinstance(item.get("path"), str) and
                item["path"].startswith("build/private-base-diagnostic/runtime-identity-") and
                item["path"].endswith(".json")), None)
    if row is None:
        refuse("Tier B/T1 runtime identity is absent from the immutable context")
    expected_path = ROOT / row["path"]
    expected = _bound_bytes(context, row["path"], "profile", 8192)
    before = digest_file(adapter)
    try:
        completed = subprocess.run(
            [str(adapter), "--runtime-identity-json"], cwd=ROOT,
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=10, check=False,
        )
    except (OSError, subprocess.SubprocessError) as exc:
        refuse(f"Tier B/T1 runtime identity could not be read: {exc}")
    if digest_file(adapter) != before or completed.returncode != 0:
        refuse("BASE runtime identity invocation changed or refused the adapter")
    try:
        identity = json.loads(completed.stdout)
    except (UnicodeError, json.JSONDecodeError):
        refuse("Tier B/T1 runtime identity is malformed")
    if (completed.stdout != expected or not isinstance(identity, dict) or
            identity.get("profile_id") != "ntsc-cart-4m-openipl3-rdram-b-t1-v1"):
        refuse("selected BASE adapter is not bound to the Tier B/T1 context")


def _write_variant(path: pathlib.Path, context, member: dict) -> None:
    registration = member.get("registration_index")
    value = member.get("value_index")
    offset = context.selector_offset
    if (type(registration) is not int or type(value) is not int or
            registration < 0 or registration > 0xffff or value < 0 or value > 0xffff or
            len(context.base_bytes) > 0x00401000 or offset < 0 or offset + 4 > len(context.base_bytes)):
        refuse("source member cannot be represented by the qualified selector")
    selector = registration.to_bytes(2, "big") + value.to_bytes(2, "big")
    image = context.base_bytes[:offset] + selector + context.base_bytes[offset + 4:]
    if hashlib.sha256(image).hexdigest() != member.get("selected_rom_sha256"):
        refuse("selector variant differs from its accepted image identity")
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                 getattr(os, "O_NOFOLLOW", 0), 0o600)
    with os.fdopen(fd, "wb") as output:
        output.write(image)
        output.flush()
        os.fsync(output.fileno())


def _first_base_member(context: dict) -> dict:
    authority = json.loads((PRIVATE / "source-authority.json").read_bytes())
    members = authority.get("members")
    if not isinstance(members, list):
        refuse("independently derived BASE authority has no members")
    for member in members:
        if (isinstance(member, dict) and member.get("required") is True and
                member.get("lane") == "required-base" and
                member.get("execution") in {"once", "values"}):
            return member
    refuse("independently derived BASE authority has no dispatchable member")


def _assert_source_closure_bindings(context: dict, original_relative: str,
                                    expected_sha256: str, original_bytes: bytes,
                                    repository_root: pathlib.Path = ROOT) -> None:
    mirror_relative = ("build/private-base-diagnostic/source-image-source-closure-" +
                       expected_sha256[:16] + ".json")
    mirror = repository_root / mirror_relative
    if not any(binding.get("path") == mirror_relative and
               binding.get("sha256") == expected_sha256
               for binding in context["bindings"]["runtime"]):
        refuse("source closure mirror is absent from the runtime role")
    if not any(binding.get("path") == original_relative and
               binding.get("sha256") == expected_sha256
               for binding in context["bindings"]["asset"]):
        refuse("original source closure is absent from the asset role")
    if mirror.is_symlink() or not mirror.is_file() or mirror.read_bytes() != original_bytes:
        refuse("source closure mirror bytes differ from the accepted artifact")


def _load_qualified_source_image(cpu_runner, context: dict):
    seal_path = ROOT / "build/private-cpu-diagnostic/qualified-build.json"
    if not any(row.get("path") == "build/private-cpu-diagnostic/qualified-build.json"
               for row in context["bindings"]["build"]):
        refuse("accepted source-image build seal is absent from the BASE context")
    try:
        seal = json.loads(seal_path.read_bytes())
    except (OSError, UnicodeError, json.JSONDecodeError):
        refuse("accepted source-image build seal cannot be read")
    expected_keys = {"base_rom", "composition", "inventory", "qualification",
                     "source_closure", "source_projection"}
    if (not isinstance(seal, dict) or seal.get("schema") != "t64.cpu-qualified-build" or
            seal.get("schema_version") not in (1, 2) or seal.get("status") != "qualified" or
            set(seal.get("artifacts", {})) != expected_keys):
        refuse("accepted source-image build seal is malformed")

    def artifact(name: str):
        row = seal["artifacts"][name]
        if not isinstance(row, dict) or set(row) != {"path", "sha256"}:
            refuse("accepted source-image artifact reference is malformed")
        relative = pathlib.PurePosixPath("build/private-cpu-diagnostic") / pathlib.PurePosixPath(row["path"])
        if relative.is_absolute() or any(part in {"", ".", ".."} for part in relative.parts):
            refuse("accepted source-image artifact path escapes the repository")
        path = ROOT.joinpath(*relative.parts)
        if path.is_symlink() or not path.is_file():
            refuse("accepted source-image artifact is missing or linked")
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != row["sha256"]:
            refuse("accepted source-image artifact digest changed")
        role = {"base_rom": "image", "composition": "composition",
                "inventory": "build", "qualification": "runtime"}.get(name)
        if role is None and name == "source_projection":
            source_projection = context.get("source_projection", {})
            if (source_projection.get("path") != relative.as_posix() or
                    source_projection.get("sha256") != row["sha256"]):
                refuse("source projection artifact differs from its independent binding")
        elif role is None and name == "source_closure":
            _assert_source_closure_bindings(context, relative.as_posix(),
                                            row["sha256"], data, ROOT)
        elif not any(binding.get("path") == relative.as_posix() and
                     binding.get("sha256") == row["sha256"]
                     for binding in context["bindings"][role]):
            refuse(f"{role} source-image artifact is not bound to its qualification role")
        return path, data, row["sha256"]

    resolved = {name: artifact(name) for name in expected_keys}
    qualification = json.loads(resolved["qualification"][1])
    asset_closure = qualification.get("assets", {}).get("closure_report", {})
    source_closure = seal["artifacts"]["source_closure"]
    source_closure_path = pathlib.PurePosixPath("build/private-cpu-diagnostic") / pathlib.PurePosixPath(source_closure["path"])
    asset_closure_path = pathlib.Path(asset_closure.get("path", ""))
    if asset_closure_path.is_absolute():
        try:
            asset_closure_path = asset_closure_path.relative_to(ROOT)
        except ValueError:
            refuse("asset closure path escapes the repository")
    if (asset_closure_path.as_posix() != source_closure_path.as_posix() or
            asset_closure.get("sha256") != source_closure.get("sha256")):
        refuse("source/asset closure role sharing is not authorized by the receipt")
    image_path, image_bytes, image_sha = resolved["base_rom"]
    selector = seal.get("selector")
    composition = json.loads(resolved["composition"][1])
    projection = json.loads((ROOT / "build/private-cpu-diagnostic/source-projection.json").read_bytes())
    if (not isinstance(selector, dict) or
            selector.get("encoding") != "registration-index-u16be:value-index-u16be" or
            selector.get("width") != 4 or type(selector.get("rom_offset")) is not int or
            selector["rom_offset"] < 0 or selector["rom_offset"] + 4 > len(image_bytes) or
            image_bytes[selector["rom_offset"]:selector["rom_offset"] + 4] != bytes(4) or
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
            projection.get("source_revision") != context["source_revision"]):
        refuse("accepted source-image composition differs from its immutable binding")
    inventory = json.loads(resolved["inventory"][1])
    members = cpu_runner.required_members(inventory)
    cpu_context = SimpleNamespace(base_bytes=image_bytes,
                                  selector_offset=selector["rom_offset"])
    return cpu_context, members[0]


def _write_private(path: pathlib.Path, data: bytes) -> None:
    if path.exists() or path.is_symlink():
        refuse("private BASE output already exists")
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                 getattr(os, "O_NOFOLLOW", 0), 0o600)
    with os.fdopen(fd, "wb") as output:
        output.write(data)
        output.flush()
        os.fsync(output.fileno())


def classify_attempt(attempt: dict) -> str:
    if type(attempt.get("timed_out")) is not bool or type(attempt.get("cleanup_verified")) is not bool:
        refuse("member attempt timeout or cleanup state is missing")
    if attempt["timed_out"]:
        return "timeout"
    if not attempt["cleanup_verified"]:
        return "harness-error"
    code = attempt.get("returncode")
    if code == 0 and attempt.get("record_authenticated") is True:
        return "pass"
    if code == 2 and attempt.get("record_authenticated") is True:
        return "fail"
    if code == 3:
        return "incomplete"
    return "harness-error"


def _production_attempt(run_directory: pathlib.Path, adapter: pathlib.Path,
                        context: dict, repository_root: pathlib.Path) -> dict:
    if context.get("schema_version") != 1:
        refuse("combined-image contexts use the bound --run-all path")
    if os.environ.get("CI", "").lower() in {"1", "true", "yes"} or os.environ.get("T64_REQUIRE_FIXTURES") == "1":
        refuse("qualified BASE execution is local-only")
    _verify_runner_binding(context)
    _verify_runtime_profile(context, adapter)
    cpu_runner = _load_module(CPU_RUNNER, "t64_cpu_member_runner")
    cpu_context, cpu_member = _load_qualified_source_image(cpu_runner, context)
    base_member = _first_base_member(context)
    if (base_member["index"] != cpu_member["registration_index"] or
            base_member["value_index"] != cpu_member["value_index"]):
        refuse("first dispatchable BASE member is not in the accepted image selector closure")

    # The old CPU contract validates the immutable source image only. The new
    # BASE C runner selects Tier B/T1 and executes the canonical core directly.
    run_directory.mkdir(mode=0o700)
    variant = run_directory / "member.z64"
    record_path = run_directory / "member-record.json"
    capture_path = run_directory / "guest-capture.bin"
    _write_variant(variant, cpu_context, cpu_member)
    record_path_text = str(record_path)
    capture_path_text = str(capture_path)
    command = [
        str(adapter), "--run-local", "--local-only",
        str(repository_root / "build/private-base-diagnostic/source-authority.json"),
        context["source_authority"]["sha256"],
        str(repository_root / "build/private-base-diagnostic/inventory.json"),
        str(variant), str(len(cpu_context.base_bytes)),
        cpu_member["selected_rom_sha256"], str(cpu_runner.TICK_LIMIT),
        str(cpu_member["registration_index"]), str(cpu_member["value_index"]),
        record_path_text, capture_path_text,
    ]
    command_before = digest_file(adapter)
    timed_out = False
    cleanup_verified = False
    returncode = 4
    stdout = b""
    stderr = b""
    try:
        returncode, stdout, stderr = cpu_runner.run_bounded(
            command, cwd=repository_root, env=cpu_runner.make_env(),
            timeout=cpu_runner.RUN_TIMEOUT, maximum=cpu_runner.MAX_GUEST_OUTPUT,
        )
        cleanup_verified = True
    except Exception as exc:
        stderr = str(exc).encode("utf-8", "replace")[:cpu_runner.MAX_GUEST_OUTPUT]
        timed_out = "timed out" in str(exc)
        cleanup_verified = False
    if digest_file(adapter) != command_before:
        refuse("BASE adapter changed during the one-member attempt")
    _write_private(run_directory / "command.json",
                   (json.dumps(command, ensure_ascii=False, separators=(",", ":")) + "\n").encode())
    _write_private(run_directory / "c-stdout.log", stdout)
    _write_private(run_directory / "c-stderr.log", stderr)
    record_authenticated = False
    if (not timed_out and returncode in (0, 2, 3) and
            record_path.is_file() and capture_path.is_file()):
        auth_command = [
            str(adapter), "--aggregate",
            str(repository_root / "build/private-base-diagnostic/source-authority.json"),
            context["source_authority"]["sha256"],
            str(repository_root / "build/private-base-diagnostic/inventory.json"),
            str(record_path), str(capture_path),
        ]
        try:
            auth_code, auth_stdout, auth_stderr = cpu_runner.run_bounded(
                auth_command, cwd=repository_root, env=cpu_runner.make_env(),
                timeout=10, maximum=cpu_runner.MAX_GUEST_OUTPUT,
            )
        except Exception as exc:
            auth_code, auth_stdout, auth_stderr = 4, b"", str(exc).encode()
        _write_private(run_directory / "aggregate-stdout.log", auth_stdout)
        _write_private(run_directory / "aggregate-stderr.log", auth_stderr)
        record_authenticated = auth_code == 0
    status = "timeout" if timed_out else (
        "pass" if returncode == 0 and record_authenticated else
        "fail" if returncode == 2 and record_authenticated else
        "incomplete" if returncode == 3 and record_authenticated else
        "harness-error")
    attempt = {
        "cleanup_verified": cleanup_verified,
        "record_authenticated": record_authenticated,
        "record_path": record_path.name if record_path.is_file() else None,
        "returncode": returncode,
        "stderr": stderr.decode("utf-8", "replace"),
        "status": status,
        "timed_out": timed_out,
    }
    return attempt


def _load_combined_source_image(context: dict):
    relative = "build/private-base-diagnostic/combined-image.json"
    image = _canonical_object(_bound_bytes(context, relative, "composition"))
    keys = {"schema", "schema_version", "status", "guest_execution_verified",
            "source_revision", "base_rom", "selector", "source_bindings", "combined_overlay", "members"}
    if (set(image) != keys or image.get("schema") != "t64.base-combined-image" or
            type(image.get("schema_version")) is not int or image["schema_version"] != 1 or
            image.get("status") != "PASS_POSTBUILD_COMPOSITION" or
            image.get("guest_execution_verified") is not False or
            image.get("source_revision") != context.get("source_revision")):
        refuse("combined image is not the qualified unexecuted composition")
    bindings = image.get("source_bindings")
    if (not isinstance(bindings, dict) or
            bindings.get("cpu_source_projection_sha256") != context["source_projection"]["sha256"] or
            bindings.get("rsp_source_projection_sha256") != context["rsp_source_projection"]["sha256"]):
        refuse("combined image targets another independently verified source")
    if (not isinstance(image.get("combined_overlay"), dict) or
            bindings.get("combined_overlay_sha256") !=
                hashlib.sha256(canonical(image["combined_overlay"])).hexdigest()):
        refuse("combined overlay differs from its composition identity")
    base_ref = image.get("base_rom")
    if (not isinstance(base_ref, dict) or set(base_ref) != {"path", "sha256"} or
            base_ref.get("path") != "build/private-base-diagnostic/base.z64"):
        refuse("combined ROM reference is malformed")
    base_bytes = _bound_bytes(context, base_ref["path"], "image", 0x00401000)
    if hashlib.sha256(base_bytes).hexdigest() != base_ref["sha256"]:
        refuse("combined ROM differs from its composition identity")
    selector = image.get("selector")
    if (not isinstance(selector, dict) or set(selector) != {"encoding", "width", "rom_offset"} or
            selector.get("encoding") != "registration-index-u16be:value-index-u16be" or
            selector.get("width") != 4 or type(selector.get("rom_offset")) is not int or
            not 0 <= selector["rom_offset"] <= len(base_bytes) - 4):
        refuse("combined image selector layout is malformed")
    selected = {}
    members = image.get("members")
    if not isinstance(members, list):
        refuse("combined selector membership is missing")
    for row in members:
        if (not isinstance(row, dict) or set(row) != {"group", "lane", "level",
                "registration_index", "value_index", "selector_hex", "selected_rom_sha256"}):
            refuse("combined selector member is malformed")
        reg, val = row["registration_index"], row["value_index"]
        if (type(reg) is not int or type(val) is not int or
                not 0 <= reg <= 0xffff or not 0 <= val <= 0xffff or (reg, val) in selected or
                row["selector_hex"] != (reg.to_bytes(2, "big") + val.to_bytes(2, "big")).hex()):
            refuse("combined selector ordinals are invalid or duplicated")
        sha = row["selected_rom_sha256"]
        if not isinstance(sha, str) or len(sha) != 64 or any(c not in "0123456789abcdef" for c in sha):
            refuse("combined selector image identity is invalid")
        selected[(reg, val)] = row
    return SimpleNamespace(base_bytes=base_bytes, selector_offset=selector["rom_offset"],
                           members=selected)


def _frozen_adapter(snapshot: dict) -> pathlib.Path:
    context = snapshot["context"]
    executable_rows = context["bindings"]["executable"]
    if len(executable_rows) != 1:
        refuse("BASE executable binding must be singular")
    context_sha = hashlib.sha256(snapshot["context_bytes"]).hexdigest()
    adapter = PRIVATE / "execution-snapshots" / context_sha / "executable"
    if (adapter.is_symlink() or
            digest_file(adapter) != executable_rows[0]["sha256"]):
        refuse("retained exact BASE consumer is unavailable")
    return adapter


def _batch_adapter(snapshot, batch):
    adapter = batch / "consumer"
    if (adapter.is_symlink() or not os.access(adapter, os.X_OK) or
            digest_file(adapter) != snapshot["context"]["bindings"]["executable"][0]["sha256"]):
        refuse("batch consumer differs from the retained exact executable")
    return adapter


def _classify_manifest(adapter, snapshot, batch_directory, rows, cpu_runner):
    manifest = batch_directory / f"classification-input-{secrets.token_hex(8)}.json"
    report = batch_directory / f"classification-{secrets.token_hex(8)}.json"
    _write_private(manifest, canonical({"members": rows,
        "schema": "t64.base-classification-input", "schema_version": 1}))
    command = [str(adapter), "--classify-manifest",
        str(batch_directory / "source-authority.json"),
        snapshot["context"]["source_authority"]["sha256"],
        str(batch_directory / "inventory.json"),
        hashlib.sha256(snapshot["context_bytes"]).hexdigest(), str(manifest), str(report)]
    before = digest_file(adapter)
    code, stdout, stderr = cpu_runner.run_bounded(command, cwd=ROOT,
        env=cpu_runner.make_env(), timeout=60, maximum=cpu_runner.MAX_GUEST_OUTPUT)
    if digest_file(adapter) != before:
        refuse("exact classification consumer changed during use")
    _write_private(batch_directory / f"{report.stem}-stdout.log", stdout)
    _write_private(batch_directory / f"{report.stem}-stderr.log", stderr)
    if code not in (0, 3) or not report.is_file():
        refuse("C source classification refused its retained inputs")
    return code, report, manifest


def _bound_member_attempt(directory, snapshot, adapter, image, member, cpu_runner):
    # All of these checks happen before this member's fresh guest directory.
    context = snapshot["context"]
    _verify_runner_binding(context)
    _verify_runtime_profile(context, adapter)
    selector = image.members.get((member["index"], member["value_index"]))
    if (selector is None or any(selector[key] != member[key]
                               for key in ("group", "lane", "level"))):
        refuse("source member is absent from its qualified combined image")
    directory.mkdir(mode=0o700)
    _write_private(directory / "source-authority.json", snapshot["authority_bytes"])
    _write_private(directory / "inventory.json", snapshot["inventory_bytes"])
    variant = directory / "member.z64"
    _write_variant(variant, image, selector)
    record, capture = directory / "member-record.json", directory / "guest-capture.bin"
    command = [str(adapter), "--run-bound-local", "--local-only",
        str(directory / "source-authority.json"), context["source_authority"]["sha256"],
        str(directory / "inventory.json"), str(variant), str(len(image.base_bytes)),
        selector["selected_rom_sha256"], str(cpu_runner.TICK_LIMIT),
        str(member["index"]), str(member["value_index"]), str(record), str(capture),
        hashlib.sha256(snapshot["context_bytes"]).hexdigest()]
    _write_private(directory / "command.json", canonical({"argv": command}))
    before = digest_file(adapter)
    started = time.monotonic()
    code, stdout, stderr, timed_out, cleanup_verified = 4, b"", b"", False, False
    try:
        code, stdout, stderr = cpu_runner.run_bounded(command, cwd=ROOT,
            env=cpu_runner.make_env(), timeout=cpu_runner.RUN_TIMEOUT,
            maximum=cpu_runner.MAX_GUEST_OUTPUT)
        timed_out = code == 124
        # This marker is printed by the native parent only after waitpid.
        cleanup_verified = b"native_worker_reaped=yes" in stdout.splitlines()
    except cpu_runner.Refusal as exc:
        stderr = str(exc).encode()[:cpu_runner.MAX_GUEST_OUTPUT]
        timed_out = "timed out" in str(exc)
        # Never infer descendant cleanup from the text of a timeout exception.
        cleanup_verified = False
    if digest_file(adapter) != before:
        refuse("retained BASE consumer changed during the member attempt")
    _write_private(directory / "c-stdout.log", stdout)
    _write_private(directory / "c-stderr.log", stderr)
    attempt = {"schema": "t64.base-bound-attempt", "schema_version": 1,
        "returncode": code, "timed_out": timed_out, "cleanup_verified": cleanup_verified,
        "elapsed_seconds": format(time.monotonic() - started, ".6f")}
    _write_private(directory / "attempt.json", canonical(attempt))
    return attempt, record, capture, selector["selected_rom_sha256"]


def _validate_dispatch_seam(authority, image):
    members = authority.get("members")
    if not isinstance(members, list):
        refuse("verified source authority has no member list")
    source = {}
    for member in members:
        key = (member["index"], member["value_index"])
        if key in source:
            refuse("verified source has duplicate selector ordinals")
        source[key] = member
        if member["execution"] in ("once", "values"):
            selected = image.members.get(key)
            if selected is None or any(selected[field] != member[field]
                                       for field in ("group", "lane", "level")):
                refuse("dispatchable source selector is missing from the qualified image")
    for key in image.members:
        if key not in source or source[key]["execution"] not in ("once", "values"):
            refuse("qualified image selects an absent or blocked source member")


def run_required(context_root: pathlib.Path, adapter_path: pathlib.Path,
                 *, lane: str = "required") -> dict:
    """One immutable serial attempt per selected member; C owns all totals."""
    if (os.environ.get("CI", "").lower() in {"1", "true", "yes"} or
            os.environ.get("T64_REQUIRE_FIXTURES") == "1"):
        refuse("qualified BASE execution is local-only")
    if lane not in {"required", "informational", "stress"}:
        refuse("BASE lane is not source-selected")
    context_root = context_root.absolute()
    verifier = _load_module(ROOT / "scripts/verify-base-diagnostic-qualification.py",
                            "t64_base_preuse_verifier")
    cpu_runner = _load_module(CPU_RUNNER, "t64_cpu_member_runner")
    batch = context_root / f"batch-{secrets.token_hex(8)}"
    prepared = {}
    def prepare(snapshot):
        context = snapshot["context"]
        if context.get("schema_version") != 2:
            refuse("whole-BASE dispatch requires the combined-image context")
        live_adapter = _safe_adapter(context, ROOT, adapter_path.absolute())
        _verify_runner_binding(context)
        retained = _frozen_adapter(snapshot)
        _verify_runtime_profile(context, live_adapter)
        image = _load_combined_source_image(context)
        authority = _canonical_object(snapshot["authority_bytes"])
        _validate_dispatch_seam(authority, image)
        context_sha = hashlib.sha256(snapshot["context_bytes"]).hexdigest()
        marker = context_root / f"{lane}-dispatch-{context_sha}.json"
        if marker.exists() or marker.is_symlink():
            refuse("this exact context/lane already has an immutable dispatch attempt; verify it")
        _write_private(marker, canonical({"schema": "t64.base-dispatch-start",
            "schema_version": 1, "context_sha256": context_sha,
            "batch": batch.name, "lane": lane}))
        batch.mkdir(mode=0o700)
        consumer = batch / "consumer"
        _write_private(consumer, _private_bytes(retained, 512 * 1024 * 1024))
        consumer.chmod(0o700)
        adapter = _batch_adapter(snapshot, batch)
        for name, data in (("context.json", snapshot["context_bytes"]),
                           ("binding.json", snapshot["binding_bytes"]),
                           ("source-authority.json", snapshot["authority_bytes"]),
                           ("inventory.json", snapshot["inventory_bytes"])):
            _write_private(batch / name, data)
        code, stdout, stderr = cpu_runner.run_bounded([str(adapter), "--inventory",
            str(batch / "source-authority.json"), context["source_authority"]["sha256"],
            str(batch / "inventory.json")], cwd=ROOT, env=cpu_runner.make_env(),
            timeout=10, maximum=cpu_runner.MAX_GUEST_OUTPUT)
        _write_private(batch / "inventory-stdout.log", stdout)
        _write_private(batch / "inventory-stderr.log", stderr)
        if code != 0:
            refuse("real C inventory preflight refused the frozen source inputs")
        prepared.update(snapshot=snapshot, adapter=adapter, image=image, authority=authority)
    preflight_start = time.monotonic()
    verifier.verify_and_dispatch(context_root, batch, prepare,
                                repository_root=ROOT, pass_verified_snapshot=True)
    _write_private(batch / "preflight.json", canonical({
        "schema": "t64.base-preflight", "schema_version": 1,
        "elapsed_seconds": format(time.monotonic() - preflight_start, ".6f")}))
    snapshot, adapter, image = (prepared[key] for key in ("snapshot", "adapter", "image"))
    context_sha = hashlib.sha256(snapshot["context_bytes"]).hexdigest()
    rows = []
    stopped = False
    interrupted = False
    stop_reason = ""
    for ordinal, member in enumerate(prepared["authority"]["members"]):
        row = {"expected_rom_sha256": "", "original": str(batch / f"original-{ordinal}.bin"),
               "outcome": "unexecuted", "reason": "interrupted" if stopped else "not-selected",
               "record": str(batch / f"receipt-{ordinal}.json"),
               "registration_index": member["index"], "value_index": member["value_index"]}
        if lane == "required":
            selected = member["required"]
        elif lane == "informational":
            selected = member["lane"] in {"informational", "timing", "cycle", "cop0-hazard"}
        else:
            selected = member["lane"] == "stress"
        if member["execution"] == "blocked":
            row.update(outcome="blocked", reason=member["reason"])
        elif selected and not stopped:
            case = context_root / f"case-{secrets.token_hex(8)}"
            case_observed = {}
            def dispatch_case(current):
                if hashlib.sha256(current["context_bytes"]).hexdigest() != context_sha:
                    refuse("context changed between serial member attempts")
                _safe_adapter(current["context"], ROOT, adapter_path.absolute())
                case_observed["attempt"] = _bound_member_attempt(
                    case, current, adapter, image, member, cpu_runner)
            try:
                case_start = time.monotonic()
                verifier.verify_and_dispatch(context_root, case, dispatch_case,
                    repository_root=ROOT, pass_verified_snapshot=True)
                attempt, record, capture, image_sha = case_observed["attempt"]
                _write_private(case / "dispatch-timing.json", canonical({
                    "schema": "t64.base-dispatch-timing", "schema_version": 1,
                    "elapsed_seconds": format(time.monotonic() - case_start, ".6f")}))
                if (attempt["cleanup_verified"] and not attempt["timed_out"] and
                        record.is_file() and capture.is_file()):
                    row.update(outcome="record", reason="", expected_rom_sha256=image_sha,
                               record=str(record), original=str(capture))
                else:
                    row.update(outcome="timeout" if attempt["timed_out"] else "harness-error",
                        reason="host-timeout" if attempt["timed_out"] else "cleanup-unverified",
                        expected_rom_sha256=image_sha)
                if (not attempt["cleanup_verified"] or attempt["timed_out"] or
                        attempt["returncode"] not in (0, 2, 3)):
                    stopped = True
                    stop_reason = "member-harness-or-deadline-stop"
            except KeyboardInterrupt:
                stopped = interrupted = True
                stop_reason = "interrupted"
                row["reason"] = "interrupted"
                if case.exists():
                    row.update(outcome="harness-error", reason="cleanup-unverified",
                        expected_rom_sha256=image.members[(member["index"], member["value_index"])]["selected_rom_sha256"])
            except (OSError, ValueError) as exc:
                stopped = True
                stop_reason = str(exc)
                row["reason"] = "interrupted"
                if case.exists():
                    row.update(outcome="harness-error", reason="input-refused",
                        expected_rom_sha256=image.members[(member["index"], member["value_index"])]["selected_rom_sha256"])
        rows.append(row)
    code, report, manifest = _classify_manifest(adapter, snapshot, batch, rows, cpu_runner)
    result = {"schema": "t64.base-batch", "schema_version": 1,
        "context_sha256": context_sha, "lane": lane,
        "classification_input": manifest.name, "classification_report": report.name,
        "classification_exit_code": code, "interrupted": interrupted,
        "stopped": stopped, "stop_reason": stop_reason}
    _write_private(batch / "batch.json", canonical(result))
    return result


def verify_required(context_root: pathlib.Path, adapter_path: pathlib.Path,
                    *, lane: str = "required") -> dict:
    context_root = context_root.absolute()
    if lane not in {"required", "informational", "stress"}:
        refuse("BASE lane is not source-selected")
    if (os.environ.get("CI", "").lower() in {"1", "true", "yes"} or
            os.environ.get("T64_REQUIRE_FIXTURES") == "1"):
        refuse("private BASE verification is local-only")
    verifier = _load_module(ROOT / "scripts/verify-base-diagnostic-qualification.py",
                            "t64_base_preuse_verifier")
    cpu_runner = _load_module(CPU_RUNNER, "t64_cpu_member_runner")
    checked = {}
    def verify(snapshot):
        _safe_adapter(snapshot["context"], ROOT, adapter_path.absolute())
        _frozen_adapter(snapshot)
        context_sha = hashlib.sha256(snapshot["context_bytes"]).hexdigest()
        marker = _canonical_object(_private_bytes(
            context_root / f"{lane}-dispatch-{context_sha}.json"))
        if (set(marker) != {"schema", "schema_version", "context_sha256", "batch", "lane"} or
                marker.get("schema") != "t64.base-dispatch-start" or
                marker.get("schema_version") != 1 or marker.get("context_sha256") != context_sha or
                marker.get("lane") != lane or not isinstance(marker.get("batch"), str) or
                not marker["batch"].startswith("batch-") or
                pathlib.PurePosixPath(marker["batch"]).name != marker["batch"]):
            refuse("retained dispatch marker is malformed or belongs to another context")
        batch = context_root / marker["batch"]
        if batch.is_symlink() or not batch.is_dir() or stat.S_IMODE(batch.stat().st_mode) != 0o700:
            refuse("retained BASE batch is unavailable or unsafe")
        adapter = _batch_adapter(snapshot, batch)
        for name, expected in (("context.json", snapshot["context_bytes"]),
                               ("binding.json", snapshot["binding_bytes"]),
                               ("source-authority.json", snapshot["authority_bytes"]),
                               ("inventory.json", snapshot["inventory_bytes"])):
            if _private_bytes(batch / name) != expected:
                refuse("retained batch inputs differ from this exact verified context")
        result = _canonical_object(_private_bytes(batch / "batch.json"))
        if result.get("context_sha256") != context_sha or result.get("lane") != lane:
            refuse("retained batch receipt belongs to another target context")
        input_name = result.get("classification_input")
        if not isinstance(input_name, str) or pathlib.PurePosixPath(input_name).name != input_name:
            refuse("retained classification input escapes the immutable batch")
        manifest = _canonical_object(_private_bytes(batch / input_name))
        if (set(manifest) != {"schema", "schema_version", "members"} or
                manifest.get("schema") != "t64.base-classification-input" or
                manifest.get("schema_version") != 1 or not isinstance(manifest.get("members"), list)):
            refuse("retained classification input is malformed")
        image = _load_combined_source_image(snapshot["context"])
        authority = _canonical_object(snapshot["authority_bytes"])
        _validate_dispatch_seam(authority, image)
        expected = {(member["index"], member["value_index"]): member
                    for member in authority["members"]}
        rows = []
        for row in manifest["members"]:
            if not isinstance(row, dict) or set(row) != {"expected_rom_sha256", "original",
                    "outcome", "reason", "record", "registration_index", "value_index"}:
                refuse("retained member input is malformed")
            key = (row["registration_index"], row["value_index"])
            if key not in expected:
                refuse("retained member input is foreign to the verified source")
            for field in ("record", "original"):
                path = pathlib.Path(row[field])
                try:
                    if not path.is_absolute() or path != path.resolve(strict=True):
                        refuse("retained member path is not absolute and canonical")
                    path.resolve(strict=True).relative_to(context_root.resolve(strict=True))
                except (OSError, ValueError):
                    refuse("retained member input escapes the private context")
                cursor = path
                while cursor != context_root:
                    if cursor.is_symlink():
                        refuse("retained member input traverses a symlink")
                    cursor = cursor.parent
            record = _canonical_object(_private_bytes(pathlib.Path(row["record"]), 8192))
            outcome = record.get("outcome")
            selected = image.members.get(key)
            expected_sha = "" if outcome in {"blocked", "unexecuted"} else (
                selected["selected_rom_sha256"] if selected is not None else "")
            if expected_sha != row["expected_rom_sha256"]:
                refuse("retained member image differs from independent qualification")
            rows.append({**row, "outcome": "record", "reason": ""})
        code, report, _ = _classify_manifest(adapter, snapshot, batch, rows, cpu_runner)
        checked.update(classification_exit_code=code, classification_report=str(report))
    verifier.verify_and_dispatch(context_root, context_root / f"verify-{secrets.token_hex(8)}",
                                verify, repository_root=ROOT, pass_verified_snapshot=True)
    return checked


def run_one(context_root: pathlib.Path, run_directory: pathlib.Path,
            adapter_path: pathlib.Path, *, repository_root: pathlib.Path | None = None,
            dispatch=None) -> dict:
    repository_root = (ROOT if repository_root is None else repository_root).resolve(strict=True)
    context_root = context_root.absolute()
    run_directory = run_directory.absolute()
    adapter_path = adapter_path.absolute()
    verifier = _load_module(
        ROOT / "scripts/verify-base-diagnostic-qualification.py",
        "t64_base_preuse_verifier",
    )
    observed: dict = {}

    def normal_dispatch(verified_snapshot) -> dict:
        context = verified_snapshot["context"]
        adapter = _safe_adapter(context, repository_root, adapter_path)
        if dispatch is None:
            attempt = _production_attempt(
                run_directory, adapter, context, repository_root
            )
        else:
            run_directory.mkdir(mode=0o700)
            attempt = dispatch(run_directory)
        if not isinstance(attempt, dict):
            refuse("member dispatch did not return a closed attempt result")
        status = classify_attempt(attempt)
        observed.update(attempt)
        result = {
            "attempts": 1,
            "cleanup_verified": attempt["cleanup_verified"],
            "returncode": attempt.get("returncode"),
            "schema": "t64.base-member-attempt",
            "schema_version": 1,
            "status": status,
            "timed_out": attempt["timed_out"],
        }
        _write_private(
            run_directory / "attempt.json",
            (json.dumps(result, sort_keys=True, separators=(",", ":")) + "\n").encode(),
        )
        return result

    try:
        result = verifier.verify_and_dispatch(
            context_root, run_directory, normal_dispatch,
            repository_root=repository_root,
            pass_verified_snapshot=True,
        )
    except Exception as exc:
        if isinstance(exc, (KeyboardInterrupt, SystemExit)):
            raise
        if run_directory.exists() and not observed:
            refuse("pre-use or setup refusal occurred before member dispatch")
        raise
    if result.get("status") != "pass":
        print("classification=attempt-retained")
    return result


def main(argv=None) -> int:
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--run-one", action="store_true")
    mode.add_argument("--run-all", action="store_true")
    mode.add_argument("--verify-complete", action="store_true")
    parser.add_argument("--local-only", action="store_true")
    parser.add_argument("--adapter", required=True)
    args = parser.parse_args(argv)
    if not args.run_one and not args.local_only:
        print("BASE diagnostic refused: bound batch commands require --local-only", file=sys.stderr)
        return 4
    run_directory = PRIVATE / f"run-{secrets.token_hex(8)}"
    try:
        if args.run_all:
            result = run_required(PRIVATE, pathlib.Path(args.adapter))
        elif args.verify_complete:
            result = verify_required(PRIVATE, pathlib.Path(args.adapter))
        else:
            result = run_one(PRIVATE, run_directory, pathlib.Path(args.adapter))
    except (OSError, ValueError) as exc:
        print(f"BASE diagnostic refused: {exc}", file=sys.stderr)
        return 4
    if args.run_one:
        return 0 if result.get("status") == "pass" else 3
    print("classification=retained-private-see-completeness-and-pass-flags")
    return 130 if result.get("interrupted") else result["classification_exit_code"]


if __name__ == "__main__":
    raise SystemExit(main())

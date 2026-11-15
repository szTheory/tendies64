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
from types import SimpleNamespace

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-base-diagnostic"
CPU_RUNNER = ROOT / "scripts/run-cpu-diagnostic.py"
RUNNER_RELATIVE = "scripts/run-base-diagnostic.py"


class Refusal(ValueError):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def digest_file(path: pathlib.Path, maximum: int = 512 * 1024 * 1024) -> str:
    before = path.lstat()
    if path.is_symlink() or not stat.S_ISREG(before.st_mode) or before.st_size <= 0 or before.st_size > maximum:
        refuse("qualified adapter is not a bounded regular file")
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
    hasher = hashlib.sha256()
    try:
        opened = os.fstat(descriptor)
        if (opened.st_dev, opened.st_ino, opened.st_size) != (before.st_dev, before.st_ino, before.st_size):
            refuse("qualified adapter changed while opening")
        while True:
            block = os.read(descriptor, 1024 * 1024)
            if not block:
                break
            hasher.update(block)
    finally:
        os.close(descriptor)
    return hasher.hexdigest()


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
    expected = expected_path.read_bytes()
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
            len(context.base_bytes) > 4_210_688 or offset < 0 or offset + 4 > len(context.base_bytes)):
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
        cleanup_verified = timed_out or "capture limit" in str(exc)
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

    def normal_dispatch() -> dict:
        context = json.loads((context_root / "context.json").read_bytes())
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
    parser.add_argument("--run-one", action="store_true", required=True)
    parser.add_argument("--adapter", required=True)
    args = parser.parse_args(argv)
    run_directory = PRIVATE / f"run-{secrets.token_hex(8)}"
    try:
        result = run_one(PRIVATE, run_directory, pathlib.Path(args.adapter))
    except (OSError, Refusal) as exc:
        print(f"BASE diagnostic refused: {exc}", file=sys.stderr)
        return 4
    return 0 if result.get("status") == "pass" else 3


if __name__ == "__main__":
    raise SystemExit(main())

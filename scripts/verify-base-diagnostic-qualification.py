#!/usr/bin/env python3
"""Independent private pre-use gate for the whole-BASE diagnostic context."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import re
import stat
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE_RELATIVE = pathlib.PurePosixPath("build/private-base-diagnostic")
HEX64 = re.compile(r"^[0-9a-f]{64}$")
HEX40 = re.compile(r"^[0-9a-f]{40}$")
PROJECTION_RELATIVE = "build/private-cpu-diagnostic/source-projection.json"
AUTHORITY_RELATIVE = "build/private-base-diagnostic/source-authority.json"
INVENTORY_RELATIVE = "build/private-base-diagnostic/inventory.json"
RETENTION_DIRECTORY = "execution-snapshots"
RETENTION_FILES = {"binding.json", "context.json", "executable"}
MAX_RETAINED_EXECUTABLE_BYTES = 512 * 1024 * 1024
CONTEXT_KEYS = {
    "bindings", "inventory", "profile", "schema", "schema_version",
    "source_authority", "source_projection", "source_revision",
}
RSP_PROJECTION_RELATIVE = "build/private-rsp-diagnostic/rsp-source-projection.json"
RSP_ROLES = {"rsp_qualification", "rsp_source_closure", "rsp_overlay"}
BINDING_KEYS = {
    "context_sha256", "inventory_sha256", "schema", "schema_version",
    "source_authority_sha256", "source_projection_sha256",
}
ROLES = {
    "asset", "boot", "build", "compiler", "composition", "executable",
    "image", "lock", "profile", "runtime", "source", "tool", "verifier",
    "source_verifier",
}


class Refusal(ValueError):
    pass


def refuse(message: str) -> None:
    raise Refusal(message)


def canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _project_path(repository_root: pathlib.Path, relative: object,
                  label: str) -> pathlib.Path:
    if (not isinstance(relative, str) or not relative or "\\" in relative):
        refuse(f"{label} path is malformed")
    posix = pathlib.PurePosixPath(relative)
    if posix.is_absolute() or any(part in {"", ".", ".."} for part in posix.parts):
        refuse(f"{label} path escapes the repository")
    candidate = repository_root.joinpath(*posix.parts)
    cursor = repository_root
    for part in posix.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse(f"{label} path traverses a symlink")
    try:
        candidate.relative_to(repository_root)
    except ValueError:
        refuse(f"{label} path escapes the repository")
    if not candidate.is_file():
        refuse(f"{label} is missing or not a regular file")
    mode = stat.S_IMODE(candidate.stat(follow_symlinks=False).st_mode)
    if mode & 0o022:
        refuse(f"{label} is group- or world-writable")
    return candidate


def _private_file(path: pathlib.Path, label: str,
                  maximum: int = 4 * 1024 * 1024) -> bytes:
    try:
        before = path.lstat()
        if (not stat.S_ISREG(before.st_mode) or
                stat.S_IMODE(before.st_mode) != 0o600 or
                not 0 < before.st_size <= maximum):
            refuse(f"{label} is not a bounded owner-only regular file")
        descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) |
                             getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(descriptor, "rb") as stream:
            opened = os.fstat(stream.fileno())
            if (not stat.S_ISREG(opened.st_mode) or
                    stat.S_IMODE(opened.st_mode) != 0o600 or
                    (opened.st_dev, opened.st_ino, opened.st_size) !=
                    (before.st_dev, before.st_ino, before.st_size)):
                refuse(f"{label} changed while opening")
            data = stream.read(maximum + 1)
            after = os.fstat(stream.fileno())
            if (len(data) != opened.st_size or len(data) > maximum or
                    (after.st_size, after.st_mtime_ns, after.st_ctime_ns) !=
                    (opened.st_size, opened.st_mtime_ns, opened.st_ctime_ns)):
                refuse(f"{label} changed while reading")
    except OSError as exc:
        refuse(f"{label} cannot be read: {exc}")
    if not data:
        refuse(f"{label} is empty")
    return data


def _canonical_json(data: bytes, label: str) -> dict:
    try:
        value = json.loads(data)
    except (UnicodeError, json.JSONDecodeError) as exc:
        refuse(f"{label} is invalid JSON: {exc}")
    if not isinstance(value, dict) or canonical(value) != data:
        refuse(f"{label} is not a canonical closed JSON object")
    return value


def _load_deriver(repository_root: pathlib.Path):
    path = repository_root / "scripts/derive-base-diagnostic-source.py"
    if path.is_symlink() or not path.is_file():
        refuse("BASE source derivation entry is missing or linked")
    spec = importlib.util.spec_from_file_location("t64_base_source_deriver", path)
    if spec is None or spec.loader is None:
        refuse("BASE source derivation entry cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except Exception as exc:
        refuse(f"BASE source derivation entry failed to load: {exc}")
    return module


def _verify_bindings(context: dict, repository_root: pathlib.Path,
                     projection: dict) -> None:
    bindings = context.get("bindings")
    roles = ROLES | RSP_ROLES if context.get("schema_version") == 2 else ROLES
    if not isinstance(bindings, dict) or set(bindings) != roles:
        refuse("context bindings do not match the reviewed closed role set")
    seen_paths: set[str] = set()
    for role in sorted(roles):
        rows = bindings[role]
        if not isinstance(rows, list) or not rows:
            refuse(f"{role} binding is missing")
        for row in rows:
            if (not isinstance(row, dict) or set(row) != {"path", "sha256"}
                    or not isinstance(row.get("path"), str)
                    or not isinstance(row.get("sha256"), str)
                    or HEX64.fullmatch(row["sha256"]) is None):
                refuse(f"{role} binding row is malformed")
            relative = row["path"]
            if relative in seen_paths:
                refuse("context contains duplicate file bindings")
            seen_paths.add(relative)
            path = _project_path(repository_root, relative, f"{role} binding")
            if _regular_digest(path) != row["sha256"]:
                refuse(f"{role} closure digest changed")

    reviewed = projection.get("reviewed_source_file_sha256")
    testlist_sha = projection.get("testlist_sha256")
    if not isinstance(reviewed, dict) or not isinstance(testlist_sha, str) or HEX64.fullmatch(testlist_sha) is None:
        refuse("accepted projection has no reviewed source file partition")
    expected_source = dict(reviewed)
    expected_source.setdefault("src/tests/testlist.rs", testlist_sha)
    source_rows = bindings["source"]
    actual_source: dict[str, str] = {}
    source_root = pathlib.PurePosixPath("build/private-cpu-diagnostic/nemu64-test")
    for row in source_rows:
        path = pathlib.PurePosixPath(row["path"])
        try:
            relative = path.relative_to(source_root).as_posix()
        except ValueError:
            refuse("source closure path is outside the accepted source checkout")
        if relative in actual_source:
            refuse("source closure contains duplicate source paths")
        actual_source[relative] = row["sha256"]
    if actual_source != expected_source:
        refuse("source closure differs from the accepted source projection")

    verifier_rows = bindings["verifier"]
    expected_verifier = "scripts/verify-base-diagnostic-qualification.py"
    if len(verifier_rows) != 1 or verifier_rows[0]["path"] != expected_verifier:
        refuse("host verifier binding does not name the production entry")
    source_verifier_rows = bindings["source_verifier"]
    if (len(source_verifier_rows) != 1
            or source_verifier_rows[0]["path"] != "scripts/verify-cpu-diagnostic-qualification.py"):
        refuse("reviewed source verifier binding does not name the accepted entry")


def _regular_digest(path: pathlib.Path) -> str:
    """Hash bounded regular bytes without a pathname-to-FIFO blocking race."""
    maximum = MAX_RETAINED_EXECUTABLE_BYTES
    before = path.lstat()
    if (not stat.S_ISREG(before.st_mode) or before.st_mode & 0o022 or
            not 0 < before.st_size <= maximum):
        refuse("bound input is not a bounded regular file")
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) |
                         getattr(os, "O_NONBLOCK", 0))
    hasher = hashlib.sha256()
    total = 0
    with os.fdopen(descriptor, "rb") as stream:
        opened = os.fstat(stream.fileno())
        if (not stat.S_ISREG(opened.st_mode) or opened.st_mode & 0o022 or
                (opened.st_dev, opened.st_ino, opened.st_size) !=
                (before.st_dev, before.st_ino, before.st_size)):
            refuse("bound input changed while opening")
        while True:
            block = stream.read(min(1024 * 1024, maximum + 1 - total))
            if not block:
                break
            total += len(block)
            if total > maximum:
                refuse("bound input exceeded its size bound")
            hasher.update(block)
        after = os.fstat(stream.fileno())
        if (total != opened.st_size or
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns) !=
                (opened.st_size, opened.st_mtime_ns, opened.st_ctime_ns)):
            refuse("bound input changed while reading")
    return hasher.hexdigest()


def _verify_retained_execution_pair(context_root: pathlib.Path,
                                    context: dict, context_bytes: bytes,
                                    binding_bytes: bytes) -> None:
    context_sha256 = sha256(context_bytes)
    directory_flags = (os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) |
                       getattr(os, "O_NOFOLLOW", 0))
    file_flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)

    def check_directory(descriptor: int, label: str) -> None:
        info = os.fstat(descriptor)
        if (not stat.S_ISDIR(info.st_mode) or
                stat.S_IMODE(info.st_mode) != 0o700 or
                info.st_uid != os.geteuid()):
            refuse(f"{label} is not an owner-only directory")

    def read_private_entry(directory_fd: int, name: str, label: str) -> bytes:
        try:
            descriptor = os.open(name, file_flags, dir_fd=directory_fd)
        except OSError as exc:
            refuse(f"{label} is missing or linked: {exc}")
        try:
            before = os.fstat(descriptor)
            if (not stat.S_ISREG(before.st_mode) or
                    stat.S_IMODE(before.st_mode) != 0o600 or
                    before.st_uid != os.geteuid() or
                    before.st_size <= 0 or
                    before.st_size > MAX_RETAINED_EXECUTABLE_BYTES):
                refuse(f"{label} is not a bounded owner-only regular file")
            pieces = []
            total = 0
            while True:
                block = os.read(descriptor, 1024 * 1024)
                if not block:
                    break
                total += len(block)
                if total > MAX_RETAINED_EXECUTABLE_BYTES:
                    refuse(f"{label} exceeds its retention size bound")
                pieces.append(block)
            after = os.fstat(descriptor)
            if ((after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) !=
                    (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) or
                    total != before.st_size):
                refuse(f"{label} changed while being read")
            return b"".join(pieces)
        finally:
            os.close(descriptor)

    root_fd = os.open(context_root, directory_flags)
    try:
        check_directory(root_fd, "private BASE root")
        try:
            snapshots_fd = os.open(
                RETENTION_DIRECTORY, directory_flags, dir_fd=root_fd
            )
        except OSError as exc:
            refuse(f"retained BASE execution snapshot collection is missing or linked: {exc}")
        try:
            check_directory(snapshots_fd, "retained BASE snapshot collection")
            try:
                snapshot_fd = os.open(
                    context_sha256, directory_flags, dir_fd=snapshots_fd
                )
            except OSError as exc:
                refuse(f"retained BASE execution snapshot is missing or linked: {exc}")
            try:
                check_directory(snapshot_fd, "retained BASE execution snapshot")
                retained_names = set(os.listdir(snapshot_fd))
                missing_names = RETENTION_FILES - retained_names
                if missing_names:
                    name = sorted(missing_names)[0]
                    refuse(f"retained BASE {name} is missing or linked")
                if retained_names != RETENTION_FILES:
                    refuse("retained BASE execution snapshot has an unexpected file set")
                snapshot = {
                    name: read_private_entry(
                        snapshot_fd, name, f"retained BASE {name}"
                    )
                    for name in sorted(RETENTION_FILES)
                }
            finally:
                os.close(snapshot_fd)
        finally:
            os.close(snapshots_fd)
    finally:
        os.close(root_fd)

    if (snapshot["context.json"] != context_bytes or
            snapshot["binding.json"] != binding_bytes):
        refuse("retained BASE context/binding pair differs from the active pair")
    executable_rows = context.get("bindings", {}).get("executable")
    if not isinstance(executable_rows, list) or len(executable_rows) != 1:
        refuse("active BASE executable binding is not singular")
    executable_binding = executable_rows[0]
    if (not isinstance(executable_binding, dict) or
            not isinstance(executable_binding.get("sha256"), str) or
            HEX64.fullmatch(executable_binding["sha256"]) is None):
        refuse("active BASE executable binding is malformed")
    if sha256(snapshot["executable"]) != executable_binding["sha256"]:
        refuse("retained BASE executable digest differs from the active binding")


def _verify_live_source_authority(repository_root: pathlib.Path,
                                  projection: dict, *,
                                  source_closure_checked_by_rsp: bool = False) -> None:
    if repository_root == ROOT.resolve(strict=True):
        path = repository_root / "scripts/verify-cpu-diagnostic-qualification.py"
        spec = importlib.util.spec_from_file_location("t64_reviewed_cpu_source", path)
        if spec is None or spec.loader is None:
            refuse("reviewed source verifier cannot be loaded")
        module = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(module)
            if (projection.get("source_revision") != module.SOURCE_REVISION
                    or projection.get("projection_sha256") != module.SOURCE_PROJECTION_SHA256):
                refuse("source projection differs from the independently reviewed seal")
            if not source_closure_checked_by_rsp:
                receipt = repository_root / "build/private-cpu-diagnostic/qualification.json"
                if receipt.is_symlink() or not receipt.is_file():
                    refuse("accepted CPU source qualification receipt is missing or linked")
                module.validate(receipt, repository_root)
        except Refusal:
            raise
        except Exception as exc:
            refuse(f"accepted source qualification refused current inputs: {exc}")
        return
    temp_root = pathlib.Path(tempfile.gettempdir()).resolve()
    try:
        repository_root.relative_to(temp_root)
    except ValueError:
        refuse("non-production qualification roots are restricted to isolated fixtures")


def _verify_rsp_source_authority(repository_root: pathlib.Path, context: dict,
                                 projection: dict, projection_bytes: bytes) -> dict:
    reference = context.get("rsp_source_projection")
    if (not isinstance(reference, dict) or set(reference) != {"path", "sha256"} or
            reference.get("path") != RSP_PROJECTION_RELATIVE):
        refuse("RSP source projection reference is malformed")
    path = _project_path(repository_root, reference["path"], "RSP source projection")
    data = _private_file(path, "RSP source projection", 16 * 1024 * 1024)
    rsp = _canonical_json(data, "RSP source projection")
    if (sha256(data) != reference.get("sha256") or
            rsp.get("source_revision") != projection.get("source_revision") or
            rsp.get("inputs", {}).get("cpu_source_projection_sha256") !=
                sha256(projection_bytes)):
        refuse("RSP source projection differs from the accepted CPU source context")
    # This is a reviewed provider, not a membership function nominated by input.
    provider_path = ROOT / "scripts/verify-rsp-diagnostic-qualification.py"
    spec = importlib.util.spec_from_file_location("t64_base_rsp_source_provider", provider_path)
    if spec is None or spec.loader is None:
        refuse("reviewed RSP source verifier cannot be loaded")
    provider = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(provider)
    if rsp.get("registrations") != provider.derive_rsp_membership(projection.get("registrations")):
        refuse("RSP projection differs from independently derived source membership")
    expected_paths = {
        "rsp_qualification": "build/private-rsp-diagnostic/qualification.json",
        "rsp_source_closure": "build/private-rsp-diagnostic/rsp-preuse-closure.json",
        "rsp_overlay": "build/private-rsp-diagnostic/rsp-overlay-preparation.json",
    }
    for role, relative in expected_paths.items():
        rows = context["bindings"].get(role)
        if not isinstance(rows, list) or len(rows) != 1 or rows[0].get("path") != relative:
            refuse(f"{role} does not bind its fixed reviewed input")
    qualification = _canonical_json(_private_file(
        repository_root / expected_paths["rsp_qualification"], "RSP qualification"),
        "RSP qualification")
    membership = qualification.get("membership")
    if (not isinstance(membership, dict) or membership.get("path") != reference["path"] or
            membership.get("sha256") != sha256(data)):
        refuse("RSP qualification does not authenticate the projected membership")
    if repository_root == ROOT.resolve(strict=True):
        for relative in ("scripts/prepare-rsp-diagnostic.py",
                         "scripts/verify-rsp-diagnostic-qualification.py"):
            if not any(row.get("path") == relative and
                       row.get("sha256") == _regular_digest(repository_root / relative)
                       for row in context["bindings"]["build"]):
                refuse("RSP source provider is absent from the immutable build closure")
        try:
            provider.verify_preuse(repository_root / expected_paths["rsp_qualification"],
                                   verify_current_identity=False)
        except Exception as exc:
            refuse(f"accepted RSP source closure refused current inputs: {exc}")
    return rsp


def _component_source_files(repository_root, context, deriver):
    """Recover exact supported-module maps from the independently checked receipts."""
    cpu_path = "build/private-cpu-diagnostic/qualification.json"
    cpu_bytes = _private_file(_project_path(repository_root, cpu_path, "CPU qualification"),
                              "CPU qualification", 16 * 1024 * 1024)
    if not any(row.get("path") == cpu_path and row.get("sha256") == sha256(cpu_bytes)
               for row in context["bindings"]["runtime"]):
        refuse("accepted CPU source qualification is absent from the immutable runtime closure")
    cpu = json.loads(cpu_bytes)
    reference = cpu.get("assets", {}).get("closure_report")
    if (not isinstance(reference, dict) or set(reference) != {"path", "sha256"}):
        refuse("accepted CPU module closure reference is malformed")
    closure_bytes = _private_file(_project_path(repository_root, reference["path"],
        "CPU source closure"), "CPU source closure", 16 * 1024 * 1024)
    if (sha256(closure_bytes) != reference["sha256"] or
            not any(row.get("path") == reference["path"] and
                    row.get("sha256") == reference["sha256"]
                    for row in context["bindings"]["asset"])):
        refuse("accepted CPU module closure differs from its immutable asset binding")
    cpu_closure = json.loads(closure_bytes)
    rsp_closure = json.loads(_private_file(repository_root /
        "build/private-rsp-diagnostic/rsp-preuse-closure.json", "RSP source closure",
        16 * 1024 * 1024))
    if (cpu_closure.get("status") != "PASS_PREBUILD_SOURCE" or
            rsp_closure.get("status") != "PASS_PREBUILD_RSP" or
            cpu_closure.get("source_revision") != context["source_revision"] or
            rsp_closure.get("source_revision") != context["source_revision"]):
        refuse("component module closures target another source or lack qualification")
    return (deriver._private_source_files(cpu_closure.get("module_files"), "CPU source"),
            deriver._private_source_files(rsp_closure.get("overlay", {}).get("files"), "RSP overlay"))


def verify_and_dispatch(context_root: pathlib.Path, run_directory: pathlib.Path,
                        dispatch, repository_root: pathlib.Path = ROOT,
                        *, pass_verified_snapshot: bool = False):
    """Verify all bound inputs before creating a run directory or dispatching."""
    repository_root = repository_root.resolve(strict=True)
    context_root = context_root.absolute()
    if context_root.is_symlink():
        refuse("BASE qualification root is linked")
    context_root = context_root.resolve(strict=True)
    expected_root = repository_root.joinpath(*PRIVATE_RELATIVE.parts)
    if context_root != expected_root:
        refuse("BASE qualification context is outside its fixed private root")
    if context_root.is_symlink() or not context_root.is_dir():
        refuse("BASE qualification root is missing or linked")
    if stat.S_IMODE(context_root.stat(follow_symlinks=False).st_mode) != 0o700:
        refuse("BASE qualification root permissions are not owner-only")

    context_bytes = _private_file(context_root / "context.json", "context")
    binding_bytes = _private_file(context_root / "binding.json", "binding")
    context = _canonical_json(context_bytes, "context")
    binding = _canonical_json(binding_bytes, "binding")
    version = context.get("schema_version")
    context_keys = CONTEXT_KEYS | {"rsp_source_projection"} if version == 2 else CONTEXT_KEYS
    if (set(context) != context_keys or context.get("schema") != "t64.base-qualification-context"
            or type(version) is not int or version not in (1, 2)):
        refuse("context fields do not match the closed schema")
    if set(binding) != BINDING_KEYS or binding.get("schema") != "t64.base-qualification-binding" or binding.get("schema_version") != 1:
        refuse("binding fields do not match the closed schema")
    if sha256(context_bytes) != binding.get("context_sha256"):
        refuse("immutable context digest changed")
    if context.get("profile") != "tier-b-t1":
        refuse("BASE context is not the qualified Tier B/T1 profile")
    source_revision = context.get("source_revision")
    if not isinstance(source_revision, str) or HEX40.fullmatch(source_revision) is None:
        refuse("context source revision is malformed")

    projection_ref = context.get("source_projection")
    if (not isinstance(projection_ref, dict)
            or set(projection_ref) != {"path", "sha256"}
            or projection_ref.get("path") != PROJECTION_RELATIVE):
        refuse("source projection binding is malformed")
    projection_path = _project_path(repository_root, projection_ref["path"], "source projection")
    projection_bytes = _private_file(projection_path, "source projection", 16 * 1024 * 1024)
    projection = _canonical_json(projection_bytes, "source projection")
    projection_seal = projection.get("projection_sha256")
    projection_body = {key: value for key, value in projection.items() if key != "projection_sha256"}
    if (sha256(projection_bytes) != projection_ref.get("sha256")
            or not isinstance(projection_seal, str)
            or sha256(canonical(projection_body)) != projection_seal
            or projection.get("source_revision") != source_revision):
        refuse("source projection identity or seal changed")

    _verify_bindings(context, repository_root, projection)
    rsp_projection = _verify_rsp_source_authority(
        repository_root, context, projection, projection_bytes) if version == 2 else None
    deriver = _load_deriver(repository_root)
    try:
        if version == 2:
            cpu_files, rsp_files = _component_source_files(repository_root, context, deriver)
            members = deriver.derive_members(projection, rsp_projection, cpu_files, rsp_files)
        else:
            members = deriver.derive_members(projection)
    except Exception as exc:
        refuse(f"source projection cannot derive BASE membership: {exc}")
    authority_bytes = canonical({
        "members": members,
        "schema": "t64.base-source-authority",
        "schema_version": 1,
        "source_revision": source_revision,
    })
    authority_sha = sha256(authority_bytes)
    inventory_bytes = canonical({
        "members": members,
        "schema": "t64.base-inventory",
        "schema_version": 1,
        "source_authority_sha256": authority_sha,
        "source_revision": source_revision,
    })

    for key, expected_bytes in (("source_authority", authority_bytes),
                                ("inventory", inventory_bytes)):
        reference = context.get(key)
        expected_path = AUTHORITY_RELATIVE if key == "source_authority" else INVENTORY_RELATIVE
        if (not isinstance(reference, dict) or set(reference) != {"path", "sha256"}
                or reference.get("path") != expected_path):
            refuse(f"{key.replace('_', ' ')} reference is malformed")
        path = _project_path(repository_root, reference.get("path"), key)
        actual_bytes = _private_file(path, key)
        if actual_bytes != expected_bytes or sha256(actual_bytes) != reference.get("sha256"):
            refuse(f"{key.replace('_', ' ')} differs from the independently derived authority")

    if (binding.get("source_authority_sha256") != authority_sha
            or binding.get("inventory_sha256") != sha256(inventory_bytes)
            or binding.get("source_projection_sha256") != sha256(projection_bytes)):
        refuse("independent source binding does not match the reviewed projection")
    _verify_retained_execution_pair(
        context_root, context, context_bytes, binding_bytes
    )
    if version == 2:
        # The fixed RSP provider above already performs CPU.validate. Keep the
        # independent CPU anchor check here without repeating that entire audit.
        # This flag is internal control flow, never a context/CLI field.
        _verify_live_source_authority(repository_root, projection,
            source_closure_checked_by_rsp=repository_root == ROOT.resolve(strict=True))
    else:
        _verify_live_source_authority(repository_root, projection)

    run_directory = run_directory.absolute()
    try:
        run_parent = run_directory.parent.resolve(strict=True)
    except OSError:
        refuse("run directory parent is missing")
    if run_parent != context_root or run_directory.exists() or run_directory.is_symlink():
        refuse("run directory must be a fresh child of the private context")
    if not callable(dispatch):
        refuse("normal dispatch callback is unavailable")
    if pass_verified_snapshot:
        return dispatch({"context": context, "context_bytes": context_bytes,
                         "binding_bytes": binding_bytes,
                         "authority_bytes": authority_bytes,
                         "inventory_bytes": inventory_bytes})
    return dispatch()


def main() -> int:
    if len(sys.argv) != 4 or sys.argv[1] != "--verify":
        print("usage: verify-base-diagnostic-qualification.py --verify PRIVATE_ROOT RUN_DIRECTORY", file=sys.stderr)
        return 2
    context_root = pathlib.Path(sys.argv[2])
    run_directory = pathlib.Path(sys.argv[3])
    try:
        verify_and_dispatch(context_root, run_directory,
                            lambda: "pre-use-context-accepted")
    except (OSError, Refusal) as exc:
        print(f"pre-use BASE qualification refused: {exc}", file=sys.stderr)
        return 4
    print("pre-use BASE qualification accepted")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

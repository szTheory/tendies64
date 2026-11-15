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
CONTEXT_KEYS = {
    "bindings", "inventory", "profile", "schema", "schema_version",
    "source_authority", "source_projection", "source_revision",
}
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


def _private_file(path: pathlib.Path, label: str) -> bytes:
    if path.is_symlink() or not path.is_file():
        refuse(f"{label} is missing or linked")
    if stat.S_IMODE(path.stat(follow_symlinks=False).st_mode) != 0o600:
        refuse(f"{label} permissions are not owner-only")
    try:
        data = path.read_bytes()
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
    if not isinstance(bindings, dict) or set(bindings) != ROLES:
        refuse("context bindings do not match the reviewed closed role set")
    seen_paths: set[str] = set()
    for role in sorted(ROLES):
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
            if sha256(path.read_bytes()) != row["sha256"]:
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


def _verify_live_source_authority(repository_root: pathlib.Path,
                                  projection: dict) -> None:
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


def verify_and_dispatch(context_root: pathlib.Path, run_directory: pathlib.Path,
                        dispatch, repository_root: pathlib.Path = ROOT):
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
    if set(context) != CONTEXT_KEYS or context.get("schema") != "t64.base-qualification-context" or context.get("schema_version") != 1:
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
    projection_bytes = _private_file(projection_path, "source projection")
    projection = _canonical_json(projection_bytes, "source projection")
    projection_seal = projection.get("projection_sha256")
    projection_body = {key: value for key, value in projection.items() if key != "projection_sha256"}
    if (sha256(projection_bytes) != projection_ref.get("sha256")
            or not isinstance(projection_seal, str)
            or sha256(canonical(projection_body)) != projection_seal
            or projection.get("source_revision") != source_revision):
        refuse("source projection identity or seal changed")

    deriver = _load_deriver(repository_root)
    try:
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
    _verify_bindings(context, repository_root, projection)
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

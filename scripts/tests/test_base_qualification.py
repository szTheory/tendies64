#!/usr/bin/env python3
"""Behavioral controls for full BASE pre-use qualification and dispatch."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import shutil
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
VERIFIER = ROOT / "scripts/verify-base-diagnostic-qualification.py"


def canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_private(path: pathlib.Path, data: bytes) -> None:
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    path.write_bytes(data)
    path.chmod(0o600)


def load_verifier():
    spec = importlib.util.spec_from_file_location("t64_base_qualification", VERIFIER)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load production BASE verifier")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_deriver():
    path = ROOT / "scripts/derive-base-diagnostic-source.py"
    spec = importlib.util.spec_from_file_location("t64_base_source_deriver", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load production BASE source derivation")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def create_context(repository: pathlib.Path) -> pathlib.Path:
    context_root = repository / "build/private-base-diagnostic"
    context_root.mkdir(mode=0o700, parents=True)
    projection_root = repository / "build/private-cpu-diagnostic"
    projection_root.mkdir(mode=0o700, parents=True)
    rows = [
        {
            "effective_value_count": 1,
            "group": "arithmetic",
            "index": 0,
            "lane": "required-base",
            "level": "BasicFunctionality",
            "name": "empty selector",
            "reason": "required",
            "source_group": "arithmetic",
            "source_value_count": 0,
            "source_value_ids": ["once"],
            "values": [{"id": "once", "index": 0, "selector": "r0000v00000"}],
        },
        {
            "effective_value_count": 1,
            "group": "non-cpu",
            "index": 1,
            "lane": "skipped-non-cpu",
            "level": "RDPBasic",
            "name": "blocked asset",
            "reason": "not in cpu profile",
            "source_group": "rdp",
            "source_value_count": 1,
            "source_value_ids": ["asset-value"],
            "values": [],
        },
    ]
    source_root = projection_root / "nemu64-test/src/tests"
    source_root.mkdir(mode=0o700, parents=True)
    testlist = source_root / "testlist.rs"
    testlist.write_bytes(b"authored source testlist\n")
    testlist.chmod(0o600)
    projection_body = {
        "registrations": rows,
        "reviewed_source_file_sha256": {
            "src/tests/testlist.rs": digest(testlist.read_bytes()),
        },
        "schema": "t64.cpu-source-projection",
        "schema_version": 2,
        "source_revision": "a" * 40,
        "testlist_sha256": digest(testlist.read_bytes()),
    }
    projection = dict(projection_body)
    projection["projection_sha256"] = digest(canonical(projection_body))
    projection_bytes = canonical(projection)
    projection_path = projection_root / "source-projection.json"
    write_private(projection_path, projection_bytes)
    deriver = load_deriver()
    source_rows = deriver.derive_members(projection)
    authority_bytes = canonical({
        "members": source_rows,
        "schema": "t64.base-source-authority",
        "schema_version": 1,
        "source_revision": projection_body["source_revision"],
    })
    authority_sha = digest(authority_bytes)
    inventory_bytes = canonical({
        "members": source_rows,
        "schema": "t64.base-inventory",
        "schema_version": 1,
        "source_authority_sha256": authority_sha,
        "source_revision": projection_body["source_revision"],
    })
    authority_path = context_root / "source-authority.json"
    inventory_path = context_root / "inventory.json"
    write_private(authority_path, authority_bytes)
    write_private(inventory_path, inventory_bytes)

    scripts = repository / "scripts"
    scripts.mkdir(mode=0o700)
    shutil.copyfile(ROOT / "scripts/derive-base-diagnostic-source.py",
                    scripts / "derive-base-diagnostic-source.py")
    shutil.copyfile(VERIFIER, scripts / "verify-base-diagnostic-qualification.py")
    shutil.copyfile(ROOT / "scripts/verify-cpu-diagnostic-qualification.py",
                    scripts / "verify-cpu-diagnostic-qualification.py")

    roles = (
        "source", "tool", "lock", "runtime", "boot", "image", "composition",
        "profile", "build", "compiler", "asset", "executable", "verifier",
        "source_verifier",
    )
    bindings = {}
    for role in roles:
        if role == "source":
            source = projection_root / "nemu64-test/src/tests/testlist.rs"
            bindings[role] = [{
                "path": source.relative_to(repository).as_posix(),
                "sha256": digest(source.read_bytes()),
            }]
            continue
        if role in {"verifier", "source_verifier"}:
            name = ("verify-base-diagnostic-qualification.py" if role == "verifier"
                    else "verify-cpu-diagnostic-qualification.py")
            verifier_path = scripts / name
            bindings[role] = [{
                "path": verifier_path.relative_to(repository).as_posix(),
                "sha256": digest(verifier_path.read_bytes()),
            }]
            continue
        path = context_root / f"{role}.evidence"
        content = f"authored {role} evidence\n".encode()
        write_private(path, content)
        bindings[role] = [{"path": path.relative_to(repository).as_posix(),
                           "sha256": digest(content)}]

    context = {
        "bindings": bindings,
        "inventory": {
            "path": inventory_path.relative_to(repository).as_posix(),
            "sha256": digest(inventory_bytes),
        },
        "profile": "tier-b-t1",
        "schema": "t64.base-qualification-context",
        "schema_version": 1,
        "source_authority": {
            "path": authority_path.relative_to(repository).as_posix(),
            "sha256": authority_sha,
        },
        "source_projection": {
            "path": projection_path.relative_to(repository).as_posix(),
            "sha256": digest(projection_bytes),
        },
        "source_revision": projection_body["source_revision"],
    }
    context_bytes = canonical(context)
    context_path = context_root / "context.json"
    write_private(context_path, context_bytes)
    binding = {
        "context_sha256": digest(context_bytes),
        "inventory_sha256": digest(inventory_bytes),
        "schema": "t64.base-qualification-binding",
        "schema_version": 1,
        "source_authority_sha256": authority_sha,
        "source_projection_sha256": digest(projection_bytes),
    }
    write_private(context_root / "binding.json", canonical(binding))
    return context_root


class BaseQualificationControls(unittest.TestCase):
    def test_accepted_authored_context_reaches_normal_dispatch(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-qualification-") as temp:
            repository = pathlib.Path(temp)
            private_build = repository / "build"
            private_build.mkdir(mode=0o700)
            context_root = create_context(repository)
            run_directory = private_build / "private-base-diagnostic/run-authored"
            reached: list[str] = []
            verifier = load_verifier()

            def dispatch() -> str:
                run_directory.mkdir(mode=0o700)
                marker = run_directory / "dispatch.marker"
                marker.write_text("normal dispatch reached\n", encoding="utf-8")
                marker.chmod(0o600)
                reached.append("dispatch")
                return "reached"

            result = verifier.verify_and_dispatch(
                context_root, run_directory, dispatch, repository_root=repository
            )
            self.assertEqual(result, "reached")
            self.assertEqual(reached, ["dispatch"])
            self.assertTrue((run_directory / "dispatch.marker").is_file())

    def test_resealed_reduced_source_authority_is_rejected_before_dispatch(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-reduced-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            context_path = context_root / "context.json"
            binding_path = context_root / "binding.json"
            context = json.loads(context_path.read_bytes())
            binding = json.loads(binding_path.read_bytes())
            authority_path = repository / context["source_authority"]["path"]
            inventory_path = repository / context["inventory"]["path"]
            authority = json.loads(authority_path.read_bytes())
            inventory = json.loads(inventory_path.read_bytes())
            authority["members"].pop(1)
            inventory["members"].pop(1)
            authority_bytes = canonical(authority)
            authority_sha = digest(authority_bytes)
            inventory["source_authority_sha256"] = authority_sha
            inventory_bytes = canonical(inventory)
            write_private(authority_path, authority_bytes)
            write_private(inventory_path, inventory_bytes)
            context["source_authority"]["sha256"] = authority_sha
            context["inventory"]["sha256"] = digest(inventory_bytes)
            context_bytes = canonical(context)
            write_private(context_path, context_bytes)
            binding.update({
                "context_sha256": digest(context_bytes),
                "inventory_sha256": digest(inventory_bytes),
                "source_authority_sha256": authority_sha,
            })
            write_private(binding_path, canonical(binding))

            run_directory = context_root / "run-reduced"
            reached: list[str] = []
            verifier = load_verifier()
            with self.assertRaises(verifier.Refusal) as raised:
                verifier.verify_and_dispatch(
                    context_root, run_directory,
                    lambda: reached.append("unexpected") or "reached",
                    repository_root=repository,
                )
            self.assertEqual(
                str(raised.exception),
                "source authority differs from the independently derived authority",
            )
            self.assertEqual(reached, [])
            self.assertFalse(run_directory.exists())

    def test_qualified_tool_lock_boot_image_and_profile_drift_refuse(self) -> None:
        for role in ("tool", "lock", "runtime", "boot", "image", "profile",
                     "build", "compiler", "asset", "executable", "composition",
                     "source_verifier"):
            with self.subTest(role=role), tempfile.TemporaryDirectory(
                prefix="base-drift-"
            ) as temp:
                repository = pathlib.Path(temp)
                (repository / "build").mkdir(mode=0o700)
                context_root = create_context(repository)
                context = json.loads((context_root / "context.json").read_bytes())
                changed = repository / context["bindings"][role][0]["path"]
                changed.write_bytes(b"changed after qualification\n")
                changed.chmod(0o600)
                run_directory = context_root / "run-drift"
                reached: list[str] = []
                verifier = load_verifier()
                with self.assertRaises(verifier.Refusal) as raised:
                    verifier.verify_and_dispatch(
                        context_root, run_directory,
                        lambda: reached.append("unexpected") or "reached",
                        repository_root=repository,
                    )
                self.assertEqual(str(raised.exception), f"{role} closure digest changed")
                self.assertEqual(reached, [])
                self.assertFalse(run_directory.exists())

    def test_symlinked_input_unsafe_permissions_and_setup_error_refuse(self) -> None:
        verifier = load_verifier()
        with tempfile.TemporaryDirectory(prefix="base-symlink-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            context = json.loads((context_root / "context.json").read_bytes())
            asset = repository / context["bindings"]["asset"][0]["path"]
            target = context_root / "tool.evidence"
            asset.unlink()
            asset.symlink_to(target)
            run_directory = context_root / "run-symlink"
            reached: list[str] = []
            with self.assertRaises(verifier.Refusal) as raised:
                verifier.verify_and_dispatch(
                    context_root, run_directory,
                    lambda: reached.append("unexpected") or "reached",
                    repository_root=repository,
                )
            self.assertEqual(str(raised.exception), "asset binding path traverses a symlink")
            self.assertEqual(reached, [])
            self.assertFalse(run_directory.exists())

        with tempfile.TemporaryDirectory(prefix="base-permissions-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            run_directory = context_root / "run-permissions"
            context_root.chmod(0o755)
            reached = []
            with self.assertRaises(verifier.Refusal) as raised:
                verifier.verify_and_dispatch(
                    context_root, run_directory,
                    lambda: reached.append("unexpected") or "reached",
                    repository_root=repository,
                )
            self.assertEqual(str(raised.exception), "BASE qualification root permissions are not owner-only")
            self.assertEqual(reached, [])
            self.assertFalse(run_directory.exists())

        with tempfile.TemporaryDirectory(prefix="base-setup-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            run_directory = context_root / "run-setup"
            run_directory.mkdir(mode=0o700)
            reached = []
            with self.assertRaises(verifier.Refusal) as raised:
                verifier.verify_and_dispatch(
                    context_root, run_directory,
                    lambda: reached.append("unexpected") or "reached",
                    repository_root=repository,
                )
            self.assertEqual(
                str(raised.exception),
                "run directory must be a fresh child of the private context",
            )
            self.assertEqual(reached, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)

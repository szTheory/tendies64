#!/usr/bin/env python3
"""Behavioral controls for full BASE pre-use qualification and dispatch."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
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
    projection_body = {
        "registrations": rows,
        "schema": "t64.cpu-source-projection",
        "schema_version": 2,
        "source_revision": "a" * 40,
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

    roles = (
        "source", "tool", "lock", "runtime", "boot", "image", "composition",
        "profile", "build", "compiler", "asset", "executable", "verifier",
    )
    bindings = {}
    for role in roles:
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


if __name__ == "__main__":
    unittest.main(verbosity=2)

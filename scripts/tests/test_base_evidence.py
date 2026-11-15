#!/usr/bin/env python3
"""Authored full-entry controls for the BASE evidence validator."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import pathlib
import shutil
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load_file(name: str, path: pathlib.Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load authored BASE evidence fixture")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


FIXTURES = load_file("t64_base_qualification_fixtures",
                     ROOT / "scripts/tests/test_base_qualification.py")
CHECKER = load_file("t64_base_evidence_checker",
                    ROOT / "scripts/check-base-evidence.py")


class BaseEvidenceControls(unittest.TestCase):
    def public_repository(self, temporary: str) -> tuple[pathlib.Path, dict]:
        repository = pathlib.Path(temporary)
        copied = (
            "CMakeLists.txt",
            "fixtures/manifest.json",
            "docs/evidence/phase06-cold-boot.md",
            "docs/spec/cpu-timing.md",
            "docs/claims/phase06.md",
        )
        for relative in copied:
            destination = repository / relative
            destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)

        receipt_path = repository / "docs/evidence/phase06-final.json"
        receipt_path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        receipt = json.loads((ROOT / "docs/evidence/phase06-final.json").read_bytes())
        artifact_bytes = {
            "runner-fixture.bin": b"authored validator test runner\n",
            "determinism-fixture.bin": b"authored validator test fixture\n",
        }
        artifacts = {name: CHECKER.sha256(data)
                     for name, data in artifact_bytes.items()}
        binding = {
            "artifacts": artifacts,
            "caps": {"format": 1},
            "dirty_source": receipt["source_annotation_dirty"],
            "intent": "temporary public receipt validation fixture",
            "source_head": receipt["source_annotation"],
        }
        binding_path = repository / CHECKER.AUTHORED_BINDING
        binding_path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        binding_bytes = CHECKER.canonical(binding)
        binding_path.write_bytes(binding_bytes)
        for name, data in artifact_bytes.items():
            (binding_path.parent / name).write_bytes(data)

        manifest_bytes = (repository / CHECKER.AUTHORED_MANIFEST).read_bytes()
        receipt["artifact_binding"] = {
            "path": CHECKER.AUTHORED_BINDING.as_posix(),
            "sha256": CHECKER.sha256(binding_bytes),
            "artifacts": [{"name": name, "sha256": digest}
                          for name, digest in sorted(artifacts.items())],
        }
        receipt["fixture_manifest"]["sha256"] = CHECKER.sha256(manifest_bytes)
        receipt_path.write_bytes(CHECKER.canonical(receipt))
        return repository, receipt

    def full_entry(self, context_root: pathlib.Path, repository: pathlib.Path,
                   reached: list[bool], *, require_live: bool = False) -> None:
        CHECKER.validate_artifact_pair(
            context_root, repository, require_live_executable=require_live)
        verifier = FIXTURES.load_verifier()
        run_directory = context_root / "authored-normal-run"

        def dispatch() -> str:
            run_directory.mkdir(mode=0o700)
            FIXTURES.write_private(run_directory / "dispatch.marker",
                                   b"normal dispatch reached\n")
            reached.append(True)
            return "reached"

        self.assertEqual(verifier.verify_and_dispatch(
            context_root, run_directory, dispatch, repository_root=repository), "reached")

    def test_authored_pair_passes_and_normal_dispatch_is_reached(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-evidence-entry-") as temp:
            repository = pathlib.Path(temp)
            context_root = FIXTURES.create_context(repository)
            self.assertEqual(CHECKER.validate_artifact_pair(
                context_root, repository, require_live_executable=True)["status"],
                "verified")

            reached = []
            self.full_entry(context_root, repository, reached, require_live=True)
            self.assertEqual(reached, [True])
            self.assertTrue((context_root / "authored-normal-run/dispatch.marker").is_file())

    def test_resealed_reduced_authority_and_inventory_do_not_pass_binding(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-evidence-reduced-") as temp:
            repository = pathlib.Path(temp)
            context_root = FIXTURES.create_context(repository)
            self.assertEqual(CHECKER.validate_artifact_pair(
                context_root, repository)["status"], "verified")
            authority_path = context_root / "source-authority.json"
            inventory_path = context_root / "inventory.json"
            authority = json.loads(authority_path.read_bytes())
            inventory = json.loads(inventory_path.read_bytes())
            authority["members"] = authority["members"][:-1]
            inventory["members"] = inventory["members"][:-1]
            authority_bytes = FIXTURES.canonical(authority)
            inventory["source_authority_sha256"] = FIXTURES.digest(authority_bytes)
            inventory_bytes = FIXTURES.canonical(inventory)
            FIXTURES.write_private(authority_path, authority_bytes)
            FIXTURES.write_private(inventory_path, inventory_bytes)

            reached = []
            with self.assertRaises(ValueError):
                self.full_entry(context_root, repository, reached)
            self.assertEqual(reached, [])

    def test_missing_archived_binding_refuses_before_normal_dispatch(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-evidence-archive-") as temp:
            repository = pathlib.Path(temp)
            context_root = FIXTURES.create_context(repository)
            context_bytes = (context_root / "context.json").read_bytes()
            snapshot = context_root / "execution-snapshots" / hashlib.sha256(
                context_bytes).hexdigest()
            self.assertEqual(CHECKER.validate_artifact_pair(
                context_root, repository)["status"], "verified")
            (snapshot / "binding.json").unlink()
            reached = []
            with self.assertRaises(ValueError):
                self.full_entry(context_root, repository, reached)
            self.assertEqual(reached, [])

    def test_new_live_executable_cannot_use_the_old_binding(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-evidence-new-binary-") as temp:
            repository = pathlib.Path(temp)
            context_root = FIXTURES.create_context(repository)
            executable = repository / "build/private-base-diagnostic/executable.evidence"
            self.assertEqual(CHECKER.validate_artifact_pair(
                context_root, repository, require_live_executable=True)["status"], "verified")
            executable.write_bytes(b"a different authored executable\n")
            executable.chmod(0o700)
            reached = []
            with self.assertRaises(CHECKER.Refusal):
                self.full_entry(context_root, repository, reached, require_live=True)
            self.assertEqual(reached, [])

    def test_rejects_symlinked_private_root_before_dispatch(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-evidence-link-") as temp:
            parent = pathlib.Path(temp)
            repository = parent / "repo"
            repository.mkdir()
            context_root = FIXTURES.create_context(repository)
            self.assertEqual(CHECKER.validate_artifact_pair(
                context_root, repository)["status"], "verified")
            alias = parent / "private-root-link"
            alias.symlink_to(context_root, target_is_directory=True)

            reached = []
            with self.assertRaises(CHECKER.Refusal):
                self.full_entry(alias, repository, reached)
            self.assertEqual(reached, [])

    def test_public_receipt_rejects_private_fields_and_quoted_aliases(self) -> None:
        receipt_path = ROOT / "docs/evidence/phase06-final.json"
        baseline = json.loads(receipt_path.read_bytes())
        self.assertIsNone(CHECKER.validate_public_schema(baseline))

        top_level = json.loads(json.dumps(baseline))
        top_level["external_counts"] = {"pass": 1}
        with self.assertRaises(CHECKER.Refusal):
            CHECKER.validate_public_schema(top_level)

        nested = json.loads(json.dumps(baseline))
        nested["profiles"][0]["selected_rom_sha256"] = "authored-mutation-control"
        with self.assertRaises(CHECKER.Refusal):
            CHECKER.validate_public_schema(nested)

        quoted = json.loads(json.dumps(baseline))
        quoted["profiles"][0]["not_claimed"] += '; quoted alias "external_results"'
        reached = []
        original_scan = CHECKER._privacy_scan

        def observe_scan(value):
            reached.append(True)
            return original_scan(value)

        CHECKER._privacy_scan = observe_scan
        try:
            with self.assertRaises(CHECKER.Refusal):
                CHECKER.validate_public_schema(quoted)
        finally:
            CHECKER._privacy_scan = original_scan
        self.assertTrue(reached)

    def test_public_receipt_accepts_and_rejects_profile_build_oracle_mutations(self) -> None:
        for mutation in ("profile", "build", "oracle"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory(
                    prefix="base-public-receipt-") as temp:
                repository, receipt = self.public_repository(temp)
                self.assertEqual(CHECKER.validate_public_receipt(repository)["status"],
                                 "verified")
                if mutation == "profile":
                    receipt["profiles"][0]["model"] += 1
                elif mutation == "build":
                    receipt["core_build_id"] = "t64-build-v1-" + "0" * 64
                else:
                    receipt["profiles"][0]["oracle_revision"] = "wrong-oracle"
                (repository / "docs/evidence/phase06-final.json").write_bytes(
                    CHECKER.canonical(receipt))
                with self.assertRaises(CHECKER.Refusal):
                    CHECKER.validate_public_receipt(repository)

    def test_public_receipt_rejects_resealed_wrong_manifest_and_unsupported_claim(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-public-manifest-") as temp:
            repository, receipt = self.public_repository(temp)
            manifest_path = repository / CHECKER.AUTHORED_MANIFEST
            manifest = json.loads(manifest_path.read_bytes())
            manifest["entries"][0]["expected"]["revision"] = "wrong-oracle"
            manifest_bytes = CHECKER.canonical(manifest)
            manifest_path.write_bytes(manifest_bytes)
            receipt["fixture_manifest"]["sha256"] = CHECKER.sha256(manifest_bytes)
            (repository / "docs/evidence/phase06-final.json").write_bytes(
                CHECKER.canonical(receipt))
            with self.assertRaises(CHECKER.Refusal):
                CHECKER.validate_public_receipt(repository)

        with tempfile.TemporaryDirectory(prefix="base-public-claim-") as temp:
            repository, _ = self.public_repository(temp)
            claims_path = repository / CHECKER.AUTHORED_CLAIMS
            claims_path.write_text(claims_path.read_text().replace(
                "passes self-checking fixtures", "supports broad N64 compatibility", 1))
            with self.assertRaises(CHECKER.Refusal):
                CHECKER.validate_public_receipt(repository)


if __name__ == "__main__":
    unittest.main(verbosity=2)

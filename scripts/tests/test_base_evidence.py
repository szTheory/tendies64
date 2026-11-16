#!/usr/bin/env python3
"""Authored full-entry controls for the BASE evidence validator."""

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
        """Original-format authored records, without any private input tree."""
        repository = pathlib.Path(temporary)
        for relative in ("CMakeLists.txt", "fixtures/manifest.json", "docs/claims/phase06.md",
                         "fixtures/hello/hello.S", "fixtures/hello/hello.ld",
                         "tools/fixture/t64_rom_pack.c", "scripts/build-boot-fixtures.sh"):
            destination = repository / relative
            destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        receipt_path = repository / "docs/evidence/phase06-final.json"
        receipt_path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        receipt = json.loads((ROOT / "docs/evidence/phase06-final.json").read_bytes())
        artifact_bytes = {name: (receipt["core_build_id"] + "\n" +
                                 receipt["profiles"][0]["output_text"]).encode()
                          for name in ("hello.z64", "t64-cold-boot-probe",
                                       "test_boot_determinism", "test_cold_boot")}
        artifacts = {name: CHECKER.sha256(data) for name, data in artifact_bytes.items()}
        binding = {"artifacts": artifacts, "caps": {"format": 1},
                   "dirty_source": receipt["source_annotation_dirty"],
                   "intent": "authored original-format control", "source_head": receipt["source_annotation"]}
        binding_path = repository / CHECKER.AUTHORED_BINDING
        binding_path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        binding_bytes = CHECKER.canonical(binding)
        binding_path.write_bytes(binding_bytes)
        for name, data in artifact_bytes.items():
            (binding_path.parent / name).write_bytes(data)
        receipt["artifact_binding"] = {"path": CHECKER.AUTHORED_BINDING.as_posix(),
            "sha256": CHECKER.sha256(binding_bytes),
            "artifacts": [{"name": name, "sha256": value} for name, value in sorted(artifacts.items())]}
        manifest_path = repository / CHECKER.AUTHORED_MANIFEST
        manifest = json.loads(manifest_path.read_bytes())
        for row in manifest["entries"]:
            if row["id"] in {p["fixture_id"] for p in receipt["profiles"]}:
                row["sha256"] = artifacts["hello.z64"]
        manifest_path.write_bytes(CHECKER.canonical(manifest))
        receipt["fixture_manifest"]["sha256"] = CHECKER.sha256(manifest_path.read_bytes())
        for row in receipt["fixture_manifest"]["fixtures"] + receipt["profiles"]:
            row["rom_sha256"] = artifacts["hello.z64"]
        lines = ["comparison-identity fixture-a=hello-openipl3-rdram-a-t1 fixture-b=hello-openipl3-rdram-b-t1 "
                 f"rom-sha256={artifacts['hello.z64']} ipl3-payload-sha256={'0' * 64} "
                 f"build-id={receipt['core_build_id']} source-revision={receipt['source_annotation']} "
                 "compiler-policy=bound-by-build-id-v1"]
        for profile in receipt["profiles"]:
            pid = profile["profile_id"]
            protocol = int("-b-t1-" in pid)
            output = profile["output_text"].encode().hex()
            lines += [f"profile-identity profile={pid} sha256={profile['profile_sha256']}",
                      "terminal-policy=authenticated-architectural-break-v1",
                      f"profile={pid} stop=qualified-terminal stop_tick={profile['terminal_tick']} "
                      f"retired={profile['retired_instructions']} terminal=0x80000478/0x0000000d "
                      "completion-seen=1 last-retired=0x80000474/0xa50e0016 terminal-in-delay-slot=0 "
                      f"protocol-trace-complete={protocol} protocol-state-complete={protocol} "
                      f"output-bytes=30 output-exact=1 output-lost=0 hello-hex={output}",
                      f"boot-observation profile={pid} header-entry-retired=1 tick={profile['header_entry_tick']}",
                      f"cpu-state profile={pid} stopped=0 attempt-outcome=2 "
                      "exception=1/9/badva:0x0/epc:0xffffffff80000478/bd:0",
                      f"pi-state profile={pid}",
                      f"retirement-tail profile={pid} count=1 80000474/a50e0016"]
        lines += ["classification=compare-pass comparison=equal terminal=0x80000478/0x0000000d "
                  "last-retired=0x80000474/0xa50e0016 terminal-in-delay-slot=0 " + f"hello-hex={output}"]
        hello = "\n".join(lines) + "\n"
        startup = ("classification=observed profile=ntsc-cart-4m-openipl3-rdram-b-t1-v1 "
                   "operation=read address=0x0470000c width=4 value=0x00000000 "
                   "pc=0xa4000094 opcode=0x8c42000c stop=guest-tick-bound output_bytes=30 dropped=0\n")
        sections, attempts = [], []
        for index, (mode, output, args) in enumerate((
                ("hello", hello, "--compare --profile-a ntsc-cart-4m-openipl3-rdram-a-t1-v1 "
                 "--profile-b ntsc-cart-4m-openipl3-rdram-b-t1-v1 --fixture"),
                ("startup", startup, "--probe --profile ntsc-cart-4m-openipl3-rdram-b-t1-v1 --fixture")), 1):
            relative = f"build/gate/06-05-cold-{mode}-attempt-01"
            directory = repository / relative
            directory.mkdir(parents=True)
            command = f"/authored/build/gate/runner/t64-cold-boot-probe {args} /authored/build/fixtures/hello.z64\n".encode()
            outcome = b"exit_code=0\n" + output.encode()
            (directory / "command.txt").write_bytes(command)
            (directory / "outcome.txt").write_bytes(outcome)
            attempts.append({"test": f"integration.cold-{mode}", "path": relative,
                             "command_sha256": CHECKER.sha256(command), "outcome_sha256": CHECKER.sha256(outcome)})
            wrapper = "--integration" if mode == "hello" else "--startup"
            sections.append(f"{index}/2 Test: integration.cold-{mode}\n"
                            f"Command: /authored/test_cold_boot {wrapper} /authored/t64-cold-boot-probe /authored/tendies64\n"
                            "Directory: /authored\nOutput:\n" + output + "Test time = 0 sec\nTest Passed.\n")
        archive = repository / CHECKER.AUTHORED_EXECUTION
        logs = {"LastTest.log": "\n".join(sections).encode(), "ctest.log":
                b"1/2 Test #94: integration.cold-hello ... Passed\n2/2 Test #93: integration.cold-startup ... Passed\n100% tests passed out of 2\n"}
        for name, data in logs.items():
            (archive / name).write_bytes(data)
        log_manifest = CHECKER.canonical({name: CHECKER.sha256(data) for name, data in logs.items()})
        (archive / "log-sha256.json").write_bytes(log_manifest)
        receipt["execution_record"] = {"attempts": attempts, "binding_sha256": CHECKER.sha256(binding_bytes),
            "build_id": receipt["core_build_id"], "compiler_policy": "bound-by-build-id-v1",
            "source_revision": receipt["source_annotation"], "path": CHECKER.AUTHORED_EXECUTION.as_posix(),
            "ctest_log_sha256": CHECKER.sha256(logs["ctest.log"]),
            "last_test_log_sha256": CHECKER.sha256(logs["LastTest.log"]),
            "log_manifest_sha256": CHECKER.sha256(log_manifest)}
        # Render claims from this authored fixture, never borrow evidence from a report.
        blocks = []
        for profile in receipt["profiles"]:
            values = CHECKER._render_claim(profile, receipt)
            blocks.append("```t64-claim\n" + "\n".join(f"{k}: {v}" for k, v in values.items()) + "\n```\n")
        (repository / CHECKER.AUTHORED_CLAIMS).write_text("\n".join(blocks))
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
            self.assertEqual(CHECKER.validate_artifact_pair(
                context_root, repository, require_live_executable=False)["status"],
                "verified")
            reached = []
            with self.assertRaises(CHECKER.Refusal):
                self.full_entry(context_root, repository, reached, require_live=True)
            self.assertEqual(reached, [])

    def test_private_path_rejects_parent_component_that_resolves_inside_root(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-evidence-path-") as temp:
            root = pathlib.Path(temp)
            target = root / "record.json"
            FIXTURES.write_private(target, b"{}\n")
            traversing = str(root / ".." / root.name / target.name)
            with self.assertRaises(CHECKER.Refusal):
                CHECKER._private_path(root, traversing)

    def test_private_reader_rejects_fifo_without_blocking(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-evidence-fifo-") as temp:
            fifo = pathlib.Path(temp) / "record.fifo"
            os.mkfifo(fifo, 0o600)
            with self.assertRaises(CHECKER.Refusal):
                CHECKER.read_private(fifo)

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

if os.environ.get("T64_BASE_EVIDENCE_ACTUAL_TEST") == "1":
    class ActualRetainedEvidenceControls(unittest.TestCase):
        def test_actual_entry_and_fully_resealed_reduction(self) -> None:
            self.assertEqual(CHECKER.check_current()["status"], "verified")
            private_root = ROOT / CHECKER.PRIVATE_ROOT
            summary = CHECKER.parse_canonical(CHECKER.read_private(
                private_root / "plan06-06-execution-summary.json",
                maximum=2 * 1024 * 1024))
            lane = next(row for row in summary["lanes"] if row.get("lane") == "required")
            batch_root = ROOT / lane["batch"]
            context_bytes = CHECKER.read_private(batch_root / "context.json",
                                                  maximum=2 * 1024 * 1024)
            context_sha = CHECKER.sha256(context_bytes)
            snapshot_source = private_root / "execution-snapshots" / context_sha

            with tempfile.TemporaryDirectory(prefix="base-evidence-reseal-") as temp:
                temp_root = pathlib.Path(temp)

                def copy_private_file(source: pathlib.Path, destination: pathlib.Path) -> None:
                    data = CHECKER.read_private(source, maximum=MAX_COPY_BYTES)
                    destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                    destination.write_bytes(data)
                    destination.chmod(0o600)

                MAX_COPY_BYTES = 512 * 1024 * 1024
                context_copy = temp_root / "batch"
                context_copy.mkdir(mode=0o700)
                snapshot_root = temp_root / "snapshot-root"
                snapshot_root.mkdir(mode=0o700)
                snapshot_collection = snapshot_root / "execution-snapshots"
                snapshot_collection.mkdir(mode=0o700)
                snapshot_copy = snapshot_collection / context_sha
                snapshot_copy.mkdir(mode=0o700)
                for name in ("context.json", "binding.json", "source-authority.json",
                             "inventory.json"):
                    copy_private_file(batch_root / name, context_copy / name)
                for name in ("context.json", "binding.json", "executable"):
                    copy_private_file(snapshot_source / name, snapshot_copy / name)

                self.assertEqual(CHECKER.validate_artifact_pair(
                    context_copy, ROOT, snapshot_root=snapshot_root)["status"],
                    "verified")

                context = CHECKER.parse_canonical((context_copy / "context.json").read_bytes())
                binding = CHECKER.parse_canonical((context_copy / "binding.json").read_bytes())
                authority = CHECKER.parse_canonical(
                    (context_copy / "source-authority.json").read_bytes())
                inventory = CHECKER.parse_canonical(
                    (context_copy / "inventory.json").read_bytes())
                authority["members"] = authority["members"][:-1]
                authority_bytes = CHECKER.canonical(authority)
                authority_sha = CHECKER.sha256(authority_bytes)
                inventory["members"] = inventory["members"][:-1]
                inventory["source_authority_sha256"] = authority_sha
                inventory_bytes = CHECKER.canonical(inventory)
                inventory_sha = CHECKER.sha256(inventory_bytes)
                context["source_authority"]["sha256"] = authority_sha
                context["inventory"]["sha256"] = inventory_sha
                context_bytes = CHECKER.canonical(context)
                context_sha = CHECKER.sha256(context_bytes)
                binding.update({
                    "context_sha256": context_sha,
                    "source_authority_sha256": authority_sha,
                    "inventory_sha256": inventory_sha,
                })
                binding_bytes = CHECKER.canonical(binding)

                for name, data in (("source-authority.json", authority_bytes),
                                   ("inventory.json", inventory_bytes),
                                   ("context.json", context_bytes),
                                   ("binding.json", binding_bytes)):
                    path = context_copy / name
                    path.write_bytes(data)
                    path.chmod(0o600)

                resealed_snapshot = snapshot_collection / context_sha
                resealed_snapshot.mkdir(mode=0o700)
                for name, data in (("context.json", context_bytes),
                                   ("binding.json", binding_bytes),
                                   ("executable", (snapshot_copy / "executable").read_bytes())):
                    path = resealed_snapshot / name
                    path.write_bytes(data)
                    path.chmod(0o600)

                derived = []
                original_deriver = CHECKER._independent_source_authority

                def observe_derivation(*args, **kwargs):
                    result = original_deriver(*args, **kwargs)
                    derived.append(True)
                    return result

                CHECKER._independent_source_authority = observe_derivation
                try:
                    with self.assertRaises(CHECKER.Refusal):
                        CHECKER.validate_artifact_pair(
                            context_copy, ROOT, snapshot_root=snapshot_root)
                finally:
                    CHECKER._independent_source_authority = original_deriver
                self.assertEqual(derived, [True])

        def test_actual_resealed_selected_rom_is_independently_refused(self) -> None:
            import subprocess
            private = ROOT / CHECKER.PRIVATE_ROOT
            index = CHECKER.parse_canonical(CHECKER.read_private(
                private / "plan06-06-execution-summary.json"))
            location = next(row["batch"] for row in index["lanes"] if row["lane"] == "required")
            original = ROOT / location
            CHECKER._validate_batch(original, ROOT, "required")
            with tempfile.TemporaryDirectory(prefix="image-control-", dir=private) as temp:
                copied = pathlib.Path(temp)
                copied.chmod(0o700)
                for name in ("context.json", "binding.json", "source-authority.json", "inventory.json", "batch.json", "consumer"):
                    shutil.copyfile(original / name, copied / name)
                    (copied / name).chmod(0o700 if name == "consumer" else 0o600)
                receipt = CHECKER.parse_canonical(CHECKER.read_private(copied / "batch.json"))
                manifest = CHECKER.parse_canonical(CHECKER.read_private(original / receipt["classification_input"]))
                row = next(row for row in manifest["members"] if row["expected_rom_sha256"])
                old_digest = row["expected_rom_sha256"]
                changed_digest = "f" * 64
                self.assertNotEqual(old_digest, changed_digest)
                original_record = CHECKER.read_private(pathlib.Path(row["record"]), maximum=8192)
                changed = original_record.replace(old_digest.encode(), changed_digest.encode())
                self.assertNotEqual(original_record, changed)
                record_path = copied / "changed-record.json"
                record_path.write_bytes(changed)
                record_path.chmod(0o600)
                row.update(expected_rom_sha256=changed_digest, record=str(record_path))
                # Recompute the report using original C, so a stale report hash
                # cannot be the reason the new independent guard refuses.
                for item in manifest["members"]:
                    item.update(outcome="record", reason="")
                input_path = copied / receipt["classification_input"]
                input_path.write_bytes(CHECKER.canonical(manifest))
                input_path.chmod(0o600)
                binding = CHECKER.parse_canonical(CHECKER.read_private(copied / "binding.json"))
                report_path = copied / receipt["classification_report"]
                command = [str(copied / "consumer"), "--classify-manifest", str(copied / "source-authority.json"),
                           binding["source_authority_sha256"], str(copied / "inventory.json"),
                           binding["context_sha256"], str(input_path), str(report_path)]
                completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                    timeout=60, check=False, env={"PATH": os.environ.get("PATH", ""), "LC_ALL": "C"})
                if completed.returncode != receipt["classification_exit_code"] or not report_path.is_file():
                    self.fail("authored mutation could not reach the original C classification seam")
                with self.assertRaisesRegex(CHECKER.Refusal, "selected image differs"):
                    CHECKER._validate_batch(copied, ROOT, "required")


class PublicEvidenceControls(unittest.TestCase):
    public_repository = BaseEvidenceControls.public_repository

    def test_bound_selector_bytes_are_the_expected_rom_authority(self) -> None:
        from types import SimpleNamespace
        base = b"authored-0000-ROM"
        offset, key = 9, (3, 7)
        selected = base[:offset] + b"\0\3\0\7" + base[offset + 4:]
        digest = CHECKER.sha256(selected)
        image = SimpleNamespace(base_bytes=base, selector_offset=offset,
                                members={key: {"selected_rom_sha256": digest}})
        row = {"registration_index": 3, "value_index": 7, "expected_rom_sha256": digest}
        CHECKER._check_selected_image(row, {"outcome": "pass"}, image)
        with self.assertRaisesRegex(CHECKER.Refusal, "selected image differs"):
            CHECKER._check_selected_image(dict(row, expected_rom_sha256="f" * 64),
                                          {"outcome": "pass", "selected_rom_sha256": "f" * 64}, image)

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
                    receipt["profiles"][0]["profile_sha256"] = "0" * 64
                elif mutation == "build":
                    receipt["core_build_id"] = "t64-build-v1-" + "0" * 64
                else:
                    receipt["profiles"][0]["oracle_revision"] = "wrong-oracle"
                (repository / "docs/evidence/phase06-final.json").write_bytes(
                    CHECKER.canonical(receipt))
                with self.assertRaises(CHECKER.Refusal):
                    CHECKER.validate_public_receipt(repository)

    def test_original_record_mutations_reach_runtime_validation(self) -> None:
        for mutation in ("terminal", "capture", "profile", "cleanup", "missing"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory(prefix="base-original-") as temp:
                repository, receipt = self.public_repository(temp)
                self.assertEqual(CHECKER.validate_public_receipt(repository)["status"], "verified")
                archive = repository / CHECKER.AUTHORED_EXECUTION
                attempt = receipt["execution_record"]["attempts"][0]
                outcome_path = repository / attempt["path"] / "outcome.txt"
                if mutation == "missing":
                    outcome_path.unlink()
                else:
                    old = outcome_path.read_bytes()
                    replacements = {
                        "terminal": (b"terminal=0x80000478/0x0000000d", b"terminal=0x80000478/0x00000000"),
                        "capture": (b"output-exact=1", b"output-exact=0"),
                        "profile": (receipt["profiles"][0]["profile_sha256"].encode(), b"0" * 64),
                        "cleanup": (b"exit_code=0", b"exit_code=124"),
                    }
                    before, after = replacements[mutation]
                    self.assertIn(before, old)
                    changed = old.replace(before, after)
                    outcome_path.write_bytes(changed)
                    attempt["outcome_sha256"] = CHECKER.sha256(changed)
                    if mutation != "cleanup":
                        last = archive / "LastTest.log"
                        last.write_bytes(last.read_bytes().replace(old[len(b"exit_code=0\n"):],
                                                                  changed[len(b"exit_code=0\n"):]))
                        log_hashes = {name: CHECKER.sha256((archive / name).read_bytes())
                                      for name in ("LastTest.log", "ctest.log")}
                        (archive / "log-sha256.json").write_bytes(CHECKER.canonical(log_hashes))
                        receipt["execution_record"]["last_test_log_sha256"] = log_hashes["LastTest.log"]
                        receipt["execution_record"]["log_manifest_sha256"] = CHECKER.sha256(
                            (archive / "log-sha256.json").read_bytes())
                (repository / "docs/evidence/phase06-final.json").write_bytes(CHECKER.canonical(receipt))
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

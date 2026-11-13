#!/usr/bin/env python3
"""Authored synthetic controls for the private CPU batch runner."""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import select
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from dataclasses import replace
from unittest.mock import patch


def load_runner(source_root: pathlib.Path):
    path = source_root / "scripts/run-cpu-diagnostic.py"
    spec = importlib.util.spec_from_file_location("cpu_diagnostic_runner", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load the authored CPU runner")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_private(path: pathlib.Path, data: bytes) -> None:
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0), 0o600)
    try:
        view = memoryview(data)
        while view:
            written = os.write(fd, view)
            if written <= 0:
                raise OSError("short private fixture write")
            view = view[written:]
        os.fsync(fd)
    finally:
        os.close(fd)


def canonical(value) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode()


def fake_guest_main() -> int:
    if len(sys.argv) < 5 or sys.argv[1] != "--fake-run-local":
        return 64
    adapter = pathlib.Path(sys.argv[2])
    scenario = sys.argv[3]
    canonical_directory = pathlib.Path(sys.argv[4])
    arguments = sys.argv[5:]
    if len(arguments) != 10 or arguments[:2] != ["--run-local", "--local-only"]:
        return 65
    inventory_path = pathlib.Path(arguments[2])
    registration = int(arguments[7])
    value = int(arguments[8])
    output_path = pathlib.Path(arguments[9])
    if scenario == "omission" and registration == 0:
        return 3
    template = canonical_directory / f"batch-r{registration:04d}v{value:05d}-pass.json"
    if scenario == "complete-fail" and registration == 1:
        template = canonical_directory / f"batch-r{registration:04d}v{value:05d}-fail.json"
    batch = template.read_bytes()
    if registration == 0 and scenario == "malformed":
        batch = b"{broken-json}\n"
    elif registration == 0 and scenario == "missing-done":
        record = json.loads(batch)
        record["records"] = record["records"][:-1]
        batch = canonical(record)
    elif registration == 0 and scenario == "wrong-identity":
        record = json.loads(batch)
        record["identity"]["tool_receipt_sha256"] = "f" * 64
        batch = canonical(record)
    # Confirm the production inventory validator accepts the exact same
    # synthetic inventory before routing a fake guest result.
    write_private(output_path, batch)
    return 0


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "--fake-run-local":
        return fake_guest_main()
    if len(sys.argv) != 4 or sys.argv[1] != "--controls":
        print("usage: test_cpu_diagnostic_run_controls.py --controls SOURCE_ROOT ADAPTER", file=sys.stderr)
        return 64
    source_root = pathlib.Path(sys.argv[2]).resolve()
    adapter = pathlib.Path(sys.argv[3]).resolve()
    module = load_runner(source_root)

    class RunControls(unittest.TestCase):
        @classmethod
        def setUpClass(cls):
            # Authored controls need no private diagnostic checkout or inputs.
            build_root = source_root / "build"
            if build_root.is_symlink() or not build_root.is_dir():
                raise RuntimeError("authored fixture build root is missing or linked")
            cls.control_root = pathlib.Path(tempfile.mkdtemp(
                prefix="authored-run-controls-", dir=build_root))
            os.chmod(cls.control_root, 0o700)
            cls.adapter = adapter
            cls.fake_script = pathlib.Path(__file__).resolve()
            cls.canonical_directory = cls.control_root / "canonical-batches"
            os.mkdir(cls.canonical_directory, 0o700)
            cls.base = bytes((index * 7) & 0xFF for index in range(4096))
            cls.offset = 8
            code, runtime_bytes, _ = module.run_bounded(
                [adapter, "--runtime-identity-json"], cwd=source_root,
                env=module.make_env(controls=True), timeout=10, maximum=4096,
            )
            if code != 0:
                raise RuntimeError("production C runtime identity command refused synthetic controls")
            runtime_identity = json.loads(runtime_bytes)
            identity = {
                "base_rom_sha256": digest(cls.base),
                "core_build_sha256": runtime_identity["core_build_sha256"],
                "overlay_sha256": "2" * 64,
                "profile_sha256": runtime_identity["profile_sha256"],
                "source_revision": "4" * 40,
                "tool_receipt_sha256": "5" * 64,
            }
            registrations = []
            for index, group in enumerate(module.GROUPS):
                selector = f"r{index:04d}v00000"
                variant = bytearray(cls.base)
                variant[cls.offset:cls.offset + 4] = index.to_bytes(2, "big") + bytes(2)
                registrations.append({
                    "execution": "once",
                    "group": group,
                    "index": index,
                    "lane": "required-base",
                    "level": "BasicFunctionality",
                    "name": f"authored {group}",
                    "reason": "required",
                    "values": [{
                        "id": "once", "index": 0,
                        "selected_rom_sha256": digest(bytes(variant)),
                        "selector": selector,
                    }],
                })
            cls.inventory = {
                "declared_counts": {"registrations": "14", "required_values": "14", "values": "14"},
                "identity": identity,
                "registrations": registrations,
                "schema": "t64.cpu-inventory",
                "schema_version": 3,
            }
            cls.inventory_bytes = canonical(cls.inventory)
            cls.inventory_path = cls.control_root / "required-inventory.json"
            write_private(cls.inventory_path, cls.inventory_bytes)
            write_private(cls.control_root / "base.z64", cls.base)
            cls.adapter_path = adapter.relative_to(source_root).as_posix()
            cls.adapter_sha256 = module.digest_regular_file(adapter)
            cls.qualified_bytes = canonical({
                "adapter": {"path": cls.adapter_path, "sha256": cls.adapter_sha256},
                "artifacts": {},
                "schema": "t64.cpu-qualified-build",
                "schema_version": 1,
                "selector": {},
                "status": "qualified",
            })
            write_private(cls.control_root / "qualified-build.json", cls.qualified_bytes)
            cls.context = module.Inputs(
                private_root=cls.control_root,
                adapter=adapter,
                adapter_path=cls.adapter_path,
                adapter_sha256=cls.adapter_sha256,
                qualified_build_path="qualified-build.json",
                qualified_build_sha256=digest(cls.qualified_bytes),
                inventory_path="required-inventory.json",
                inventory_sha256=digest(cls.inventory_bytes),
                inventory_bytes=cls.inventory_bytes,
                inventory=cls.inventory,
                base_bytes=cls.base,
                selector_offset=cls.offset,
            )
            module.check_adapter(cls.context)
            code, _, _ = module.invoke_adapter(
                cls.context, "--validate-inventory", str(cls.inventory_path),
                timeout=10, maximum=4096, controls=True,
            )
            if code != 0:
                raise RuntimeError("production C inventory validator refused synthetic fixture")
            for registration in registrations:
                index = registration["index"]
                for outcome in ("pass", "fail"):
                    code, encoded, _ = module.invoke_adapter(
                        cls.context, "--encode-inventory", str(cls.inventory_path),
                        str(index), "0", outcome, timeout=10, maximum=4096, controls=True,
                    )
                    if code != 0:
                        raise RuntimeError("production C batch encoder refused synthetic fixture")
                    path = cls.canonical_directory / f"batch-r{index:04d}v00000-{outcome}.json"
                    write_private(path, encoded)

        @classmethod
        def tearDownClass(cls):
            shutil.rmtree(cls.control_root)

        def run_case(self, scenario: str):
            command = [
                sys.executable, "-B", str(self.fake_script), "--fake-run-local",
                str(self.adapter), scenario, str(self.canonical_directory),
            ]
            run_directory, code = module.run_all(
                self.context, fake_guest_command=command, control_mode=True,
            )
            self.assertEqual(code, 0, "every routed member returned an allowed guest status")
            return run_directory

        def rejected_report(self, scenario: str, expected_status: str):
            run_directory = self.run_case(scenario)
            with self.assertRaisesRegex(module.Refusal, expected_status):
                module.verify_complete(self.context, explicit_run=run_directory.name)

        def test_complete_pass_set_uses_shared_group_report(self):
            run_directory = self.run_case("complete-pass")
            self.assertEqual(module.verify_complete(self.context, explicit_run=run_directory.name), 0)
            report = json.loads((run_directory / "group-status.json").read_text())
            self.assertEqual(report["schema"], "t64.cpu-group-status")
            self.assertEqual(report["classification"], "complete")
            self.assertEqual(len(report["groups"]), 14)
            self.assertTrue(all(row["expected"] == row["reached"] == row["passed"] == 1 for row in report["groups"]))
            self.assertEqual(len((run_directory / "value-status.jsonl").read_bytes().splitlines()), 14)

        def test_sealed_real_adapter_is_accepted(self):
            module.check_adapter(self.context)
            identity = module.adapter_json(self.context, "--runtime-identity-json")
            self.assertEqual(identity["core_build_sha256"], self.inventory["identity"]["core_build_sha256"])
            self.assertEqual(identity["profile_sha256"], self.inventory["identity"]["profile_sha256"])

        def test_compat_selection_is_explicit_and_sealed(self):
            code, output, _ = module.run_bounded(
                [self.adapter, "--compat", "--runtime-identity-json"], cwd=source_root,
                env=module.make_env(controls=True), timeout=10, maximum=4096,
            )
            self.assertEqual(code, 0, "explicit compat runtime selection is accepted")
            identity = json.loads(output)
            self.assertEqual(identity["core_build_sha256"], self.inventory["identity"]["core_build_sha256"])
            self.assertNotEqual(identity["profile_sha256"], self.inventory["identity"]["profile_sha256"])
            seal = json.loads(self.qualified_bytes)
            seal.update(schema_version=2, profile_id="ntsc-cart-4m-openipl3-vr4300-compat-v1")
            seal_bytes = canonical(seal)
            write_private(self.control_root / "compat-qualified.json", seal_bytes)
            context = replace(self.context, qualified_build_path="compat-qualified.json",
                              qualified_build_sha256=digest(seal_bytes), profile_id=seal["profile_id"])
            self.assertEqual(module.adapter_json(context, "--runtime-identity-json"), identity)
            with self.assertRaisesRegex(module.Refusal, "profile binding"):
                module.invoke_adapter(replace(context, profile_id=module.STRICT_PROFILE_ID), "--runtime-identity-json")
            with self.assertRaisesRegex(module.Refusal, "profile binding"):
                module.invoke_adapter(replace(self.context, profile_id=seal["profile_id"]), "--runtime-identity-json")

        def test_compat_loader_and_aggregator_bind_inventory_profile(self):
            root = self.control_root / "compat-loader"
            os.mkdir(root, 0o700)
            projection = json.loads(self.inventory_bytes)
            projection.update(projection_sha256="6" * 64, source_revision="4" * 40)
            projection_bytes = canonical(projection)
            qualification = {"source_revision": "4" * 40, "source": {
                "overlay_sha256": "2" * 64, "projection": {
                    "path": (root / "source_projection.json").relative_to(source_root).as_posix(),
                    "sha256": digest(projection_bytes), "projection_sha256": "6" * 64}}}
            qualification_bytes = canonical(qualification)
            code, output, _ = module.invoke_adapter(
                replace(self.context), "--runtime-identity-json", controls=True)
            self.assertEqual(code, 0)
            strict_identity = json.loads(output)
            code, output, _ = module.run_bounded(
                [self.adapter, "--compat", "--runtime-identity-json"], cwd=source_root,
                env=module.make_env(controls=True), timeout=10, maximum=4096)
            self.assertEqual(code, 0)
            compat_identity = json.loads(output)
            base = bytearray(self.base)
            base[self.offset:self.offset + 4] = bytes(4)
            inventory = json.loads(self.inventory_bytes)
            inventory["identity"].update(base_rom_sha256=digest(base),
                profile_sha256=compat_identity["profile_sha256"],
                tool_receipt_sha256=digest(qualification_bytes))
            for row in inventory["registrations"]:
                variant = bytearray(base)
                variant[self.offset:self.offset + 4] = row["index"].to_bytes(2, "big") + bytes(2)
                row["values"][0]["selected_rom_sha256"] = digest(variant)
            artifacts = {"base_rom": bytes(base), "inventory": canonical(inventory),
                "qualification": qualification_bytes, "source_projection": projection_bytes,
                "source_closure": canonical({"status": "PASS_PREBUILD_SOURCE",
                    "source_projection_sha256": "6" * 64, "source_revision": "4" * 40}),
                "composition": canonical({"schema": "t64.cpu-build-composition", "schema_version": 1,
                    "status": "PASS_POSTBUILD_COMPOSITION", "guest_execution_verified": False,
                    "rom_sha256": digest(base), "qualification_sha256": digest(qualification_bytes),
                    "layout": {"selector_rom_offset": self.offset, "selector_bytes": 4}})}
            composition = json.loads(artifacts["composition"])
            composition["source_closure_sha256"] = digest(artifacts["source_closure"])
            artifacts["composition"] = canonical(composition)
            seal = json.loads(self.qualified_bytes)
            seal.update(schema_version=2, profile_id=module.COMPAT_PROFILE_ID,
                selector={"encoding": "registration-index-u16be:value-index-u16be",
                          "rom_offset": self.offset, "width": 4}, artifacts={})
            for key, data in artifacts.items():
                name = key + (".z64" if key == "base_rom" else ".json")
                write_private(root / name, data)
                seal["artifacts"][key] = {"path": name, "sha256": digest(data)}
            write_private(root / "qualified-build.json", canonical(seal))
            real_run = module.run_bounded
            qualification_calls = []

            def authored_qualification(command, **kwargs):
                # This test owns routing/identity, not upstream qualification.
                # Only that separate verifier is stubbed; every adapter call is real.
                if command[1:3] == ["-B", "scripts/verify-cpu-diagnostic-qualification.py"]:
                    qualification_calls.append(command)
                    return 0, b"", b""
                return real_run(command, **kwargs)

            with patch.object(module, "PRIVATE", root), \
                    patch.object(module, "ADAPTER_REPO_PATH", self.adapter_path), \
                    patch.object(module, "run_bounded", side_effect=authored_qualification):
                context = module.load_qualified_inputs(self.adapter)
                self.assertEqual(context.profile_id, module.COMPAT_PROFILE_ID)
                self.assertEqual(context.inventory["identity"]["profile_sha256"], compat_identity["profile_sha256"])
                run = module.create_run_directory(root)
                module.create_manifest(context, run)
                # Real C encoding and aggregation accept the complete compat set.
                for row in inventory["registrations"]:
                    code, encoded, _ = module.invoke_adapter(context, "--encode-inventory",
                        str(root / "inventory.json"), str(row["index"]), "0", "pass", controls=True)
                    self.assertEqual(code, 0)
                    write_private(run / "batches" / f"batch-{row['index']:04d}.json", encoded)
                self.assertEqual(module.verify_complete(context, explicit_run=run.name), 0)
                strict = json.loads(canonical(inventory))
                strict["identity"]["profile_sha256"] = strict_identity["profile_sha256"]
                write_private(root / "strict-inventory.json", canonical(strict))
                code, encoded, _ = module.invoke_adapter(self.context, "--encode-inventory",
                    str(root / "strict-inventory.json"), "0", "0", "pass", controls=True)
                self.assertEqual(code, 0)
                cross = module.create_run_directory(root)
                module.create_manifest(context, cross)
                write_private(cross / "batches" / "strict-batch.json", encoded)
                with self.assertRaisesRegex(module.Refusal, "identity-mismatch"):
                    module.verify_complete(context, explicit_run=cross.name)
                # Re-seal a structurally valid strict inventory under compat:
                # the loader must compare the real selected runtime identity.
                seal["artifacts"]["inventory"] = {"path": "strict-inventory.json", "sha256": digest(canonical(strict))}
                (root / "qualified-build.json").write_bytes(canonical(seal))
                with self.assertRaisesRegex(module.Refusal, "running adapter core or profile differs"):
                    module.load_qualified_inputs(self.adapter)
            self.assertEqual(len(qualification_calls), 2)

        def test_unknown_profile_and_boolean_seal_version_are_refused(self):
            seal = json.loads(self.qualified_bytes)
            seal.update(schema_version=2, profile_id="unknown-profile")
            with self.assertRaisesRegex(module.Refusal, "profile selection"):
                module.parse_qualified_seal(canonical(seal))
            seal.update(schema_version=True, profile_id="ntsc-cart-4m-openipl3-vr4300-compat-v1")
            with self.assertRaises(module.Refusal):
                module.parse_qualified_seal(canonical(seal))

        def test_fake_adapter_with_matching_identity_and_complete_report_is_rejected(self):
            fake_adapter = self.control_root / "fake-adapter.py"
            marker = self.control_root / "fake-adapter-was-run"
            runtime = {
                "core_build_sha256": self.inventory["identity"]["core_build_sha256"],
                "profile_sha256": self.inventory["identity"]["profile_sha256"],
                "source_revision": self.inventory["identity"]["source_revision"],
            }
            script = (
                "#!/usr/bin/env python3\n"
                "import json,pathlib,sys\n"
                f"pathlib.Path({str(marker)!r}).write_text('invoked')\n"
                f"runtime={runtime!r}\n"
                "if sys.argv[1:] == ['--runtime-identity-json']:\n"
                " print(json.dumps(runtime,sort_keys=True,separators=(',',':')))\n"
                "elif sys.argv[1:2] == ['--verify-complete-json']:\n"
                " print('{\"classification\":\"complete\",\"groups\":[]}')\n"
                "else: sys.exit(0)\n"
            ).encode()
            write_private(fake_adapter, script)
            os.chmod(fake_adapter, 0o700)
            code, runtime_bytes, _ = module.run_bounded(
                [sys.executable, str(fake_adapter), "--runtime-identity-json"],
                cwd=source_root, env=module.make_env(controls=True), timeout=10, maximum=4096,
            )
            self.assertEqual(code, 0)
            fake_identity = json.loads(runtime_bytes)
            self.assertEqual(fake_identity["core_build_sha256"], runtime["core_build_sha256"])
            self.assertEqual(fake_identity["profile_sha256"], runtime["profile_sha256"])
            code, report, _ = module.run_bounded(
                [sys.executable, str(fake_adapter), "--verify-complete-json", "unused-inventory", "unused-batches"],
                cwd=source_root, env=module.make_env(controls=True), timeout=10, maximum=4096,
            )
            self.assertEqual(code, 0)
            self.assertEqual(json.loads(report)["classification"], "complete")
            marker.unlink()
            # Exercise the same path substitution under the selected normal
            # or instrumented authored target, without changing production's
            # fixed adapter selection.
            with patch.object(module, "ADAPTER_REPO_PATH", self.context.adapter_path):
                self.assertEqual(module.check_adapter_path(
                    self.adapter, self.context.adapter_path, self.context.adapter_sha256), self.adapter)
                with self.assertRaisesRegex(module.Refusal, "requested adapter path differs"):
                    module.check_adapter_path(fake_adapter, self.context.adapter_path, self.context.adapter_sha256)
            with self.assertRaisesRegex(module.Refusal, "loaded adapter path differs"):
                module.invoke_adapter(replace(self.context, adapter=fake_adapter),
                                      "--verify-complete-json", "unused-inventory", "unused-batches")
            self.assertFalse(marker.exists(), "the rejected adapter was not invoked by the sealed path guard")

        def test_changed_adapter_after_loading_is_refused_before_guest(self):
            adapter_copy = self.control_root / "bound-adapter-copy"
            shutil.copyfile(self.adapter, adapter_copy)
            os.chmod(adapter_copy, 0o700)
            relative = adapter_copy.relative_to(source_root).as_posix()
            adapter_sha = module.digest_regular_file(adapter_copy)
            seal = canonical({
                "adapter": {"path": relative, "sha256": adapter_sha},
                "artifacts": {},
                "schema": "t64.cpu-qualified-build",
                "schema_version": 1,
                "selector": {},
                "status": "qualified",
            })
            write_private(self.control_root / "qualified-copy.json", seal)
            bound_context = replace(
                self.context,
                adapter=adapter_copy,
                adapter_path=relative,
                adapter_sha256=adapter_sha,
                qualified_build_path="qualified-copy.json",
                qualified_build_sha256=digest(seal),
            )
            module.check_adapter(bound_context)
            with adapter_copy.open("ab") as changed:
                changed.write(b"changed")
            with self.assertRaisesRegex(module.Refusal, "production adapter changed"):
                module.invoke_adapter(bound_context, "--runtime-identity-json")
            run_directory = module.create_run_directory(self.control_root)
            write_private(run_directory / "value-status.jsonl", b"")
            code = module.run_one_member(
                bound_context, run_directory, module.required_members(self.inventory)[0],
                fake_guest_command=[sys.executable, "-c", "raise SystemExit(0)"],
                control_mode=True,
            )
            self.assertEqual(code, 4)
            self.assertFalse((run_directory / "batches/batch-r0000v00000.json").exists())

        def test_complete_diagnostic_retains_executed_failure(self):
            run_directory = self.run_case("complete-fail")
            self.assertEqual(module.verify_complete(self.context, explicit_run=run_directory.name), 0)
            report = json.loads((run_directory / "group-status.json").read_text())
            arithmetic = next(row for row in report["groups"] if row["group"] == "arithmetic")
            self.assertEqual((arithmetic["status"], arithmetic["expected"], arithmetic["reached"], arithmetic["failed"]),
                             ("fail", 1, 1, 1))

        def test_malformed_batch_is_refused_by_shared_aggregator(self):
            self.rejected_report("malformed", "invalid-json")

        def test_missing_done_is_refused_by_shared_aggregator(self):
            self.rejected_report("missing-done", "status=incomplete")

        def test_wrong_identity_is_refused_by_shared_aggregator(self):
            self.rejected_report("wrong-identity", "identity-mismatch")

        def test_duplicate_member_is_refused_by_shared_aggregator(self):
            run_directory = self.run_case("complete-pass")
            original = (run_directory / "batches/batch-r0000v00000.json").read_bytes()
            write_private(run_directory / "batches/duplicate-member.json", original)
            with self.assertRaisesRegex(module.Refusal, "duplicate-member"):
                module.verify_complete(self.context, explicit_run=run_directory.name)

        def test_omitted_member_stays_incomplete(self):
            run_directory = self.run_case("omission")
            self.assertEqual(module.verify_complete(self.context, explicit_run=run_directory.name), 3)
            report = json.loads((run_directory / "group-status.json").read_text())
            address = next(row for row in report["groups"] if row["group"] == "address_error_exception")
            self.assertEqual((address["status"], address["expected"], address["reached"]),
                             ("incomplete", 1, 0))

        def test_bounded_guest_router_timeout_closes_descendant(self):
            fifo = self.control_root / "runner-child.fifo"
            pid_path = self.control_root / "runner-child.pid"
            os.mkfifo(fifo, 0o600)
            reader = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)
            pid_literal = repr(str(pid_path))
            fifo_literal = repr(str(fifo))
            child_code = (
                "import os,pathlib,time; child=os.fork(); "
                f"pathlib.Path({pid_literal}).write_text(str(os.getpid())) if child == 0 else None; "
                f"writer=os.open({fifo_literal},os.O_WRONLY) if child == 0 else None; "
                "os.write(writer,b'R') if child == 0 else None; time.sleep(5)"
            )
            cleaned = False
            try:
                with self.assertRaisesRegex(module.Refusal, "timed out"):
                    module.run_bounded(
                        [sys.executable, "-c", child_code], cwd=source_root,
                        env=module.make_env(controls=True), timeout=1, maximum=4096,
                    )
                self.assertTrue(pid_path.is_file(), "routed descendant actually started")
                self.assertEqual(os.read(reader, 1), b"R", "descendant held its FIFO writer")
                self.assertTrue(select.select([reader], [], [], 1)[0],
                                "descendant writer closed within the cleanup bound")
                self.assertEqual(os.read(reader, 1), b"", "timeout terminated the descendant group")
                cleaned = True
                started = time.monotonic()
                with self.assertRaisesRegex(module.Refusal, "timed out"):
                    module.run_bounded(
                        [sys.executable, "-c", "import os,time; os.close(1); os.close(2); time.sleep(5)"],
                        cwd=source_root, env=module.make_env(controls=True),
                        timeout=1, maximum=4096,
                    )
                self.assertLess(time.monotonic() - started, 3,
                                "closed child pipes do not bypass the process deadline")
            finally:
                os.close(reader)
                if not cleaned and pid_path.is_file():
                    try:
                        os.kill(int(pid_path.read_text()), 9)
                    except ProcessLookupError:
                        pass

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(RunControls)
    result = unittest.TextTestRunner(stream=sys.stdout, verbosity=1).run(suite)
    if result.wasSuccessful():
        print("all 14 authored CPU batch-control assertions passed")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

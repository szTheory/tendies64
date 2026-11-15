#!/usr/bin/env python3
"""Authored controls for one bounded BASE member dispatch."""

from __future__ import annotations

import importlib.util
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from test_base_qualification import (
    canonical, create_context, digest, load_builder, write_private,
)

ROOT = pathlib.Path(__file__).resolve().parents[2]
RUNNER = ROOT / "scripts/run-base-diagnostic.py"


def load_runner():
    spec = importlib.util.spec_from_file_location("t64_base_runner", RUNNER)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load production BASE runner")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def create_combined_context(repository, control_adapter, *, missing_selector=False):
    """A complete authored context; its tiny ROM deliberately fails admission."""
    repository = repository.resolve(strict=True)
    context_root = create_context(repository)
    context = json.loads((context_root / "context.json").read_bytes())
    projection_path = repository / context["source_projection"]["path"]
    projection = json.loads(projection_path.read_bytes())
    for row in projection["registrations"]:
        row["source_file"] = "src/tests/testlist.rs"
        row["source_file_sha256"] = projection["testlist_sha256"]
    rsp_source = repository / "build/private-cpu-diagnostic/nemu64-test/src/tests/rsp/authored.rs"
    write_private(rsp_source, b"authored RSP module\n")
    rsp_sha = digest(rsp_source.read_bytes())
    rdp_source = repository / "build/private-cpu-diagnostic/nemu64-test/src/tests/rdp/authored.rs"
    write_private(rdp_source, b"authored RDP module\n")
    projection["registrations"][1]["source_file"] = "src/tests/rdp/authored.rs"
    projection["registrations"][1]["source_file_sha256"] = digest(rdp_source.read_bytes())
    projection["registrations"][1]["source_value_ids"] = ["v00000"]
    projection["reviewed_source_file_sha256"]["src/tests/rdp/authored.rs"] = digest(rdp_source.read_bytes())
    projection["reviewed_source_file_sha256"]["src/tests/rsp/authored.rs"] = rsp_sha
    projection["registrations"].append({"index": 2, "group": "non-cpu",
        "source_group": "rsp", "name": "authored RSP", "source_name": "authored RSP",
        "constructor": "rsp::Authored", "source_file": "src/tests/rsp/authored.rs",
        "source_file_sha256": rsp_sha, "values_method_sha256": "0" * 64,
        "lane": "skipped-non-cpu", "level": "BasicFunctionality",
        "source_value_count": 0, "effective_value_count": 1,
        "source_value_ids": ["once"], "values": []})
    projection.pop("projection_sha256")
    projection["projection_sha256"] = digest(canonical(projection))
    projection_bytes = canonical(projection)
    write_private(projection_path, projection_bytes)
    context["source_projection"]["sha256"] = digest(projection_bytes)
    provider_spec = importlib.util.spec_from_file_location("authored_rsp_membership",
        ROOT / "scripts/prepare-rsp-diagnostic.py")
    provider = importlib.util.module_from_spec(provider_spec)
    provider_spec.loader.exec_module(provider)
    rsp = {"component": "rsp", "schema": "t64.rsp-source-projection",
        "schema_version": 1, "status": "membership-prepared",
        "source_revision": context["source_revision"],
        "registrations": provider.derive_rsp_membership(projection["registrations"]),
        "inputs": {"cpu_source_projection_sha256": digest(projection_bytes)}}
    rsp_path = repository / "build/private-rsp-diagnostic/rsp-source-projection.json"
    write_private(rsp_path, canonical(rsp))
    context["rsp_source_projection"] = {"path": rsp_path.relative_to(repository).as_posix(),
                                        "sha256": digest(canonical(rsp))}
    context["schema_version"] = 2
    def bind(role, path):
        context["bindings"].setdefault(role, []).append({
            "path": path.relative_to(repository).as_posix(), "sha256": digest(path.read_bytes())})
    bind("source", rsp_source)
    bind("source", rdp_source)
    for role, filename, value in (
        ("rsp_qualification", "qualification.json", {"membership": context["rsp_source_projection"]}),
        ("rsp_source_closure", "rsp-preuse-closure.json", {
            "status": "PASS_PREBUILD_RSP", "source_revision": context["source_revision"],
            "overlay": {"files": []}}),
        ("rsp_overlay", "rsp-overlay-preparation.json", {"authored": "overlay"})):
        path = rsp_path.parent / filename
        write_private(path, canonical(value))
        bind(role, path)
    closure = repository / "build/private-cpu-diagnostic/source-closure.json"
    write_private(closure, canonical({"status": "PASS_PREBUILD_SOURCE",
        "source_revision": context["source_revision"], "module_files": [
            {"path": "src/tests/testlist.rs", "sha256": projection["testlist_sha256"]}]}))
    bind("asset", closure)
    qualification = repository / "build/private-cpu-diagnostic/qualification.json"
    write_private(qualification, canonical({"assets": {"closure_report": {
        "path": closure.relative_to(repository).as_posix(), "sha256": digest(closure.read_bytes())}}}))
    bind("runtime", qualification)
    # Re-derive through the real v2 provider and explicit supported-module maps.
    from test_base_qualification import load_deriver
    members = load_deriver().derive_members(projection, rsp,
        {"src/tests/testlist.rs": projection["testlist_sha256"]}, {})
    authority_bytes = canonical({"members": members, "schema": "t64.base-source-authority",
        "schema_version": 1, "source_revision": context["source_revision"]})
    inventory_bytes = canonical({"members": members, "schema": "t64.base-inventory",
        "schema_version": 1, "source_revision": context["source_revision"],
        "source_authority_sha256": digest(authority_bytes)})
    write_private(context_root / "source-authority.json", authority_bytes)
    write_private(context_root / "inventory.json", inventory_bytes)
    context["source_authority"]["sha256"] = digest(authority_bytes)
    context["inventory"]["sha256"] = digest(inventory_bytes)
    for name in ("run-base-diagnostic.py", "run-cpu-diagnostic.py"):
        path = repository / "scripts" / name
        shutil.copyfile(ROOT / "scripts" / name, path)
        path.chmod(0o600)
        bind("build", path)
    adapter = repository / "build/gate/runner/t64-base-diagnostic"
    adapter.parent.mkdir(mode=0o700, parents=True)
    shutil.copyfile(control_adapter, adapter)
    adapter.chmod(0o700)
    context["bindings"]["executable"] = []
    bind("executable", adapter)
    identity = subprocess.check_output([str(adapter), "--runtime-identity-json"], timeout=5)
    identity_path = context_root / "runtime-identity-authored.json"
    write_private(identity_path, identity)
    bind("profile", identity_path)
    image_bytes = bytes(4) + b"authored invalid ROM"
    image_path = context_root / "base.z64"
    write_private(image_path, image_bytes)
    bind("image", image_path)
    composition = {"schema": "t64.base-combined-image", "schema_version": 1,
        "status": "PASS_POSTBUILD_COMPOSITION", "guest_execution_verified": False,
        "source_revision": context["source_revision"],
        "base_rom": {"path": image_path.relative_to(repository).as_posix(),
                     "sha256": digest(image_bytes)},
        "selector": {"encoding": "registration-index-u16be:value-index-u16be",
                     "width": 4, "rom_offset": 0},
        "source_bindings": {"cpu_source_projection_sha256": digest(projection_bytes),
                            "rsp_source_projection_sha256": digest(canonical(rsp)),
                            "combined_overlay_sha256": digest(canonical({"authored": True}))},
        "combined_overlay": {"authored": True},
        "members": [] if missing_selector else [{"group": "arithmetic", "lane": "required-base",
            "level": "BasicFunctionality", "registration_index": 0, "value_index": 0,
            "selector_hex": "00000000", "selected_rom_sha256": digest(image_bytes)}]}
    composition_path = context_root / "combined-image.json"
    write_private(composition_path, canonical(composition))
    bind("composition", composition_path)
    context_bytes = canonical(context)
    write_private(context_root / "context.json", context_bytes)
    binding = json.loads((context_root / "binding.json").read_bytes())
    binding.update(context_sha256=digest(context_bytes), source_projection_sha256=digest(projection_bytes),
                   source_authority_sha256=digest(authority_bytes), inventory_sha256=digest(inventory_bytes))
    binding_bytes = canonical(binding)
    write_private(context_root / "binding.json", binding_bytes)
    load_builder().retain_execution_snapshot(repository, context_root, context_bytes, binding_bytes)
    runner = load_runner()
    original_loader = runner._load_module
    def loader(path, name):
        loaded = original_loader(path, name)
        if name == "t64_base_preuse_verifier":
            # Isolated fixtures use the actual independent providers, without
            # treating synthetic source receipts as the production checkout.
            loaded.ROOT = ROOT
        return loaded
    runner.ROOT, runner.PRIVATE = repository, context_root
    runner.CPU_RUNNER = repository / "scripts/run-cpu-diagnostic.py"
    runner._load_module = loader
    return runner, context_root, adapter


class BaseRunControls(unittest.TestCase):
    def test_informational_lane_runs_through_complete_entry(self):
        adapter = pathlib.Path(os.environ.get("T64_BASE_CONTROL_ADAPTER",
            ROOT / "build/gate/runner/t64-base-diagnostic-controls"))
        spec = importlib.util.spec_from_file_location("base_lane_entry_control",
            ROOT / "scripts/base-diagnostic-lanes.py")
        wrapper = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(wrapper)
        with tempfile.TemporaryDirectory(prefix="base-lane-entry-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            runner, context, selected = create_combined_context(repository, adapter)
            wrapper.ROOT, wrapper.PRIVATE = repository.resolve(), context
            with patch.object(wrapper, "_load_runner", return_value=runner), patch.dict(os.environ,
                {"CI": "", "T64_REQUIRE_FIXTURES": "0", "T64_BASE_CONTROL_ROOT": str(context)}):
                self.assertEqual(wrapper.main(["--run-informational", "--local-only"]), 0)
                marker = next(context.glob("informational-dispatch-*.json"))
                batch = context / json.loads(marker.read_bytes())["batch"]
                receipt = json.loads((batch / "lane-invocation.json").read_bytes())
                self.assertEqual(receipt["lane"], "informational")
                self.assertFalse(receipt["authenticated_terminal_observed"])
                self.assertEqual(list(context.glob("case-*")), [])

    def test_real_adapter_admits_cleared_boot_before_single_tick(self):
        """Positive allocator/admission control, with no external diagnostic."""
        adapter = pathlib.Path(os.environ.get("T64_BASE_CONTROL_ADAPTER",
            ROOT / "build/gate/runner/t64-base-diagnostic-controls"))
        boot = ROOT / "build/fixtures/ipl3_compat.z64"
        boot_bytes = boot.read_bytes()
        self.assertEqual(digest(boot_bytes),
                         "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068")
        with tempfile.TemporaryDirectory(prefix="base-positive-admission-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            _, context, selected = create_combined_context(repository, adapter)
            authority = (context / "source-authority.json").read_bytes()
            env = {**os.environ, "CI": "", "T64_REQUIRE_FIXTURES": "0",
                   "T64_BASE_CONTROL_ROOT": str(context)}
            result = subprocess.run([str(selected), "--run-bound-local", "--local-only",
                str(context / "source-authority.json"), digest(authority),
                str(context / "inventory.json"), str(boot), str(len(boot_bytes)), digest(boot_bytes),
                "1", "0", "0", str(context / "positive-record.json"),
                str(context / "positive-capture.bin"), digest((context / "context.json").read_bytes())],
                cwd=ROOT, env=env, capture_output=True, timeout=2)
            self.assertEqual(result.returncode, 3, result.stdout)
            self.assertIn(b"stage=guest-tick-bound", result.stdout)
            record = json.loads((context / "positive-record.json").read_bytes())
            self.assertEqual(record["tick"], "1")
            self.assertEqual(record["reason"], "guest-tick-bound")

    def test_interrupted_batch_retains_honest_dispositions(self):
        adapter = pathlib.Path(os.environ.get("T64_BASE_CONTROL_ADAPTER",
            ROOT / "build/gate/runner/t64-base-diagnostic-controls"))
        for started in (False, True):
            with self.subTest(started=started), tempfile.TemporaryDirectory(prefix="base-interrupt-") as temp:
                repository = pathlib.Path(temp)
                (repository / "build").mkdir(mode=0o700)
                runner, context_root, selected = create_combined_context(repository, adapter)
                def interrupt(directory, *args):
                    if started:
                        directory.mkdir(mode=0o700)
                        runner._write_private(directory / "partial.bin", b"partial authored write")
                    raise KeyboardInterrupt
                with patch.object(runner, "_bound_member_attempt", side_effect=interrupt), patch.dict(
                        os.environ, {"CI": "", "T64_REQUIRE_FIXTURES": "0",
                                     "T64_BASE_CONTROL_ROOT": str(context_root)}):
                    self.assertEqual(runner.main(["--run-all", "--local-only",
                                                  "--adapter", str(selected)]), 130)
                    batch = next(context_root.glob("batch-*"))
                    receipt = json.loads((batch / "batch.json").read_bytes())
                    report = json.loads((batch / receipt["classification_report"]).read_bytes())
                    self.assertTrue(report["source_complete"])
                    self.assertFalse(report["authenticated_execution_complete"])
                    self.assertFalse(report["all_pass"])
                    self.assertEqual(report["members"][0]["outcome"],
                                     "harness-error" if started else "unexecuted")
                    self.assertEqual(runner.main(["--verify-complete", "--local-only",
                                                  "--adapter", str(selected)]), 0)

    def test_expired_deadline_cannot_return_zero_after_wait(self):
        runner = load_runner()
        cpu = runner._load_module(ROOT / "scripts/run-cpu-diagnostic.py", "base_deadline_control")
        clock = [0.0]
        pipes = []
        for _ in range(2):
            read_fd, write_fd = os.pipe()
            os.close(write_fd)
            pipes.append(os.fdopen(read_fd, "rb"))
        class Process:
            pid = 2000000000
            stdout, stderr = pipes
            def wait(self, timeout=None):
                clock[0] = 2.0
                return 0
        with patch.object(cpu.subprocess, "Popen", return_value=Process()), patch.object(
                cpu.time, "monotonic", side_effect=lambda: clock[0]), patch.object(cpu.os, "killpg") as kill:
            with self.assertRaisesRegex(cpu.Refusal, "timed out"):
                cpu.run_bounded(["authored-deadline"], cwd=ROOT, env={}, timeout=1, maximum=4096)
            kill.assert_called_once()
        self.assertTrue(all(pipe.closed for pipe in pipes))

    def test_complete_cli_with_real_c_producer_and_classifier(self):
        adapter = pathlib.Path(os.environ.get("T64_BASE_CONTROL_ADAPTER",
            ROOT / "build/gate/runner/t64-base-diagnostic-controls"))
        with tempfile.TemporaryDirectory(prefix="base-bound-full-entry-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            runner, context_root, selected = create_combined_context(repository, adapter)
            with patch.dict(os.environ, {"CI": "", "T64_REQUIRE_FIXTURES": "0",
                                        "T64_BASE_CONTROL_ROOT": str(context_root)}):
                code = runner.main(["--run-all", "--local-only", "--adapter", str(selected)])
                self.assertEqual(code, 0, "source-complete classification is a separate contract")
                batches = list(context_root.glob("batch-*"))
                self.assertEqual(len(batches), 1)
                receipt = json.loads((batches[0] / "batch.json").read_bytes())
                report = json.loads((batches[0] / receipt["classification_report"]).read_bytes())
                self.assertTrue(report["source_complete"])
                self.assertFalse(report["authenticated_execution_complete"])
                self.assertFalse(report["all_pass"])
                self.assertEqual(report["members"][0]["outcome"], "harness-error")
                self.assertEqual(report["members"][1]["outcome"], "blocked")
                cases = list(context_root.glob("case-*"))
                self.assertEqual(len(cases), 1)
                self.assertFalse((cases[0] / "source-authority.json").exists())
                self.assertFalse((cases[0] / "inventory.json").exists())
                command = json.loads((cases[0] / "command.json").read_bytes())["argv"]
                self.assertEqual(command[3], str(batches[0] / "source-authority.json"))
                self.assertEqual(command[5], str(batches[0] / "inventory.json"))
                native_record = json.loads((cases[0] / "member-record.json").read_bytes())
                self.assertEqual(native_record["schema_version"], 2)
                self.assertEqual(native_record["reason"], "adapter-harness-error")
                self.assertIn("native_worker_reaped=yes", (cases[0] / "c-stdout.log").read_text())
                self.assertEqual(runner.main(["--verify-complete", "--local-only",
                                             "--adapter", str(selected)]), 0)
                self.assertEqual(runner.main(["--run-all", "--local-only",
                                             "--adapter", str(selected)]), 4)
                self.assertEqual(list(context_root.glob("case-*")), cases)

    def test_combined_selector_preflight_fails_before_guest_directory(self):
        adapter = pathlib.Path(os.environ.get("T64_BASE_CONTROL_ADAPTER",
            ROOT / "build/gate/runner/t64-base-diagnostic-controls"))
        with tempfile.TemporaryDirectory(prefix="base-selector-preflight-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            runner, context_root, selected = create_combined_context(
                repository, adapter, missing_selector=True)
            with patch.dict(os.environ, {"CI": "", "T64_REQUIRE_FIXTURES": "0",
                                        "T64_BASE_CONTROL_ROOT": str(context_root)}):
                with self.assertRaisesRegex(Exception, "selector is missing"):
                    runner.run_required(context_root, selected)
            self.assertEqual(list(context_root.glob("case-*")), [])
            self.assertEqual(list(context_root.glob("batch-*")), [])

    def test_native_deadline_control_and_production_exclusion(self):
        adapter = pathlib.Path(os.environ.get("T64_BASE_CONTROL_ADAPTER",
            ROOT / "build/gate/runner/t64-base-diagnostic-controls"))
        result = subprocess.run([str(adapter), "--control-deadline"],
                                capture_output=True, timeout=2)
        self.assertEqual(result.returncode, 124)
        self.assertIn(b"native_worker_reaped=yes", result.stdout)
        self.assertIn(b"native-wall-deadline", result.stdout)
        production = adapter.with_name("t64-base-diagnostic")
        if production != adapter and production.exists():
            result = subprocess.run([str(production), "--control-deadline"],
                                    capture_output=True, timeout=2)
            self.assertEqual(result.returncode, 64)

    def test_dispatch_consumes_verified_context_snapshot(self) -> None:
        """Changing the pathname after verification cannot substitute its data."""
        with tempfile.TemporaryDirectory(prefix="base-context-snapshot-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            runner = load_runner()
            original_loader = runner._load_module
            def loader(path, name):
                module = original_loader(path, name)
                if name == "t64_base_preuse_verifier":
                    original_check = module._verify_live_source_authority
                    def replace_after_check(*args):
                        original_check(*args)
                        changed = json.loads((context_root / "context.json").read_bytes())
                        changed["bindings"]["executable"][0]["sha256"] = "f" * 64
                        write_private(context_root / "context.json", canonical(changed))
                    module._verify_live_source_authority = replace_after_check
                return module
            adapter = context_root / "executable.evidence"
            runner._load_module = loader
            try:
                result = runner.run_one(context_root, context_root / "run-snapshot",
                    adapter, repository_root=repository,
                    dispatch=lambda directory: {"cleanup_verified": True,
                        "timed_out": False, "returncode": 0,
                        "record_authenticated": True})
            except Exception as exc:
                self.fail(f"verified context was reopened and replaced at dispatch: {exc}")
            self.assertEqual(result["status"], "pass")

    def test_full_cli_entry_retains_synthetic_c_result(self) -> None:
        """Exercise the full private entry with authored bytes; this is not a guest run."""
        with tempfile.TemporaryDirectory(prefix="base-full-entry-synthetic-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            cpu_root = repository / "build/private-cpu-diagnostic"
            scripts = repository / "scripts"
            shutil.copyfile(ROOT / "scripts/run-cpu-diagnostic.py",
                            scripts / "run-cpu-diagnostic.py")
            (scripts / "run-cpu-diagnostic.py").chmod(0o600)
            shutil.copyfile(RUNNER, scripts / "run-base-diagnostic.py")
            (scripts / "run-base-diagnostic.py").chmod(0o600)

            image_bytes = bytes(4) + b"synthetic authored image"
            image_path = cpu_root / "base-rom.z64"
            write_private(image_path, image_bytes)
            selected_hash = digest(bytes(4) + image_bytes[4:])
            cpu_inventory = {
                "registrations": [{
                    "index": 0,
                    "lane": "required-base",
                    "values": [{
                        "index": 0,
                        "selector": "r0000v00000",
                        "selected_rom_sha256": selected_hash,
                    }],
                }],
            }
            cpu_inventory_bytes = canonical(cpu_inventory)
            inventory_path = cpu_root / "inventory.json"
            write_private(inventory_path, cpu_inventory_bytes)
            projection_path = cpu_root / "source-projection.json"
            projection_bytes = projection_path.read_bytes()
            closure_bytes = canonical({
                "schema": "t64.synthetic-source-closure",
                "status": "PASS_PREBUILD_SOURCE",
            })
            closure_path = cpu_root / "source-closure.json"
            write_private(closure_path, closure_bytes)
            qualification = {
                "assets": {"closure_report": {
                    "path": closure_path.relative_to(repository).as_posix(),
                    "sha256": digest(closure_bytes),
                }},
            }
            qualification_bytes = canonical(qualification)
            qualification_path = cpu_root / "qualification.json"
            write_private(qualification_path, qualification_bytes)
            composition = {
                "guest_execution_verified": False,
                "layout": {"selector_bytes": 4, "selector_rom_offset": 0},
                "qualification_sha256": digest(qualification_bytes),
                "rom_sha256": digest(image_bytes),
                "schema": "t64.cpu-build-composition",
                "schema_version": 1,
                "source_closure_sha256": digest(closure_bytes),
                "status": "PASS_POSTBUILD_COMPOSITION",
            }
            composition_bytes = canonical(composition)
            composition_path = cpu_root / "composition.json"
            write_private(composition_path, composition_bytes)
            artifacts = {
                "base_rom": (image_path, image_bytes),
                "composition": (composition_path, composition_bytes),
                "inventory": (inventory_path, cpu_inventory_bytes),
                "qualification": (qualification_path, qualification_bytes),
                "source_closure": (closure_path, closure_bytes),
                "source_projection": (projection_path, projection_bytes),
            }
            qualified = {
                "adapter": {"path": "build/gate/runner/t64-cpu-diagnostic",
                            "sha256": "0" * 64},
                "artifacts": {
                    name: {"path": path.relative_to(cpu_root).as_posix(),
                           "sha256": digest(data)}
                    for name, (path, data) in artifacts.items()
                },
                "schema": "t64.cpu-qualified-build",
                "schema_version": 2,
                "selector": {
                    "encoding": "registration-index-u16be:value-index-u16be",
                    "rom_offset": 0,
                    "width": 4,
                },
                "status": "qualified",
            }
            qualified_bytes = canonical(qualified)
            qualified_path = cpu_root / "qualified-build.json"
            write_private(qualified_path, qualified_bytes)

            identity = {
                "core_build_identity": "t64-build-v1-" + "1" * 64,
                "profile_id": "ntsc-cart-4m-openipl3-rdram-b-t1-v1",
                "profile_sha256": "2" * 64,
                "source_revision": "3" * 40,
            }
            identity_bytes = canonical(identity)
            identity_path = context_root / f"runtime-identity-{digest(identity_bytes)[:16]}.json"
            write_private(identity_path, identity_bytes)
            fake_adapter = repository / "build/gate/runner/t64-base-diagnostic"
            fake_adapter.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
            fake_adapter.write_text(
                "#!/usr/bin/env python3\n"
                "import hashlib,json,os,pathlib,sys\n"
                f"IDENTITY={identity_bytes.decode()!r}\n"
                "args=sys.argv[1:]\n"
                "if args == ['--runtime-identity-json']:\n"
                " sys.stdout.write(IDENTITY); raise SystemExit(0)\n"
                "if args[:2] == ['--run-local','--local-only']:\n"
                " inv=pathlib.Path(args[4]).read_bytes()\n"
                " capture=b'SYNTHETIC_AUTHORED_GUEST_CAPTURE\\n'\n"
                " reg,val=int(args[9]),int(args[10])\n"
                " result={'inventory_sha256':hashlib.sha256(inv).hexdigest(),'original_sha256':hashlib.sha256(capture).hexdigest(),'outcome':'incomplete','registration_index':reg,'schema':'t64.base-member-record','schema_version':1,'value_index':val}\n"
                " rec=pathlib.Path(args[11]); cap=pathlib.Path(args[12])\n"
                " rec.write_text(json.dumps(result,sort_keys=True,separators=(',',':'))+'\\n'); os.chmod(rec,0o600)\n"
                " cap.write_bytes(capture); os.chmod(cap,0o600)\n"
                " print('classification=incomplete synthetic=true'); raise SystemExit(3)\n"
                "print('synthetic adapter refused',file=sys.stderr); raise SystemExit(4)\n",
                encoding="utf-8",
            )
            fake_adapter.chmod(0o700)

            context = json.loads((context_root / "context.json").read_bytes())

            def bind(role: str, path: pathlib.Path) -> None:
                context["bindings"][role].append({
                    "path": path.relative_to(repository).as_posix(),
                    "sha256": digest(path.read_bytes()),
                })

            bind("build", scripts / "run-base-diagnostic.py")
            bind("build", scripts / "run-cpu-diagnostic.py")
            bind("build", qualified_path)
            bind("build", inventory_path)
            bind("asset", closure_path)
            mirror = context_root / f"source-image-source-closure-{digest(closure_bytes)[:16]}.json"
            write_private(mirror, closure_bytes)
            bind("runtime", qualification_path)
            bind("runtime", mirror)
            bind("profile", identity_path)
            bind("image", image_path)
            bind("composition", composition_path)
            context["bindings"]["executable"] = []
            bind("executable", fake_adapter)
            context_bytes = canonical(context)
            context_path = context_root / "context.json"
            context_path.write_bytes(context_bytes)
            context_path.chmod(0o600)
            binding_path = context_root / "binding.json"
            binding = json.loads(binding_path.read_bytes())
            binding["context_sha256"] = digest(context_bytes)
            binding_bytes = canonical(binding)
            binding_path.write_bytes(binding_bytes)
            binding_path.chmod(0o600)
            load_builder().retain_execution_snapshot(
                repository, context_root, context_bytes, binding_bytes
            )

            run = load_runner()
            original_root = run.ROOT
            original_private = run.PRIVATE
            original_cpu_runner = run.CPU_RUNNER
            original_loader = run._load_module
            real_base_adapter = ROOT / "build/gate/runner/t64-base-diagnostic"

            def fixture_loader(path: pathlib.Path, name: str):
                loaded = original_loader(path, name)
                if name == "t64_base_preuse_verifier":
                    loaded.ROOT = ROOT
                if name == "t64_cpu_member_runner":
                    original_bounded = loaded.run_bounded

                    def bounded(command, **kwargs):
                        if len(command) > 1 and command[1] == "--aggregate":
                            command = [str(real_base_adapter), *command[1:]]
                        return original_bounded(command, **kwargs)

                    loaded.run_bounded = bounded
                return loaded

            run.ROOT = repository
            run.PRIVATE = context_root
            run.CPU_RUNNER = scripts / "run-cpu-diagnostic.py"
            run._load_module = fixture_loader
            try:
                with patch.dict(os.environ, {"CI": "", "T64_REQUIRE_FIXTURES": "0"}):
                    result = run.main(["--run-one", "--adapter", str(fake_adapter)])
            finally:
                run.ROOT = original_root
                run.PRIVATE = original_private
                run.CPU_RUNNER = original_cpu_runner
                run._load_module = original_loader

            self.assertEqual(result, 3)
            run_directories = [path for path in context_root.glob("run-*") if path.is_dir()]
            self.assertEqual(len(run_directories), 1)
            attempt = json.loads((run_directories[0] / "attempt.json").read_bytes())
            self.assertEqual(attempt["status"], "incomplete")
            self.assertEqual(attempt["attempts"], 1)
            self.assertTrue((run_directories[0] / "member-record.json").is_file())
            aggregate_output = (run_directories[0] / "aggregate-stdout.log").read_text()
            self.assertIn("classification=member-record-authenticated", aggregate_output)

    def test_source_image_closure_uses_exact_asset_and_runtime_roles(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-source-closure-") as temp:
            repository = pathlib.Path(temp)
            original = repository / "build/private-cpu-diagnostic/source-closure.json"
            original.parent.mkdir(mode=0o700, parents=True)
            original_bytes = b"accepted source and asset closure\n"
            original.write_bytes(original_bytes)
            original.chmod(0o600)
            runner = load_runner()
            digest = runner.hashlib.sha256(original_bytes).hexdigest()
            mirror_relative = (
                "build/private-base-diagnostic/source-image-source-closure-"
                + digest[:16] + ".json"
            )
            mirror = repository / mirror_relative
            mirror.parent.mkdir(mode=0o700, parents=True)
            mirror.write_bytes(original_bytes)
            mirror.chmod(0o600)
            original_relative = original.relative_to(repository).as_posix()
            context = {"bindings": {
                "asset": [{"path": original_relative, "sha256": digest}],
                "runtime": [{"path": mirror_relative, "sha256": digest}],
            }}

            runner._assert_source_closure_bindings(
                context, original_relative, digest, original_bytes,
                repository_root=repository,
            )
            context["bindings"]["runtime"] = []
            with self.assertRaisesRegex(Exception, "source closure mirror is absent from the runtime role"):
                runner._assert_source_closure_bindings(
                    context, original_relative, digest, original_bytes,
                    repository_root=repository,
                )

    def test_authenticated_single_member_is_retained_once(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-run-pass-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            adapter_path = context_root / "executable.evidence"
            adapter_path.chmod(0o700)
            run_directory = context_root / "run-pass"
            attempts: list[pathlib.Path] = []
            runner = load_runner()

            def completed_attempt(directory: pathlib.Path) -> dict:
                attempts.append(directory)
                return {
                    "cleanup_verified": True,
                    "record_authenticated": True,
                    "returncode": 0,
                    "timed_out": False,
                }

            result = runner.run_one(
                context_root, run_directory, adapter_path,
                repository_root=repository, dispatch=completed_attempt,
            )
            self.assertEqual(result["status"], "pass")
            self.assertEqual(result["attempts"], 1)
            self.assertEqual(attempts, [run_directory])
            self.assertTrue((run_directory / "attempt.json").is_file())

    def test_timeout_remains_timeout_after_cleanup_without_retry(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-run-timeout-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            adapter_path = context_root / "executable.evidence"
            adapter_path.chmod(0o700)
            run_directory = context_root / "run-timeout"
            attempts: list[pathlib.Path] = []
            runner = load_runner()

            def timed_out_attempt(directory: pathlib.Path) -> dict:
                attempts.append(directory)
                return {
                    "cleanup_verified": True,
                    "returncode": 0,
                    "stderr": "",
                    "stdout": "classification=pass\n",
                    "timed_out": True,
                }

            result = runner.run_one(
                context_root, run_directory, adapter_path,
                repository_root=repository, dispatch=timed_out_attempt,
            )
            self.assertEqual(result["status"], "timeout")
            self.assertEqual(result["attempts"], 1)
            self.assertEqual(attempts, [run_directory])
            self.assertTrue(run_directory.is_dir())

    def test_changed_context_refuses_before_run_directory_creation(self) -> None:
        with tempfile.TemporaryDirectory(prefix="base-run-refused-") as temp:
            repository = pathlib.Path(temp)
            (repository / "build").mkdir(mode=0o700)
            context_root = create_context(repository)
            adapter_path = context_root / "executable.evidence"
            adapter_path.chmod(0o700)
            context = json.loads((context_root / "context.json").read_bytes())
            changed = repository / context["bindings"]["tool"][0]["path"]
            changed.write_bytes(b"changed tool bytes\n")
            changed.chmod(0o600)
            run_directory = context_root / "run-refused"
            attempts: list[pathlib.Path] = []
            runner = load_runner()
            with self.assertRaisesRegex(Exception, "tool closure digest changed"):
                runner.run_one(
                    context_root, run_directory, adapter_path,
                    repository_root=repository,
                    dispatch=lambda directory: attempts.append(directory),
                )
            self.assertEqual(attempts, [])
            self.assertFalse(run_directory.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)

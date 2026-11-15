#!/usr/bin/env python3
"""Authored controls for one bounded BASE member dispatch."""

from __future__ import annotations

import importlib.util
import json
import os
import pathlib
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

from test_base_qualification import canonical, create_context, digest, write_private

ROOT = pathlib.Path(__file__).resolve().parents[2]
RUNNER = ROOT / "scripts/run-base-diagnostic.py"


def load_runner():
    spec = importlib.util.spec_from_file_location("t64_base_runner", RUNNER)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load production BASE runner")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BaseRunControls(unittest.TestCase):
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
            bind("executable", fake_adapter)
            context_bytes = canonical(context)
            context_path = context_root / "context.json"
            context_path.write_bytes(context_bytes)
            context_path.chmod(0o600)
            binding_path = context_root / "binding.json"
            binding = json.loads(binding_path.read_bytes())
            binding["context_sha256"] = digest(context_bytes)
            binding_path.write_bytes(canonical(binding))
            binding_path.chmod(0o600)

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

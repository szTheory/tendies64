"""End-to-end tests for the authored, rights-cleared RSP smoke ROMs."""

import json
import os
import shlex
import tempfile
from pathlib import Path
import subprocess
import unittest


RUNNER = os.environ.get("T64_RSP_SMOKE_RUNNER", "build/gate/runner/t64-rsp-smoke")
OUT = Path("build/fixtures/rsp-smoke")
MANIFEST = Path("fixtures/manifest.json")
VARIANTS = (
    (0, "rsp-smoke", "t64-authored-rsp-smoke"),
    (1, "rsp-smoke-variant1", "t64-authored-rsp-smoke-wrong-result"),
    (2, "rsp-smoke-variant2", "t64-authored-rsp-smoke-unsupported-opcode"),
    (3, "rsp-smoke-variant3", "t64-authored-rsp-smoke-missing-break"),
    (4, "rsp-smoke-variant4", "t64-authored-rsp-smoke-altered-delay"),
    (5, "rsp-smoke-variant5", "t64-authored-rsp-smoke-same-bank-wrap"),
)


class RspSmokeTests(unittest.TestCase):
    def test_baseline_then_each_negative_reaches_its_classification(self):
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        entries = {entry["id"]: entry for entry in manifest["entries"]}
        baseline_semantics = None

        for variant, name, manifest_id in VARIANTS:
            step_partitions = (16, 1) if variant == 0 else (16,)
            for step_max in step_partitions:
                baseline_ok = False
                with self.subTest(variant=variant, step_max=step_max, purpose=manifest_id):
                    receipt_path = OUT / f"{name}.build.json"
                    rom_path = OUT / f"{name}.z64"
                    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
                    fixture = receipt["fixture"]
                    entry = entries[manifest_id]
                    expected_class = {0: "pass", 1: "guest-fail", 2: "refused",
                                      3: "missing-break", 4: "completion-delay-mismatch",
                                      5: "guest-fail"}[variant]
                    self.assertEqual(expected_class, fixture["expected"]["classification"])

                    self.assertEqual("t64.rsp-smoke-build", receipt["schema"])
                    self.assertEqual(variant, fixture["variant"])
                    self.assertEqual(
                        "ntsc-cart-4m-openipl3-rsp-functional-v1", fixture["profile"]
                    )
                    self.assertEqual(6, fixture["model"])
                    self.assertEqual(12, fixture["state"])
                    self.assertEqual(fixture["sha256"], entry["sha256"])
                    self.assertEqual(fixture["size_bytes"], entry["size"])
                    self.assertEqual("authored-here", entry["source_rights"])
                    self.assertEqual("cleared", entry["binary_rights"])
                    self.assertEqual(["build-only"], entry["distribution"])
                    self.assertIn("Unlicense", entry["rights_evidence"])

                    result = subprocess.run(
                        [
                            RUNNER, "--run", str(rom_path), fixture["sha256"],
                            "--boot-step-max", str(step_max),
                        ],
                        capture_output=True,
                        check=False,
                        text=True,
                        timeout=12,
                    )
                    lines = result.stdout.splitlines()
                    observed = next(
                        (line for line in lines if line.startswith("classification=")), ""
                    )
                    observed_fields = {}
                    for field in observed.split():
                        self.assertIn("=", field)
                        key, value = field.split("=", 1)
                        self.assertNotIn(key, observed_fields)
                        observed_fields[key] = value
                    self.assertEqual(
                        expected_class, observed_fields.get("classification"),
                        f"variant={variant} exit={result.returncode}\n"
                        f"{result.stdout}{result.stderr}",
                    )
                    self.assertEqual(0 if variant == 0 else 2, result.returncode)

                    if variant == 0:
                        for field in (
                            f"step_max={step_max}",
                            "rsp_branch_pc=010",
                            "rsp_delay_pc=014",
                            "rsp_pair_first_pc=018",
                            "rsp_pair_second_pc=01c",
                            "rsp_load_use_stall=1",
                            "rsp_break_pc=09c",
                            "rsp_retired=40",
                            "rsp_stall_slots=",
                            "cpu_terminal_pc=800006c0",
                            "cpu_terminal_opcode=1000ffff",
                            "delay_pc=800006c4",
                            "delay_opcode=00000000",
                            "rsp_status=00000003",
                            "rsp_semaphore=1",
                            "capture_bytes=19",
                            "transcript_hex=5436342d5253502d534d4f4b453a504153530a",
                            "dmem_80_9f=8899aabb7fffeeff00000000000000000000000000000000008899aabb7fffee",
                            "flag_word=00000002",
                            "load_use_word=11223344",
                            "input_word=11223344",
                            "vsar=0000,0001,7ffe",
                            "divider=0000,ffff",
                        ):
                            key, value = field.split("=", 1)
                            if value:
                                self.assertEqual(value, observed_fields.get(key))
                            else:
                                self.assertIn(key, observed_fields)
                        self.assertTrue(observed_fields.get("build_id", "").startswith("t64-build-v1-"))
                        stall_text = observed.split("rsp_stall_slots=", 1)[1].split()[0]
                        self.assertGreater(int(stall_text), 0)
                        semantic_state = {
                            field.split("=", 1)[0]: field.split("=", 1)[1]
                            for field in observed.split()
                            if "=" in field and not field.startswith("step_max=")
                        }
                        if step_max == 16:
                            baseline_semantics = semantic_state
                        else:
                            self.assertEqual(baseline_semantics, semantic_state)
                    elif variant == 1:
                        self.assertIn("guest_compare_expected=7f", result.stdout)
                    elif variant == 2:
                        self.assertIn("rsp_pc=01c opcode=4a0208f8", result.stdout)
                    elif variant == 3:
                        self.assertIn("rsp_pc=09c opcode=00000000", result.stdout)
                    elif variant == 4:
                        self.assertIn("branch_pc=800006c0", result.stdout)
                        self.assertIn("delay_pc=800006c4 delay_opcode=24170001", result.stdout)
                    elif variant == 5:
                        self.assertIn("guest_compare_expected=88", result.stdout)
                    baseline_ok = True
                if variant == 0 and not baseline_ok:
                    return  # Subtest already failed; no dependent negative may run.


    def test_changed_assembler_is_rejected_before_execution(self):
        with tempfile.TemporaryDirectory(prefix="tool-control-", dir=OUT) as name:
            directory = Path(name).resolve()
            marker = directory / "executed"
            fake = directory / "assembler"
            fake.write_text(
                "#!/bin/sh\n"
                + "printf executed > " + shlex.quote(str(marker)) + "\n"
                + "printf 'GNU assembler (GNU Binutils) 2.45\\nconfigured for mips64-elf\\n'\n",
                encoding="utf-8",
            )
            fake.chmod(0o700)
            environment = os.environ.copy()
            environment["T64_MIPS_AS"] = str(fake)
            result = subprocess.run(
                ["sh", "scripts/build-rsp-smoke.sh", "--selftest"],
                env=environment, capture_output=True, text=True, timeout=12,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertFalse(marker.exists(), "unqualified assembler executed before hash rejection")



    def test_changed_packer_is_rejected_before_execution(self):
        with tempfile.TemporaryDirectory(prefix="packer-control-", dir=OUT) as name:
            directory = Path(name).resolve()
            marker = directory / "executed"
            fake = directory / "packer"
            fake.write_text("#!/bin/sh\nprintf executed > " + shlex.quote(str(marker)) + "\n")
            fake.chmod(0o700)
            source = Path("scripts/build-rsp-smoke.sh").read_text()
            source = source.replace(
                'ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)',
                "ROOT=" + shlex.quote(str(Path.cwd())), 1,
            ).replace("PACKER=build/fixtures/t64_rom_pack", "PACKER=" + shlex.quote(str(fake)), 1)
            script = directory / "builder.sh"
            script.write_text(source)
            result = subprocess.run(["sh", str(script), "--selftest"],
                                    capture_output=True, text=True, timeout=12)
            self.assertNotEqual(0, result.returncode)
            self.assertIn("ROM packer differs", result.stderr)
            self.assertFalse(marker.exists(), "unqualified saved packer was executed")

    def test_refuses_unknown_changed_and_nonregular_roms_before_guest(self):
        receipt = json.loads((OUT / "rsp-smoke.build.json").read_text())
        known_sha = receipt["fixture"]["sha256"]
        with tempfile.TemporaryDirectory(prefix="input-control-", dir=OUT) as name:
            directory = Path(name)
            cases = [(directory / "absent", "0" * 64, "unknown-authored-fixture")]
            changed = directory / "changed.z64"
            data = bytearray((OUT / "rsp-smoke.z64").read_bytes())
            data[-1] ^= 1
            changed.write_bytes(data)
            cases.append((changed, known_sha, "rom-digest"))
            fifo = directory / "fifo"
            os.mkfifo(fifo, 0o600)
            cases.append((fifo, known_sha, "rom-metadata"))
            for path, digest, reason in cases:
                with self.subTest(reason=reason):
                    result = subprocess.run(
                        [RUNNER, "--run", str(path), digest],
                        capture_output=True, text=True, timeout=12,
                    )
                    self.assertEqual(4, result.returncode, result.stdout + result.stderr)
                    self.assertEqual(
                        f"classification=harness-error reason={reason}",
                        result.stdout.strip(),
                    )


if __name__ == "__main__":
    unittest.main(verbosity=2)

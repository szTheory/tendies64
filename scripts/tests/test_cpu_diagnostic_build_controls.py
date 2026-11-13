#!/usr/bin/env python3
"""Authored, offline controls for post-build layout/composition checks."""
import importlib.util
import json
import os
import pathlib
import struct
import tempfile
import types
import unittest
from unittest.mock import patch

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "verify-cpu-diagnostic-build.py"
SPEC = importlib.util.spec_from_file_location("composition", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def authored_layout():
    base = MODULE.BASE
    payload = bytearray(80)
    payload[:16] = bytes.fromhex("3c1d804067bdfff00800010c00000000")
    payload[32:40] = bytes.fromhex("0800010800000000")
    elf = bytearray(336)
    elf[:7] = b"\x7fELF\x02\x02\x01"
    struct.pack_into(">HHIQ", elf, 16, 2, 8, 1, MODULE.ENTRY)
    rows = [
        (".t64_entrypoint", 16, base + 0x400, 256, "CONTENTS, ALLOC, LOAD, READONLY, CODE"),
        (".t64_park", 8, base + 0x420, 288, "CONTENTS, ALLOC, LOAD, READONLY, CODE"),
        (".text", 16, base + 0x430, 304, "CONTENTS, ALLOC, LOAD, READONLY, CODE"),
        (".t64_selector", 16, base + 0x440, 320, "CONTENTS, ALLOC, LOAD, DATA"),
        (".bss", 16, base + 0x450, 336, "ALLOC"),
    ]
    for _, size, address, offset, flags in rows:
        if "CONTENTS" in flags:
            elf[offset:offset + size] = payload[address - MODULE.ENTRY:address - MODULE.ENTRY + size]
    symbols = {
        "entrypoint": MODULE.ENTRY, "park": MODULE.PARK,
        "loaded_rom_start": MODULE.ENTRY, "loaded_rom_end": base + 0x450,
        "__bss_start": base + 0x450, "__bss_end": base + 0x460,
        "__cpu_heap_limit": MODULE.HEAP_END, "__cpu_stack_bottom": MODULE.HEAP_END,
        "__cpu_stack_top": MODULE.RDRAM_END, "__cpu_rdram_start": base,
        "__cpu_rdram_end": MODULE.RDRAM_END, "__t64_selector_start": base + 0x440,
        "__t64_selector_end": base + 0x444, "T64_CPU_SELECTOR": base + 0x440,
    }
    text = "authored: file format elf64-bigmips\nstart address 0xffffffff80000400\nSections:\n"
    for index, (name, size, address, offset, flags) in enumerate(rows):
        text += f" {index} {name} {size:016x} {address:016x} {address:016x} {offset:08x} 2**4\n {flags}\n"
    text += "SYMBOL TABLE:\n"
    for name, address in symbols.items():
        text += f"{address:016x} g *ABS* 0000000000000000 {name}\n"
    return bytes(elf), bytes(payload), text, symbols


class LayoutControls(unittest.TestCase):
    def setUp(self):
        self.elf, self.payload, self.dump, self.symbols = authored_layout()

    def test_accepted_canonical_layout(self):
        result = MODULE.verify_layout(self.elf, self.payload, self.dump)
        self.assertEqual(result["selector_rom_offset"], 0x1040)
        self.assertEqual(result["loaded_bytes"], 80)
        self.assertEqual(result["available_heap_bytes"], 0x300000 - 0x460)

    def test_independent_symbol_mutations(self):
        mutations = {"entrypoint": MODULE.ENTRY + 4, "park": MODULE.PARK + 4,
                     "__cpu_stack_top": MODULE.RDRAM_END + 16,
                     "__bss_end": MODULE.HEAP_END + 16,
                     "__bss_start": MODULE.BASE + 0x470,
                     "__t64_selector_end": MODULE.BASE + 0x448,
                     "__cpu_rdram_start": 0x80000000}
        for name, value in mutations.items():
            with self.subTest(name=name):
                old = f"{self.symbols[name]:016x} g *ABS* 0000000000000000 {name}"
                new = f"{value:016x} g *ABS* 0000000000000000 {name}"
                self.assertIn(old, self.dump)
                with self.assertRaises(ValueError):
                    MODULE.verify_layout(self.elf, self.payload, self.dump.replace(old, new))

    def test_symbol_and_entry_ambiguity_refuses(self):
        for suffix in ("\nstart address 0xffffffff80000400\n",
                       f"\n{MODULE.PARK:016x} g *ABS* 0000000000000000 park\n"):
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                MODULE.verify_layout(self.elf, self.payload, self.dump + suffix)
        with self.assertRaisesRegex(ValueError, "incomplete"):
            MODULE.verify_layout(self.elf, self.payload, self.dump.replace(" T64_CPU_SELECTOR\n", " omitted\n"))

    def test_payload_tampering_refuses(self):
        payload = bytearray(self.payload)
        payload[18] = 1  # Non-section gap must stay zero too.
        with self.assertRaisesRegex(ValueError, "flat payload differs"):
            MODULE.verify_layout(self.elf, payload, self.dump)

    def test_actual_park_opcode_and_selector_initializer(self):
        for offset, reason in ((32, "park opcode"), (64, "selector"), (0, "stack setup")):
            payload, elf = bytearray(self.payload), bytearray(self.elf)
            payload[offset] ^= 1
            elf[256 + offset] ^= 1
            with self.subTest(offset=offset), self.assertRaisesRegex(ValueError, reason):
                MODULE.verify_layout(elf, payload, self.dump)

    def test_section_overlap_and_truncation_refuse(self):
        wrong = self.dump.replace("ffffffff80000430 ffffffff80000430", "ffffffff80000420 ffffffff80000420")
        with self.assertRaisesRegex(ValueError, "overlap"):
            MODULE.verify_layout(self.elf, self.payload, wrong)
        with self.assertRaisesRegex(ValueError, "truncated"):
            MODULE.verify_layout(self.elf[:-1], self.payload, self.dump)

    def test_format_and_header_mismatch_refuse(self):
        for data in (b"", self.elf.replace(b"\x7fELF\x02\x02", b"\x7fELF\x02\x01", 1)):
            with self.subTest(data=data[:7]), self.assertRaises(ValueError):
                MODULE.verify_layout(data, self.payload, self.dump)
        with self.assertRaisesRegex(ValueError, "format"):
            MODULE.verify_layout(self.elf, self.payload, self.dump.replace("elf64-bigmips", "elf64-littlemips"))


class CompositionControls(unittest.TestCase):
    def setUp(self):
        self.elf, self.payload, _, _ = authored_layout()
        self.boot = bytes([3]) * 4096
        self.font = bytes(index % 251 for index in range(762))
        header = bytearray(64)
        struct.pack_into(">6I", header, 0, 0x80371240, 15, 0x80000400, 0, len(self.payload), 0)
        title = b"TENDIES64 HELLO     "
        header[0x20:0x20 + len(title)] = title
        header[0x3A] = ord("E")
        self.rom = bytes(header) + self.boot[64:] + self.payload
        self.patches = [patch.object(MODULE, "BOOT_SHA", MODULE.digest(self.boot)),
                        patch.object(MODULE, "FONT_SHA", MODULE.digest(self.font))]
        for item in self.patches:
            item.start()
            self.addCleanup(item.stop)

    def test_accepted_authored_composition(self):
        MODULE.verify_composition(self.elf, self.payload, self.rom, self.boot, self.font)

    def test_each_composed_region_tamper_refuses(self):
        for offset in (4, 8, 16, 32, 100, 4097):
            mutated = bytearray(self.rom)
            mutated[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                MODULE.verify_composition(self.elf, self.payload, mutated, self.boot, self.font)

    def test_excluded_asset_in_actual_elf_refuses(self):
        with self.assertRaisesRegex(ValueError, "font bytes remain"):
            MODULE.verify_composition(self.elf + self.font, self.payload, self.rom, self.boot, self.font)

    def test_wrong_asset_identities_refuse(self):
        with self.assertRaisesRegex(ValueError, "IPL3 identity"):
            MODULE.verify_composition(self.elf, self.payload, self.rom, bytes(4096), self.font)
        with self.assertRaisesRegex(ValueError, "absence-scan identity"):
            MODULE.verify_composition(self.elf, self.payload, self.rom, self.boot, bytes(762))

    def test_private_path_and_bounded_reads(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            file = root / "sample"
            file.write_bytes(b"sample")
            self.assertEqual(MODULE.read_bounded(file, 6), b"sample")
            with self.assertRaises(ValueError):
                MODULE.read_bounded(file, 5)
            (root / "alias").symlink_to(file)
            with self.assertRaises(ValueError):
                MODULE.private_path(root, str(root / "alias"))
            with self.assertRaises(ValueError):
                MODULE.private_path(root, str(root.parent / "outside"))

    def test_main_writes_bound_private_report_with_committed_qualification_api(self):
        self.run_main_fixture(False)

    def test_tool_replaced_after_qualification_refuses_before_invocation(self):
        self.run_main_fixture(True)

    def test_symlinked_build_refuses_before_qualification_or_report(self):
        self.run_main_fixture(False, "symlink")

    def test_exposed_root_refuses_before_qualification_or_report(self):
        self.run_main_fixture(False, "exposed")

    def run_main_fixture(self, replace_tool, unsafe_root=None):
        with tempfile.TemporaryDirectory() as temporary:
            repository = pathlib.Path(temporary).resolve()
            scripts = repository / "scripts"
            scripts.mkdir()
            private = repository / "build/private-cpu-diagnostic"
            private.mkdir(mode=0o700, parents=True)
            fixtures = repository / "build/fixtures"
            fixtures.mkdir()
            (fixtures / "ipl3_compat.z64").write_bytes(self.boot)
            _, _, dump, _ = authored_layout()
            font_source = ("pub const FONT_GENEVA_9: [u8; 762] = ["
                           + ",".join(str(value) for value in self.font) + "];\n").encode()
            closure = json.dumps({"status": "PASS_PREBUILD_SOURCE",
                                  "source_revision": MODULE.SOURCE_REVISION}).encode()
            tool = private / "authored-objdump"
            tool.write_bytes(b"authored tool identity; never executed")
            receipt = {"assets": {"closure_report": {"sha256": MODULE.digest(closure)}},
                       "tools": {"target_binutils": [{"name": "objdump", "path": str(tool),
                                                       "sha256": MODULE.digest(tool.read_bytes())}]}}
            for name, data in {"elf": self.elf, "payload": self.payload,
                               "rom": self.rom, "font-source": font_source,
                               "closure": closure,
                               "qualification.json": json.dumps(receipt).encode()}.items():
                (private / name).write_bytes(data)
            # The committed verifier returns only these two keys. The tool is
            # not executed: the authored objdump response is injected below.
            qualification_calls = []
            def validate(*_):
                qualification_calls.append(True)
                if unsafe_root is not None:
                    raise AssertionError("qualification invoked before private root refusal")
                if replace_tool:
                    tool.write_bytes(b"changed after receipt verification")
                return {"host_c_compiler": "/unused", "target_binutils": {"objdump": str(tool)}}
            qualification = types.SimpleNamespace(validate=validate, Refusal=ValueError)
            spec = types.SimpleNamespace(loader=types.SimpleNamespace(exec_module=lambda _: None))
            argv = ["verifier"]
            for name in ("elf", "payload", "rom", "closure", "font-source"):
                argv += ["--" + name, str(private / name)]
            report = private / "report.json"
            argv += ["--boot", str(fixtures / "ipl3_compat.z64"),
                     "--report", str(report)]
            if unsafe_root == "symlink":
                (repository / "build").rename(repository / "redirected-build")
                (repository / "build").symlink_to(repository / "redirected-build", target_is_directory=True)
            elif unsafe_root == "exposed":
                private.chmod(0o755)
            previous_directory = pathlib.Path.cwd()
            try:
                with patch.object(MODULE, "__file__", str(scripts / "verifier.py")), \
                     patch.object(MODULE, "FONT_SOURCE_SHA", MODULE.digest(font_source)), \
                     patch.object(MODULE.sys, "argv", argv), \
                     patch.object(MODULE.importlib.util, "spec_from_file_location", return_value=spec), \
                     patch.object(MODULE.importlib.util, "module_from_spec", return_value=qualification), \
                     patch.object(MODULE.subprocess, "run", return_value=types.SimpleNamespace(stdout=dump.encode())) as invoke:
                    if unsafe_root is not None:
                        with self.assertRaisesRegex(ValueError, "private root"):
                            MODULE.main()
                        self.assertEqual(qualification_calls, [])
                        invoke.assert_not_called()
                        self.assertFalse(report.exists())
                        return
                    if replace_tool:
                        with self.assertRaisesRegex(ValueError, "objdump differs from the qualified receipt"):
                            MODULE.main()
                        invoke.assert_not_called()
                        self.assertFalse(report.exists())
                        return
                    self.assertEqual(MODULE.main(), 0)
            finally:
                os.chdir(previous_directory)
            result = json.loads(report.read_text())
            self.assertEqual(result["status"], "PASS_POSTBUILD_COMPOSITION")
            self.assertEqual(result["objdump_sha256"], MODULE.digest(tool.read_bytes()))
            self.assertFalse(result["guest_execution_verified"])
            self.assertEqual(report.stat().st_mode & 0o777, 0o600)


if __name__ == "__main__":
    unittest.main()

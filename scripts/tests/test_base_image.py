#!/usr/bin/env python3
"""Synthetic controls for source-complete BASE selection and selector bytes."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import pathlib
import os
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load_module(path: pathlib.Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load production module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


DERIVER = load_module(ROOT / "scripts/derive-base-diagnostic-source.py",
                      "t64_base_deriver_test")
OVERLAY = load_module(ROOT / "scripts/apply-base-diagnostic-overlay.py",
                      "t64_base_overlay_test")


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) +
            "\n").encode()


def fixture_inputs(root: pathlib.Path):
    source = root / "source"
    rsp = root / "rsp"
    source.mkdir()
    rsp.mkdir()
    (source / "src/tests/cpu").mkdir(parents=True)
    (source / "src/tests/rdp").mkdir(parents=True)
    (source / "src/tests/rsp").mkdir(parents=True)
    (source / "src/rsp").mkdir(parents=True)
    (rsp / "src/rsp").mkdir(parents=True)
    (rsp / "src/tests/rsp").mkdir(parents=True)
    (source / "src/tests/mod.rs").write_text("pub mod cpu;\npub mod testlist;\n")
    (source / "src/main.rs").write_text("static T64_CPU_SELECTOR: [u8; 4] = [0; 4];\n")
    (rsp / "src/rsp/mod.rs").write_text("pub mod runtime;\n")
    (rsp / "src/rsp/runtime.rs").write_text("pub fn runtime() {}\n")
    (rsp / "src/tests/rsp/mod.rs").write_text("pub mod rdp_example;\n")
    (rsp / "src/tests/rsp/rdp_example.rs").write_text("pub struct RdpExample;\n")
    (rsp / "src/tests/soft_asserts.rs").write_text("pub fn soft_assert() {}\n")
    (rsp / "src/text_out.rs").write_text("pub fn output() {}\n")
    (rsp / "src/print.rs").write_text("pub fn print() {}\n")
    (rsp / "src/graphics/mod.rs").parent.mkdir(parents=True)
    (rsp / "src/graphics/mod.rs").write_text("pub fn graphics() {}\n")
    constructors = ["CpuBasic", "CpuRare", "CpuWeird", "RdpBasic", "RdpInfo", "CpuTiming"]
    levels = ["BasicFunctionality", "RarelyUsed", "Weird", "RDPBasic",
              "RDPPrecise", "Timing"]
    source_files = [
        "src/tests/cpu/basic.rs", "src/tests/cpu/rare.rs",
        "src/tests/cpu/weird.rs", "src/tests/rdp/basic.rs",
        "src/tests/rsp/rdp_example.rs", "src/tests/cpu/timing.rs",
    ]
    for path in source_files:
        target = (source if not path.startswith("src/tests/rsp/") else rsp) / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text("pub struct Synthetic;\n")
    (source / "src/tests/testlist.rs").write_text(
        "\n".join(f"Box::new({name}{{}})," for name in constructors) + "\n")
    counts = [0, 1, 2, 1, 1, 1]
    rows = []
    audit_rows = []
    for index, (constructor, level, source_file, count) in enumerate(
            zip(constructors, levels, source_files, counts, strict=True)):
        impl_root = rsp if source_file.startswith("src/tests/rsp/") else source
        source_sha = sha((impl_root / source_file).read_bytes())
        group = "cpu" if index in {0, 1, 2, 5} else "non-cpu"
        lane = ("timing" if index == 5 else
                "required-base" if group == "cpu" else "skipped-non-cpu")
        ids = (["once"] if count == 0 else
               [f"v{value:05d}" for value in range(count)])
        values = ([{"id": "once", "index": 0,
                    "selector": f"r{index:04d}v00000"}] if count == 0 else
                  [{"id": ids[value], "index": value,
                    "selector": f"r{index:04d}v{value:05d}"}
                   for value in range(count)] if group == "cpu" else [])
        row = {
            "constructor": constructor, "effective_value_count": max(1, count),
            "group": group, "index": index, "lane": lane,
            "level": level, "name": f"synthetic {index}",
            "source_file": source_file,
            "source_file_sha256": source_sha,
            "source_group": "rsp" if index == 4 else "rdp" if index == 3 else group,
            "source_name": f"synthetic {index}",
            "source_value_count": count, "source_value_ids": ids,
            "values_method_sha256": "b" * 64,
            "values": values,
        }
        rows.append(row)
        audit_rows.append({
            "index": index, "line": index,
            "path": constructor, "source_file": source_file,
            "source_sha256": source_sha,
            "upstream_name": row["source_name"],
            "level": level, "source_value_count": count,
            "values_method_sha256": row["values_method_sha256"],
        })
    body = {
        "schema": "t64.cpu-source-projection", "schema_version": 2,
        "source_revision": "a" * 40, "registrations": rows,
    }
    projection = dict(body, projection_sha256=sha(canonical(body)))
    rsp_row = {
        "constructor": constructors[4], "effective_value_count": 1,
        "group": "rsp", "index": 4, "lane": "informational",
        "level": "RDPPrecise", "name": "synthetic info",
        "source_file": source_files[4], "source_file_sha256": rows[4]["source_file_sha256"],
        "source_name": rows[4]["source_name"], "source_value_count": 1,
        "values_method_sha256": rows[4]["values_method_sha256"],
        "values": [{"id": "v00000", "index": 0, "selector": "r0004v00000"}],
    }
    rsp_projection = {
        "schema": "t64.rsp-source-projection", "schema_version": 1,
        "component": "rsp", "status": "membership-prepared",
        "source_revision": body["source_revision"],
        "inputs": {"cpu_source_projection_sha256": sha(canonical(projection))},
        "registrations": [rsp_row],
    }
    audit = {"registrations": audit_rows}
    rsp_files = []
    for relative in (
            "src/rsp/mod.rs", "src/rsp/runtime.rs", "src/tests/rsp/mod.rs",
            "src/tests/rsp/rdp_example.rs", "src/tests/soft_asserts.rs",
            "src/text_out.rs", "src/print.rs", "src/graphics/mod.rs"):
        data = (rsp / relative).read_bytes()
        rsp_files.append({"path": relative, "sha256": sha(data)})
    rsp_closure = {"overlay": {"files": rsp_files}}
    cpu_closure = {"module_files": [
        {"path": path, "sha256": sha((source / path).read_bytes())}
        for path in source_files
        if (not path.startswith("src/tests/rsp/") and
            not path.startswith("src/tests/rdp/"))
    ]}
    return source, rsp, projection, rsp_projection, audit, cpu_closure, rsp_closure


class BaseImageControls(unittest.TestCase):
    def test_absence_scan_decodes_payload_after_source_identity_check(self) -> None:
        from types import SimpleNamespace
        builder = load_module(ROOT / "scripts/build-qualified-base-diagnostic.py", "base_font_guard")
        source = ("// authored zero array, not an external font\nconst FONT_GENEVA_9: [u8; 762] = [" +
                  ",".join("0" for _ in range(762)) + "];\n").encode()
        verifier = SimpleNamespace(FONT_SOURCE_SHA=sha(source), FONT_SHA=sha(bytes(762)))
        self.assertEqual(builder.excluded_font_bytes(source, verifier), bytes(762))
        with self.assertRaisesRegex(builder.Refusal, "source differs"):
            builder.excluded_font_bytes(source + b" ", verifier)
        with self.assertRaisesRegex(builder.Refusal, "bytes differ"):
            builder.excluded_font_bytes(source, SimpleNamespace(FONT_SOURCE_SHA=sha(source), FONT_SHA="0" * 64))

    def test_partial_qualification_inputs_are_archived_before_rederivation(self) -> None:
        from unittest.mock import patch
        with tempfile.TemporaryDirectory(prefix="base-stage-history-") as temporary:
            root = pathlib.Path(temporary)
            private, original = root / "active", root / "history/precombined-active"
            private.mkdir(mode=0o700)
            original.mkdir(mode=0o700, parents=True)
            old, partial = b"authored original authority\n", b"authored partial derivation\n"
            for path, data in ((private / "source-authority.json", old),
                               (original / "source-authority.json", old)):
                path.write_bytes(data)
                path.chmod(0o600)
            manifest = {"schema": "t64.base-precombined-active-archive.v1", "source_present": True,
                        "files": [{"path": "source-authority.json", "sha256": sha(old)}]}
            (original / "archive-manifest.json").write_bytes(canonical(manifest))
            (original / "archive-manifest.json").chmod(0o600)
            (private / "source-authority.json").write_bytes(partial)
            with patch.object(DERIVER, "HISTORICAL_ARCHIVE", original):
                DERIVER._verify_historical_archive(private)
                snapshots = list(original.parent.glob("inputs-*"))
                self.assertEqual(len(snapshots), 1)
                self.assertEqual((snapshots[0] / "source-authority.json").read_bytes(), partial)
                self.assertEqual((original / "source-authority.json").read_bytes(), old)
                DERIVER._verify_historical_archive(private)
                self.assertEqual(list(original.parent.glob("inputs-*")), snapshots)
                (snapshots[0] / "source-authority.json").write_bytes(b"tampered archive")
                with self.assertRaises(DERIVER.Refusal):
                    DERIVER._verify_historical_archive(private)

    def test_constructor_anchor_skips_only_adjacent_comment_lines(self) -> None:
        anchor = {"line": 0, "path": "authored::Selector"}
        self.assertEqual(OVERLAY.registration_expression(
            ["// authored registration note", "", "Box::new(authored::Selector {}),"], anchor),
            "Box::new(authored::Selector {})")
        with self.assertRaises(OVERLAY.Refusal):
            OVERLAY.registration_expression(["// authored note",
                "Box::new(authored::Foreign {}),", "Box::new(authored::Selector {}),"], anchor)

    def test_constructor_is_emitted_once_per_registration(self) -> None:
        rows = [{"registration_index": 7, "value_index": value,
                 "expression": "Box::new(authored::Selector {})"} for value in (0, 1)]
        rendered = OVERLAY.render_testlist(rows)
        self.assertEqual(rendered.count(b"(7usize,"), 1)

    def test_existing_module_visibility_does_not_duplicate_declarations(self) -> None:
        existing = b"mod soft_asserts;\npub(crate) mod rsp;\n"
        self.assertEqual(OVERLAY.merge_test_modules(existing), existing)
        missing = OVERLAY.merge_test_modules(b"pub mod rsp;\n")
        self.assertEqual(missing.count(b"mod rsp;"), 1)
        self.assertEqual(missing.count(b"mod soft_asserts;"), 1)

    def test_build_command_starts_and_enforces_its_deadline(self) -> None:
        builder = load_module(ROOT / "scripts/build-qualified-base-diagnostic.py",
                              "t64_base_build_startup_test")
        with tempfile.TemporaryDirectory(prefix="base-build-startup-") as temporary:
            work = pathlib.Path(temporary)
            output = builder.run_private_command([sys.executable, "-c", "print('authored startup')"],
                                                work, "startup", timeout=2)
            self.assertEqual(output, b"authored startup\n")
            with self.assertRaisesRegex(builder.Refusal, "failed"):
                builder.run_private_command([sys.executable, "-c", "import time; time.sleep(5)"],
                                            work, "deadline", timeout=0.1)
            receipt = json.loads((work / "deadline.command.json").read_bytes())
            self.assertTrue(receipt["timed_out"])
            self.assertTrue(receipt["process_group_empty"])

    def test_source_authority_keeps_five_levels_and_informational_rows(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            _, _, projection, rsp_projection, _, cpu_closure, rsp_closure = fixture_inputs(root)
            cpu_files = {row["path"]: row["sha256"]
                         for row in cpu_closure["module_files"]}
            rsp_files = {row["path"]: row["sha256"]
                         for row in rsp_closure["overlay"]["files"]}
            members = DERIVER.derive_members(
                projection, rsp_projection, cpu_files, rsp_files)
            self.assertEqual(
                {(row["index"], row["level"]) for row in members},
                {(0, "BasicFunctionality"), (1, "RarelyUsed"),
                 (2, "Weird"), (3, "RDPBasic"), (4, "RDPPrecise"),
                 (5, "Timing")},
            )
            self.assertIn((5, "Timing"),
                          {(row["index"], row["level"]) for row in members})
            blocked = next(row for row in members if row["index"] == 3)
            self.assertTrue(blocked["required"])
            self.assertEqual(blocked["execution"], "blocked")
            self.assertEqual(blocked["lane"], "blocked-non-cpu")
            informational = next(row for row in members if row["index"] == 4)
            self.assertFalse(informational["required"])
            self.assertEqual(informational["lane"], "informational")
            self.assertEqual(informational["execution"], "values")
            timing = next(row for row in members if row["index"] == 5)
            self.assertFalse(timing["required"])
            self.assertEqual(timing["lane"], "timing")
            self.assertEqual(timing["execution"], "values")
            self.assertEqual(
                [(row["index"], row["value_index"]) for row in members
                 if row["index"] == 2], [(2, 0), (2, 1)],
            )
            self.assertEqual(members[0]["execution"], "once")

    def test_non_cpu_member_runs_only_when_its_module_is_in_the_accepted_closure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source, _, projection, rsp_projection, audit, cpu_closure, rsp_closure = fixture_inputs(root)
            cpu_files = {row["path"]: row["sha256"]
                         for row in cpu_closure["module_files"]}
            rsp_files = {row["path"]: row["sha256"]
                         for row in rsp_closure["overlay"]["files"]}
            blocked = DERIVER.derive_members(
                projection, rsp_projection, cpu_files, rsp_files)
            rdp = next(row for row in blocked if row["index"] == 3)
            self.assertTrue(rdp["required"])
            self.assertEqual(rdp["lane"], "blocked-non-cpu")
            self.assertEqual(rdp["execution"], "blocked")

            rdp_path = "src/tests/rdp/basic.rs"
            cpu_files[rdp_path] = sha((source / rdp_path).read_bytes())
            dispatched = DERIVER.derive_members(
                projection, rsp_projection, cpu_files, rsp_files)
            rdp = next(row for row in dispatched if row["index"] == 3)
            self.assertTrue(rdp["required"])
            self.assertEqual(rdp["lane"], "required-base")
            self.assertEqual(rdp["execution"], "values")

            output = root / "closed-rdp"
            selected, _ = OVERLAY.apply_combined_overlay(
                source, root / "rsp", {"module_files": [
                    {"path": path, "sha256": value}
                    for path, value in cpu_files.items()
                ]}, rsp_closure, projection, rsp_projection, audit, source, output,
            )
            self.assertIn((3, 0), {
                (row["registration_index"], row["value_index"])
                for row in selected
            })

    def test_combined_overlay_preserves_global_ordinals_and_four_byte_selector(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            (source, rsp, projection, rsp_projection, audit,
             cpu_closure, closure) = fixture_inputs(root)
            output = root / "combined"
            selected, overlay = OVERLAY.apply_combined_overlay(
                source, rsp, cpu_closure, closure, projection,
                rsp_projection, audit, source, output,
            )
            self.assertTrue(overlay["source_revision"] == "a" * 40)
            pairs = [(row["registration_index"], row["value_index"])
                     for row in selected]
            self.assertEqual(pairs, [(0, 0), (1, 0), (2, 0), (2, 1), (4, 0), (5, 0)])
            generated = (output / "src/tests/testlist.rs").read_text()
            self.assertIn("(4usize, Box::new(RdpInfo {})", generated)
            self.assertIn("pub mod rsp;", (output / "src/tests/mod.rs").read_text())
            self.assertIn("pub mod soft_asserts;", (output / "src/tests/mod.rs").read_text())

            base = bytearray(range(64))
            base[12:16] = bytes(4)
            expected = hashlib.sha256(
                bytes(base[:12]) + bytes.fromhex("00020001") + bytes(base[16:])
            ).hexdigest()
            self.assertEqual(OVERLAY.selector_value(2, 1), bytes.fromhex("00020001"))
            self.assertEqual(OVERLAY.selected_rom_sha256(bytes(base), 12, 2, 1), expected)
            with self.assertRaises(OVERLAY.Refusal):
                OVERLAY.selected_rom_sha256(bytes(base), 13, 2, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)

#!/usr/bin/env python3
"""Self-contained controls for the production CPU source overlay applier."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

PIN = "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_patch(path: pathlib.Path, source: bytes, offset: int, old: bytes,
                replacement: bytes, *, bad_file_hash: bool = False) -> None:
    patch = {
        "schema": "t64.cpu-source-overlay",
        "schema_version": 1,
        "source_revision": PIN,
        "edits": [{
            "path": "src/sample.rs",
            "file_sha256": "0" * 64 if bad_file_hash else sha256(source),
            "offset": offset,
            "length": len(old),
            "range_sha256": sha256(old),
            "replacement_utf8": replacement.decode("ascii"),
        }],
    }
    path.write_text(json.dumps(patch, sort_keys=True, separators=(",", ":")) + "\n")


def write_document(path: pathlib.Path, edits: list[dict[str, object]]) -> None:
    patch = {
        "schema": "t64.cpu-source-overlay",
        "schema_version": 1,
        "source_revision": PIN,
        "edits": edits,
    }
    path.write_text(json.dumps(patch, sort_keys=True, separators=(",", ":")) + "\n")


def run_case(repo: pathlib.Path, applier: pathlib.Path, name: str,
             test_fn) -> tuple[str, str]:
    try:
        test_fn(repo, applier)
        return name, "passed"
    except AssertionError as error:
        return name, f"failed: {error}"


def test_exact_range_replacement(repo: pathlib.Path, applier: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        (root / "src").mkdir(parents=True)
        source = b"prefix OLD suffix\n"
        target = root / "src/sample.rs"
        target.write_bytes(source)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_patch(patch, source, 7, b"OLD", b"NEW")
        result = subprocess.run(
            [sys.executable, "-B", str(applier), "--apply", str(root), str(patch)],
            check=False, text=True, capture_output=True,
        )
        assert result.returncode == 0, f"applier returned {result.returncode}: {result.stderr.strip()}"
        assert target.read_bytes() == b"prefix NEW suffix\n", (
            "exact guarded replacement did not change only its selected byte range; "
            f"observed {target.read_bytes()!r}"
        )


def test_bad_file_hash_refused_before_write(repo: pathlib.Path, applier: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        (root / "src").mkdir(parents=True)
        source = b"prefix OLD suffix\n"
        target = root / "src/sample.rs"
        target.write_bytes(source)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_patch(patch, source, 7, b"OLD", b"NEW", bad_file_hash=True)
        result = subprocess.run(
            [sys.executable, "-B", str(applier), "--apply", str(root), str(patch)],
            check=False, text=True, capture_output=True,
        )
        assert result.returncode == 4 and "source file SHA-256 mismatch" in result.stderr, (
            f"source-hash refusal was not reached: exit={result.returncode}, stderr={result.stderr!r}"
        )
        assert target.read_bytes() == source, "refused patch changed its source file"


def test_overlapping_edits_refused_without_partial_write(repo: pathlib.Path, applier: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        (root / "src").mkdir(parents=True)
        first = b"abcdef\n"
        second = b"other\n"
        first_path = root / "src/first.rs"
        second_path = root / "src/second.rs"
        first_path.write_bytes(first)
        second_path.write_bytes(second)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_document(patch, [
            {"path": "src/first.rs", "file_sha256": sha256(first), "offset": 1,
             "length": 3, "range_sha256": sha256(first[1:4]), "replacement_utf8": "XYZ"},
            {"path": "src/first.rs", "file_sha256": sha256(first), "offset": 2,
             "length": 2, "range_sha256": sha256(first[2:4]), "replacement_utf8": "QR"},
            {"path": "src/second.rs", "file_sha256": sha256(second), "offset": 0,
             "length": 5, "range_sha256": sha256(second[:5]), "replacement_utf8": "OTHER"},
        ])
        result = subprocess.run(
            [sys.executable, "-B", str(applier), "--apply", str(root), str(patch)],
            check=False, text=True, capture_output=True,
        )
        assert result.returncode == 4 and "byte ranges overlap" in result.stderr, (
            f"overlap refusal was not reached: exit={result.returncode}, stderr={result.stderr!r}"
        )
        assert first_path.read_bytes() == first and second_path.read_bytes() == second, (
            "an invalid multi-file overlay partially wrote source files"
        )


def test_symlinked_source_path_refused(repo: pathlib.Path, applier: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        outside = pathlib.Path(temporary) / "outside"
        root.mkdir()
        outside.mkdir()
        source = b"prefix OLD suffix\n"
        target = outside / "sample.rs"
        target.write_bytes(source)
        (root / "src").symlink_to(outside, target_is_directory=True)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_patch(patch, source, 7, b"OLD", b"NEW")
        result = subprocess.run(
            [sys.executable, "-B", str(applier), "--apply", str(root), str(patch)],
            check=False, text=True, capture_output=True,
        )
        assert result.returncode == 4 and "traverses a symlink" in result.stderr, (
            f"symlink refusal was not reached: exit={result.returncode}, stderr={result.stderr!r}"
        )
        assert target.read_bytes() == source, "symlink-refused overlay changed the external target"


def test_path_escape_refused(repo: pathlib.Path, applier: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        (root / "src").mkdir(parents=True)
        outside = pathlib.Path(temporary) / "outside.rs"
        source = b"prefix OLD suffix\n"
        outside.write_bytes(source)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_patch(patch, source, 7, b"OLD", b"NEW")
        document = json.loads(patch.read_text())
        document["edits"][0]["path"] = "../outside.rs"
        patch.write_text(json.dumps(document))
        result = subprocess.run(
            [sys.executable, "-B", str(applier), "--apply", str(root), str(patch)],
            check=False, text=True, capture_output=True,
        )
        assert result.returncode == 4 and "path escapes source root" in result.stderr, (
            f"path-escape refusal was not reached: exit={result.returncode}, stderr={result.stderr!r}"
        )
        assert outside.read_bytes() == source, "path-escape-refused overlay changed external bytes"


def main() -> int:
    repository = pathlib.Path(__file__).resolve().parents[2]
    applier = repository / "scripts/apply-cpu-diagnostic-overlay.py"
    cases = [
        ("exact_range_replacement", test_exact_range_replacement),
        ("bad_file_hash_refused_before_write", test_bad_file_hash_refused_before_write),
        ("overlapping_edits_refused_without_partial_write", test_overlapping_edits_refused_without_partial_write),
        ("symlinked_source_path_refused", test_symlinked_source_path_refused),
        ("path_escape_refused", test_path_escape_refused),
    ]
    suite = ET.Element("testsuite", name="cpu-diagnostic-overlay", tests=str(len(cases)),
                       failures="0", errors="0")
    results: list[dict[str, str]] = []
    for name, test_fn in cases:
        case = ET.SubElement(suite, "testcase", classname="cpu-diagnostic-overlay", name=name)
        result_name, status = run_case(repository, applier, name, test_fn)
        results.append({"case": result_name, "status": status})
        if status.startswith("failed:"):
            suite.set("failures", str(int(suite.get("failures", "0")) + 1))
            failure = ET.SubElement(case, "failure", message=status[8:])
            failure.text = status
    report = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else None
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(suite).write(report, encoding="utf-8", xml_declaration=True)
    print(json.dumps({"tests": len(cases), "failures": int(suite.get("failures", "0")),
                      "results": results}, sort_keys=True))
    return int(suite.get("failures", "0") != "0")


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Self-contained controls for the production CPU source overlay applier."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

PIN = "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_patch(path: pathlib.Path, source: bytes, offset: int, old: bytes,
                replacement: bytes, *, bad_file_hash: bool = False,
                target_path: str = "src/sample.rs", schema_version: object = 1) -> None:
    patch = {
        "schema": "t64.cpu-source-overlay",
        "schema_version": schema_version,
        "source_revision": PIN,
        "edits": [{
            "path": target_path,
            "file_sha256": "0" * 64 if bad_file_hash else sha256(source),
            "offset": offset,
            "length": len(old),
            "range_sha256": sha256(old),
            "replacement_utf8": replacement.decode("ascii"),
        }],
    }
    path.write_text(json.dumps(patch, sort_keys=True, separators=(",", ":")) + "\n")


def write_document(path: pathlib.Path, edits: list[dict[str, object]],
                   *, schema_version: object = 1) -> None:
    patch = {
        "schema": "t64.cpu-source-overlay",
        "schema_version": schema_version,
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
        assert result.returncode == 4 and "path escapes its permitted directory" in result.stderr, (
            f"path-escape refusal was not reached: exit={result.returncode}, stderr={result.stderr!r}"
        )
        assert outside.read_bytes() == source, "path-escape-refused overlay changed external bytes"


def test_git_metadata_target_refused(repo: pathlib.Path, applier: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        metadata = root / ".git"
        metadata.mkdir(parents=True)
        source = b"config\n"
        target = metadata / "config"
        target.write_bytes(source)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_patch(patch, source, 0, source, b"mutated\n", target_path=".git/config")
        result = subprocess.run(
            [sys.executable, "-B", str(applier), "--apply", str(root), str(patch)],
            check=False, text=True, capture_output=True,
        )
        assert result.returncode == 4 and "names .git" in result.stderr, (
            f"Git-metadata refusal was not reached: exit={result.returncode}, stderr={result.stderr!r}"
        )
        assert target.read_bytes() == source, "Git-metadata refusal changed repository metadata"


def test_schema_version_types_refused(repo: pathlib.Path, applier: pathlib.Path) -> None:
    for version in (True, 1.0):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary) / "checkout"
            (root / "src").mkdir(parents=True)
            source = b"prefix OLD suffix\n"
            target = root / "src/sample.rs"
            target.write_bytes(source)
            patch = pathlib.Path(temporary) / "overlay.json"
            write_patch(patch, source, 7, b"OLD", b"NEW", schema_version=version)
            result = subprocess.run(
                [sys.executable, "-B", str(applier), "--apply", str(root), str(patch)],
                check=False, text=True, capture_output=True,
            )
            assert result.returncode == 4 and "unsupported overlay schema" in result.stderr, (
                f"schema version type {type(version).__name__} was not refused: "
                f"exit={result.returncode}, stderr={result.stderr!r}"
            )
            assert target.read_bytes() == source, "invalid schema version changed source bytes"


def test_later_publish_failure_rolls_back(repo: pathlib.Path, applier: pathlib.Path) -> None:
    spec = importlib.util.spec_from_file_location("t64_cpu_overlay_under_test", applier)
    assert spec is not None and spec.loader is not None, "could not load production overlay module"
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        (root / "src").mkdir(parents=True)
        first = b"first old\n"
        second = b"second old\n"
        first_path = root / "src/first.rs"
        second_path = root / "src/second.rs"
        first_path.write_bytes(first)
        second_path.write_bytes(second)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_document(patch, [
            {"path": "src/first.rs", "file_sha256": sha256(first), "offset": 6,
             "length": 3, "range_sha256": sha256(b"old"), "replacement_utf8": "new"},
            {"path": "src/second.rs", "file_sha256": sha256(second), "offset": 7,
             "length": 3, "range_sha256": sha256(b"old"), "replacement_utf8": "new"},
        ])
        calls = 0

        def fail_second_publish(source_path: pathlib.Path, target_path: pathlib.Path) -> None:
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("injected later-file publish failure")
            os.replace(source_path, target_path)

        try:
            module.apply_overlay(root, patch, replace_operation=fail_second_publish)
        except ValueError as error:
            assert "already-published edits were restored" in str(error), (
                f"rollback failure was not reported precisely: {error}"
            )
        else:
            raise AssertionError("injected later-file publish failure was accepted")
        assert calls == 3, f"expected failed second publish plus one rollback, observed {calls} operations"
        assert first_path.read_bytes() == first and second_path.read_bytes() == second, (
            "later-file publication failure did not restore every original file"
        )
        leftovers = list((root / "src").glob(".cpu-overlay*"))
        assert not leftovers, f"failed transaction left temporary files: {leftovers}"


def main() -> int:
    repository = pathlib.Path(__file__).resolve().parents[2]
    applier = repository / "scripts/apply-cpu-diagnostic-overlay.py"
    cases = [
        ("exact_range_replacement", test_exact_range_replacement),
        ("bad_file_hash_refused_before_write", test_bad_file_hash_refused_before_write),
        ("overlapping_edits_refused_without_partial_write", test_overlapping_edits_refused_without_partial_write),
        ("symlinked_source_path_refused", test_symlinked_source_path_refused),
        ("path_escape_refused", test_path_escape_refused),
        ("git_metadata_target_refused", test_git_metadata_target_refused),
        ("schema_version_types_refused", test_schema_version_types_refused),
        ("later_publish_failure_rolls_back", test_later_publish_failure_rolls_back),
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

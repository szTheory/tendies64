#!/usr/bin/env python3
"""Self-contained controls for the production CPU source overlay applier."""

from __future__ import annotations

import hashlib
import copy
import contextlib
import importlib.util
import io
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


def load_applier(applier: pathlib.Path):
    spec = importlib.util.spec_from_file_location("t64_cpu_overlay_under_test", applier)
    assert spec is not None and spec.loader is not None, "could not load production overlay module"
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_source_closure(project: pathlib.Path):
    script = project / "scripts/verify-cpu-diagnostic-source-closure.py"
    spec = importlib.util.spec_from_file_location("t64_cpu_source_closure_under_test", script)
    assert spec is not None and spec.loader is not None, "could not load source closure verifier"
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_unit_graph_verifier(project: pathlib.Path):
    script = project / "scripts/verify-cpu-diagnostic-unit-graph.py"
    spec = importlib.util.spec_from_file_location("t64_cpu_unit_graph_under_test", script)
    assert spec is not None and spec.loader is not None, "could not load Cargo unit-graph verifier"
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_vendor_preparer(project: pathlib.Path):
    script = project / "scripts/prepare-cpu-diagnostic-vendor.py"
    spec = importlib.util.spec_from_file_location("t64_cpu_vendor_under_test", script)
    assert spec is not None and spec.loader is not None, "could not load vendor preparer"
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


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


def test_registration_expression_forms_preserved(repo: pathlib.Path, applier: pathlib.Path) -> None:
    module = load_applier(applier)
    cases = [
        (
            ["    Box::new(", "        super::sample::Braced {},", "    ),"],
            "super::sample::Braced",
            "Box::new(super::sample::Braced {})",
        ),
        (
            ["    Box::new(", "        super::sample::Unit,", "    ),"],
            "super::sample::Unit",
            "Box::new(super::sample::Unit)",
        ),
    ]
    for lines, constructor, expected in cases:
        rendered = module.render_registration_expression(lines, 0, constructor)
        assert rendered == expected, (
            f"source registration form changed: expected {expected!r}, observed {rendered!r}"
        )


def test_registration_expression_unknown_forms_refused(repo: pathlib.Path, applier: pathlib.Path) -> None:
    module = load_applier(applier)
    cases = [
        (["Box::new(super::sample::Other {})"], "super::sample::Expected"),
        (["Box::new(super::sample::Expected { field: true })"], "super::sample::Expected"),
        (["Box::new(super::sample::Expected(true))"], "super::sample::Expected"),
    ]
    for lines, constructor in cases:
        try:
            module.render_registration_expression(lines, 0, constructor)
        except ValueError as error:
            assert "pinned source expression" in str(error), (
                f"unknown source form had an unexpected refusal: {error}"
            )
        else:
            raise AssertionError(f"unrecognized registration form was accepted: {lines!r}")


def test_public_manifest_replacement_hashes_match(repo: pathlib.Path, applier: pathlib.Path) -> None:
    overlay_root = repo / "tests/cpu-diagnostic"
    manifest_path = overlay_root / "overlay.patch"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest.get("schema") == "t64.cpu-source-overlay", "public overlay schema is wrong"
    edits = manifest.get("edits")
    assert isinstance(edits, list), "public overlay edits are missing"
    replacements: dict[str, str] = {}
    for edit in edits:
        if "replacement_file" not in edit:
            continue
        relative = pathlib.PurePosixPath(edit["replacement_file"])
        assert (
            not relative.is_absolute()
            and ".." not in relative.parts
            and ".git" not in relative.parts
        ), f"public replacement path escapes overlay root: {relative}"
        path = overlay_root.joinpath(*relative.parts)
        assert not path.is_symlink() and path.is_file(), f"public replacement is missing or linked: {relative}"
        expected = edit.get("replacement_sha256")
        assert isinstance(expected, str) and len(expected) == 64, (
            f"public replacement hash is malformed: {relative}"
        )
        observed = sha256(path.read_bytes())
        assert observed == expected, (
            f"public replacement bytes differ from manifest: {relative}: "
            f"expected {expected}, observed {observed}"
        )
        replacements[str(relative)] = expected
    assert len(replacements) == 14, (
        f"expected 14 authored replacement files, observed {len(replacements)}"
    )


def test_exception_storm_park_directive_is_exact_and_fail_closed(repo: pathlib.Path, applier: pathlib.Path) -> None:
    manifest = json.loads((repo / "tests/cpu-diagnostic/overlay.patch").read_text(encoding="utf-8"))
    edits = [
        edit for edit in manifest.get("edits", [])
        if isinstance(edit, dict) and edit.get("path") == "src/exception_handler.rs"
        and edit.get("offset") == 11179
    ]
    assert len(edits) == 1, (
        "the pinned exception-storm emulator exit must have exactly one guarded overlay edit"
    )
    edit = edits[0]
    assert (
        edit.get("file_sha256") == "55e323fbc7407930ef6f0caed2fd97de8337002fcfb77a5e029a6f4a73bc47d2"
        and edit.get("length") == 37
        and edit.get("range_sha256") == "eeb7ed4517e01dd552c6a5948e01fd609b088f9babf0e10f420bad4f3d452270"
        and edit.get("replacement_utf8") == ""
    ), "emulator exit removal is not bound to the exact reviewed byte range or does not preserve the following park"
    module = load_applier(applier)
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        (root / "src").mkdir(parents=True)
        target = root / "src/exception_handler.rs"
        source = b"before\n    emux::xioctl_exit();\n    loop {}\nafter\n"
        target.write_bytes(source)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_patch(
            patch, source, len(b"before\n"), b"    emux::xioctl_exit();\n", b"",
            target_path="src/exception_handler.rs",
        )
        result = subprocess.run(
            [sys.executable, "-B", str(applier), "--apply", str(root), str(patch)],
            check=False, text=True, capture_output=True,
        )
        assert result.returncode == 0, f"exact authored park edit was refused: {result.stderr.strip()}"
        assert target.read_bytes() == b"before\n    loop {}\nafter\n", (
            "exact line removal did not preserve the following fatal park loop"
        )


def test_cpu_profile_bss_guard_addresses(repo: pathlib.Path, applier: pathlib.Path) -> None:
    source = (repo / "tests/cpu-diagnostic/replacements/main.rs").read_text(encoding="utf-8")
    assert "const CPU_PROFILE_RDRAM_BASE: usize = 0xffff_ffff_8000_0000;" in source, (
        "RDRAM base is not represented as the canonical 64-bit MIPS address"
    )
    assert "const CPU_PROFILE_RDRAM_END: usize = CPU_PROFILE_RDRAM_BASE + CPU_PROFILE_RDRAM_BYTES;" in source, (
        "RDRAM end does not preserve the canonical 64-bit address domain"
    )
    terms = [
        "bss_start_address < CPU_PROFILE_RDRAM_BASE",
        "bss_start_address > bss_end_address",
        "bss_end_address > heap_limit",
        "bss_end_address > CPU_PROFILE_RDRAM_END",
        "heap_limit > stack_bottom",
        "stack_bottom < CPU_PROFILE_RDRAM_BASE",
        "stack_bottom > stack_top",
        "stack_top > CPU_PROFILE_RDRAM_END",
    ]
    guard_start = source.index("if bss_start_address < CPU_PROFILE_RDRAM_BASE")
    guard_end = source.index("\n    {", guard_start)
    guard = source[guard_start:guard_end]
    positions = [guard.find(term) for term in terms]
    assert all(position >= 0 for position in positions) and positions == sorted(positions), (
        "RDRAM layout guard does not retain the exact reviewed comparison order"
    )
    assert source.index("ptr::write_bytes(bss_start, 0, bss_length)") > guard_end, (
        "BSS is cleared before the complete address/layout guard"
    )

    base = 0xFFFF_FFFF_8000_0000
    end = base + 4 * 1024 * 1024

    def failure(start: int, bss_end: int, heap: int, stack_bottom: int, stack_top: int) -> str | None:
        predicates = [
            (start < base, "bss_start_below_rdram"),
            (start > bss_end, "bss_end_before_start"),
            (bss_end > heap, "bss_end_above_heap"),
            (bss_end > end, "bss_end_above_rdram"),
            (heap > stack_bottom, "heap_overlaps_stack"),
            (stack_bottom < base, "stack_below_rdram"),
            (stack_bottom > stack_top, "stack_end_before_start"),
            (stack_top > end, "stack_above_rdram"),
        ]
        return next((reason for rejected, reason in predicates if rejected), None)

    assert failure(base + 0x1000, base + 0x2000, base + 0x300000,
                   base + 0x3F0000, end) is None, "canonical in-range BSS/heap/stack was rejected"
    assert failure(0x80001000, 0x80002000, 0x80300000,
                   0x803F0000, 0x80400000) == "bss_start_below_rdram", (
        "zero-extended MIPS addresses were accepted as canonical"
    )
    assert failure(base + 0x1000, base + 0x310000, base + 0x300000,
                   base + 0x3F0000, end) == "bss_end_above_heap", (
        "BSS overlap with the heap was accepted"
    )
    assert failure(base + 0x1000, end + 1, end + 2,
                   end + 3, end + 4) == "bss_end_above_rdram", (
        "BSS beyond the 4 MiB profile was accepted"
    )
    assert failure(base + 0x2000, base + 0x1000, base + 0x300000,
                   base + 0x3F0000, end) == "bss_end_before_start", (
        "end-before-start BSS symbols were accepted"
    )
    assert failure(base + 0x1000, base + 0x2000, base + 0x3F0001,
                   base + 0x3F0000, end) == "heap_overlaps_stack", (
        "heap/stack overlap was accepted"
    )


def test_frozen_unit_graph_acceptance_and_refusals(repo: pathlib.Path, applier: pathlib.Path) -> None:
    verifier = load_unit_graph_verifier(repo)
    target_path = "/tmp/t64-authored-target.json"
    project_root = pathlib.Path("/tmp/t64-authored-project")
    sysroot_root = pathlib.Path("/tmp/t64-authored-sysroot/library")
    root_id = "path+file:///tmp/t64-authored-project#fixture-root@1.0.0"
    core_id = "path+file:///tmp/t64-authored-sysroot/library/core#0.0.0"
    alloc_id = "path+file:///tmp/t64-authored-sysroot/library/alloc#0.0.0"
    builtins_id = "path+file:///tmp/t64-authored-sysroot/library/compiler-builtins/compiler-builtins#compiler_builtins@0.1.160"

    def unit(package: str, name: str, kind: list[str], platform: str | None, panic: str,
             *, mode: str = "build", features: list[str] | None = None, is_std: bool = False) -> dict:
        return {
            "dependencies": [], "features": features or [], "is_std": is_std, "mode": mode,
            "pkg_id": package, "platform": platform,
            "profile": {
                "codegen_backend": None, "codegen_units": None, "debug_assertions": False,
                "debuginfo": 0, "incremental": False, "lto": "fat", "name": "release",
                "opt_level": "3", "overflow_checks": False, "panic": panic, "rpath": False,
                "split_debuginfo": None, "strip": {"resolved": {"Named": "debuginfo"}},
            },
            "target": {
                "crate_types": ["bin" if kind == ["bin"] else "lib"], "doc": True,
                "doctest": False, "edition": "2021", "kind": kind,
                "name": name, "src_path": f"/tmp/{name}.rs", "test": kind == ["bin"],
            },
        }

    builtin_features = ["arch", "compiler-builtins", "default", "unmangled-names"]
    graph = {
        "version": 1, "roots": [0], "units": [
            unit(root_id, "fixture-root", ["bin"], target_path, "abort", features=["base", "default", "quick"]),
            unit(core_id, "core", ["lib"], target_path, "abort", is_std=True),
            unit(alloc_id, "alloc", ["lib"], target_path, "abort", is_std=True),
            unit(builtins_id, "compiler_builtins", ["lib"], target_path, "abort", features=builtin_features, is_std=True),
            unit(builtins_id, "build-script-build", ["custom-build"], None, "unwind", mode="build", features=builtin_features, is_std=True),
            unit(builtins_id, "build-script-build", ["custom-build"], target_path, "unwind", mode="run-custom-build", features=builtin_features, is_std=True),
        ],
    }
    project_manifest = {
        "package": {"name": "fixture-root", "version": "1.0.0"},
        "features": {"default": ["base"], "base": ["quick"], "quick": []},
    }
    project_lock = {"package": [{"name": "fixture-root", "version": "1.0.0", "dependencies": []}]}
    sysroot_lock = {"package": [
        {"name": "core", "version": "0.0.0", "dependencies": []},
        {"name": "alloc", "version": "0.0.0", "dependencies": ["core", "compiler_builtins"]},
        {"name": "compiler_builtins", "version": "0.1.160", "dependencies": ["core"]},
    ]}
    target_spec = {"panic-strategy": "abort", "__path__": target_path}
    cargo_config = {"unstable": {"build-std": ["core", "alloc"]}}
    vendor = {
        "status": "PASS_PROJECT_AND_SYSROOT_RESOLUTION_SOURCES_MATERIALIZED",
        "project_packages": [], "sysroot_registry_packages": [],
        "sysroot_path_packages": [
            {"name": "core", "version": "0.0.0"},
            {"name": "alloc", "version": "0.0.0"},
            {"name": "compiler_builtins", "version": "0.1.160"},
        ],
    }
    review = {"status": "PASS_INPUT_AUDIT_NO_USE", "external_code_executed": False, "packages": []}
    rust_src = {
        "build_std_requested": ["core", "alloc"],
        "source_derived_expected_compile_packages": [
            {"name": "core", "version": "0.0.0", "build_script_review": "No build.rs"},
            {"name": "alloc", "version": "0.0.0", "build_script_review": "No build.rs"},
            {"name": "compiler_builtins", "version": "0.1.160", "build_script_review": "Reviewed build.rs; c branch disabled"},
        ],
    }

    def validate(graph_value: dict) -> dict:
        return verifier.validate_graph(graph_value, project_manifest, project_lock, sysroot_lock,
                                       target_spec, cargo_config, vendor, review, rust_src,
                                       project_root, sysroot_root)

    accepted = validate(graph)
    assert accepted["status"] == "PASS_FROZEN_OFFLINE_UNIT_GRAPH" and accepted["built"] is False, (
        "valid authored graph did not pass as graph-only evidence"
    )
    for field, value, reason in (
        ("version", True, "Cargo unit graph schema is unsupported"),
        ("version", 1.0, "Cargo unit graph schema is unsupported"),
        ("roots", [True], "Cargo unit graph root index is malformed or outside the graph"),
        ("roots", [1.0], "Cargo unit graph root index is malformed or outside the graph"),
    ):
        malformed = copy.deepcopy(graph)
        malformed[field] = value
        try:
            validate(malformed)
        except verifier.Refusal as error:
            assert reason in str(error), f"wrong type refusal for {field}: {error}"
        else:
            raise AssertionError(f"unit graph accepted invalid {field} type {value!r}")
    cases = []
    mismatch = copy.deepcopy(graph)
    mismatch["units"][0]["profile"]["panic"] = "unwind"
    cases.append((mismatch, "target panic mismatch was accepted"))
    enabled_c = copy.deepcopy(graph)
    enabled_c["units"][3]["features"].append("c")
    cases.append((enabled_c, "compiler_builtins C feature was accepted"))
    extra_feature = copy.deepcopy(graph)
    extra_feature["units"][0]["features"].append("cycle")
    cases.append((extra_feature, "unapproved root feature was accepted"))
    for candidate, message in cases:
        refused = False
        try:
            validate(candidate)
        except verifier.Refusal:
            refused = True
        assert refused, message


def test_static_module_closure_follows_only_explicit_items(repo: pathlib.Path, applier: pathlib.Path) -> None:
    module = load_source_closure(repo)
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary)
        (root / "src/alpha").mkdir(parents=True)
        (root / "src/inline").mkdir(parents=True)
        (root / "src/main.rs").write_text(
            '// mod comment_only;\n'
            'const TEXT: &str = "mod string_only;";\n'
            'mod alpha;\n'
            'mod inline { pub mod nested; }\n',
            encoding="utf-8",
        )
        (root / "src/alpha.rs").write_text("pub mod leaf;\n", encoding="utf-8")
        (root / "src/alpha/leaf.rs").write_text("pub const VALUE: u8 = 1;\n", encoding="utf-8")
        (root / "src/inline/nested.rs").write_text("pub const VALUE: u8 = 2;\n", encoding="utf-8")
        files, edges = module.discover_module_closure(root)
        expected = {
            "src/main.rs", "src/alpha.rs", "src/alpha/leaf.rs", "src/inline/nested.rs"
        }
        assert set(files) == expected, f"module closure differs: {sorted(files)}"
        assert len(edges) == 3, f"expected three external modules, observed {len(edges)}"


def test_static_module_closure_refuses_unknown_inclusion_forms(repo: pathlib.Path, applier: pathlib.Path) -> None:
    module = load_source_closure(repo)
    cases = [
        ("mod missing;\n", "missing external module"),
        ('#[path = "elsewhere.rs"] mod linked;\n', "unreviewed Rust path attribute"),
        ('#[cfg(feature = "optional")] mod conditional;\n', "conditional source closure"),
        ('include!("extra.rs");\n', "unreviewed source inclusion macro"),
    ]
    for source, reason in cases:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            (root / "src").mkdir()
            (root / "src/main.rs").write_text(source, encoding="utf-8")
            try:
                module.discover_module_closure(root)
            except ValueError as error:
                assert reason in str(error), f"unexpected source closure refusal: {error}"
            else:
                raise AssertionError(f"unreviewed module inclusion form was accepted: {source!r}")


def test_prebuild_private_root_refusals_before_side_effects(repo: pathlib.Path, applier: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        outside = pathlib.Path(temporary) / "outside"
        (root / "scripts").mkdir(parents=True)
        outside.mkdir()
        build_script = repo / "scripts/build-cpu-diagnostic.sh"
        (root / "scripts/build-cpu-diagnostic.sh").write_bytes(build_script.read_bytes())
        fake_bin = root / "fake-bin"
        fake_bin.mkdir()
        marker = root / "git-invoked"
        fake_git = fake_bin / "git"
        fake_git.write_text(f"#!/bin/sh\nprintf called > '{marker}'\nexit 93\n", encoding="utf-8")
        fake_git.chmod(0o755)
        (root / "build").symlink_to(outside, target_is_directory=True)
        result = subprocess.run(
            ["sh", str(root / "scripts/build-cpu-diagnostic.sh"), "--prepare-source"],
            check=False, text=True, capture_output=True,
            env={"PATH": f"{fake_bin}:/usr/bin:/bin", "HOME": str(root)},
        )
        assert result.returncode != 0 and "private directory ancestor is linked" in result.stderr, (
            f"symlinked build ancestor was not refused first: {result.stderr!r}"
        )
        assert not marker.exists(), "prepare-source invoked Git before private-root confinement"

    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        private = root / "build/private-cpu-diagnostic"
        (root / "scripts").mkdir(parents=True)
        private.mkdir(parents=True)
        private.chmod(0o755)
        (root / "scripts/build-cpu-diagnostic.sh").write_bytes(
            (repo / "scripts/build-cpu-diagnostic.sh").read_bytes()
        )
        fake_bin = root / "fake-bin"
        fake_bin.mkdir()
        marker = root / "git-invoked"
        fake_git = fake_bin / "git"
        fake_git.write_text(f"#!/bin/sh\nprintf called > '{marker}'\nexit 93\n", encoding="utf-8")
        fake_git.chmod(0o755)
        result = subprocess.run(
            ["sh", str(root / "scripts/build-cpu-diagnostic.sh"), "--prepare-source"],
            check=False, text=True, capture_output=True,
            env={"PATH": f"{fake_bin}:/usr/bin:/bin", "HOME": str(root)},
        )
        assert result.returncode != 0 and "accessible to group or other users" in result.stderr, (
            f"exposed private root was not refused first: {result.stderr!r}"
        )
        assert not marker.exists(), "prepare-source invoked Git before checking private-root mode"


def test_vendor_and_source_closure_root_refusals(repo: pathlib.Path, applier: pathlib.Path) -> None:
    for exposure in ("symlink", "mode"):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary) / "checkout"
            scripts = root / "scripts"
            scripts.mkdir(parents=True)
            for filename in ("verify-cpu-diagnostic-source-closure.py", "prepare-cpu-diagnostic-vendor.py"):
                (scripts / filename).write_bytes((repo / "scripts" / filename).read_bytes())
            if exposure == "symlink":
                outside = pathlib.Path(temporary) / "outside"
                outside.mkdir()
                (root / "build").symlink_to(outside, target_is_directory=True)
            else:
                private = root / "build/private-cpu-diagnostic"
                private.mkdir(parents=True)
                private.chmod(0o755)
            closure = subprocess.run(
                [sys.executable, "-B", str(scripts / "verify-cpu-diagnostic-source-closure.py"),
                 "--source-root", "build/private-cpu-diagnostic/overlay",
                 "--pinned-source", "build/private-cpu-diagnostic/nemu64-test",
                 "--overlay", "tests/cpu-diagnostic/overlay.patch",
                 "--source-projection", "build/private-cpu-diagnostic/source-projection.json",
                 "--source-audit", "build/private-cpu-diagnostic/source-audit-full.json",
                 "--output", "build/private-cpu-diagnostic/closure.json"],
                cwd=root, check=False, text=True, capture_output=True,
            )
            reason = "traverses a symlink" if exposure == "symlink" else "accessible to group or other users"
            assert closure.returncode == 4 and reason in closure.stderr, (
                f"source-closure {exposure} root refusal was not reached: {closure.stderr!r}"
            )
            assert not (root / "build/private-cpu-diagnostic/closure.json").exists(), (
                "source-closure refusal wrote a private report"
            )
            vendor = subprocess.run(
                [sys.executable, "-B", str(scripts / "prepare-cpu-diagnostic-vendor.py"), "--prepare", str(root)],
                cwd=root, check=False, text=True, capture_output=True,
            )
            assert vendor.returncode == 4 and reason in vendor.stderr, (
                f"vendor-preparer {exposure} root refusal was not reached: {vendor.stderr!r}"
            )
            assert not (root / "build/private-cpu-diagnostic/cargo-vendor").exists(), (
                "vendor-preparer refusal created a private tree"
            )


def test_source_closure_success_stdout_is_generic(repo: pathlib.Path, applier: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "checkout"
        scripts = root / "scripts"
        private = root / "build/private-cpu-diagnostic"
        scripts.mkdir(parents=True)
        private.mkdir(parents=True)
        root = root.resolve()
        scripts = root / "scripts"
        private = root / "build/private-cpu-diagnostic"
        private.chmod(0o700)
        script = scripts / "verify-cpu-diagnostic-source-closure.py"
        script.write_bytes((repo / "scripts/verify-cpu-diagnostic-source-closure.py").read_bytes())
        module = load_source_closure(root)
        source_root = private / "overlay"
        pinned = private / "nemu64-test"
        source_root.mkdir()
        pinned.mkdir()
        projection = private / "projection.json"
        audit = private / "audit.json"
        projection.write_text("{}\n")
        audit.write_text("{}\n")
        overlay = root / "tests/cpu-diagnostic/overlay.patch"
        overlay.parent.mkdir(parents=True)
        overlay.write_text("authored-only\n")
        module.audit_source_closure = lambda *args: {
            "status": "PRIVATE_SENTINEL_STATUS", "overlay_sha256": "PRIVATE_SENTINEL_HASH",
            "profile": {"static_module_count": 123456, "required_registration_count": 789,
                        "required_value_count": 654321},
        }
        module.write_private_report = lambda *args: None
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = module.main([
                "--source-root", str(source_root), "--pinned-source", str(pinned),
                "--overlay", str(overlay), "--source-projection", str(projection),
                "--source-audit", str(audit), "--output", str(private / "closure.json"),
            ])
        assert code == 0 and output.getvalue() == "PASS: pinned source closure verified; private report written\n", (
            f"source-closure success stdout was not generic: stdout={output.getvalue()!r}, stderr from guard is private"
        )


def test_unit_graph_private_path_traversal_refused(repo: pathlib.Path, applier: pathlib.Path) -> None:
    verifier = load_unit_graph_verifier(repo)
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary) / "private"
        root.mkdir()
        outside = pathlib.Path(temporary) / "outside.json"
        outside.write_text("{}\n")
        labels = [
            "graph", "project manifest", "project lock", "sysroot lock", "target spec",
            "Cargo config", "Cargo home config", "vendor report", "project review",
            "rust-src audit", "qualification", "toolchain install", "source projection",
            "source audit", "source closure", "unit graph review output",
        ]
        for label in labels:
            traversal = root / ".." / "outside.json"
            try:
                if label == "unit graph review output":
                    verifier.private_output(traversal, root, label)
                else:
                    verifier.private_file(traversal, root, label)
            except verifier.Refusal as error:
                assert "path traversal" in str(error), f"wrong path refusal for {label}: {error}"
            else:
                raise AssertionError(f"unit-graph verifier accepted traversal for {label}")


def test_unit_graph_private_root_refusals_before_report(repo: pathlib.Path, applier: pathlib.Path) -> None:
    input_flags = [
        "graph", "project-manifest", "project-lock", "sysroot-lock", "target-spec",
        "cargo-config", "cargo-home-config", "vendor-report", "project-review",
        "rust-src-audit", "qualification", "toolchain-install", "source-projection",
        "source-audit", "source-closure",
    ]
    for exposure in ("traversal", "outside", "symlink", "mode"):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary) / "checkout"
            scripts = root / "scripts"
            scripts.mkdir(parents=True)
            verifier = scripts / "verify-cpu-diagnostic-unit-graph.py"
            verifier.write_bytes((repo / "scripts/verify-cpu-diagnostic-unit-graph.py").read_bytes())
            private = root / "build/private-cpu-diagnostic"
            outside = pathlib.Path(temporary) / "outside"
            outside.mkdir()
            if exposure == "symlink":
                (root / "build").symlink_to(outside, target_is_directory=True)
                private_arg = "build/private-cpu-diagnostic"
            elif exposure == "mode":
                private.mkdir(parents=True)
                private.chmod(0o755)
                private_arg = "build/private-cpu-diagnostic"
            elif exposure == "traversal":
                private_arg = "build/../outside"
            else:
                private_arg = str(outside)
            argv = [sys.executable, "-B", str(verifier)]
            for flag in input_flags:
                argv.extend([f"--{flag}", f"build/private-cpu-diagnostic/{flag}.json"])
            argv.extend(["--output", "build/private-cpu-diagnostic/result.json", "--private-root", private_arg])
            result = subprocess.run(argv, cwd=root, check=False, text=True, capture_output=True)
            expected = {
                "traversal": "contains a path traversal",
                "outside": "outside the checkout",
                "symlink": "private diagnostic root is missing, linked, or exposed",
                "mode": "private diagnostic root is missing, linked, or exposed",
            }[exposure]
            assert result.returncode == 4 and expected in result.stderr, (
                f"unit-graph {exposure} root refusal was not reached: {result.stderr!r}"
            )
            assert not (private / "result.json").exists(), "unit-graph root refusal wrote a report"


def test_later_publish_failure_rolls_back(repo: pathlib.Path, applier: pathlib.Path) -> None:
    module = load_applier(applier)
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


def test_rollback_failure_preserves_recovery_copy(repo: pathlib.Path, applier: pathlib.Path) -> None:
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
        first_path.chmod(0o640)
        second_path.chmod(0o600)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_document(patch, [
            {"path": "src/first.rs", "file_sha256": sha256(first), "offset": 6,
             "length": 3, "range_sha256": sha256(b"old"), "replacement_utf8": "new"},
            {"path": "src/second.rs", "file_sha256": sha256(second), "offset": 7,
             "length": 3, "range_sha256": sha256(b"old"), "replacement_utf8": "new"},
        ])
        calls = 0

        def fail_publish_and_rollback(source_path: pathlib.Path, target_path: pathlib.Path) -> None:
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("injected later-file publish failure")
            if calls == 3:
                raise OSError("injected rollback failure")
            os.replace(source_path, target_path)

        try:
            module.apply_overlay(root, patch, replace_operation=fail_publish_and_rollback)
        except ValueError as error:
            assert "rollback was incomplete" in str(error), (
                f"double failure was not reported as an incomplete rollback: {error}"
            )
            failure_message = str(error)
        else:
            raise AssertionError("injected publication and rollback failures were accepted")
        assert calls == 3, f"expected two publish failures and one rollback attempt, observed {calls} operations"
        recovery_copies = list((root / "src").glob(".cpu-overlay-recovery.*"))
        assert len(recovery_copies) == 1, (
            "rollback recovery copy was not retained after the restore failed; "
            f"observed {len(recovery_copies)} recovery copies"
        )
        recovery_copy = recovery_copies[0]
        assert recovery_copy.read_bytes() == first, "retained recovery copy does not contain the original bytes"
        assert recovery_copy.stat().st_mode & 0o777 == 0o640, "retained recovery copy lost original mode"
        assert recovery_copy.name in failure_message, "incomplete-rollback error omitted the recovery filename"
        assert first_path.read_bytes() == b"first new\n" and second_path.read_bytes() == second, (
            "double-failure control did not reach the expected patched/original target state"
        )
        assert first_path.stat().st_mode & 0o777 == 0o640, "failed rollback changed the target mode"
        leftovers = list((root / "src").glob(".cpu-overlay.*"))
        assert not leftovers, f"failed transaction left disposable staging files: {leftovers}"


def test_recovery_mode_failure_preserves_original_bytes(repo: pathlib.Path, applier: pathlib.Path) -> None:
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
        first_path.chmod(0o640)
        second_path.chmod(0o600)
        patch = pathlib.Path(temporary) / "overlay.json"
        write_document(patch, [
            {"path": "src/first.rs", "file_sha256": sha256(first), "offset": 6,
             "length": 3, "range_sha256": sha256(b"old"), "replacement_utf8": "new"},
            {"path": "src/second.rs", "file_sha256": sha256(second), "offset": 7,
             "length": 3, "range_sha256": sha256(b"old"), "replacement_utf8": "new"},
        ])
        original_chmod = pathlib.Path.chmod
        bytes_seen_at_mode_failure: list[bytes] = []
        publish_calls = 0

        def fail_recovery_mode(path: pathlib.Path, mode: int, *args, **kwargs) -> None:
            if path.name.startswith(".cpu-overlay-recovery."):
                bytes_seen_at_mode_failure.append(path.read_bytes())
                raise OSError("injected recovery-copy mode failure")
            original_chmod(path, mode, *args, **kwargs)

        def fail_later_publish(source_path: pathlib.Path, target_path: pathlib.Path) -> None:
            nonlocal publish_calls
            publish_calls += 1
            if publish_calls == 2:
                raise OSError("injected later-file publish failure")
            os.replace(source_path, target_path)

        pathlib.Path.chmod = fail_recovery_mode
        try:
            try:
                module.apply_overlay(root, patch, replace_operation=fail_later_publish)
            except ValueError as error:
                assert "rollback was incomplete" in str(error), (
                    f"mode failure was not reported as an incomplete rollback: {error}"
                )
                failure_message = str(error)
            else:
                raise AssertionError("injected publication and mode failures were accepted")
        finally:
            pathlib.Path.chmod = original_chmod

        assert publish_calls == 2, f"expected one successful and one failed publication, observed {publish_calls}"
        assert bytes_seen_at_mode_failure == [first], (
            "mode failure was not injected after a complete original-byte recovery copy was written"
        )
        recovery_copies = list((root / "src").glob(".cpu-overlay-recovery.*"))
        assert len(recovery_copies) == 1, (
            "complete recovery bytes were discarded after recovery-copy mode failure; "
            f"observed {len(recovery_copies)} recovery copies"
        )
        recovery_copy = recovery_copies[0]
        assert recovery_copy.read_bytes() == first, "retained recovery copy does not contain the original bytes"
        assert recovery_copy.name in failure_message, "incomplete-rollback error omitted the recovery filename"
        assert "recovery-copy mode could not be restored" in failure_message, (
            "incomplete-rollback error did not report the recovery metadata failure"
        )
        assert first_path.read_bytes() == b"first new\n" and second_path.read_bytes() == second, (
            "mode-failure control did not reach the expected patched/original target state"
        )
        assert first_path.stat().st_mode & 0o777 == 0o640, "mode failure changed the target mode"
        assert recovery_copy.stat().st_mode & 0o777 == 0o600, (
            "injected mode failure unexpectedly restored the recovery-copy mode"
        )


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
        ("registration_expression_forms_preserved", test_registration_expression_forms_preserved),
        ("registration_expression_unknown_forms_refused", test_registration_expression_unknown_forms_refused),
        ("public_manifest_replacement_hashes_match", test_public_manifest_replacement_hashes_match),
        ("exception_storm_park_directive_is_exact_and_fail_closed", test_exception_storm_park_directive_is_exact_and_fail_closed),
        ("cpu_profile_bss_guard_addresses", test_cpu_profile_bss_guard_addresses),
        ("frozen_unit_graph_acceptance_and_refusals", test_frozen_unit_graph_acceptance_and_refusals),
        ("prebuild_private_root_refusals_before_side_effects", test_prebuild_private_root_refusals_before_side_effects),
        ("vendor_and_source_closure_root_refusals", test_vendor_and_source_closure_root_refusals),
        ("source_closure_success_stdout_is_generic", test_source_closure_success_stdout_is_generic),
        ("unit_graph_private_path_traversal_refused", test_unit_graph_private_path_traversal_refused),
        ("unit_graph_private_root_refusals_before_report", test_unit_graph_private_root_refusals_before_report),
        ("static_module_closure_follows_only_explicit_items", test_static_module_closure_follows_only_explicit_items),
        ("static_module_closure_refuses_unknown_inclusion_forms", test_static_module_closure_refuses_unknown_inclusion_forms),
        ("later_publish_failure_rolls_back", test_later_publish_failure_rolls_back),
        ("rollback_failure_preserves_recovery_copy", test_rollback_failure_preserves_recovery_copy),
        ("recovery_mode_failure_preserves_original_bytes", test_recovery_mode_failure_preserves_original_bytes),
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

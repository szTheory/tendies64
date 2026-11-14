#!/usr/bin/env python3
"""Exercise the RSP no-HLE source, build, mutation, and fetched controls."""

from __future__ import annotations

import collections
import importlib.util
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[2]
CHECKER_PATH = ROOT / "scripts" / "check-rsp-no-hle.py"
BUILD = pathlib.Path(
    os.environ.get("T64_RSP_NO_HLE_BUILD_DIR", str(ROOT / "build" / "gate"))
).resolve()
SPEC = importlib.util.spec_from_file_location("check_rsp_no_hle", CHECKER_PATH)
if SPEC is None or SPEC.loader is None:
    raise SystemExit("no-HLE checker module could not be loaded")
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


MUTATION_SEEDS = (
    "MicroCode_Signature task selector",
    "uCode_CRC case variation",
    "TASK_ID formatted selector",
    "HLE replacement callback",
    "content-digest selection",
    "hidden production source",
    "hidden softfloat source",
    "hidden archive object",
    "hidden object file",
    "missing canonical archive object",
    "zero production sources",
    "comments and literals are not dispatch code",
)


def require_nonzero_count(count: int, label: str) -> None:
    if count <= 0:
        raise AssertionError(f"{label} count is zero; control suite failed closed")


def _copy_clean_fixture(destination: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path]:
    source_root = destination / "source"
    build_root = destination / "build"
    core = source_root / "core"
    softfloat = core / "softfloat"
    core.mkdir(parents=True)
    softfloat.mkdir(parents=True)
    shutil.copy2(ROOT / "core" / "CMakeLists.txt", core / "CMakeLists.txt")
    shutil.copy2(
        ROOT / "core" / "softfloat" / "sources.cmake", softfloat / "sources.cmake"
    )
    for name in CHECKER.CANONICAL_CORE_SOURCES:
        shutil.copy2(ROOT / "core" / name, core / name)
    softfloat_names, findings = CHECKER._softfloat_sources(ROOT)
    if findings or not softfloat_names:
        raise AssertionError("repository softfloat source inventory is empty or invalid")
    for name in softfloat_names:
        shutil.copy2(ROOT / "core" / "softfloat" / name, softfloat / name)

    build_root.mkdir(parents=True)
    object_destination = build_root / "core" / "CMakeFiles" / "t64_core.dir"
    object_destination.parent.mkdir(parents=True)
    shutil.copytree(
        BUILD / "core" / "CMakeFiles" / "t64_core.dir",
        object_destination,
    )
    shutil.copy2(BUILD / "core" / "libt64_core.a", build_root / "core" / "libt64_core.a")
    ninja = (BUILD / "build.ninja").read_text(encoding="utf-8")
    ninja = ninja.replace(str(ROOT), str(source_root))
    (build_root / "build.ninja").write_text(ninja, encoding="utf-8")
    return source_root, build_root


def _assert_clean(source_root: pathlib.Path, build_root: pathlib.Path) -> None:
    findings, _, scanned = CHECKER.inspect_tree(source_root, build_root)
    require_nonzero_count(scanned, "scanned production source")
    if findings:
        raise AssertionError("clean temporary tree failed: " + " | ".join(findings))


def _must_reject(label: str, source_root: pathlib.Path,
                 build_root: pathlib.Path) -> None:
    findings, _, scanned = CHECKER.inspect_tree(source_root, build_root)
    require_nonzero_count(scanned, "scanned production source")
    if not findings:
        raise AssertionError(f"mutation accepted: {label}")


def _mutate_source(source_root: pathlib.Path, text: str) -> None:
    path = source_root / "core" / "t64_rsp.c"
    original = path.read_text(encoding="utf-8")
    anchor = "  instruction = rsp_read_imem_word(machine, pc);"
    if anchor not in original:
        raise AssertionError("source mutation anchor no longer exists")
    path.write_text(original.replace(anchor, text + "\n" + anchor, 1), encoding="utf-8")


def _restore_source(source_root: pathlib.Path) -> None:
    shutil.copy2(ROOT / "core" / "t64_rsp.c", source_root / "core" / "t64_rsp.c")


def _check_mutation_suite(temp_root: pathlib.Path) -> int:
    source_root, build_root = _copy_clean_fixture(temp_root)
    _assert_clean(source_root, build_root)
    seeds_run = 0

    for label, code in (
        ("MicroCode_Signature task selector", "  if (MicroCode_Signature(machine)) return status;"),
        ("uCode_CRC case variation", "  if (uCode_CRC(machine) == 1u) return status;"),
        ("TASK_ID formatted selector", "  if (TASK_ID(machine) != 0u) return status;"),
        (
            "content-digest selection",
            "  if (machine->content_digest[0] != 0u) return status;",
        ),
    ):
        _mutate_source(source_root, code)
        _must_reject(label, source_root, build_root)
        _restore_source(source_root)
        seeds_run += 1

    rsp_path = source_root / "core" / "t64_rsp.c"
    rsp_code = rsp_path.read_text(encoding="utf-8")
    callback = "token, t64_rsp_issue_event, &reservation"
    if callback not in rsp_code:
        raise AssertionError("scheduled RSP callback mutation anchor no longer exists")
    rsp_path.write_text(
        rsp_code.replace(callback, "token, t64_rsp_hLeReplacement, &reservation", 1),
        encoding="utf-8",
    )
    _must_reject("HLE replacement callback", source_root, build_root)
    _restore_source(source_root)
    seeds_run += 1

    # The checker strips prose and literals before inspecting code identifiers.
    _mutate_source(
        source_root,
        '  (void)"microcode signature task id HLE are documentation text"; /* uCode_CRC */',
    )
    findings, _, _ = CHECKER.inspect_tree(source_root, build_root)
    if findings:
        raise AssertionError("comment/string-only words were treated as dispatch code")
    _restore_source(source_root)
    seeds_run += 1

    hidden_source = source_root / "core" / "t64_rsp_hle.c"
    hidden_source.write_text("int hidden_hle_source;\n", encoding="utf-8")
    _must_reject("hidden production source", source_root, build_root)
    hidden_source.unlink()
    seeds_run += 1

    softfloat_sources = source_root / "core" / "softfloat" / "sources.cmake"
    original_softfloat_sources = softfloat_sources.read_text(encoding="utf-8")
    header_anchor = ")\nset(T64_SOFTFLOAT_HEADERS"
    if header_anchor not in original_softfloat_sources:
        raise AssertionError("softfloat source mutation anchor no longer exists")
    softfloat_sources.write_text(
        original_softfloat_sources.replace(
            header_anchor,
            '  "${CMAKE_CURRENT_LIST_DIR}/t64_rsp_hle.c"\n' + header_anchor,
            1,
        ),
        encoding="utf-8",
    )
    (source_root / "core" / "softfloat" / "t64_rsp_hle.c").write_text(
        "int hidden_softfloat_hle_source;\n", encoding="utf-8"
    )
    _must_reject("hidden softfloat source", source_root, build_root)
    softfloat_sources.write_text(original_softfloat_sources, encoding="utf-8")
    (source_root / "core" / "softfloat" / "t64_rsp_hle.c").unlink()
    seeds_run += 1

    ninja_path = build_root / "build.ninja"
    original_ninja = ninja_path.read_text(encoding="utf-8")
    archive_line = next(
        line for line in original_ninja.splitlines()
        if line.startswith("build core/libt64_core.a:")
    )
    hidden_object = "core/CMakeFiles/t64_core.dir/t64_rsp_hle.c.o"
    ninja_path.write_text(
        original_ninja.replace(
            archive_line, archive_line + " " + hidden_object, 1
        ),
        encoding="utf-8",
    )
    hidden_object_path = build_root / hidden_object
    hidden_object_path.parent.mkdir(parents=True, exist_ok=True)
    hidden_object_path.write_bytes(b"injected object edge\n")
    _must_reject("hidden archive object", source_root, build_root)
    ninja_path.write_text(original_ninja, encoding="utf-8")
    hidden_object_path.unlink()
    seeds_run += 1

    orphan_object = (
        build_root / "core" / "CMakeFiles" / "t64_core.dir" / "t64_rsp_hle.c.o"
    )
    orphan_object.write_bytes(b"unlisted object\n")
    _must_reject("hidden object file", source_root, build_root)
    orphan_object.unlink()
    seeds_run += 1

    rsp_target = "core/CMakeFiles/t64_core.dir/t64_rsp.c.o"
    ninja_path.write_text(
        archive_line.replace(" " + rsp_target, "", 1)
        + "\n"
        + "\n".join(
            line for line in original_ninja.splitlines()
            if not line.startswith("build core/libt64_core.a:")
        ),
        encoding="utf-8",
    )
    _must_reject("missing canonical archive object", source_root, build_root)
    ninja_path.write_text(original_ninja, encoding="utf-8")
    seeds_run += 1

    for path in (source_root / "core").glob("t64_*.c"):
        path.unlink()
    _must_reject("zero production sources", source_root, build_root)
    seeds_run += 1

    require_nonzero_count(seeds_run, "mutation seed")
    if seeds_run != len(MUTATION_SEEDS):
        raise AssertionError(
            f"mutation seed count mismatch: ran={seeds_run} declared={len(MUTATION_SEEDS)}"
        )
    try:
        require_nonzero_count(0, "mutation seed")
    except AssertionError:
        pass
    else:
        raise AssertionError("zero-mutation-seed control was accepted")
    return seeds_run


def _check_guest_controls() -> None:
    binary = BUILD / "core" / "test_rsp_fuzz"
    if not binary.is_file():
        raise AssertionError("built test_rsp_fuzz binary is missing")
    result = subprocess.run(
        [str(binary), "--no-hle-controls"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        timeout=20,
    )
    if result.returncode != 0:
        raise AssertionError(
            "fetched no-HLE guest controls failed: "
            + result.stdout[-1000:]
            + result.stderr[-1000:]
        )
    required = (
        "same fetched ADDIU opcode and initial state produced content-dependent results",
        "unsupported fetched opcode refused with complete RSP/SP state preserved",
    )
    for phrase in required:
        if phrase not in result.stdout:
            raise AssertionError(f"fetched no-HLE control output omitted {phrase!r}")
    log_path = BUILD / "05-08-task2-no-hle-guest-controls.log"
    log_path.write_text(result.stdout, encoding="utf-8")


def main() -> int:
    findings, evidence, scanned = CHECKER.inspect_tree(ROOT, BUILD)
    require_nonzero_count(scanned, "scanned production source")
    if findings:
        for finding in findings:
            print(f"FAIL: {finding}", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="t64-rsp-no-hle-") as temporary:
        seeds_run = _check_mutation_suite(pathlib.Path(temporary))
    _check_guest_controls()
    print(f"PASS: repo.rsp-no-hle scanned={scanned} mutation_seeds={seeds_run}")
    for item in evidence:
        print(f"route: {item}")
    print("controls: same-program/different-content and unsupported fetched-opcode refusal passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, AssertionError) as error:
        print(f"FAIL: repo.rsp-no-hle: {error}", file=sys.stderr)
        raise SystemExit(1)

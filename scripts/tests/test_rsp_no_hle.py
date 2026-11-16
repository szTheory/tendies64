#!/usr/bin/env python3
"""Exercise the RSP no-HLE source, build, mutation, and fetched controls."""

from __future__ import annotations

import collections
import importlib.util
import os
import pathlib
import re
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
    "host-start-dma-effect",
    "host-sp-write-effect",
    "sp-completion-effect",
    "sp-dma-commit-effect",
    "unreviewed-host-helper",
    "MicroCode_Signature task selector",
    "uCode_CRC case variation",
    "TASK_ID formatted selector",
    "HLE replacement callback",
    "content-digest selection",
    "unnamed DMEM signature early return",
    "hidden production source",
    "hidden RDP/VI source roster entry",
    "t64_core omits RDP/VI source roster",
    "hidden softfloat source",
    "hidden archive object",
    "hidden object file",
    "missing canonical archive object",
    "zero production sources",
    "comments and literals are not dispatch code",
    "anonymous DMEM-dependent decode-helper effect",
    "unreviewed static helper definition",
    "removed static helper definition",
    "anonymous DMEM-dependent SP admission-helper effect",
    "unreviewed SP DMA helper definition",
    "removed SP DMA helper definition",
    "RDP fill production body mutation",
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


def _copy_source_only_fixture(destination: pathlib.Path) -> pathlib.Path:
    """Build a minimal source fixture for helper-route controls, without native artifacts."""

    source_root = destination / "source"
    core = source_root / "core"
    softfloat = core / "softfloat"
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
    return source_root


def _assert_clean(source_root: pathlib.Path, build_root: pathlib.Path) -> None:
    findings, _, scanned = CHECKER.inspect_tree(source_root, build_root)
    require_nonzero_count(scanned, "scanned production source")
    if findings:
        raise AssertionError("clean temporary tree failed: " + " | ".join(findings))


def _must_reject(label: str, source_root: pathlib.Path,
                 build_root: pathlib.Path | None = None) -> None:
    findings, _, scanned = CHECKER.inspect_tree(source_root, build_root)
    require_nonzero_count(scanned, "scanned production source")
    if not findings:
        raise AssertionError(f"mutation accepted: {label}")


def _function_definition_span(source: str, name: str) -> tuple[int, int] | None:
    cleaned = CHECKER.strip_c_comments_and_literals(source)
    pattern = r"\b" + re.escape(name) + r"\s*\("
    for match in re.finditer(pattern, cleaned):
        open_paren = cleaned.find("(", match.start())
        paren_depth = 0
        close_paren = None
        for index in range(open_paren, len(cleaned)):
            if cleaned[index] == "(":
                paren_depth += 1
            elif cleaned[index] == ")":
                paren_depth -= 1
                if paren_depth == 0:
                    close_paren = index
                    break
        if close_paren is None:
            continue
        cursor = close_paren + 1
        while cursor < len(cleaned) and cleaned[cursor].isspace():
            cursor += 1
        if cursor >= len(cleaned) or cleaned[cursor] != "{":
            continue
        brace_depth = 0
        for index in range(cursor, len(cleaned)):
            if cleaned[index] == "{":
                brace_depth += 1
            elif cleaned[index] == "}":
                brace_depth -= 1
                if brace_depth == 0:
                    line_start = cleaned.rfind("\n", 0, match.start()) + 1
                    return line_start, index + 1
    return None


def _check_helper_inventory_mutations(temp_root: pathlib.Path) -> int:
    source_root = _copy_source_only_fixture(temp_root / "anonymous-effect")
    findings, _, scanned = CHECKER.inspect_tree(source_root)
    require_nonzero_count(scanned, "scanned production source")
    if findings:
        raise AssertionError("clean source-only fixture failed: " + " | ".join(findings))
    path = source_root / "core" / "t64_rsp.c"
    original = path.read_text(encoding="utf-8")
    anchor = "  return true;\n}\n\nstatic void rsp_commit_vector_effect"
    if anchor not in original:
        raise AssertionError("vector decode helper mutation anchor no longer exists")
    mutation = (
        "  if (machine->sp_dmem[0] == UINT8_C(0x42)) {\n"
        "    effect->values[0] ^= UINT16_C(1);\n"
        "  }\n"
    )
    path.write_text(original.replace(anchor, mutation + anchor, 1), encoding="utf-8")
    _must_reject("anonymous DMEM-dependent decode-helper effect", source_root)

    source_root = _copy_source_only_fixture(temp_root / "added-helper")
    path = source_root / "core" / "t64_rsp.c"
    with path.open("a", encoding="utf-8") as source:
        source.write("\nstatic uint32_t rsp_unreviewed_helper(void) { return 0u; }\n")
    _must_reject("unreviewed static helper definition", source_root)

    source_root = _copy_source_only_fixture(temp_root / "removed-helper")
    path = source_root / "core" / "t64_rsp.c"
    original = path.read_text(encoding="utf-8")
    span = _function_definition_span(original, "rsp_decode_vector_basic")
    if span is None:
        raise AssertionError("vector decode helper definition no longer exists")
    path.write_text(original[: span[0]] + original[span[1] :], encoding="utf-8")
    _must_reject("removed static helper definition", source_root)

    source_root = _copy_source_only_fixture(temp_root / "sp-admission-effect")
    path = source_root / "core" / "t64_sp.c"
    original = path.read_text(encoding="utf-8")
    anchor = "  projected_active = &machine->sp.active;"
    if anchor not in original:
        raise AssertionError("SP DMA admission helper mutation anchor no longer exists")
    mutation = (
        "  if (machine->sp_dmem[0] == UINT8_C(0x42)) {\n"
        "    admission_out->status = T64_STATUS_BUS_DEVICE_REFUSED;\n"
        "    admission_out->refusal_reason = T64_RSP_REFUSAL_DMA_SPAN;\n"
        "    return admission_out->status;\n"
        "  }\n"
    )
    path.write_text(original.replace(anchor, mutation + anchor, 1), encoding="utf-8")
    _must_reject("anonymous DMEM-dependent SP admission-helper effect", source_root)

    for name in ("start_dma", "t64_sp_write_register",
                 "t64_sp_complete", "t64_sp_rsp_dma_commit"):
        source_root = _copy_source_only_fixture(temp_root / ("host-entry-" + name))
        path = source_root / "core" / "t64_sp.c"
        original = path.read_text(encoding="utf-8")
        span = _function_definition_span(original, name)
        if span is None:
            raise AssertionError("host SP entry mutation anchor no longer exists")
        at = original.index("{", span[0]) + 1
        mutation = (
            "\n  if (machine != NULL && machine->sp_dmem[0] == UINT8_C(0x42)) {\n"
            "    machine->sp_dmem[1] = UINT8_C(0x99);\n  }\n"
        )
        path.write_text(original[:at] + mutation + original[at:], encoding="utf-8")
        _must_reject("anonymous DMEM-dependent host SP entry effect", source_root)

    source_root = _copy_source_only_fixture(temp_root / "unreviewed-host-helper")
    path = source_root / "core" / "t64_sp.c"
    with path.open("a", encoding="utf-8") as source:
        source.write("\nuint32_t t64_sp_unreviewed_host_helper(void) { return 0u; }\n")
    _must_reject("unreviewed non-DMA-prefixed SP helper", source_root)

    source_root = _copy_source_only_fixture(temp_root / "added-sp-helper")
    path = source_root / "core" / "t64_sp.c"
    with path.open("a", encoding="utf-8") as source:
        source.write("\nbool t64_sp_rsp_dma_unreviewed(void) { return false; }\n")
    _must_reject("unreviewed SP DMA helper definition", source_root)

    source_root = _copy_source_only_fixture(temp_root / "removed-sp-helper")
    path = source_root / "core" / "t64_sp.c"
    original = path.read_text(encoding="utf-8")
    span = _function_definition_span(original, "t64_sp_rsp_dma_admission_refuse")
    if span is None:
        raise AssertionError("SP DMA admission helper definition no longer exists")
    path.write_text(original[: span[0]] + original[span[1] :], encoding="utf-8")
    _must_reject("removed SP DMA helper definition", source_root)
    return 11


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
        ("unnamed DMEM signature early return",
         "  if (machine->sp_dmem[0] == 0x42u) { machine->rsp.gpr[1] = 7u; return status; }"),
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
    rsp_path.write_text(
        '/* uCode_CRC microcode signature task id HLE */\n'
        'static const char * const prose_only = "microcode signature task id HLE";\n'
        + rsp_code,
        encoding="utf-8",
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

    hidden_rdp_source = source_root / "core" / "t64_rdp_unreviewed.c"
    hidden_rdp_source.write_text(
        "int hidden_rdp_source;\n", encoding="utf-8"
    )
    _must_reject("hidden RDP/VI source roster entry", source_root, build_root)
    hidden_rdp_source.unlink()
    seeds_run += 1

    roster_source = _copy_source_only_fixture(temp_root / "missing-rdp-roster")
    cmake_path = roster_source / "core" / "CMakeLists.txt"
    original_cmake = cmake_path.read_text(encoding="utf-8")
    roster_anchor = "add_library(t64_core STATIC\n  ${T64_CORE_SOURCES}\n"
    if roster_anchor not in original_cmake:
        raise AssertionError("t64_core RDP/VI source-roster anchor no longer exists")
    cmake_path.write_text(
        original_cmake.replace(
            roster_anchor, "add_library(t64_core STATIC\n", 1
        ),
        encoding="utf-8",
    )
    _must_reject("t64_core omits RDP/VI source roster", roster_source)
    seeds_run += 1

    rdp_path = source_root / "core" / "t64_rdp.c"
    original_rdp = rdp_path.read_text(encoding="utf-8")
    rdp_anchor = "rdp->fill_color = (uint32_t)word;"
    if rdp_anchor not in original_rdp:
        raise AssertionError("RDP fill-color body mutation anchor no longer exists")
    rdp_path.write_text(
        original_rdp.replace(
            rdp_anchor, "rdp->fill_color = (uint32_t)word ^ UINT32_C(1);", 1
        ),
        encoding="utf-8",
    )
    _must_reject("RDP fill production body mutation", source_root, build_root)
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
    seeds_run += _check_helper_inventory_mutations(temp_root / "helper-inventory")
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
        "same fetched LW program and initial RSP state produced data-dependent results",
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

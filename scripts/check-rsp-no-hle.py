#!/usr/bin/env python3
"""Check the canonical RSP fetched-instruction route and production objects."""

from __future__ import annotations

import argparse
import collections
import hashlib
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys


CANONICAL_CORE_SOURCES = frozenset(
    {
        "t64_bus.c",
        "t64_cache.c",
        "t64_cop1.c",
        "t64_core_stub.c",
        "t64_cp0.c",
        "t64_cpu.c",
        "t64_digest.c",
        "t64_exception.c",
        "t64_isviewer.c",
        "t64_machine.c",
        "t64_memory.c",
        "t64_mi.c",
        "t64_mmu.c",
        "t64_observe.c",
        "t64_pi.c",
        "t64_rom.c",
        "t64_rsp.c",
        "t64_rsp_divider.c",
        "t64_scheduler.c",
        "t64_softfloat.c",
        "t64_sp.c",
        "t64_trace.c",
        "t64_warm_devices.c",
    }
)
CANONICAL_SOFTFLOAT_SOURCES = frozenset(
    {
        "f32_add.c", "f32_div.c", "f32_eq.c", "f32_le.c", "f32_lt.c",
        "f32_mul.c", "f32_sqrt.c", "f32_sub.c", "f32_to_f64.c",
        "f32_to_i32.c", "f32_to_i64.c", "f64_add.c", "f64_div.c",
        "f64_eq.c", "f64_le.c", "f64_lt.c", "f64_mul.c", "f64_sqrt.c",
        "f64_sub.c", "f64_to_f32.c", "f64_to_i32.c", "f64_to_i64.c",
        "i32_to_f32.c", "i32_to_f64.c", "i64_to_f32.c", "i64_to_f64.c",
        "s_addMagsF32.c", "s_addMagsF64.c", "s_approxRecip32_1.c",
        "s_approxRecipSqrt32_1.c", "s_approxRecipSqrt_1Ks.c",
        "s_approxRecip_1Ks.c", "s_countLeadingZeros8.c",
        "s_countLeadingZeros32.c", "s_countLeadingZeros64.c",
        "s_mul64To128.c", "s_normRoundPackToF32.c", "s_normRoundPackToF64.c",
        "s_normSubnormalF32Sig.c", "s_normSubnormalF64Sig.c",
        "s_roundPackToF32.c", "s_roundPackToF64.c", "s_roundToI32.c",
        "s_roundToI64.c", "s_shiftRightJam32.c", "s_shiftRightJam64.c",
        "s_shiftRightJam64Extra.c", "s_shortShiftRightJam64.c",
        "s_subMagsF32.c", "s_subMagsF64.c",
    }
)


# Every file-scope static helper in t64_rsp.c is part of the execution inventory,
# including pure arithmetic and byte helpers whose outputs feed decode effects.
# This is a bounded normalized-source guard, not proof of arbitrary C behavior.
REVIEWED_RSP_STATIC_FUNCTIONS = frozenset(
    {
        "t64_rsp_issue_event",
        "t64_rsp_schedule_next",
        "rsp_read_imem_word",
        "rsp_read_vector_element",
        "rsp_write_vector_element",
        "rsp_signed16",
        "rsp_sign_extend16",
        "rsp_signed_less32",
        "rsp_arithmetic_shift_right32",
        "rsp_select_vector_lane",
        "rsp_vector_memory_add_address",
        "rsp_vector_memory_add_load_byte",
        "rsp_vector_memory_add_store_byte",
        "rsp_plan_vector_memory",
        "rsp_vector_memory_loaded_byte",
        "rsp_ready_tick_after",
        "rsp_decode_vadd",
        "rsp_commit_vadd",
        "rsp_decode_vector_basic",
        "rsp_commit_vector_effect",
        "rsp_decode_divider",
        "rsp_signed48",
        "rsp_clamp_signed_accumulator",
        "rsp_clamp_unsigned_accumulator",
        "rsp_decode_vector_multiply",
        "rsp_decode_vector_compare",
    }
)
REVIEWED_SP_DMA_API_FUNCTIONS = frozenset(
    {
        "t64_sp_rsp_dma_active",
        "t64_sp_rsp_dma_admission_preflight",
        "t64_sp_rsp_dma_admission_commit",
        "t64_sp_rsp_dma_admission_refuse",
        "t64_sp_rsp_dma_preflight",
        "t64_sp_rsp_dma_commit",
    }
)
REVIEWED_SP_DMA_HELPER_FUNCTIONS = frozenset(
    {
        "warm_zero_source_sp_transfer",
        "dma_busy",
        "dma_full",
        "trace_refusal",
        "refuse",
        "decode_descriptor",
        "rsp_pc_overlaps_dma_beat",
    }
)
REVIEWED_SP_STATUS_FUNCTIONS = frozenset(
    {"t64_rsp_validate_status_command", "t64_sp_rsp_guest_status_write"}
)
REVIEWED_SP_FUNCTIONS = (
    REVIEWED_SP_DMA_API_FUNCTIONS
    | REVIEWED_SP_DMA_HELPER_FUNCTIONS
    | REVIEWED_SP_STATUS_FUNCTIONS
    | frozenset(
        {
            "start_dma", "start_rsp_dma", "write_rsp_status",
            "t64_sp_complete", "t64_sp_peek_register", "t64_sp_read_register",
            "t64_sp_reset_machine_state", "t64_sp_write_register",
        }
    )
)

# Reviewed normalized function bodies. A changed body requires an explicit
# review and digest refresh; static definition additions/removals also fail the
# inventory check below.
REVIEWED_BODY_DIGESTS = {
    "core/t64_sp.c :: t64_sp_write_register": "3620ac7c6c379837ebdedcfcf21ca6c887e580880e314311633b08698f75ce09",
    "core/t64_sp.c :: t64_sp_reset_machine_state": "f410fa77785c8b063cde69306ced58702d70dae4c5bca62af2b8adf08529670a",
    "core/t64_sp.c :: t64_sp_read_register": "495be610b9df612b2637f74356824f1a4c6970981041d87dfb666a2f56946e5e",
    "core/t64_sp.c :: t64_sp_peek_register": "8e061feec47e6809739915b64f8e832cba96a8705521eb7b6e2a0cea2d7d4e5d",
    "core/t64_sp.c :: t64_sp_complete": "931b7e164ac314c2671e61bb5e77dfea2409c0106a9aa8bb54a7533471b94293",
    "core/t64_sp.c :: start_dma": "e960624eb7510895ae8c1721620b8dc3aa948c0d1c4cc6298aa9c43f1f0f7fae",
    "core/t64_sp.c :: t64_sp_rsp_guest_status_write": "aa575856b41e3be20241aee8db88efd7d2def2007cad2cb9cd0424c11417beb6",
    "core/t64_sp.c :: t64_rsp_validate_status_command": "e750e5da69933b8e68350a243266836a41d259724c203e8e4b805dee49f2dc81",
    "core/t64_rsp.c :: rsp_arithmetic_shift_right32": "2ffdbfc144cc0d544303a2c9d6544d4f8a507c7f95ce5c6fb5a2c5f6cbedc8b2",
    "core/t64_rsp.c :: rsp_clamp_signed_accumulator": "5543a4680a99621460790929bbecd6338f6b0cb98f2ace605988b67dbbdb2faf",
    "core/t64_rsp.c :: rsp_clamp_unsigned_accumulator": "d203d306b5bb83931666bbbe8565bf2be28b0b14637c65bdd9da7d616461a7f7",
    "core/t64_rsp.c :: rsp_commit_vadd": "9fda6b6f2ec351237bb4f681a9e77cd0624e3ddf1f2dfae98315951a4d087025",
    "core/t64_rsp.c :: rsp_commit_vector_effect": "493558efd0852f44c6eccc9c683702db375699ab50f3ae44e7e9a982711f50af",
    "core/t64_rsp.c :: rsp_decode_divider": "a8cb7d31faaa548edba4be5cfc197bd557105f0b71809503215e21b35928ef0c",
    "core/t64_rsp.c :: rsp_decode_vadd": "fe08db8d3d2e49994398ee7d76de1dde81c0d9bebda3fe40be75be501feff9a0",
    "core/t64_rsp.c :: rsp_decode_vector_basic": "bc3f5a778d7ec0ebe7e0d81ca9e847f7f8f3febba8df3f28bd17d3aca65eb7b1",
    "core/t64_rsp.c :: rsp_decode_vector_compare": "047454f1508577f5521c8b9a80bb9eefbad5059600ff0b948af3b3b488a91b7f",
    "core/t64_rsp.c :: rsp_decode_vector_multiply": "a89194966324203bde8b8d30dee9c1843a9e31b21ecdbd3fe0e02c26afd7b2a1",
    "core/t64_rsp.c :: rsp_plan_vector_memory": "2c45cef2220b904e3715d57e40cd35d10939d834e9b3d749ecd4f38e58b620fe",
    "core/t64_rsp.c :: rsp_read_imem_word": "db7a2c1d21484afdd935008961692093d22515afe3bb383dbe19ec8fd68479e1",
    "core/t64_rsp.c :: rsp_read_vector_element": "44f09265b81c3e7d891389b338bf3869f05ed1c905ebbe2b813a58edf36622e7",
    "core/t64_rsp.c :: rsp_ready_tick_after": "d920721d8f0e1ecf83be81d167fb79557f72f64fbb8f927890e99015661980cd",
    "core/t64_rsp.c :: rsp_select_vector_lane": "702353a47c99b149e59087cc1dce99574b12fd3b30f2452383db133b38e0d193",
    "core/t64_rsp.c :: rsp_sign_extend16": "57ef376a78778052585f55ec04ce962cbf9b260749cfec3892f3a70b5607a6d7",
    "core/t64_rsp.c :: rsp_signed16": "2ad56dbb5f2ad65b4bac1e81206888862d6523d9e4717ed349549fa53407040b",
    "core/t64_rsp.c :: rsp_signed48": "06efd07d2b91c580875b5d483b05e62337ea765c4d86a1b14aa8f71bad5669d9",
    "core/t64_rsp.c :: rsp_signed_less32": "485d921a30b6b37fcba6602bb3980e1d4888620e9333f8e5195e389e4a2f0574",
    "core/t64_rsp.c :: rsp_vector_memory_add_address": "8025f869af91a4bad6b448f6de94cbeb88e7a30502ab89b0c98d3ca0197ea93f",
    "core/t64_rsp.c :: rsp_vector_memory_add_load_byte": "d44c46bff06098b1a67a15d94602a633b7443d8192e3da4aaa1e5feaee5e6970",
    "core/t64_rsp.c :: rsp_vector_memory_add_store_byte": "e2833f80313951cbe5f61aad8a23afe2e29a8a4d21223e46cdf4b0f8d81bd7e8",
    "core/t64_rsp.c :: rsp_vector_memory_loaded_byte": "530efea13ab67b3f5e92288e0ca372195613b5402593efaa0b57fd25af753073",
    "core/t64_rsp.c :: rsp_write_vector_element": "3d6b40d7c541286102647e6241ce0b6466937797d8844cb676f2f220ec255b73",
    "core/t64_rsp.c :: t64_rsp_ensure_opportunity": "d37ee11dced86400787fe5774fb85544f3eb2a625d0e9916e6a90f9de54c5bbb",
    "core/t64_rsp.c :: t64_rsp_issue_event": "6630326692dde41a1f34da8d002037245d518de15c88a166fff71bb4be092826",
    "core/t64_rsp.c :: t64_rsp_schedule_next": "7bceb405fe698a622902fd136009ac272c585eb62288c1371db49b42fa1bc339",
    "core/t64_rsp.c :: t64_rsp_start": "844ab432a1a02296d0f0d768dab18a5c60d9ee167bc2959637214b304e6e1ef1",
    "core/t64_sp.c :: decode_descriptor": "bb8ebf0fa2a9c2161076806566f090449af9c20d7de8d74bba461d4503c53352",
    "core/t64_sp.c :: dma_busy": "ef1efda4327bde7a879ae04b7d7947457b10c13263c0f5cb40547fd02b203cfe",
    "core/t64_sp.c :: dma_full": "92dc417b13b19cfe01ce1c1bfdb61c35bb952b69f07950c34920518dddd50bba",
    "core/t64_sp.c :: refuse": "14c52d2fb1695e35a264c9face6e0ee3fce0a62d44e8fbf621a75584904cf475",
    "core/t64_sp.c :: rsp_pc_overlaps_dma_beat": "206b3bb9a2847981e36c6a146d64f34b10fe9cbb931a1b5b087ce1b49a8765ed",
    "core/t64_sp.c :: start_rsp_dma": "a789536986e683cdf3efa0762c2ee0365c52fd7b0cc1f90291ae484838b1ac1a",
    "core/t64_sp.c :: t64_sp_rsp_dma_active": "c5eb4df345f87ff3f73ac0f81b9a039dc3b6ca13e9a8bb4faf59ee26a4035bda",
    "core/t64_sp.c :: t64_sp_rsp_dma_admission_commit": "8e52fa80dd6ae74fc13d8d7a46d64b802ce68aa8c3744cd10eb79e0d7fab4eff",
    "core/t64_sp.c :: t64_sp_rsp_dma_admission_preflight": "63d6631c0e409082581aebb7778c0298ca12459b5e4f8e6fd71a8e829f6ba18e",
    "core/t64_sp.c :: t64_sp_rsp_dma_admission_refuse": "1e014801570acaa111ae5e932ea0260840aa6fb5d37f27bcf3018759550a5c3c",
    "core/t64_sp.c :: t64_sp_rsp_dma_commit": "d045e6d0dda1e7c0cd339b8bda7a0738439f230c2c680ed9adab80b7326bde4e",
    "core/t64_sp.c :: t64_sp_rsp_dma_preflight": "e5b6d9024c565659e7b610795fd64dcea905238edc5f74b09b0495c401eb108f",
    "core/t64_sp.c :: trace_refusal": "d142faecf8d4f78e427aa40e7454d0a24cc05c20ca433e225bed118982517dfc",
    "core/t64_sp.c :: warm_zero_source_sp_transfer": "46b55d7252273228780a497b4f34180b53dc93676f6bac901aaff9fae6d86508",
    "core/t64_sp.c :: write_rsp_status": "8d1f00710b955fc530864d080ae8d99733711e73749c75b6800d7cc590dbf74c",
}


def strip_c_comments_and_literals(source: str) -> str:
    """Blank comments and literals while preserving newlines for diagnostics."""

    pattern = re.compile(
        r"/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"|"
        r"'(?:\\.|[^'\\])*'",
        re.DOTALL,
    )

    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if char == "\n" else " " for char in match.group(0))

    return pattern.sub(blank, source)


def function_body(source: str, name: str) -> str | None:
    """Return one C function body, ignoring declarations and call sites."""

    for match in re.finditer(r"\b" + re.escape(name) + r"\s*\(", source):
        open_paren = source.find("(", match.start())
        depth = 0
        close_paren = None
        for index in range(open_paren, len(source)):
            if source[index] == "(":
                depth += 1
            elif source[index] == ")":
                depth -= 1
                if depth == 0:
                    close_paren = index
                    break
        if close_paren is None:
            continue
        cursor = close_paren + 1
        while cursor < len(source) and source[cursor].isspace():
            cursor += 1
        if cursor >= len(source) or source[cursor] != "{":
            continue
        depth = 0
        for index in range(cursor, len(source)):
            if source[index] == "{":
                depth += 1
            elif source[index] == "}":
                depth -= 1
                if depth == 0:
                    return source[cursor + 1 : index]
    return None


def static_function_names(source: str) -> set[str]:
    """Return file-scope static function definitions, not just declarations."""

    pattern = re.compile(
        r"(?m)^static\b[^;{}]*?\b([A-Za-z_]\w*)\s*\("
    )
    names = set()
    for match in pattern.finditer(source):
        name = match.group(1)
        if function_body(source, name) is not None:
            names.add(name)
    return names


def file_function_names(source: str) -> set[str]:
    """Return file-scope function definitions regardless of storage class."""

    pattern = re.compile(
        r"(?m)^(?:static\s+)?[^;{}]*?\b([A-Za-z_]\w*)\s*\("
    )
    names = set()
    cursor = 0
    depth = 0
    for match in pattern.finditer(source):
        prefix = source[cursor:match.start()]
        depth += prefix.count("{") - prefix.count("}")
        cursor = match.start()
        name = match.group(1)
        if depth == 0 and name not in {"if", "for", "while", "switch"} and function_body(source, name) is not None:
            names.add(name)
    return names


def _softfloat_sources(root: pathlib.Path) -> tuple[set[str], list[str]]:
    findings: list[str] = []
    cmake_path = root / "core" / "softfloat" / "sources.cmake"
    if not cmake_path.is_file():
        return set(), ["core/softfloat/sources.cmake: explicit source ledger is missing"]
    text = re.sub(r"#[^\n]*", "", cmake_path.read_text(encoding="utf-8"))
    match = re.search(r"set\s*\(\s*T64_SOFTFLOAT_SOURCES\b(.*?)\)", text, re.S)
    if match is None:
        return set(), [
            "core/softfloat/sources.cmake: T64_SOFTFLOAT_SOURCES ledger is missing"
        ]
    names = set(re.findall(r"/([^/\s\"]+\.c)\s*\"", match.group(1)))
    if not names:
        findings.append(
            "core/softfloat/sources.cmake: compiled softfloat source count is zero"
        )
    missing = sorted(CANONICAL_SOFTFLOAT_SOURCES - names)
    unexpected = sorted(names - CANONICAL_SOFTFLOAT_SOURCES)
    if missing:
        findings.append(
            "core/softfloat/sources.cmake: reviewed source(s) missing: "
            + ", ".join(missing)
        )
    if unexpected:
        findings.append(
            "core/softfloat/sources.cmake: unreviewed source(s): "
            + ", ".join(unexpected)
        )
    for name in sorted(names):
        if not (root / "core" / "softfloat" / name).is_file():
            findings.append(f"core/softfloat/{name}: listed production source is missing")
    return names, findings


def _source_inventory(root: pathlib.Path) -> tuple[set[str], list[str]]:
    core = root / "core"
    actual = {path.name for path in core.glob("t64_*.c") if path.is_file()}
    findings: list[str] = []
    if not actual:
        findings.append("core: scanned production source count is zero")
    missing = sorted(CANONICAL_CORE_SOURCES - actual)
    unexpected = sorted(actual - CANONICAL_CORE_SOURCES)
    if missing:
        findings.append(
            "core: canonical production source(s) missing: " + ", ".join(missing)
        )
    if unexpected:
        findings.append(
            "core: unreviewed t64_*.c production source(s): " + ", ".join(unexpected)
        )
    return actual, findings


def _check_cmake_route(root: pathlib.Path) -> list[str]:
    path = root / "core" / "CMakeLists.txt"
    if not path.is_file():
        return ["core/CMakeLists.txt: production build route is missing"]
    text = re.sub(r"#[^\n]*", "", path.read_text(encoding="utf-8"))
    compact = re.sub(r"\s+", " ", text)
    findings = []
    if not re.search(
        r'file\s*\(\s*GLOB\s+T64_CORE_SOURCES\s+CONFIGURE_DEPENDS\s+'
        r'"\$\{CMAKE_CURRENT_SOURCE_DIR\}/t64_\*\.c"\s*\)',
        compact,
    ):
        findings.append(
            "core/CMakeLists.txt: reviewed top-level t64_*.c source inventory route changed"
        )
    library = re.search(r"add_library\s*\(\s*t64_core\s+STATIC\b(.*?)\)", text, re.S)
    if library is None:
        findings.append("core/CMakeLists.txt: t64_core static library route is missing")
    else:
        body = library.group(1)
        if "${T64_CORE_SOURCES}" not in body or "${T64_SOFTFLOAT_SOURCES}" not in body:
            findings.append(
                "core/CMakeLists.txt: t64_core omits the reviewed core or softfloat source ledger"
            )
    return findings


def _check_source_route(root: pathlib.Path) -> tuple[list[str], list[str], int]:
    findings: list[str] = []
    evidence: list[str] = []
    _, inventory_findings = _source_inventory(root)
    findings.extend(inventory_findings)
    softfloat, softfloat_findings = _softfloat_sources(root)
    findings.extend(softfloat_findings)
    findings.extend(_check_cmake_route(root))

    rsp_path = root / "core" / "t64_rsp.c"
    sp_path = root / "core" / "t64_sp.c"
    if not rsp_path.is_file() or not sp_path.is_file():
        findings.append("core/t64_rsp.c or core/t64_sp.c: canonical dispatch source is missing")
        return findings, evidence, len(softfloat)

    rsp = strip_c_comments_and_literals(rsp_path.read_text(encoding="utf-8"))
    sp = strip_c_comments_and_literals(sp_path.read_text(encoding="utf-8"))
    sp_function_names = file_function_names(sp)
    if sp_function_names != REVIEWED_SP_FUNCTIONS:
        findings.append("core/t64_sp.c: complete SP function inventory changed; route review is required")
    actual_sp_dma_apis = {
        name for name in sp_function_names if name.startswith("t64_sp_rsp_dma_")
    }
    if actual_sp_dma_apis != REVIEWED_SP_DMA_API_FUNCTIONS:
        missing = sorted(REVIEWED_SP_DMA_API_FUNCTIONS - actual_sp_dma_apis)
        unexpected = sorted(actual_sp_dma_apis - REVIEWED_SP_DMA_API_FUNCTIONS)
        if missing:
            findings.append(
                "core/t64_sp.c: reviewed RSP DMA helper definition(s) missing: "
                + ", ".join(missing)
            )
        if unexpected:
            findings.append(
                "core/t64_sp.c: unreviewed RSP DMA helper definition(s): "
                + ", ".join(unexpected)
            )
    routes = {
        "core/t64_rsp.c :: rsp_read_imem_word": function_body(rsp, "rsp_read_imem_word"),
        "core/t64_rsp.c :: t64_rsp_schedule_next": function_body(
            rsp, "t64_rsp_schedule_next"
        ),
        "core/t64_rsp.c :: t64_rsp_ensure_opportunity": function_body(
            rsp, "t64_rsp_ensure_opportunity"
        ),
        "core/t64_rsp.c :: t64_rsp_start": function_body(rsp, "t64_rsp_start"),
        "core/t64_rsp.c :: t64_rsp_issue_event": function_body(
            rsp, "t64_rsp_issue_event"
        ),
        "core/t64_sp.c :: start_rsp_dma": function_body(sp, "start_rsp_dma"),
        "core/t64_sp.c :: write_rsp_status": function_body(sp, "write_rsp_status"),
    }
    static_names = static_function_names(rsp)
    if static_names != REVIEWED_RSP_STATIC_FUNCTIONS:
        missing = sorted(REVIEWED_RSP_STATIC_FUNCTIONS - static_names)
        unexpected = sorted(static_names - REVIEWED_RSP_STATIC_FUNCTIONS)
        if missing:
            findings.append(
                "core/t64_rsp.c: reviewed static helper definition(s) missing: "
                + ", ".join(missing)
            )
        if unexpected:
            findings.append(
                "core/t64_rsp.c: unreviewed static helper definition(s): "
                + ", ".join(unexpected)
            )
    for name in REVIEWED_RSP_STATIC_FUNCTIONS:
        routes[f"core/t64_rsp.c :: {name}"] = function_body(rsp, name)
    for name in REVIEWED_SP_FUNCTIONS:
        routes[f"core/t64_sp.c :: {name}"] = function_body(sp, name)
    for label, body in routes.items():
        if body is None:
            findings.append(f"{label}: required canonical route function is missing")
        else:
            expected_digest = REVIEWED_BODY_DIGESTS.get(label)
            if expected_digest is None:
                findings.append(f"{label}: reviewed normalized body digest is missing")
            elif hashlib.sha256(re.sub(r"\s+", "", body).encode()).hexdigest() != expected_digest:
                findings.append(
                    f"{label}: executable body changed; fetched-path review is required"
                )

    read_body = routes["core/t64_rsp.c :: rsp_read_imem_word"] or ""
    if "machine->sp_imem" not in read_body or "bytes[0]" not in read_body:
        findings.append(
            "core/t64_rsp.c :: rsp_read_imem_word: fetched instruction no longer reads SP IMEM bytes"
        )
    schedule_body = routes["core/t64_rsp.c :: t64_rsp_schedule_next"] or ""
    if not all(
        token in schedule_body
        for token in (
            "t64_schedule_reserve_after",
            "T64_SP_EVENT_RSP_ISSUE",
            "t64_rsp_issue_event",
        )
    ):
        findings.append(
            "core/t64_rsp.c :: t64_rsp_schedule_next: scheduled issue callback is not t64_rsp_issue_event"
        )
    ensure_body = routes["core/t64_rsp.c :: t64_rsp_ensure_opportunity"] or ""
    start_body = routes["core/t64_rsp.c :: t64_rsp_start"] or ""
    if "t64_rsp_schedule_next(machine)" not in ensure_body:
        findings.append(
            "core/t64_rsp.c :: t64_rsp_ensure_opportunity: no scheduled fetched issue route"
        )
    if "t64_rsp_ensure_opportunity(machine)" not in start_body:
        findings.append(
            "core/t64_rsp.c :: t64_rsp_start: public execution entry does not route to scheduled issue"
        )
    issue_body = routes["core/t64_rsp.c :: t64_rsp_issue_event"] or ""
    fetch_match = re.search(r"instruction\s*=\s*rsp_read_imem_word\s*\(\s*machine\s*,\s*pc\s*\)", issue_body)
    operation_match = re.search(r"operation\s*=\s*instruction\s*>>\s*26", issue_body)
    required_dispatch = (
        "rsp_decode_vadd",
        "rsp_decode_vector_basic",
        "rsp_decode_vector_multiply",
        "rsp_decode_vector_compare",
        "rsp_decode_divider",
        "rsp_plan_vector_memory",
        "T64_RSP_ATTEMPT_RETIRED",
        "T64_RSP_ATTEMPT_REFUSED",
        "T64_RSP_REFUSAL_UNSUPPORTED_OPCODE",
    )
    if fetch_match is None:
        findings.append(
            "core/t64_rsp.c :: t64_rsp_issue_event: event does not fetch instruction from current IMEM PC"
        )
    if operation_match is None:
        findings.append(
            "core/t64_rsp.c :: t64_rsp_issue_event: opcode is not derived from fetched instruction bits"
        )
    if fetch_match is not None and operation_match is not None and fetch_match.start() > operation_match.start():
        findings.append(
            "core/t64_rsp.c :: t64_rsp_issue_event: opcode derivation precedes fetched instruction"
        )
    for token in required_dispatch:
        if token not in issue_body:
            findings.append(
                f"core/t64_rsp.c :: t64_rsp_issue_event: required decode/refusal path {token} is absent"
            )

    for label in (
        "core/t64_sp.c :: start_rsp_dma",
        "core/t64_sp.c :: write_rsp_status",
    ):
        body = routes[label] or ""
        if "t64_rsp_ensure_opportunity(machine)" not in body:
            findings.append(f"{label}: SP execution entry bypasses scheduled RSP opportunity")

    dispatch_sources = {"core/t64_rsp.c": rsp, "core/t64_sp.c": sp}
    for path, code in dispatch_sources.items():
        for token in re.findall(r"[A-Za-z_][A-Za-z_0-9]*", code):
            compact = token.casefold().replace("_", "")
            suspicious = (
                "microcode" in compact
                or "ucode" in compact
                or "highlevel" in compact
                or "hle" in compact
                or compact.startswith("task")
                or "signature" in compact
            )
            if suspicious:
                findings.append(
                    f"{path}: executable identifier {token} selects or names a task/signature/HLE route"
                )
        if path == "core/t64_rsp.c" and re.search(
            r"machine\s*->\s*(?:image_storage|image_length|content_digest|rom_header|rom_crc|image_digest)",
            code,
        ):
            findings.append(
                "core/t64_rsp.c :: t64_rsp_issue_event: dispatch reads content identity instead of fetched opcode"
            )

    if not findings:
        evidence.extend(
            [
                "core/t64_rsp.c :: t64_rsp_start -> t64_rsp_ensure_opportunity -> t64_rsp_schedule_next",
                "core/t64_rsp.c :: t64_rsp_schedule_next -> t64_schedule_reserve_after(T64_SP_EVENT_RSP_ISSUE, t64_rsp_issue_event)",
                "core/t64_rsp.c :: t64_rsp_issue_event -> rsp_read_imem_word(machine, pc) -> operation = instruction >> 26 -> decode/refuse/retire",
                "core/t64_sp.c :: start_rsp_dma and write_rsp_status -> t64_rsp_ensure_opportunity",
            ]
        )
    return findings, evidence, len(CANONICAL_CORE_SOURCES) + len(softfloat)


def _expected_object_sources(root: pathlib.Path) -> dict[str, pathlib.Path]:
    names, findings = _softfloat_sources(root)
    if findings:
        return {}
    expected: dict[str, pathlib.Path] = {}
    for name in CANONICAL_CORE_SOURCES:
        target = f"core/CMakeFiles/t64_core.dir/{name}.o"
        expected[target] = root / "core" / name
    for name in names:
        target = f"core/CMakeFiles/t64_core.dir/softfloat/{name}.o"
        expected[target] = root / "core" / "softfloat" / name
    return expected


def _check_build_route(root: pathlib.Path, build: pathlib.Path) -> tuple[list[str], list[str]]:
    findings: list[str] = []
    evidence: list[str] = []
    expected = _expected_object_sources(root)
    if not expected:
        return ["build route: expected production source count is zero"], evidence
    ninja_path = build / "build.ninja"
    if not ninja_path.is_file():
        return ["build/build.ninja: active Ninja route is missing"], evidence

    object_edges: dict[str, str] = {}
    archive_objects: list[str] | None = None
    for line in ninja_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("build core/CMakeFiles/t64_core.dir/") and ".c.o:" in line:
            left, right = line.split(":", 1)
            target = left[len("build ") :]
            prerequisites = right.split(" ||", 1)[0].strip().split()
            if len(prerequisites) < 2:
                findings.append(f"build/build.ninja :: {target}: source edge is incomplete")
                continue
            source = prerequisites[1]
            object_edges[target] = source
        elif line.startswith("build core/libt64_core.a:"):
            tokens = shlex.split(line.split(":", 1)[1])
            archive_objects = [
                token
                for token in tokens
                if token.startswith("core/CMakeFiles/t64_core.dir/")
                and token.endswith(".c.o")
            ]

    if archive_objects is None:
        findings.append("build/build.ninja :: core/libt64_core.a: archive link edge is missing")
        archive_objects = []
    expected_targets = set(expected)
    actual_targets = set(object_edges)
    if actual_targets != expected_targets:
        missing = sorted(expected_targets - actual_targets)
        extra = sorted(actual_targets - expected_targets)
        if missing:
            findings.append("build/build.ninja: missing core object edge(s): " + ", ".join(missing))
        if extra:
            findings.append("build/build.ninja: unreviewed core object edge(s): " + ", ".join(extra))
    for target, source in object_edges.items():
        if target not in expected:
            continue
        expected_source = expected[target].resolve()
        actual_source = pathlib.Path(source).resolve()
        if actual_source != expected_source:
            findings.append(
                f"build/build.ninja :: {target}: source edge points to {source}, expected core-relative {expected[target].relative_to(root)}"
            )
    if collections.Counter(archive_objects) != collections.Counter(expected_targets):
        missing = sorted((collections.Counter(expected_targets) - collections.Counter(archive_objects)).elements())
        extra = sorted((collections.Counter(archive_objects) - collections.Counter(expected_targets)).elements())
        if missing:
            findings.append("build/build.ninja :: core/libt64_core.a: RSP/core object(s) omitted: " + ", ".join(missing))
        if extra:
            findings.append("build/build.ninja :: core/libt64_core.a: unreviewed object(s) linked: " + ", ".join(extra))

    object_root = build / "core" / "CMakeFiles" / "t64_core.dir"
    if object_root.is_dir():
        active_object_files = list(object_root.glob("*.c.o"))
        softfloat_object_root = object_root / "softfloat"
        if softfloat_object_root.is_dir():
            active_object_files.extend(softfloat_object_root.rglob("*.c.o"))
        disk_objects = {
            "core/CMakeFiles/t64_core.dir/" + path.relative_to(object_root).as_posix()
            for path in active_object_files
        }
    else:
        disk_objects = set()
    if disk_objects != expected_targets:
        missing = sorted(expected_targets - disk_objects)
        extra = sorted(disk_objects - expected_targets)
        if missing:
            findings.append("build/core/CMakeFiles/t64_core.dir: object file(s) missing: " + ", ".join(missing))
        if extra:
            findings.append("build/core/CMakeFiles/t64_core.dir: hidden object file(s): " + ", ".join(extra))

    archive_path = build / "core" / "libt64_core.a"
    if not archive_path.is_file():
        findings.append("build/core/libt64_core.a: canonical core archive is missing")
    else:
        ar = shutil.which("ar")
        if ar is None:
            findings.append("build route: `ar` unavailable for archive member inspection")
        else:
            result = subprocess.run(
                [ar, "-t", str(archive_path)],
                check=False,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode != 0:
                findings.append("build/core/libt64_core.a: archive member inspection failed")
            else:
                members = [
                    line.strip()
                    for line in result.stdout.splitlines()
                    if line.strip() and not line.strip().startswith("__.SYMDEF")
                ]
                expected_members = collections.Counter(
                    pathlib.PurePosixPath(target).name for target in expected_targets
                )
                if collections.Counter(members) != expected_members:
                    findings.append(
                        "build/core/libt64_core.a: actual archive members differ from reviewed production object inventory"
                    )

    rsp_object = build / "core" / "CMakeFiles" / "t64_core.dir" / "t64_rsp.c.o"
    nm = shutil.which("nm")
    if not rsp_object.is_file():
        findings.append("build/core/CMakeFiles/t64_core.dir/t64_rsp.c.o: canonical RSP object is missing")
    elif nm is None:
        findings.append("build route: `nm` unavailable for canonical RSP object inspection")
    else:
        result = subprocess.run(
            [nm, "-a", str(rsp_object)],
            check=False,
            capture_output=True,
            text=True,
            timeout=10,
        )
        symbols = set()
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                pieces = line.split()
                if pieces:
                    symbol = pieces[-1]
                    while symbol.startswith("_"):
                        symbol = symbol[1:]
                    symbols.add(symbol)
        else:
            findings.append("build/core/CMakeFiles/t64_core.dir/t64_rsp.c.o: nm inspection failed")
        for symbol in ("t64_rsp_issue_event", "t64_rsp_ensure_opportunity", "t64_rsp_start"):
            if symbol not in symbols:
                findings.append(
                    f"build/core/CMakeFiles/t64_core.dir/t64_rsp.c.o: required canonical symbol {symbol} is absent"
                )
    if not findings:
        evidence.extend(
            [
                f"build/build.ninja :: core/libt64_core.a contains {len(expected)} reviewed source-object edges",
                "build/core/libt64_core.a :: archive member list matches the source-object edges",
                "build/core/CMakeFiles/t64_core.dir/t64_rsp.c.o :: nm contains t64_rsp_issue_event, t64_rsp_ensure_opportunity, t64_rsp_start",
            ]
        )
    return findings, evidence


def inspect_tree(root: pathlib.Path, build: pathlib.Path | None = None) -> tuple[list[str], list[str], int]:
    root = root.resolve()
    findings, evidence, scanned = _check_source_route(root)
    if build is not None:
        build_findings, build_evidence = _check_build_route(root, build.resolve())
        findings.extend(build_findings)
        evidence.extend(build_evidence)
    return findings, evidence, scanned


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--build",
        type=pathlib.Path,
        default=pathlib.Path(os.environ["T64_RSP_NO_HLE_BUILD_DIR"])
        if os.environ.get("T64_RSP_NO_HLE_BUILD_DIR")
        else None,
    )
    args = parser.parse_args(argv)
    findings, evidence, scanned = inspect_tree(args.root, args.build)
    if findings:
        for finding in findings:
            print(f"FAIL: {finding}", file=sys.stderr)
        print(f"rsp-no-hle: sources_scanned={scanned} findings={len(findings)}", file=sys.stderr)
        return 1
    print(f"PASS: rsp-no-hle sources_scanned={scanned}")
    for item in evidence:
        print(f"route: {item}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

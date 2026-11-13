#!/usr/bin/env python3
"""Check actual private diagnostic composition; never execute the guest.

The receipt verifier selects/hash-checks objdump before its invocation. Section
metadata is checked against the actual ELF bytes and packed flat payload. This
supplements source qualification; it does not establish runtime startup.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import stat
import struct
import subprocess
import sys

BASE = 0xFFFFFFFF80000000
ENTRY = BASE + 0x400
PARK = BASE + 0x420
HEAP_END = BASE + 0x300000
RDRAM_END = BASE + 0x400000
BOOT_SHA = "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068"
FONT_SOURCE_SHA = "d1d92bd0f88bc86806e977413d4a103e79aea1758604fcdbf4e4a9acf1bdee31"
FONT_SHA = "eeaa8d5929fbaccae3a61a1cf49ffb69fd3156d78f4cca8efdba541837e661c2"
SOURCE_REVISION = "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def parse_objdump(text: str) -> tuple[int, dict, dict]:
    """Accept the fixed GNU -f -h -t table, not arbitrary inferred layouts."""
    require(len(text) <= 32 * 1024 * 1024, "objdump report is too large")
    require(re.search(r"file format elf64-bigmips\b", text) is not None,
            "ELF is not the qualified big-endian MIPS64 format")
    entries = re.findall(r"^start address 0x([0-9a-fA-F]+)\s*$", text, re.M)
    require(len(entries) == 1, "ELF entry is missing or duplicated")
    lines = text.splitlines()
    sections: dict[str, dict] = {}
    pattern = re.compile(r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+"
                         r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+"
                         r"([0-9a-fA-F]+)\s+2\*\*\d+\s*$")
    for index, line in enumerate(lines):
        match = pattern.fullmatch(line)
        if match is None:
            continue
        name, size, vma, lma, offset = match.groups()
        require(name not in sections and index + 1 < len(lines),
                "ELF section is duplicated or has no flags")
        flags = {value.strip() for value in lines[index + 1].split(",")}
        sections[name] = dict(size=int(size, 16), address=int(vma, 16),
                              lma=int(lma, 16), offset=int(offset, 16), flags=flags)
    require(bool(sections), "ELF section table is absent")
    symbols: dict[str, int] = {}
    wanted = {"entrypoint", "park", "T64_CPU_SELECTOR", "loaded_rom_start",
              "loaded_rom_end", "__bss_start", "__bss_end", "__cpu_heap_limit",
              "__cpu_stack_bottom", "__cpu_stack_top", "__cpu_rdram_start",
              "__cpu_rdram_end", "__t64_selector_start", "__t64_selector_end"}
    for line in lines:
        fields = line.split()
        if len(fields) < 4 or fields[-1] not in wanted:
            continue
        require(re.fullmatch(r"[0-9a-fA-F]+", fields[0]) is not None,
                "required ELF symbol has no numeric address")
        name = fields[-1]
        require(name not in symbols and "*UND*" not in fields,
                "required ELF symbol is duplicated or undefined")
        symbols[name] = int(fields[0], 16)
    require(set(symbols) == wanted, "required ELF layout symbols are incomplete")
    return int(entries[0], 16), sections, symbols


def verify_layout(elf: bytes, payload: bytes, dump: str) -> dict:
    require(len(elf) >= 64 and elf[:7] == b"\x7fELF\x02\x02\x01",
            "ELF identification is not 64-bit big-endian version 1")
    require(struct.unpack_from(">HHI", elf, 16) == (2, 8, 1),
            "ELF is not a MIPS executable")
    entry, sections, symbols = parse_objdump(dump)
    require(entry == ENTRY == struct.unpack_from(">Q", elf, 24)[0],
            "actual ELF entry differs from the fixed CPU entry")
    fixed = {"entrypoint": ENTRY, "park": PARK, "loaded_rom_start": ENTRY,
             "__cpu_heap_limit": HEAP_END, "__cpu_stack_bottom": HEAP_END,
             "__cpu_stack_top": RDRAM_END, "__cpu_rdram_start": BASE,
             "__cpu_rdram_end": RDRAM_END}
    require(all(symbols[key] == value for key, value in fixed.items()),
            "actual ELF profile/entry/park symbols drifted")
    loaded_end = symbols["loaded_rom_end"]
    bss_start, bss_end = symbols["__bss_start"], symbols["__bss_end"]
    require(ENTRY < loaded_end <= bss_start <= bss_end <= HEAP_END,
            "actual ELF loaded/BSS/heap/stack ranges overlap or exceed RDRAM")
    require(len(payload) == loaded_end - ENTRY,
            "flat payload length differs from actual ELF loaded range")
    reconstructed = bytearray(len(payload))
    ranges: list[tuple[int, int]] = []
    for section in sections.values():
        flags, size, start = section["flags"], section["size"], section["address"]
        if "ALLOC" not in flags or size == 0:
            continue
        end = start + size
        require(ENTRY <= start <= end <= HEAP_END,
                "allocated ELF section lies outside the qualified load/heap range")
        require(all(end <= lo or start >= hi for lo, hi in ranges),
                "allocated ELF sections overlap")
        ranges.append((start, end))
        if "CONTENTS" not in flags:
            require(bss_start <= start <= end <= bss_end,
                    "uninitialized allocated section is outside BSS")
            continue
        require("LOAD" in flags and section["lma"] == start and end <= loaded_end,
                "ELF file-backed section has an unexpected load mapping")
        offset = section["offset"]
        require(offset <= len(elf) and size <= len(elf) - offset,
                "ELF section bytes are truncated")
        reconstructed[start - ENTRY:end - ENTRY] = elf[offset:offset + size]
    require(bytes(reconstructed) == payload,
            "flat payload differs from actual allocated ELF bytes or gap padding")
    for name, address, size in ((".t64_entrypoint", ENTRY, 16), (".t64_park", PARK, 8)):
        section = sections.get(name)
        require(section is not None and section["address"] == address
                and section["size"] == size
                and {"CONTENTS", "ALLOC", "LOAD", "CODE"} <= section["flags"],
                "actual entry/park section layout differs from the runner contract")
    require(payload[0:8] == bytes.fromhex("3c1d804067bdfff0")
            and payload[12:16] == bytes(4), "actual entry stack setup or delay slot drifted")
    jump = int.from_bytes(payload[8:12], "big")
    target = BASE | ((jump & 0x03FFFFFF) << 2)
    require(jump >> 26 == 2 and ENTRY + 0x28 <= target < loaded_end,
            "entry jump does not target loaded runtime code")
    require(payload[PARK - ENTRY:PARK - ENTRY + 8] == bytes.fromhex("0800010800000000"),
            "actual park opcode differs from authenticated completion sentinel")
    selector = symbols["__t64_selector_start"]
    require(selector == symbols["T64_CPU_SELECTOR"] and selector % 4 == 0
            and symbols["__t64_selector_end"] == selector + 4
            and ENTRY <= selector <= loaded_end - 4,
            "actual selector is not one aligned loaded u32")
    selector_section = sections.get(".t64_selector")
    require(selector_section is not None and selector_section["address"] == selector
            and selector_section["size"] >= 4
            and {"CONTENTS", "ALLOC", "LOAD", "DATA"} <= selector_section["flags"],
            "selector symbol is outside its retained section")
    require(payload[selector - ENTRY:selector - ENTRY + 4] == bytes(4),
            "base image selector is not its authored zero initializer")
    return {"entry": hex(ENTRY), "park": hex(PARK), "bss_start": hex(bss_start),
            "bss_end": hex(bss_end), "heap_end": hex(HEAP_END),
            "stack_bottom": hex(HEAP_END), "stack_top": hex(RDRAM_END),
            "selector_rom_offset": 0x1000 + selector - ENTRY,
            "selector_bytes": 4, "loaded_bytes": len(payload),
            "available_heap_bytes": HEAP_END - bss_end}


def verify_composition(elf: bytes, payload: bytes, rom: bytes, boot: bytes,
                       font: bytes) -> None:
    require(len(boot) == 4096 and digest(boot) == BOOT_SHA,
            "cleared IPL3 identity differs from the qualified asset")
    require(0 < len(payload) <= 0x400000 - 0x400
            and len(rom) == 4096 + len(payload), "packed CPU image length is invalid")
    header = bytearray(64)
    struct.pack_into(">6I", header, 0, 0x80371240, 15, 0x80000400, 0, len(payload), 0)
    title = b"TENDIES64 HELLO     "
    header[0x20:0x20 + len(title)] = title
    header[0x3A] = ord("E")
    require(rom[:64] == header and rom[64:4096] == boot[64:]
            and rom[4096:] == payload,
            "packed header/cleared IPL3/payload composition differs from the owned packer")
    require(len(font) == 762 and digest(font) == FONT_SHA,
            "excluded font absence-scan identity is not the audited payload")
    require(all(font not in data for data in (elf, payload, rom)),
            "excluded font bytes remain in an actual build artifact")


def read_bounded(path: pathlib.Path, limit: int) -> bytes:
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(descriptor, "rb") as source:
        info = os.fstat(source.fileno())
        require(stat.S_ISREG(info.st_mode) and 0 <= info.st_size <= limit,
                "artifact input is not a bounded regular file")
        data = source.read(limit + 1)
        require(len(data) == info.st_size, "artifact changed or exceeded its input bound")
        return data


def private_path(root: pathlib.Path, value: str) -> pathlib.Path:
    require(root.is_dir() and root.resolve(strict=True) == root,
            "private root has a symlinked ancestor or is not canonical")
    require(root.stat().st_mode & 0o077 == 0,
            "private root permissions expose artifacts")
    candidate = pathlib.Path(os.path.abspath(value))
    candidate.relative_to(root)
    current = root
    require(not root.is_symlink(), "private root is symlinked")
    for component in candidate.relative_to(root).parts:
        current /= component
        require(not current.is_symlink(), "private artifact path contains a symlink")
    return candidate


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("elf", "payload", "rom", "boot", "closure", "font-source", "report"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--qualification", default="build/private-cpu-diagnostic/qualification.json")
    args = parser.parse_args()
    repository = pathlib.Path(__file__).resolve().parent.parent
    os.chdir(repository)
    root = repository / "build/private-cpu-diagnostic"
    # Reuse the production pre-use gate, including executable SHA/path checks.
    spec = importlib.util.spec_from_file_location(
        "t64_build_qualification", repository / "scripts/verify-cpu-diagnostic-qualification.py")
    assert spec is not None and spec.loader is not None
    qualification = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(qualification)
    receipt_path = private_path(root, args.qualification)
    receipt_bytes = read_bounded(receipt_path, 1024 * 1024)
    try:
        tools = qualification.validate(receipt_path, repository)
    except qualification.Refusal as exc:
        raise ValueError(str(exc)) from None
    require(read_bounded(receipt_path, 1024 * 1024) == receipt_bytes,
            "qualification receipt changed during validation")
    receipt = json.loads(receipt_bytes)
    inputs = {name: private_path(root, getattr(args, name.replace("-", "_")))
              for name in ("elf", "payload", "rom", "closure", "font-source", "report")}
    elf = read_bounded(inputs["elf"], 64 * 1024 * 1024)
    payload = read_bounded(inputs["payload"], 4 * 1024 * 1024)
    rom = read_bounded(inputs["rom"], 4 * 1024 * 1024 + 4096)
    boot_path = pathlib.Path(args.boot)
    require(boot_path.resolve() == (repository / "build/fixtures/ipl3_compat.z64").resolve(),
            "boot input is not the qualified fixture path")
    boot = read_bounded(boot_path, 4096)
    closure_bytes = read_bounded(inputs["closure"], 16 * 1024 * 1024)
    closure = json.loads(closure_bytes)
    require(closure.get("status") == "PASS_PREBUILD_SOURCE"
            and closure.get("source_revision") == SOURCE_REVISION
            and digest(closure_bytes) == receipt["assets"]["closure_report"]["sha256"],
            "actual composition is not bound to the qualified source closure")
    font_source = read_bounded(inputs["font-source"], 1024 * 1024)
    require(digest(font_source) == FONT_SOURCE_SHA, "font source differs from audited identity")
    source_text = re.sub(r"//[^\n]*|/\*.*?\*/", "", font_source.decode("utf-8"), flags=re.S)
    match = re.search(r"FONT_GENEVA_9\s*:\s*\[u8;\s*762\]\s*=\s*\[(.*?)\]\s*;", source_text, re.S)
    require(match is not None, "audited excluded font array cannot be identified")
    font = bytes(int(item.strip(), 0) for item in match.group(1).split(",") if item.strip())
    verify_composition(elf, payload, rom, boot, font)
    objdump = pathlib.Path(tools["target_binutils"]["objdump"])
    approved = [row for row in receipt["tools"]["target_binutils"] if row["name"] == "objdump"]
    require(len(approved) == 1 and pathlib.Path(approved[0]["path"]).resolve() == objdump,
            "objdump path differs from the qualified receipt")
    expected_objdump_sha256 = approved[0]["sha256"]
    objdump_sha256 = digest(read_bounded(objdump, 128 * 1024 * 1024))
    require(objdump_sha256 == expected_objdump_sha256,
            "objdump differs from the qualified receipt")
    completed = subprocess.run(
        [str(objdump), "-f", "-h", "-t", str(inputs["elf"])],
        check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30,
        env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"})
    layout = verify_layout(elf, payload, completed.stdout.decode("utf-8"))
    require(read_bounded(inputs["elf"], 64 * 1024 * 1024) == elf,
            "ELF changed during objdump inspection")
    require(digest(read_bounded(objdump, 128 * 1024 * 1024)) == expected_objdump_sha256,
            "qualified objdump changed during inspection")
    report = {"schema": "t64.cpu-build-composition", "schema_version": 1,
              "status": "PASS_POSTBUILD_COMPOSITION", "layout": layout,
              "elf_sha256": digest(elf), "payload_sha256": digest(payload),
              "rom_sha256": digest(rom), "qualification_sha256": digest(receipt_bytes),
              "source_closure_sha256": digest(closure_bytes),
              "objdump_sha256": objdump_sha256,
              "excluded_font_sha256": FONT_SHA, "guest_execution_verified": False}
    descriptor = os.open(inputs["report"], os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(descriptor, "w", encoding="utf-8") as output:
        json.dump(report, output, indent=2, sort_keys=True)
        output.write("\n")
    print("PASS: actual ELF/ROM composition; guest startup remains unverified")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, TypeError, OverflowError,
            subprocess.SubprocessError) as exc:
        print(f"post-build composition refused: {exc}", file=sys.stderr)
        raise SystemExit(4)

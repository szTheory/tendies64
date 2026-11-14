#!/usr/bin/env python3
"""Inspect actual private RSP ELF, flat payload, ROM, and closure composition."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import stat
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/private-rsp-diagnostic"
PROFILE_ID = "ntsc-cart-4m-openipl3-rsp-functional-v1"
BOOT_SHA = "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068"
BASE = 0xFFFFFFFF80000000
ENTRY = BASE + 0x400
PARK = BASE + 0x420
HEAP_END = BASE + 0x300000
RDRAM_END = BASE + 0x400000


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def read_bounded(path: pathlib.Path, limit: int, root: pathlib.Path) -> bytes:
    path = pathlib.Path(os.path.abspath(path))
    try:
        path.relative_to(root)
    except ValueError:
        raise ValueError("artifact path escapes its private root") from None
    current = root
    for part in path.relative_to(root).parts:
        current /= part
        require(not current.is_symlink(), "artifact path traverses a symlink")
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
    with os.fdopen(descriptor, "rb") as stream:
        info = os.fstat(stream.fileno())
        require(stat.S_ISREG(info.st_mode) and 0 <= info.st_size <= limit, "artifact is not a bounded regular file")
        data = stream.read(limit + 1)
        require(len(data) == info.st_size, "artifact changed or exceeded its bound while reading")
        return data


def parse_elf(elf: bytes, payload: bytes, section_dump: str, symbol_dump: str) -> dict[str, object]:
    require(len(elf) >= 64 and elf[:7] == b"\x7fELF\x02\x02\x01", "ELF is not 64-bit big-endian version 1")
    require(struct.unpack_from(">HHI", elf, 16) == (2, 8, 1), "ELF is not a MIPS executable")
    entry = struct.unpack_from(">Q", elf, 24)[0]
    shoff = struct.unpack_from(">Q", elf, 40)[0]
    shentsize, shnum, shstrndx = struct.unpack_from(">HHH", elf, 58)
    require(shentsize >= 64 and 0 < shnum <= 512 and shstrndx < shnum, "ELF section table header is malformed")
    require(shoff <= len(elf) and shnum <= (len(elf) - shoff) // shentsize, "ELF section table is truncated")
    headers = []
    for index in range(shnum):
        raw = struct.unpack_from(">IIQQQQIIQQ", elf, shoff + index * shentsize)
        headers.append({"name_offset": raw[0], "type": raw[1], "flags": raw[2], "address": raw[3], "offset": raw[4], "size": raw[5], "link": raw[6], "entry_size": raw[9]})
    strings_header = headers[shstrndx]
    strings = elf[strings_header["offset"]:strings_header["offset"] + strings_header["size"]]
    require(len(strings) == strings_header["size"], "ELF section names are truncated")
    sections: dict[str, dict[str, int]] = {}
    for header in headers:
        offset = header["name_offset"]
        require(offset < len(strings), "ELF section name offset is invalid")
        end = strings.find(b"\0", offset)
        require(end >= 0, "ELF section name is unterminated")
        name = strings[offset:end].decode("ascii", errors="strict")
        if name:
            require(name not in sections, "ELF section name is duplicated")
            sections[name] = header
    wanted = {"entrypoint", "park", "T64_RSP_SELECTOR", "loaded_rom_start", "loaded_rom_end", "__bss_start", "__bss_end", "__cpu_heap_limit", "__cpu_stack_bottom", "__cpu_stack_top", "__cpu_rdram_start", "__cpu_rdram_end", "__t64_selector_start", "__t64_selector_end"}
    symbols: dict[str, int] = {}
    for header in headers:
        if header["type"] not in (2, 11) or header["entry_size"] < 24 or header["link"] >= len(headers):
            continue
        str_header = headers[header["link"]]
        names = elf[str_header["offset"]:str_header["offset"] + str_header["size"]]
        size = header["size"]
        if size % header["entry_size"] != 0 or header["offset"] + size > len(elf):
            require(False, "ELF symbol table is malformed")
        for offset in range(header["offset"], header["offset"] + size, header["entry_size"]):
            name_offset, info, other, shndx, value, symbol_size = struct.unpack_from(">IBBHQQ", elf, offset)
            if name_offset >= len(names):
                continue
            end = names.find(b"\0", name_offset)
            if end < 0:
                continue
            name = names[name_offset:end].decode("utf-8", errors="strict")
            if name in wanted:
                require(shndx != 0 and name not in symbols, "required ELF symbol is undefined or duplicated")
                symbols[name] = value
    require(set(symbols) == wanted, "required RSP ELF layout symbols are incomplete")
    require(entry == ENTRY == symbols["entrypoint"] == symbols["loaded_rom_start"], "RSP ELF entry/load address differs from its sealed profile")
    fixed = {"park": PARK, "__cpu_heap_limit": HEAP_END, "__cpu_stack_bottom": HEAP_END, "__cpu_stack_top": RDRAM_END, "__cpu_rdram_start": BASE, "__cpu_rdram_end": RDRAM_END}
    require(all(symbols[key] == value for key, value in fixed.items()), "RSP ELF memory profile symbols drifted")
    loaded_end = symbols["loaded_rom_end"]
    bss_start, bss_end = symbols["__bss_start"], symbols["__bss_end"]
    require(ENTRY < loaded_end <= bss_start <= bss_end <= HEAP_END, "RSP ELF loaded/BSS/heap/stack ranges overlap or escape 4 MiB RDRAM")
    require(len(payload) == loaded_end - ENTRY, "flat payload length differs from ELF loaded range")
    reconstructed = bytearray(len(payload))
    ranges: list[tuple[int, int]] = []
    for name, section in sections.items():
        flags, size, start = section["flags"], section["size"], section["address"]
        if not flags & 2 or size == 0:
            continue
        end = start + size
        require(ENTRY <= start <= end <= HEAP_END, "allocated ELF section escapes RSP profile memory")
        require(all(end <= lo or start >= hi for lo, hi in ranges), "allocated ELF sections overlap")
        ranges.append((start, end))
        if section["type"] == 8:  # SHT_NOBITS
            require(bss_start <= start <= end <= bss_end, "uninitialized section is outside RSP BSS")
            continue
        require(section["type"] == 1 and end <= loaded_end, "allocated RSP ELF section has an unexpected file mapping")
        offset = section["offset"]
        require(offset <= len(elf) and size <= len(elf) - offset, "ELF section contents are truncated")
        reconstructed[start - ENTRY:end - ENTRY] = elf[offset:offset + size]
    require(bytes(reconstructed) == payload, "flat payload differs from actual allocated ELF sections")
    for name, address, size in ((".t64_entrypoint", ENTRY, 16), (".t64_park", PARK, 8)):
        section = sections.get(name)
        require(section is not None and section["address"] == address and section["size"] == size and section["type"] == 1 and section["flags"] & 6 == 6, "RSP entry or park section differs from the startup contract")
    selector = symbols["__t64_selector_start"]
    require(symbols["__t64_selector_end"] == selector + 4 and symbols["T64_RSP_SELECTOR"] == selector and selector % 4 == 0 and ENTRY <= selector <= loaded_end - 4, "RSP selector symbols do not define one aligned loaded u32")
    selector_section = sections.get(".t64_selector")
    require(selector_section is not None and selector_section["address"] == selector and selector_section["size"] >= 4 and selector_section["flags"] & 2 == 2, "RSP selector is not retained in a loadable data section")
    require(payload[selector - ENTRY:selector - ENTRY + 4] == bytes(4), "RSP base selector initializer is not zero")
    require(payload[:8] == bytes.fromhex("3c1d804067bdfff0") and payload[12:16] == bytes(4), "RSP entry stack setup differs from its headless startup")
    jump = int.from_bytes(payload[8:12], "big")
    target = BASE | ((jump & 0x03FFFFFF) << 2)
    require(jump >> 26 == 2 and ENTRY + 0x28 <= target < loaded_end, "RSP entry jump does not enter loaded code")
    require(payload[PARK - ENTRY:PARK - ENTRY + 8] == bytes.fromhex("0800010800000000"), "RSP completion park opcode differs")
    require("T64_RSP_SELECTOR" in symbol_dump and ".t64_selector" in section_dump and "T64_CPU_SELECTOR" not in symbol_dump, "binutils reports a CPU selector or omits the RSP selector")
    return {"entry": hex(ENTRY), "park": hex(PARK), "selector_address": hex(selector), "selector_bytes": 4, "loaded_bytes": len(payload), "bss_start": hex(bss_start), "bss_end": hex(bss_end), "heap_end": hex(HEAP_END), "stack_top": hex(RDRAM_END), "allocated_sections": sorted(name for name, item in sections.items() if item["flags"] & 2 and item["size"])}


def verify_fixture(value: object) -> None:
    keys = {"schema", "profile_id", "selector_symbol", "required_sections", "font_graphics", "boot_sha256", "lock_sha256"}
    require(isinstance(value, dict) and set(value) == keys, "authored RSP composition fixture fields differ")
    require(value["schema"] == "t64.rsp-build-fixture.v1" and value["profile_id"] == PROFILE_ID, "authored RSP composition fixture profile differs")
    require(value["selector_symbol"] == "T64_RSP_SELECTOR", "composition fixture selector is not RSP-specific")
    require(value["required_sections"] == [".t64_entrypoint", ".t64_park", ".t64_selector", ".text"], "composition fixture section closure differs")
    require(value["font_graphics"] == "excluded-default-unqualified-stress", "composition fixture includes an unqualified asset")
    require(value["boot_sha256"] == BOOT_SHA, "composition fixture boot identity differs")
    require(re.fullmatch(r"[0-9a-f]{64}", value["lock_sha256"]) is not None, "composition fixture lock identity is malformed")


def selftest(workspace: pathlib.Path) -> None:
    root = pathlib.Path(os.path.abspath(workspace))
    if root.is_symlink() or not root.is_dir() or stat.S_IMODE(root.stat().st_mode) != 0o700:
        raise ValueError("authored selftest workspace must already exist with owner-only access")
    baseline = {"schema": "t64.rsp-build-fixture.v1", "profile_id": PROFILE_ID, "selector_symbol": "T64_RSP_SELECTOR", "required_sections": [".t64_entrypoint", ".t64_park", ".t64_selector", ".text"], "font_graphics": "excluded-default-unqualified-stress", "boot_sha256": BOOT_SHA, "lock_sha256": "a" * 64}
    verify_fixture(baseline)
    for key, changed in (("selector_symbol", "T64_CPU_SELECTOR"), ("required_sections", [".text"]), ("font_graphics", "included"), ("boot_sha256", "0" * 64), ("lock_sha256", "not-a-digest")):
        forged = dict(baseline)
        forged[key] = changed
        try:
            verify_fixture(forged)
        except ValueError:
            continue
        raise ValueError(f"authored RSP composition selftest accepted a changed {key}")
    print("PASS: authored RSP build composition controls reject selector, asset, section, boot, and lock drift")


def inspect(args: argparse.Namespace) -> None:
    private = PRIVATE.resolve(strict=True)
    require(not PRIVATE.is_symlink() and PRIVATE.stat().st_mode & 0o077 == 0, "private RSP root is not owner-only")
    qualification_path = pathlib.Path(args.qualification)
    subprocess.run([sys.executable, "-B", "scripts/verify-rsp-diagnostic-qualification.py", "--verify", str(qualification_path)], cwd=ROOT, check=True, timeout=120, env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C", "PYTHONDONTWRITEBYTECODE": "1"})
    qualification = json.loads(qualification_path.read_text(encoding="utf-8"))
    require(qualification.get("profile_id") == PROFILE_ID and isinstance(qualification.get("source_revision"), str), "RSP build qualification identity differs")
    elf_path, payload_path, rom_path, boot_path = map(pathlib.Path, (args.elf, args.payload, args.rom, args.boot))
    elf = read_bounded(elf_path, 64 * 1024 * 1024, private)
    payload = read_bounded(payload_path, 4 * 1024 * 1024, private)
    rom = read_bounded(rom_path, 4 * 1024 * 1024 + 4096, private)
    boot = read_bounded(boot_path, 4096, ROOT / "build/fixtures")
    require(boot_path.resolve(strict=True) == ROOT / "build/fixtures/ipl3_compat.z64" and len(boot) == 4096 and digest(boot) == BOOT_SHA, "RSP boot asset differs from the qualified open IPL3 bytes")
    section_path, symbol_path = pathlib.Path(args.sections_log), pathlib.Path(args.symbols_log)
    section_dump = read_bounded(section_path, 32 * 1024 * 1024, private).decode("utf-8", errors="strict")
    symbol_dump = read_bounded(symbol_path, 32 * 1024 * 1024, private).decode("utf-8", errors="strict")
    layout = parse_elf(elf, payload, section_dump, symbol_dump)
    require(0 < len(payload) <= 4 * 1024 * 1024 - 0x400 and len(rom) == 4096 + len(payload), "RSP ROM size is outside its four-MiB profile")
    header = bytearray(64)
    struct.pack_into(">6I", header, 0, 0x80371240, 0x0F, 0x80000400, 0, len(payload), 0)
    title = b"TENDIES64 HELLO     "
    header[0x20:0x20 + len(title)] = title
    header[0x3A] = ord("E")
    require(rom[:64] == header and rom[64:4096] == boot[64:] and rom[4096:] == payload, "RSP ROM header, open IPL3 bytes, payload, or selector composition differs")
    require(qualification["build_identity"]["profile_id"] == PROFILE_ID and qualification["assets"]["font_graphics"]["status"].startswith("excluded"), "RSP composition is not bound to its own profile and asset disposition")
    closure = json.loads((PRIVATE / qualification["closure"]["path"].split("/", 2)[-1]).read_text(encoding="utf-8"))
    report = {"schema": "t64.rsp-build-composition.v1", "schema_version": 1, "status": "PASS_POSTBUILD_COMPOSITION", "component": "rsp", "profile_id": PROFILE_ID, "source_revision": qualification["source_revision"], "qualification_sha256": digest(pathlib.Path(args.qualification).read_bytes()), "preuse_closure_sha256": qualification["closure"]["sha256"], "core_build_sha256": qualification["build_identity"]["core_build_sha256"], "elf_sha256": digest(elf), "payload_sha256": digest(payload), "rom_sha256": digest(rom), "boot_sha256": digest(boot), "selector_symbol": "T64_RSP_SELECTOR", "selector_bytes": 4, "font_graphics_asset_status": closure["assets"]["font_graphics"]["status"], "guest_rom_executed": False, "layout": layout}
    report_path = pathlib.Path(os.path.abspath(args.report))
    try:
        report_path.relative_to(private)
    except ValueError:
        raise ValueError("RSP composition report escapes its private root") from None
    require(not report_path.exists() and not report_path.is_symlink(), "refusing to overwrite a composition report")
    fd = os.open(report_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0), 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as stream:
        json.dump(report, stream, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        stream.write("\n")
    print("PASS: actual RSP ELF, payload, selector, boot bytes, and ROM composition verified; guest ROM not executed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--workspace", type=pathlib.Path)
    parser.add_argument("--elf", type=pathlib.Path)
    parser.add_argument("--payload", type=pathlib.Path)
    parser.add_argument("--rom", type=pathlib.Path)
    parser.add_argument("--boot", type=pathlib.Path)
    parser.add_argument("--qualification", type=pathlib.Path, default=PRIVATE / "qualification.json")
    parser.add_argument("--sections-log", type=pathlib.Path)
    parser.add_argument("--symbols-log", type=pathlib.Path)
    parser.add_argument("--report", type=pathlib.Path)
    args = parser.parse_args()
    try:
        if args.selftest:
            if args.workspace is None or args.inspect:
                raise ValueError("authored selftest requires only --workspace")
            selftest(args.workspace)
        elif args.inspect:
            if any(getattr(args, name) is None for name in ("elf", "payload", "rom", "boot", "sections_log", "symbols_log", "report")):
                raise ValueError("artifact inspection requires ELF, payload, ROM, boot, section/symbol logs, and report")
            inspect(args)
        else:
            raise ValueError("select --inspect or --selftest")
    except (OSError, ValueError, subprocess.SubprocessError, json.JSONDecodeError, UnicodeError, struct.error) as error:
        print(f"refused: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

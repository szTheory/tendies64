#!/usr/bin/env python3
"""Seal the pinned diagnostic's authored, pre-build Rust module closure privately."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

PINNED_SOURCE_REVISION = "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"
REQUIRED_TEST_MODULES = {
    "address_error_exception", "arithmetic", "cache", "cop0", "cop1",
    "cop_unusable", "endian_re", "exception_instructions", "jumps",
    "overflow_exception", "privilege", "soft_asserts", "testlist",
    "tlb", "tlb64", "traps",
}
ROOT_MODULES = {
    "allocator", "assembler", "cop0", "cop1", "exception_handler",
    "graphics", "isviewer", "math", "memory_map", "mi", "panic", "pi",
    "print", "rsp", "tests", "text_out", "uncached_memory",
}
EXCLUDED_MODULE_PATHS = (
    "src/emux.rs", "src/rdp/", "src/sc64/", "src/rsp/rsp.rs",
    "src/rsp/rsp_macros.rs", "src/graphics/font.rs",
    "src/graphics/system_font.rs", "src/graphics/framebuffer_",
    "src/graphics/vi.rs", "src/tests/cart_memory/", "src/tests/mi/",
    "src/tests/pif_memory/", "src/tests/rdp/", "src/tests/rdram/",
    "src/tests/rsp/", "src/tests/rsp_timing/", "src/tests/sp_memory/",
    "src/tests/startup/", "src/tests/timing/",
)
FORBIDDEN_SOURCE_TOKENS = {
    "font", "geneva", "framebuffer", "framebufferconsole", "cursor",
    "video", "render_console",
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def tokenize_rust(source: str) -> list[str]:
    """Tokenize only enough Rust syntax to resolve explicit `mod` items."""
    tokens: list[str] = []
    index = 0
    while index < len(source):
        if source.startswith("//", index):
            newline = source.find("\n", index)
            index = len(source) if newline < 0 else newline + 1
            continue
        if source.startswith("/*", index):
            depth = 1
            index += 2
            while index < len(source) and depth:
                if source.startswith("/*", index):
                    depth += 1
                    index += 2
                elif source.startswith("*/", index):
                    depth -= 1
                    index += 2
                else:
                    index += 1
            if depth:
                raise ValueError("Rust source has an unterminated block comment")
            continue

        raw_start = index
        if source.startswith("br", index):
            raw_start += 2
        elif source[index] == "r":
            raw_start += 1
        else:
            raw_start = -1
        if raw_start >= 0:
            cursor = raw_start
            while cursor < len(source) and source[cursor] == "#":
                cursor += 1
            if cursor < len(source) and source[cursor] == '"':
                hashes = "#" * (cursor - raw_start)
                terminator = '"' + hashes
                end = source.find(terminator, cursor + 1)
                if end < 0:
                    raise ValueError("Rust source has an unterminated raw string")
                tokens.append("STRING")
                index = end + len(terminator)
                continue

        if source[index] == '"':
            index += 1
            while index < len(source):
                if source[index] == "\\":
                    index += 2
                elif source[index] == '"':
                    index += 1
                    break
                else:
                    index += 1
            else:
                raise ValueError("Rust source has an unterminated string")
            tokens.append("STRING")
            continue
        if source[index] == "'":
            end = index + 1
            escaped = False
            while end < len(source) and source[end] != "\n":
                if not escaped and source[end] == "'":
                    break
                if source[end] == "\\" and not escaped:
                    escaped = True
                else:
                    escaped = False
                end += 1
            if end < len(source) and source[end] == "'":
                tokens.append("CHAR")
                index = end + 1
                continue

        if source[index].isspace():
            index += 1
            continue
        identifier = re.match(r"[A-Za-z_][A-Za-z0-9_]*", source[index:])
        if identifier:
            tokens.append(identifier.group(0))
            index += len(identifier.group(0))
        else:
            tokens.append(source[index])
            index += 1
    return tokens


def source_module_edges(
    root: pathlib.Path, relative: pathlib.PurePosixPath, is_crate_root: bool
) -> list[dict[str, object]]:
    source_path = root.joinpath(*relative.parts)
    if source_path.is_symlink() or not source_path.is_file():
        raise ValueError(f"source module is linked or missing: {relative.as_posix()}")
    tokens = tokenize_rust(source_path.read_text(encoding="utf-8"))
    for index in range(len(tokens) - 3):
        if tokens[index : index + 3] == ["#", "[", "path"]:
            raise ValueError(f"unreviewed Rust path attribute: {relative.as_posix()}")
        if tokens[index : index + 3] == ["#", "[", "cfg"]:
            raise ValueError(f"conditional source closure needs target review: {relative.as_posix()}")
    for index in range(len(tokens) - 1):
        if tokens[index] in {"include", "include_bytes", "include_str"} and tokens[index + 1] == "!":
            raise ValueError(f"unreviewed source inclusion macro: {relative.as_posix()}")

    inline_opens: dict[int, str] = {}
    external: dict[int, str] = {}
    for index in range(len(tokens) - 2):
        if tokens[index] != "mod" or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", tokens[index + 1]):
            continue
        if tokens[index + 2] == "{":
            inline_opens[index + 2] = tokens[index + 1]
        elif tokens[index + 2] == ";":
            external[index] = tokens[index + 1]

    base = source_path.parent if is_crate_root or source_path.name == "mod.rs" else source_path.parent / source_path.stem
    stack: list[str | None] = []
    edges: list[dict[str, object]] = []
    for index, token in enumerate(tokens):
        if index in external:
            name = external[index]
            inline_path = [part for part in stack if part is not None]
            module_base = base.joinpath(*inline_path)
            candidates = (module_base / f"{name}.rs", module_base / name / "mod.rs")
            available = [candidate for candidate in candidates if candidate.is_file()]
            if len(available) != 1:
                reason = "missing" if not available else "ambiguous"
                raise ValueError(f"{reason} external module {name} from {relative.as_posix()}")
            target = available[0]
            if target.is_symlink():
                raise ValueError(f"external module traverses a symlink: {target}")
            target_relative = pathlib.PurePosixPath(target.relative_to(root).as_posix())
            edges.append({
                "from": relative.as_posix(), "inline_path": inline_path,
                "name": name, "to": target_relative.as_posix(),
            })
        if token == "{":
            stack.append(inline_opens.get(index))
        elif token == "}":
            if not stack:
                raise ValueError(f"unbalanced module braces: {relative.as_posix()}")
            stack.pop()
    if stack:
        raise ValueError(f"unbalanced module braces: {relative.as_posix()}")
    return edges


def discover_module_closure(
    root: pathlib.Path, root_relative: str = "src/main.rs"
) -> tuple[dict[str, str], list[dict[str, object]]]:
    root = root.resolve(strict=True)
    start = pathlib.PurePosixPath(root_relative)
    queue = [(start, True)]
    files: dict[str, str] = {}
    edges: list[dict[str, object]] = []
    while queue:
        relative, is_crate_root = queue.pop(0)
        key = relative.as_posix()
        if key in files:
            continue
        source_path = root.joinpath(*relative.parts)
        cursor = root
        linked = False
        for part in relative.parts:
            cursor = cursor / part
            linked = linked or cursor.is_symlink()
        if linked or not source_path.is_file():
            raise ValueError(f"module path is linked or missing: {key}")
        data = source_path.read_bytes()
        files[key] = sha256(data)
        declarations = source_module_edges(root, relative, is_crate_root)
        edges.extend(declarations)
        for declaration in declarations:
            queue.append((pathlib.PurePosixPath(str(declaration["to"])), False))
    return files, edges


def verify_cpu_profile_surface(
    root: pathlib.Path, project: pathlib.Path, files: dict[str, str],
    edges: list[dict[str, object]], projection: dict[str, object],
) -> dict[str, object]:
    by_source: dict[str, set[str]] = {}
    for edge in edges:
        if not edge["inline_path"]:
            by_source.setdefault(str(edge["from"]), set()).add(str(edge["name"]))
    expected = {
        "src/main.rs": ROOT_MODULES,
        "src/graphics/mod.rs": {"color"},
        "src/rsp/mod.rs": {"dmem_writer", "rsp_assembler", "spmem"},
        "src/tests/mod.rs": REQUIRED_TEST_MODULES,
    }
    for source, names in expected.items():
        if by_source.get(source, set()) != names:
            raise ValueError(f"unexpected module declarations in {source}")

    excluded = [path for path in files if any(fragment in path for fragment in EXCLUDED_MODULE_PATHS)]
    if excluded:
        raise ValueError("excluded diagnostic modules entered the closure: " + ", ".join(excluded))
    main_path = root / "src/main.rs"
    token_hits: list[dict[str, str]] = []
    for relative in files:
        tokens = tokenize_rust((root / relative).read_text(encoding="utf-8"))
        for token in tokens:
            if token.lower() in FORBIDDEN_SOURCE_TOKENS:
                token_hits.append({"path": relative, "token": token})
    if token_hits:
        raise ValueError("presentation or protected asset token is reachable from the CPU closure")

    projection_rows = projection.get("registrations")
    if not isinstance(projection_rows, list) or len(projection_rows) != 1085:
        raise ValueError("source projection does not contain the complete registration set")
    required_rows = [
        row for row in projection_rows
        if isinstance(row, dict) and row.get("lane") == "required-base"
    ]
    if not required_rows or projection.get("empty_required_groups") != []:
        raise ValueError("source projection has an empty required CPU group")
    required_indices = [row.get("index") for row in required_rows]
    if required_indices != sorted(required_indices):
        raise ValueError("required source registration order is not canonical")
    required_values = [
        (row["index"], value["index"])
        for row in required_rows for value in row.get("values", [])
    ]
    if any(
        not isinstance(registration, int) or registration > 0xFFFF
        or not isinstance(value, int) or value > 0xFFFF
        for registration, value in required_values
    ):
        raise ValueError("a required selector index does not fit the owned selector word")

    isviewer = (root / "src/isviewer.rs").read_text(encoding="utf-8")
    chunk_match = re.search(r"const\s+CHUNK:\s*usize\s*=\s*0x([0-9a-fA-F]+)", isviewer)
    if not chunk_match or "chunks(CHUNK)" not in isviewer:
        raise ValueError("ISViewer source does not expose the reviewed bounded chunk writer")
    window_bytes = int(chunk_match.group(1), 16)
    core_header = (project / "core/t64_internal.h").read_text(encoding="utf-8")
    buffer_match = re.search(r"#define\s+T64_ISVIEWER_BUFFER_BYTES\s+(\d+)u", core_header)
    capture_match = re.search(r"#define\s+T64_ISVIEWER_CAPTURE_BYTES\s+(\d+)u", core_header)
    if not buffer_match or not capture_match:
        raise ValueError("shared ISViewer buffer or capture bound is unavailable")
    device_window = int(buffer_match.group(1))
    capture_bytes = int(capture_match.group(1))
    if window_bytes > device_window:
        raise ValueError("guest ISViewer chunk exceeds the shared device buffer window")

    # This is the exact canonical t64.cpu-run object accepted by the shared C parser;
    # only decimal selector indices vary. It is used for length bounds, not parsed here.
    transcript = (
        '{{"records":[{{"event":"start","registration_index":{},"value_index":{}}},'
        '{{"event":"outcome","status":"fail"}},'
        '{{"complete":true,"event":"done","record_count":"3"}}],'
        '"schema":"t64.cpu-run","schema_version":1}}\n'
    )
    max_transcript = max(
        len(transcript.format(registration, value).encode("ascii"))
        for registration, value in required_values
    )
    if max_transcript > window_bytes or max_transcript > capture_bytes:
        raise ValueError("canonical guest transcript exceeds the ISViewer window or capture cap")

    linker = (root / "linker.ld").read_text(encoding="utf-8")
    main_source = main_path.read_text(encoding="utf-8")
    memory_map = (root / "src/memory_map.rs").read_text(encoding="utf-8")
    if (
        ".t64_selector" not in linker or "KEEP(*(.t64_selector))" not in linker
        or "CPU_PROFILE_LOAD_ADDRESS: u32 = 0x8000_0400" not in main_source
        or "CPU_PROFILE_APP_ROM_OFFSET: u32 = 0x1000" not in main_source
        or "CPU_PROFILE_RDRAM_BYTES: usize = 4 * 1024 * 1024" not in main_source
        or "HEAP_END: usize = 3 * 1024 * 1024" not in memory_map
        or "__cpu_heap_limit = 0xFFFFFFFF80300000;" not in linker
        or "__cpu_stack_bottom = 0xFFFFFFFF80300000;" not in linker
        or "__cpu_stack_top = 0xFFFFFFFF80400000;" not in linker
    ):
        raise ValueError("authored CPU flat profile, selector, or heap/stack bounds disagree")

    return {
        "static_module_count": len(files),
        "module_edge_count": len(edges),
        "required_registration_count": len(required_rows),
        "required_value_count": len(required_values),
        "required_index_sha256": sha256(
            json.dumps(required_values, separators=(",", ":")).encode("ascii")
        ),
        "excluded_module_paths": [],
        "asset_or_presentation_source_tokens": [],
        "rdram_bytes": 4 * 1024 * 1024,
        "load_address": "0x80000400",
        "app_rom_offset": "0x1000",
        "elf_header_offset": "0x0c00",
        "heap_limit": "0x80300000",
        "stack_range": ["0x80300000", "0x80400000"],
        "isviewer": {
            "guest_chunk_bytes": window_bytes,
            "core_window_bytes": device_window,
            "capture_limit_bytes": capture_bytes,
            "max_canonical_guest_transcript_bytes": max_transcript,
            "single_window_flush": max_transcript <= window_bytes,
        },
        "actual_elf_bss_stack_scan_pending": True,
        "actual_packed_rom_composition_scan_pending": True,
    }


def load_overlay_module(project: pathlib.Path):
    script = project / "scripts/apply-cpu-diagnostic-overlay.py"
    spec = importlib.util.spec_from_file_location("t64_cpu_overlay", script)
    if spec is None or spec.loader is None:
        raise ValueError("source overlay applier cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def private_path(project: pathlib.Path, private_root: pathlib.Path, value: str, label: str) -> pathlib.Path:
    path = pathlib.Path(value)
    candidate = path if path.is_absolute() else project / path
    candidate = candidate.absolute()
    try:
        relative = candidate.relative_to(private_root)
    except ValueError:
        raise ValueError(f"{label} escapes the private diagnostic directory") from None
    cursor = private_root
    for part in relative.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            raise ValueError(f"{label} traverses a symlink")
    resolved = candidate.resolve(strict=True)
    try:
        resolved.relative_to(private_root)
    except ValueError:
        raise ValueError(f"{label} resolves outside the private diagnostic directory") from None
    return resolved


def require_private_root(project: pathlib.Path) -> pathlib.Path:
    expected = project / "build/private-cpu-diagnostic"
    cursor = project
    for part in pathlib.PurePosixPath("build/private-cpu-diagnostic").parts:
        cursor = cursor / part
        if cursor.is_symlink():
            raise ValueError("private diagnostic root traverses a symlink")
    if not expected.is_dir() or expected.resolve(strict=True) != expected:
        raise ValueError("private diagnostic root is missing or noncanonical")
    if os.stat(expected).st_mode & 0o077:
        raise ValueError("private diagnostic root is accessible to group or other users")
    return expected


def write_private_report(path: pathlib.Path, report: dict[str, object]) -> None:
    data = (json.dumps(report, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
    descriptor, temporary_name = tempfile.mkstemp(prefix=".source-closure.", dir=path.parent)
    temporary = pathlib.Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.chmod(0o600)
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def audit_source_closure(
    project: pathlib.Path, source_root: pathlib.Path, pinned_source: pathlib.Path,
    overlay_path: pathlib.Path,
    projection_path: pathlib.Path, source_audit_path: pathlib.Path,
) -> dict[str, object]:
    revision = subprocess.run(
        ["git", "-C", str(source_root), "rev-parse", "HEAD"],
        check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.strip()
    if revision != PINNED_SOURCE_REVISION:
        raise ValueError("source checkout revision differs from the pinned diagnostic")
    pinned_revision = subprocess.run(
        ["git", "-C", str(pinned_source), "rev-parse", "HEAD"],
        check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.strip()
    if pinned_revision != revision:
        raise ValueError("unmodified pinned source base differs from the applied checkout")
    if subprocess.run(
        ["git", "-C", str(pinned_source), "diff", "--quiet"], check=False
    ).returncode != 0 or subprocess.run(
        ["git", "-C", str(pinned_source), "diff", "--cached", "--quiet"], check=False
    ).returncode != 0:
        raise ValueError("pinned source base contains local edits")
    if subprocess.run(
        ["git", "-C", str(pinned_source), "ls-files", "--others", "--exclude-standard"],
        check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.splitlines():
        raise ValueError("pinned source base contains untracked source files")

    overlay_module = load_overlay_module(project)
    patch = overlay_module.checked_json(overlay_path, "authored source overlay")
    projection = overlay_module.checked_json(projection_path, "source projection")
    source_audit = overlay_module.checked_json(source_audit_path, "source audit")
    if not isinstance(patch, dict) or not isinstance(patch.get("edits"), list):
        raise ValueError("authored source overlay is malformed")
    if not isinstance(projection, dict) or not isinstance(source_audit, dict):
        raise ValueError("source projection or audit is malformed")
    if patch.get("source_revision") != revision:
        raise ValueError("overlay revision does not match the checked-out source")

    expected_changes = sorted({str(edit.get("path")) for edit in patch["edits"] if isinstance(edit, dict)})
    actual_changes = subprocess.run(
        ["git", "-C", str(source_root), "diff", "--name-only"],
        check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.splitlines()
    staged_changes = subprocess.run(
        ["git", "-C", str(source_root), "diff", "--cached", "--name-only"],
        check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.splitlines()
    untracked = subprocess.run(
        ["git", "-C", str(source_root), "ls-files", "--others", "--exclude-standard"],
        check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.splitlines()
    if actual_changes != expected_changes or staged_changes or untracked:
        raise ValueError("applied source membership differs from the reviewed overlay path set")

    expected_testlist = overlay_module.render_required_testlist(
        pinned_source, projection_path, source_audit_path
    )
    testlist = source_root / "src/tests/testlist.rs"
    if testlist.read_bytes() != expected_testlist:
        raise ValueError("generated required testlist differs from the sealed source projection")

    files, edges = discover_module_closure(source_root)
    profile = verify_cpu_profile_surface(source_root, project, files, edges, projection)
    changed_files = [
        {
            "path": relative,
            "sha256": sha256((source_root / relative).read_bytes()),
        }
        for relative in expected_changes
    ]
    projection_body = {key: value for key, value in projection.items() if key != "projection_sha256"}
    projection_canonical = (
        json.dumps(projection_body, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
    ).encode("utf-8")
    if sha256(projection_canonical) != projection.get("projection_sha256"):
        raise ValueError("source projection seal is invalid")
    return {
        "schema": "t64.cpu-source-closure",
        "schema_version": 1,
        "status": "PASS_PREBUILD_SOURCE",
        "source_revision": revision,
        "overlay_checkout": str(source_root),
        "overlay_sha256": sha256(overlay_path.read_bytes()),
        "excluded_before_compile": [
            "font", "framebuffer", "graphics", "non_cpu_rsp_startup"
        ],
        "source_projection_sha256": str(projection.get("projection_sha256")),
        "source_audit_sha256": sha256(source_audit_path.read_bytes()),
        "changed_files": changed_files,
        "module_files": [
            {"path": relative, "sha256": file_hash}
            for relative, file_hash in sorted(files.items())
        ],
        "module_edges": edges,
        "profile": profile,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--pinned-source", required=True)
    parser.add_argument("--overlay", required=True)
    parser.add_argument("--source-projection", required=True)
    parser.add_argument("--source-audit", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args(argv)
    project = pathlib.Path(__file__).resolve().parents[1]
    os.umask(0o077)
    try:
        private_root = require_private_root(project)
        source_root = private_path(project, private_root, args.source_root, "source checkout")
        pinned_source = private_path(project, private_root, args.pinned_source, "pinned source base")
        overlay = pathlib.Path(args.overlay)
        overlay = overlay if overlay.is_absolute() else project / overlay
        overlay = overlay.absolute()
        try:
            overlay.relative_to(project)
        except ValueError:
            raise ValueError("authored source overlay escapes the project") from None
        cursor = project
        for part in overlay.relative_to(project).parts:
            cursor = cursor / part
            if cursor.is_symlink():
                raise ValueError("authored source overlay traverses a symlink")
        overlay = overlay.resolve(strict=True)
        projection = private_path(project, private_root, args.source_projection, "source projection")
        audit = private_path(project, private_root, args.source_audit, "source audit")
        output = pathlib.Path(args.output)
        output = output if output.is_absolute() else project / output
        if output.is_symlink():
            raise ValueError("source closure output is a symlink")
        output_parent = private_path(project, private_root, str(output.parent), "source closure output parent")
        if not output_parent.is_dir():
            raise ValueError("source closure output parent is linked or not a directory")
        report = audit_source_closure(
            project, source_root, pinned_source, overlay, projection, audit
        )
        write_private_report(output_parent / output.name, report)
        print("PASS: pinned source closure verified; private report written")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError, json.JSONDecodeError) as error:
        print(f"source closure refused: {error}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())

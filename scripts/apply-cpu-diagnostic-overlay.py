#!/usr/bin/env python3
"""Apply a source-hash-guarded CPU overlay to a private pinned checkout."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import sys
import tempfile

PINNED_SOURCE_REVISION = "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate overlay JSON key: {key}")
        result[key] = value
    return result


def apply_ranges(source: bytes, edits: list[dict[str, object]]) -> bytes:
    """Apply exact, non-overlapping byte replacements after all guards pass."""
    spans: list[tuple[int, int, bytes]] = []
    for edit in edits:
        offset = edit.get("offset")
        length = edit.get("length")
        expected = edit.get("range_sha256")
        replacement = edit.get("replacement_utf8")
        if (
            not isinstance(offset, int) or isinstance(offset, bool) or offset < 0
            or not isinstance(length, int) or isinstance(length, bool) or length < 0
            or not isinstance(expected, str) or len(expected) != 64
            or not isinstance(replacement, str)
        ):
            raise ValueError("overlay byte range is malformed")
        end = offset + length
        original = source[offset:end]
        if end > len(source) or len(original) != length or sha256(original) != expected:
            raise ValueError("overlay byte-range SHA-256 mismatch")
        spans.append((offset, end, replacement.encode("utf-8")))

    spans.sort(key=lambda span: span[0])
    for previous, current in zip(spans, spans[1:]):
        if previous[1] > current[0]:
            raise ValueError("overlay byte ranges overlap")
    result = source
    for offset, end, replacement in reversed(spans):
        result = result[:offset] + replacement + result[end:]
    return result


def checked_source_path(root: pathlib.Path, relative: pathlib.PurePosixPath) -> pathlib.Path:
    cursor = root
    for part in relative.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            raise ValueError("overlay target traverses a symlink")
    try:
        resolved = cursor.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError):
        raise ValueError("overlay target escapes or is missing from source root") from None
    if not resolved.is_file():
        raise ValueError("overlay target is not a regular file")
    return resolved


def apply_overlay(source_root: pathlib.Path, patch_path: pathlib.Path) -> int:
    root = source_root.resolve(strict=True)
    if source_root.is_symlink() or not root.is_dir():
        raise ValueError("source root is missing, linked, or not a directory")
    patch = json.loads(
        patch_path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys
    )
    if not isinstance(patch, dict) or set(patch) != {
        "schema", "schema_version", "source_revision", "edits"
    }:
        raise ValueError("overlay root fields do not match the closed schema")
    if patch["schema"] != "t64.cpu-source-overlay" or patch["schema_version"] != 1:
        raise ValueError("unsupported overlay schema")
    if patch["source_revision"] != PINNED_SOURCE_REVISION:
        raise ValueError("overlay does not target the pinned source revision")
    edits = patch["edits"]
    if not isinstance(edits, list) or not edits:
        raise ValueError("overlay edits are missing")

    pending: dict[
        pathlib.PurePosixPath,
        tuple[pathlib.Path, bytes, list[dict[str, object]], str],
    ] = {}
    for edit in edits:
        if not isinstance(edit, dict) or set(edit) != {
            "path", "file_sha256", "offset", "length", "range_sha256", "replacement_utf8"
        }:
            raise ValueError("overlay edit is malformed")
        path_text = edit["path"]
        file_sha = edit["file_sha256"]
        relative = pathlib.PurePosixPath(path_text) if isinstance(path_text, str) else pathlib.PurePosixPath("")
        if relative.is_absolute() or not relative.parts or any(
            part in ("", ".", "..") for part in relative.parts
        ):
            raise ValueError("overlay path escapes source root")
        if not isinstance(file_sha, str) or len(file_sha) != 64:
            raise ValueError("overlay source-file SHA-256 is malformed")
        path = checked_source_path(root, relative)
        data = path.read_bytes()
        if sha256(data) != file_sha:
            raise ValueError(f"source file SHA-256 mismatch: {relative.as_posix()}")
        if relative in pending:
            saved_path, original, ranges, saved_sha = pending[relative]
            if saved_sha != file_sha or saved_path != path or original != data:
                raise ValueError("overlay repeats a source path with inconsistent bytes")
            ranges.append(edit)
        else:
            pending[relative] = (path, data, [edit], file_sha)

    outputs: list[tuple[pathlib.Path, bytes, int]] = []
    for path, source, ranges, _ in pending.values():
        outputs.append((path, apply_ranges(source, ranges), path.stat().st_mode & 0o777))

    for path, data, mode in outputs:
        descriptor, temporary_name = tempfile.mkstemp(prefix=".cpu-overlay.", dir=path.parent)
        temporary_path = pathlib.Path(temporary_name)
        try:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            temporary_path.chmod(mode)
            os.replace(temporary_path, path)
        except BaseException:
            try:
                temporary_path.unlink()
            except OSError:
                pass
            raise
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", nargs=2, metavar=("SOURCE_ROOT", "PATCH"))
    args = parser.parse_args(argv)
    try:
        if args.apply is None:
            parser.error("--apply SOURCE_ROOT PATCH is required")
        return apply_overlay(pathlib.Path(args.apply[0]), pathlib.Path(args.apply[1]))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"overlay refused: {error}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())

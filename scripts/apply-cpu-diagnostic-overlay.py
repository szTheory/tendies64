#!/usr/bin/env python3
"""Apply a source-hash-guarded CPU overlay to a private pinned checkout."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import sys
import tempfile
from collections.abc import Callable

PINNED_SOURCE_REVISION = "9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"
SOURCE_PROJECTION_SHA256 = "2382f2b434b4fbda80e73e26ad9ca226eb88a221817fae2e6eba35002d8469d0"
REQUIRED_LEVELS = {"BasicFunctionality", "RarelyUsed", "Weird"}


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


def safe_relative_path(text: object, label: str) -> pathlib.PurePosixPath:
    if not isinstance(text, str) or not text or "\\" in text or "\x00" in text:
        raise ValueError(f"{label} path is malformed")
    relative = pathlib.PurePosixPath(text)
    if (
        relative.is_absolute()
        or not relative.parts
        or any(part in ("", ".", "..", ".git") for part in relative.parts)
    ):
        raise ValueError(f"{label} path escapes its permitted directory or names .git")
    return relative


def checked_json(path: pathlib.Path, label: str) -> object:
    if path.is_symlink() or path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError(f"{label} is linked or exceeds its size limit")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys)


def render_registration_expression(
    source_lines: list[str], source_anchor: int, constructor: str
) -> str:
    """Recognize only the pinned list's empty-braced and unit test constructors."""
    if (
        not isinstance(source_anchor, int)
        or isinstance(source_anchor, bool)
        or source_anchor < 0
        or source_anchor >= len(source_lines)
        or not isinstance(constructor, str)
        or not constructor
    ):
        raise ValueError("registration source anchor or constructor is malformed")
    expression = re.compile(
        r"^\s*Box::new\s*\(\s*"
        + re.escape(constructor)
        + r"\s*(?P<fields>\{\s*\})?\s*,?\s*\)\s*,?\s*$",
        re.DOTALL,
    )
    for end in range(source_anchor + 1, min(source_anchor + 8, len(source_lines)) + 1):
        match = expression.fullmatch("\n".join(source_lines[source_anchor:end]))
        if match is not None:
            fields = " {}" if match.group("fields") is not None else ""
            return f"Box::new({constructor}{fields})"
    raise ValueError("registration constructor does not match its pinned source expression")


def render_required_testlist(
    source_root: pathlib.Path, projection_path: pathlib.Path, source_audit_path: pathlib.Path
) -> bytes:
    projection = checked_json(projection_path, "source projection")
    audit = checked_json(source_audit_path, "source audit")
    if not isinstance(projection, dict) or not isinstance(audit, dict):
        raise ValueError("source projection or source audit is malformed")
    projection_digest = projection.get("projection_sha256")
    body = {key: value for key, value in projection.items() if key != "projection_sha256"}
    canonical = (
        json.dumps(body, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
    ).encode("utf-8")
    if (
        projection.get("schema") != "t64.cpu-source-projection"
        or projection.get("schema_version") != 2
        or projection.get("source_revision") != PINNED_SOURCE_REVISION
        or projection_digest != SOURCE_PROJECTION_SHA256
        or sha256(canonical) != SOURCE_PROJECTION_SHA256
        or audit.get("source_revision") != PINNED_SOURCE_REVISION
        or audit.get("unclassified_or_missing") != []
        or audit.get("quick_default_registrations") != 1085
    ):
        raise ValueError("source projection or source audit differs from the reviewed seal")

    source_list = source_root / "src/tests/testlist.rs"
    source_bytes = source_list.read_bytes()
    expected_list_sha = projection.get("testlist_sha256")
    if sha256(source_bytes) != expected_list_sha or audit.get("testlist_sha256") != expected_list_sha:
        raise ValueError("pinned source registration list differs from the sealed projection")
    rows = projection.get("registrations")
    audit_rows = audit.get("registrations")
    if not isinstance(rows, list) or not isinstance(audit_rows, list) or len(rows) != 1085 or len(audit_rows) != 1085:
        raise ValueError("source projection does not preserve all 1085 registrations")
    source_lines = source_bytes.decode("utf-8").splitlines()
    output = [
        "// Generated privately from the sealed source projection; do not publish this derived file.\n",
        "use alloc::boxed::Box;\nuse alloc::vec;\nuse alloc::vec::Vec;\n",
        "use super::Test;\n\n",
        "pub fn tests() -> Vec<(usize, Box<dyn Test>)> {\n    vec![\n",
    ]
    expected_required = [
        index for index, row in enumerate(rows)
        if isinstance(row, dict) and row.get("lane") == "required-base"
    ]
    generated_required: list[int] = []
    for index, (row, audited) in enumerate(zip(rows, audit_rows, strict=True)):
        if not isinstance(row, dict) or not isinstance(audited, dict):
            raise ValueError("source registration row is malformed")
        if (
            row.get("index") != index
            or audited.get("index") != index
            or row.get("constructor") != audited.get("path")
            or row.get("source_name") != audited.get("upstream_name")
            or row.get("level") != audited.get("level")
            or row.get("source_value_count") != audited.get("source_value_count")
            or row.get("effective_value_count") != audited.get("effective_value_count")
            or row.get("source_file_sha256") != audited.get("source_sha256")
            or row.get("values_method_sha256") != audited.get("values_method_sha256")
        ):
            raise ValueError(f"source projection and audit differ at registration {index}")
        relative_source = pathlib.PurePosixPath(str(row.get("source_file", "")))
        if (
            relative_source.is_absolute()
            or not relative_source.parts
            or ".." in relative_source.parts
        ):
            raise ValueError(f"registration {index} has an invalid source implementation path")
        implementation = source_root.joinpath(*relative_source.parts)
        if implementation.is_symlink() or not implementation.is_file() or sha256(implementation.read_bytes()) != row["source_file_sha256"]:
            raise ValueError(f"registration {index} implementation bytes differ from the projection")
        if row.get("lane") != "required-base":
            continue
        if row.get("group") == "non-cpu" or row.get("level") not in REQUIRED_LEVELS:
            raise ValueError(f"non-base registration entered the required CPU list at {index}")
        constructor = row.get("constructor")
        source_anchor = audited.get("line")
        if not isinstance(constructor, str) or not isinstance(source_anchor, int) or source_anchor < 0:
            raise ValueError(f"registration {index} has no source constructor anchor")
        try:
            rendered_constructor = render_registration_expression(
                source_lines, source_anchor, constructor
            )
        except ValueError:
            raise ValueError(f"registration {index} constructor does not match its pinned source expression")
        output.append(
            f"        ({index}usize, {rendered_constructor} as Box<dyn Test>),\n"
        )
        generated_required.append(index)
    if (
        not expected_required
        or generated_required != expected_required
        or projection.get("empty_required_groups") != []
    ):
        raise ValueError("source projection has no complete required CPU set")
    output.append("    ]\n}\n")
    return "".join(output).encode("utf-8")


def plan_overlay_outputs(
    source_root: pathlib.Path,
    patch_path: pathlib.Path,
    projection_path: pathlib.Path | None = None,
    source_audit_path: pathlib.Path | None = None,
) -> list[tuple[pathlib.Path, bytes, bytes, int]]:
    """Derive the exact guarded output bytes without writing the checkout."""
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
    if (
        patch["schema"] != "t64.cpu-source-overlay"
        or type(patch["schema_version"]) is not int
        or patch["schema_version"] != 1
    ):
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
        range_keys = {"path", "file_sha256", "offset", "length", "range_sha256"}
        replacement_key_sets = (
            range_keys | {"replacement_utf8"},
            range_keys | {"replacement_file", "replacement_sha256"},
            range_keys | {"generator"},
        )
        if not isinstance(edit, dict) or set(edit) not in replacement_key_sets:
            raise ValueError("overlay edit is malformed")
        path_text = edit["path"]
        file_sha = edit["file_sha256"]
        relative = safe_relative_path(path_text, "overlay target")
        if not isinstance(file_sha, str) or len(file_sha) != 64:
            raise ValueError("overlay source-file SHA-256 is malformed")
        path = checked_source_path(root, relative)
        data = path.read_bytes()
        if sha256(data) != file_sha:
            raise ValueError(f"source file SHA-256 mismatch: {relative.as_posix()}")
        if "replacement_utf8" in edit:
            replacement = edit["replacement_utf8"]
        elif "replacement_file" in edit:
            replacement_relative = safe_relative_path(
                edit["replacement_file"], "overlay replacement"
            )
            replacement_path = checked_source_path(patch_path.parent.resolve(strict=True), replacement_relative)
            replacement_bytes = replacement_path.read_bytes()
            replacement_sha = edit["replacement_sha256"]
            if (
                not isinstance(replacement_sha, str)
                or not re.fullmatch(r"[0-9a-f]{64}", replacement_sha)
                or sha256(replacement_bytes) != replacement_sha
            ):
                raise ValueError("authored overlay replacement SHA-256 mismatch")
            try:
                replacement = replacement_bytes.decode("utf-8")
            except UnicodeDecodeError:
                raise ValueError("authored replacement is not UTF-8") from None
        else:
            if edit["generator"] != "required-testlist-v1":
                raise ValueError("overlay replacement generator is unsupported")
            if projection_path is None or source_audit_path is None:
                raise ValueError("required-testlist generator inputs are missing")
            replacement = render_required_testlist(root, projection_path, source_audit_path).decode("utf-8")
        normalized_edit = dict(edit)
        normalized_edit["replacement_utf8"] = replacement
        if relative in pending:
            saved_path, original, ranges, saved_sha = pending[relative]
            if saved_sha != file_sha or saved_path != path or original != data:
                raise ValueError("overlay repeats a source path with inconsistent bytes")
            ranges.append(normalized_edit)
        else:
            pending[relative] = (path, data, [normalized_edit], file_sha)

    outputs: list[tuple[pathlib.Path, bytes, bytes, int]] = []
    for path, source, ranges, _ in pending.values():
        outputs.append(
            (path, apply_ranges(source, ranges), source, path.stat().st_mode & 0o777)
        )
    return outputs


def apply_overlay(
    source_root: pathlib.Path,
    patch_path: pathlib.Path,
    projection_path: pathlib.Path | None = None,
    source_audit_path: pathlib.Path | None = None,
    replace_operation: Callable[[pathlib.Path, pathlib.Path], None] = os.replace,
) -> int:
    outputs = plan_overlay_outputs(
        source_root, patch_path, projection_path, source_audit_path
    )

    staged: list[tuple[pathlib.Path, pathlib.Path, bytes, int]] = []
    replaced: list[tuple[pathlib.Path, bytes, int]] = []
    try:
        for path, data, original, mode in outputs:
            descriptor, temporary_name = tempfile.mkstemp(
                prefix=".cpu-overlay.", dir=path.parent
            )
            temporary_path = pathlib.Path(temporary_name)
            staged.append((path, temporary_path, original, mode))
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            temporary_path.chmod(mode)
        for path, temporary_path, original, mode in staged:
            replace_operation(temporary_path, path)
            replaced.append((path, original, mode))
    except OSError as error:
        rollback_errors: list[str] = []
        for path, original, mode in reversed(replaced):
            backup_path: pathlib.Path | None = None
            recovery_bytes_ready = False
            recovery_mode_ready = False
            preserve_recovery = False
            try:
                descriptor, backup_name = tempfile.mkstemp(
                    prefix=".cpu-overlay-recovery.", dir=path.parent
                )
                backup_path = pathlib.Path(backup_name)
                with os.fdopen(descriptor, "wb") as stream:
                    stream.write(original)
                    stream.flush()
                    os.fsync(stream.fileno())
                recovery_bytes_ready = True
                backup_path.chmod(mode)
                recovery_mode_ready = True
                replace_operation(backup_path, path)
            except OSError as rollback_error:
                if recovery_bytes_ready and backup_path is not None and backup_path.exists():
                    preserve_recovery = True
                    if recovery_mode_ready:
                        rollback_errors.append(
                            f"{path}: {rollback_error}; original recovery copy retained at {backup_path}"
                        )
                    else:
                        rollback_errors.append(
                            f"{path}: recovery-copy mode could not be restored ({rollback_error}); "
                            f"original bytes retained at {backup_path} with mode unverified; "
                            "target remains unrestored"
                        )
                else:
                    rollback_errors.append(f"{path}: {rollback_error}")
            finally:
                if (
                    backup_path is not None
                    and backup_path.exists()
                    and not preserve_recovery
                ):
                    backup_path.unlink()
        if rollback_errors:
            raise ValueError(
                "overlay write failed and rollback was incomplete: "
                + "; ".join(rollback_errors)
            ) from error
        raise ValueError("overlay write failed; already-published edits were restored") from error
    finally:
        for _, temporary_path, _, _ in staged:
            if temporary_path.exists():
                temporary_path.unlink()
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", nargs=2, metavar=("SOURCE_ROOT", "PATCH"))
    parser.add_argument("--source-projection")
    parser.add_argument("--source-audit")
    args = parser.parse_args(argv)
    try:
        if args.apply is None:
            parser.error("--apply SOURCE_ROOT PATCH is required")
        return apply_overlay(
            pathlib.Path(args.apply[0]),
            pathlib.Path(args.apply[1]),
            pathlib.Path(args.source_projection) if args.source_projection else None,
            pathlib.Path(args.source_audit) if args.source_audit else None,
        )
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"overlay refused: {error}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())

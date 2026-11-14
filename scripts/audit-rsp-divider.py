#!/usr/bin/env python3
"""Audit the bounded, pinned ares divider algorithm inputs.

--check reads only saved local acquisition/history receipts and saved candidate
bytes. It never downloads code or invokes ares. --selftest uses authored,
synthetic inputs and has no dependency on developer research storage.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import shutil
import stat
import sys
import tempfile
from datetime import date
from urllib.parse import parse_qs, urlparse

ROOT = pathlib.Path(__file__).resolve().parents[1]
PIN = "0b928930af4cc59ed99e942996f0d3c815379388"
REPOSITORY = "https://github.com/ares-emulator/ares"
CANDIDATE = ROOT / "build/research" / f"ares-rsp-{PIN}"
HISTORY_SHA256 = "658a047d0a6648f6f1e9d2a125fad52029a9c1a1f7db62fcca15331b9fd8b465"
NOTICE_TEXT = (
    "----------------------------------------------------------------------\n"
    "ares\n\n"
    "Copyright (c) 2004-2025 ares team, Near et al\n\n"
    "Permission to use, copy, modify, and/or distribute this software for any\n"
    "purpose with or without fee is hereby granted, provided that the above\n"
    "copyright notice and this permission notice appear in all copies.\n\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n"
    "WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF\n"
    "MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR\n"
    "ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES\n"
    "WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN\n"
    "ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF\n"
    "OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n"
)
PINNED = {
    "revision": PIN,
    "repository": REPOSITORY,
    "history_api_root": "https://api.github.com/repos/ares-emulator/ares/commits",
    "notice_sha256": "808eca8b48abaea476248d13d35c951f85137b210277c9c78f26919720d39593",
    "notice_text": NOTICE_TEXT,
    "history_sha256": HISTORY_SHA256,
    "history": {
        "ares/n64/rsp/rsp.cpp": {
            "count": 44,
            "latest": "6fcaa213fbe1aab501423a61903c3299f7658805",
            "oldest": "dc1eef5c7a6a4d99d251b91958f46cbabcc34688",
            "bytes": 4146,
            "sha256": "d475d3f891710750123fbde27f4064913b3d9f5e7f8a0d1ca8fc10854f489390",
            "required": (
                "reciprocals[0] = u16(~0);",
                "u64 b = (u64(1) << 34) / a;",
                "inverseSquareRoots[index] = u16(b >> 1);",
            ),
        },
        "ares/n64/rsp/interpreter-vpu.cpp": {
            "count": 22,
            "latest": "a4ac042f27eb3c7ee5165dd25ff57a12e7140841",
            "oldest": "25771ba202a62e114c417b4a0f1a52ee3a138b97",
            "bytes": 44930,
            "sha256": "563e7d8a65fe2e4fc2312ee9e439ba0507ac92608f65af08bb4fa0bcc2ba82cf",
            "required": (
                "auto RSP::VRCP(r128& vd, u8 de, cr128& vt)",
                "auto RSP::VRCPH(r128& vd, u8 de, cr128& vt)",
                "auto RSP::VRSQ(r128& vd, u8 de, cr128& vt)",
                "auto RSP::VRSQH(r128& vd, u8 de, cr128& vt)",
            ),
        },
    },
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_json(path: pathlib.Path) -> dict:
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"receipt {path.name} is missing or not a regular file")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read receipt {path.name}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"receipt {path.name} must contain a JSON object")
    return value


def regular_bytes(directory: pathlib.Path, name: str) -> bytes:
    path = directory / name
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"saved candidate {name} is missing or not a regular file")
    return path.read_bytes()


def verify_history(history: dict, expected: dict) -> list[str]:
    errors: list[str] = []
    if history.get("schema") != "t64.rsp-divider-upstream-history":
        errors.append("history schema mismatch")
    if history.get("schema_version") != 1:
        errors.append("history schema version mismatch")
    if history.get("repository") != expected["repository"]:
        errors.append("history repository mismatch")
    if history.get("as_of_revision") != expected["revision"]:
        errors.append("history is not bounded at the pinned revision")

    revision = history.get("revision_commit")
    if not isinstance(revision, dict) or revision.get("sha") != expected["revision"]:
        errors.append("pinned revision commit record is missing")
    else:
        changed_paths = revision.get("changed_paths")
        if not isinstance(changed_paths, list) or any(not isinstance(path, str) for path in changed_paths):
            errors.append("pinned revision changed-path list is malformed")
        elif set(changed_paths) & set(expected["history"]):
            errors.append("pinned revision unexpectedly changes a selected source file")

    rows = history.get("selected_files")
    if not isinstance(rows, list):
        return errors + ["selected-file history list is missing"]
    by_path = {row.get("path"): row for row in rows if isinstance(row, dict)}
    if len(by_path) != len(rows) or set(by_path) != set(expected["history"]):
        errors.append("selected-file history path set differs from the approved scope")
        return errors

    revision_date = date.fromisoformat(revision["date"][:10]) if isinstance(revision, dict) and revision.get("date") else None
    for path, rule in expected["history"].items():
        commits = by_path[path].get("commits")
        if not isinstance(commits, list) or len(commits) != rule["count"]:
            errors.append(f"history count mismatch for {path}")
            continue
        shas = [item.get("sha") for item in commits if isinstance(item, dict)]
        if len(shas) != len(commits) or len(set(shas)) != len(shas):
            errors.append(f"history entries are malformed or duplicated for {path}")
            continue
        if not shas or shas[0] != rule["latest"] or shas[-1] != rule["oldest"]:
            errors.append(f"history endpoints differ for {path}")
        for item in commits:
            if not re.fullmatch(r"[0-9a-f]{40}", str(item.get("sha", ""))):
                errors.append(f"invalid commit id in history for {path}")
                break
            try:
                commit_date = date.fromisoformat(item["date"][:10])
            except (KeyError, TypeError, ValueError):
                errors.append(f"invalid commit date in history for {path}")
                break
            if revision_date is None or commit_date > revision_date:
                errors.append(f"history extends beyond pinned revision for {path}")
                break
            if not isinstance(item.get("subject"), str) or not item["subject"].strip():
                errors.append(f"missing commit subject in history for {path}")
                break
        url = by_path[path].get("history_url", "")
        parsed = urlparse(url)
        query = parse_qs(parsed.query)
        if (
            parsed.scheme != "https"
            or parsed.netloc != "api.github.com"
            or parsed.path != urlparse(expected["history_api_root"]).path
            or query.get("path") != [path]
            or query.get("sha") != [expected["revision"]]
            or query.get("per_page") != ["100"]
        ):
            errors.append(f"history query is not path- and revision-bounded for {path}")
    return errors


def verify_candidate(directory: pathlib.Path, expected: dict = PINNED) -> list[str]:
    """Inspect actual saved source, notice, acquisition, and history bytes."""
    errors: list[str] = []
    if directory.is_symlink() or not directory.is_dir():
        return ["candidate directory is missing or linked"]
    try:
        acquisition = read_json(directory / "acquisition.json")
        if acquisition.get("schema") != "t64.research-acquisition":
            errors.append("acquisition schema mismatch")
        if acquisition.get("repository") != expected["repository"]:
            errors.append("acquisition repository mismatch")
        if acquisition.get("revision") != expected["revision"]:
            errors.append("acquisition revision mismatch")
        license_scope = acquisition.get("license_scope", "")
        if not isinstance(license_scope, str) or "ISC" not in license_scope:
            errors.append("acquisition does not record the root ISC license scope")
        expected_acquisition_files = {
            pathlib.PurePosixPath(path).name for path in expected["history"]
        } | {"ISC-NOTICE.txt"}
        acquisition_files = acquisition.get("files")
        if not isinstance(acquisition_files, dict) or set(acquisition_files) != expected_acquisition_files:
            errors.append("acquisition file set differs from the selected source and notice scope")

        for path, rule in expected["history"].items():
            source_name = pathlib.PurePosixPath(path).name
            try:
                content = regular_bytes(directory, source_name)
            except (OSError, ValueError) as error:
                errors.append(str(error))
                continue
            if len(content) != rule["bytes"] or sha256(content) != rule["sha256"]:
                errors.append(f"saved source bytes differ for {path}")
                continue
            text = content.decode("utf-8", errors="strict")
            if any(snippet not in text for snippet in rule["required"]):
                errors.append(f"selected divider equations are missing from {path}")
            recorded = acquisition_files.get(source_name) if isinstance(acquisition_files, dict) else None
            if (
                not isinstance(recorded, dict)
                or recorded.get("sha256") != rule["sha256"]
                or recorded.get("bytes") != rule["bytes"]
            ):
                errors.append(f"acquisition source receipt differs for {path}")

        notice = regular_bytes(directory, "ISC-NOTICE.txt")
        if (
            sha256(notice) != expected["notice_sha256"]
            or notice.decode("utf-8") != expected.get("notice_text", NOTICE_TEXT)
        ):
            errors.append("saved root ISC notice differs from the accepted excerpt")
        notice_receipt = acquisition_files.get("ISC-NOTICE.txt") if isinstance(acquisition_files, dict) else None
        if (
            not isinstance(notice_receipt, dict)
            or notice_receipt.get("sha256") != expected["notice_sha256"]
            or notice_receipt.get("bytes") != len(notice)
        ):
            errors.append("acquisition notice receipt differs from saved notice bytes")

        history_bytes = regular_bytes(directory, "selected-file-history.json")
        if sha256(history_bytes) != expected["history_sha256"]:
            errors.append("saved selected-file history bytes differ from the accepted receipt")
        else:
            try:
                history = json.loads(history_bytes.decode("utf-8"))
                errors.extend(verify_history(history, expected))
            except (UnicodeError, json.JSONDecodeError) as error:
                errors.append(f"selected-file history is malformed: {error}")
    except (OSError, ValueError) as error:
        errors.append(str(error))
    return errors


def make_synthetic_fixture(directory: pathlib.Path) -> dict:
    """Create deliberately synthetic, non-upstream inputs for controls."""
    files = {
        "rsp.cpp": b"authored synthetic reciprocal generator\n",
        "interpreter-vpu.cpp": b"authored synthetic divider latch formulas\n",
    }
    notice = b"authored synthetic license notice\n"
    expected = {
        "revision": "a" * 40,
        "repository": "https://example.invalid/authored",
        "history_api_root": "https://api.github.com/repos/authored/commits",
        "notice_sha256": sha256(notice),
        "notice_text": notice.decode("utf-8"),
        "history_sha256": "",
        "history": {},
    }
    for name, content in files.items():
        (directory / name).write_bytes(content)
        expected["history"][f"synthetic/{name}"] = {
            "count": 1,
            "latest": "b" * 40,
            "oldest": "b" * 40,
            "bytes": len(content),
            "sha256": sha256(content),
            "required": (content.decode().strip(),),
        }
    (directory / "ISC-NOTICE.txt").write_bytes(notice)
    acquisition = {
        "schema": "t64.research-acquisition",
        "repository": expected["repository"],
        "revision": expected["revision"],
        "license_scope": "synthetic ISC test marker",
        "files": {
            name: {"sha256": sha256(content), "bytes": len(content)}
            for name, content in files.items()
        },
        "files_notice": {"sha256": sha256(notice), "bytes": len(notice)},
    }
    acquisition["files"]["ISC-NOTICE.txt"] = acquisition.pop("files_notice")
    (directory / "acquisition.json").write_text(json.dumps(acquisition), encoding="utf-8")
    history = {
        "schema": "t64.rsp-divider-upstream-history",
        "schema_version": 1,
        "repository": expected["repository"],
        "as_of_revision": expected["revision"],
        "revision_commit": {
            "sha": expected["revision"],
            "date": "2026-11-01T00:00:00Z",
            "changed_paths": [],
        },
        "selected_files": [
            {
                "path": path,
                "history_url": (
                    f"https://api.github.com/repos/authored/commits?path={path}"
                    f"&sha={expected['revision']}&per_page=100"
                ),
                "commits": [{"sha": "b" * 40, "date": "2026-10-01T00:00:00Z", "subject": "authored fixture"}],
            }
            for path in expected["history"]
        ],
    }
    history_path = directory / "selected-file-history.json"
    history_path.write_text(json.dumps(history, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    expected["history_sha256"] = sha256(history_path.read_bytes())
    return expected


def selftest() -> list[str]:
    """Run controls entirely against authored temporary fixture bytes."""
    failures: list[str] = []
    temp_root = ROOT / "build/test-rsp-divider-audit"
    temp_root.mkdir(mode=0o700, parents=True, exist_ok=True)
    if temp_root.is_symlink() or not temp_root.is_dir():
        return ["synthetic audit workspace is linked or not a directory"]
    temp_root.chmod(0o700)
    if stat.S_IMODE(temp_root.stat().st_mode) != 0o700:
        return ["synthetic audit workspace is not owner-only"]
    with tempfile.TemporaryDirectory(prefix="run.", dir=temp_root) as temporary:
        base = pathlib.Path(temporary)
        good = base / "good"
        good.mkdir()
        contract = make_synthetic_fixture(good)
        if verify_candidate(good, contract):
            failures.append("authored synthetic accepted baseline failed")

        cases = ("changed-source", "missing-source", "foreign-license", "changed-history", "changed-notice")
        for case in cases:
            fixture = base / case
            shutil.copytree(good, fixture)
            if case == "changed-source":
                (fixture / "rsp.cpp").write_bytes(b"changed source\n")
            elif case == "missing-source":
                (fixture / "rsp.cpp").unlink()
            elif case == "foreign-license":
                receipt = json.loads((fixture / "acquisition.json").read_text(encoding="utf-8"))
                receipt["license_scope"] = "GPL-3.0"
                (fixture / "acquisition.json").write_text(json.dumps(receipt), encoding="utf-8")
            elif case == "changed-history":
                receipt = json.loads((fixture / "selected-file-history.json").read_text(encoding="utf-8"))
                receipt["selected_files"][0]["path"] = "thirdparty/copied.cpp"
                history_path = fixture / "selected-file-history.json"
                history_path.write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n", encoding="utf-8")
                contract["history_sha256"] = sha256(history_path.read_bytes())
            elif case == "changed-notice":
                (fixture / "ISC-NOTICE.txt").write_bytes(b"other notice\n")
            if not verify_candidate(fixture, contract):
                failures.append(f"authored synthetic negative control accepted: {case}")
            if case == "changed-history":
                contract["history_sha256"] = sha256((good / "selected-file-history.json").read_bytes())
    return failures


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="audit saved pinned candidate receipts")
    mode.add_argument("--selftest", action="store_true", help="run authored synthetic negative controls")
    args = parser.parse_args(argv)
    if args.selftest:
        failures = selftest()
        if failures:
            for failure in failures:
                print(f"FAIL: {failure}")
            return 1
        print("PASS: authored synthetic divider source, notice, and history controls")
        return 0

    failures = verify_candidate(CANDIDATE)
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print("PASS: saved ares divider source bytes, ISC notice, and pinned file history")
    print("Scope: integer table generators; VRCP/VRCPH and VRSQ/VRSQH normalization/latch equations")
    print("Boundary: permitted software algorithm input only; not physical ROM or hardware validation")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

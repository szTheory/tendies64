#!/usr/bin/env python3
"""Build, validate and run one exact registered RDP/VI case suite."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "docs/spec/rdp-vi-cases.json"


def fail(message: str) -> int:
    print(f"RDP/VI case inventory error: {message}", file=sys.stderr)
    return 1


def unique_nonempty(values: object, label: str) -> list[str]:
    if not isinstance(values, list) or not values or any(
        not isinstance(value, str) or not value for value in values
    ):
        raise ValueError(f"{label} must be a nonempty string list")
    result = list(values)
    if len(result) != len(set(result)):
        raise ValueError(f"{label} contains duplicate entries")
    return result


def run(command: list[str], *, cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    print("+ " + " ".join(command), flush=True)
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True)


def output(result: subprocess.CompletedProcess[str]) -> None:
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, help="CMake build directory")
    parser.add_argument("--suite", required=True, help="registered suite name")
    parser.add_argument("--run", action="store_true", help="run this suite")
    args = parser.parse_args()

    build_dir = Path(args.build)
    if not build_dir.is_absolute():
        build_dir = ROOT / build_dir
    build_dir = build_dir.resolve()
    if not build_dir.is_dir():
        return fail(f"build directory does not exist: {build_dir}")

    try:
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        if inventory.get("schema") != "t64.rdp-vi-case-inventory" or inventory.get(
            "schema_version"
        ) != 1:
            raise ValueError("unsupported inventory schema")
        suites = inventory.get("suites")
        suite = suites.get(args.suite) if isinstance(suites, dict) else None
        if not isinstance(suite, dict):
            raise ValueError(f"unknown suite: {args.suite}")
        expected_ctest = unique_nonempty(suite.get("ctest"), "suite CTest names")
        binaries = suite.get("binaries")
        if not isinstance(binaries, list) or not binaries:
            raise ValueError("suite binaries must be a nonempty list")
        parsed_binaries: list[tuple[Path, list[str]]] = []
        paths: set[str] = set()
        for entry in binaries:
            if not isinstance(entry, dict) or not isinstance(entry.get("path"), str):
                raise ValueError("each binary entry needs a relative path")
            relative = entry["path"]
            if relative in paths or Path(relative).is_absolute() or ".." in Path(relative).parts:
                raise ValueError(f"invalid or duplicate binary path: {relative}")
            paths.add(relative)
            parsed_binaries.append(
                (build_dir / relative, unique_nonempty(entry.get("cases"), f"{relative} cases"))
            )
    except (OSError, json.JSONDecodeError, ValueError, AttributeError) as error:
        return fail(str(error))

    build = run(["cmake", "--build", str(build_dir), "--parallel", "1"])
    output(build)
    if build.returncode != 0:
        return fail(f"build failed with exit status {build.returncode}")

    temporary = build_dir / "Testing/Temporary"
    last_test = temporary / "LastTest.log"
    before_discovery = build_dir / "07-01-before-case-discovery.log"
    if last_test.is_file():
        shutil.copy2(last_test, before_discovery)
    else:
        before_discovery.write_text("LastTest.log absent before discovery\n", encoding="utf-8")

    discovery = run(["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"])
    if discovery.returncode != 0:
        output(discovery)
        return fail(f"CTest discovery failed with exit status {discovery.returncode}")
    try:
        discovered = json.loads(discovery.stdout)
        names = [test["name"] for test in discovered.get("tests", [])]
        if len(names) != len(set(names)):
            raise ValueError("CTest discovery contains duplicate names")
        missing = sorted(set(expected_ctest) - set(names))
        if missing:
            raise ValueError("registered CTest names are missing: " + ", ".join(missing))
    except (json.JSONDecodeError, KeyError, TypeError, ValueError) as error:
        return fail(str(error))

    for binary, expected_cases in parsed_binaries:
        if not binary.is_file():
            return fail(f"case-list binary is missing after build: {binary}")
        listed = run([str(binary), "--list-cases"])
        output(listed)
        if listed.returncode != 0:
            return fail(f"{binary.relative_to(build_dir)} --list-cases failed")
        actual_cases = listed.stdout.splitlines()
        if not actual_cases or len(actual_cases) != len(set(actual_cases)):
            return fail(f"{binary.relative_to(build_dir)} case list is empty or duplicated")
        if actual_cases != expected_cases:
            return fail(
                f"{binary.relative_to(build_dir)} cases differ from inventory: "
                f"expected {expected_cases!r}, got {actual_cases!r}"
            )

    if not args.run:
        print(f"validated nonempty {args.suite} case inventory")
        return 0

    before_run = build_dir / f"07-01-before-{args.suite}-suite.log"
    if last_test.is_file():
        shutil.copy2(last_test, before_run)
    else:
        before_run.write_text("LastTest.log absent before suite run\n", encoding="utf-8")
    regex = "^(" + "|".join(expected_ctest) + ")$"
    command = [
        "ctest",
        "--test-dir",
        str(build_dir),
        "--no-tests=error",
        "--output-on-failure",
        "-R",
        regex,
    ]
    command_path = build_dir / f"07-01-{args.suite}-command.json"
    command_path.write_text(json.dumps(command, indent=2) + "\n", encoding="utf-8")
    result = run(command)
    # Archive the owning output immediately after CTest returns, including failure.
    archived_log = build_dir / f"07-01-{args.suite}-LastTest.log"
    if last_test.is_file():
        shutil.copy2(last_test, archived_log)
    else:
        archived_log.write_text("LastTest.log missing after suite run\n", encoding="utf-8")
    (build_dir / f"07-01-{args.suite}-exit-status.txt").write_text(
        f"{result.returncode}\n", encoding="utf-8"
    )
    output(result)
    if result.returncode != 0:
        return fail(f"{args.suite} suite failed with exit status {result.returncode}")
    print(f"validated and ran nonempty {args.suite} case inventory")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

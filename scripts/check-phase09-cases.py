#!/usr/bin/env python3
"""Build and run one exact registered Phase 9 case suite.

Provenance: a copy of scripts/check-audio-cases.py, which is bound by the Phase
8 qualifier and is not modified or parameterized. Differences: schema id
t64.phase09-case-inventory, the error prefix, the default inventory and
artifact prefix, and the --inventory option (so the checker can be exercised
against a temporary inventory and stub binaries). Licensed under the repository
license (Apache-2.0).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "docs/spec/phase09-cases.json"


def fail(message: str) -> int:
    print(f"Phase09 case inventory error: {message}", file=sys.stderr)
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


def safe_relative(value: object, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{label} must be a nonempty relative path")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or "." in path.parts:
        raise ValueError(f"{label} must stay inside the build directory")
    return path.as_posix()


def run(
    command: list[str], *, cwd: Path = ROOT, env: dict[str, str] | None = None
) -> subprocess.CompletedProcess[str]:
    print("+ " + " ".join(command), flush=True)
    return subprocess.run(command, cwd=cwd, env=env, text=True, capture_output=True)


def output(result: subprocess.CompletedProcess[str]) -> None:
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)


def selftest() -> int:
    try:
        if unique_nonempty(["a", "b"], "valid cases") != ["a", "b"]:
            return fail("self-test valid list changed")
        for values in ([], [""], ["a", "a"], "a"):
            try:
                unique_nonempty(values, "invalid cases")
            except ValueError:
                continue
            return fail(f"self-test accepted invalid case list {values!r}")
        if safe_relative("core/test_ai", "binary") != "core/test_ai":
            return fail("self-test valid binary path changed")
        for path in ("/tmp/test", "../test", "core/../test", ""):
            try:
                safe_relative(path, "binary")
            except ValueError:
                continue
            return fail(f"self-test accepted unsafe binary path {path!r}")
    except ValueError as error:
        return fail(f"self-test failed: {error}")
    print("phase09 inventory checker self-test passed")
    return 0


def load_inventory(
    suite_name: str, inventory_path: Path = INVENTORY
) -> tuple[list[str], dict[str, list[str]], dict[str, object]]:
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    if inventory.get("schema") != "t64.phase09-case-inventory" or inventory.get(
        "schema_version"
    ) != 1:
        raise ValueError("unsupported phase09 case inventory schema")
    suites = inventory.get("suites")
    if not isinstance(suites, dict) or not suites:
        raise ValueError("inventory suites must be a nonempty object")
    selected = suites.get(suite_name)
    if not isinstance(selected, dict):
        raise ValueError(f"unknown suite: {suite_name}")

    all_ctests: list[str] = []
    all_cases: dict[str, list[str]] = {}
    for name, source_suite in suites.items():
        if not isinstance(name, str) or not isinstance(source_suite, dict):
            raise ValueError("suite entries must be named objects")
        ctests = unique_nonempty(source_suite.get("ctest"), f"{name} CTest names")
        for ctest in ctests:
            if ctest not in all_ctests:
                all_ctests.append(ctest)
        binaries = source_suite.get("binaries")
        if not isinstance(binaries, list) or not binaries:
            raise ValueError(f"{name} binaries must be a nonempty list")
        seen_paths: set[str] = set()
        for entry in binaries:
            if not isinstance(entry, dict):
                raise ValueError(f"{name} binary entries must be objects")
            relative = safe_relative(entry.get("path"), f"{name} binary path")
            if relative in seen_paths:
                raise ValueError(f"{name} contains duplicate binary path {relative}")
            seen_paths.add(relative)
            cases = unique_nonempty(entry.get("cases"), f"{relative} cases")
            aggregate = all_cases.setdefault(relative, [])
            for case in cases:
                if case not in aggregate:
                    aggregate.append(case)

    expected_ctests = unique_nonempty(selected.get("ctest"), "suite CTest names")
    binaries = selected.get("binaries")
    if not isinstance(binaries, list) or not binaries:
        raise ValueError("suite binaries must be a nonempty list")
    parsed: dict[str, object] = {}
    paths: set[str] = set()
    for entry in binaries:
        if not isinstance(entry, dict):
            raise ValueError("suite binary entries must be objects")
        relative = safe_relative(entry.get("path"), "suite binary path")
        if relative in paths:
            raise ValueError(f"suite contains duplicate binary path {relative}")
        paths.add(relative)
        parsed[relative] = entry
    return expected_ctests, all_cases, parsed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", help="CMake build directory")
    parser.add_argument("--suite", help="registered suite name")
    parser.add_argument("--run", action="store_true", help="run the suite")
    parser.add_argument(
        "--artifact-prefix", default="09-02", help="simple artifact file prefix"
    )
    parser.add_argument(
        "--inventory",
        default=str(INVENTORY),
        help="case inventory JSON (default: the committed docs/spec/phase09-cases.json)",
    )
    parser.add_argument("--selftest", action="store_true", help="test checker input gates")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    if not args.build or not args.suite:
        return fail("--build and --suite are required unless --selftest is used")
    if (
        not args.artifact_prefix
        or Path(args.artifact_prefix).name != args.artifact_prefix
        or args.artifact_prefix in {".", ".."}
    ):
        return fail("artifact prefix must be a simple file-name prefix")

    build_dir = Path(args.build)
    if not build_dir.is_absolute():
        build_dir = ROOT / build_dir
    build_dir = build_dir.resolve()
    if not build_dir.is_dir():
        return fail(f"build directory does not exist: {build_dir}")

    try:
        expected_ctests, all_cases, parsed_binaries = load_inventory(
            args.suite, Path(args.inventory)
        )
    except (OSError, json.JSONDecodeError, ValueError, AttributeError) as error:
        return fail(str(error))

    token = f"{args.artifact_prefix}-{args.suite}-{time.time_ns()}-{os.getpid()}"
    artifact = lambda suffix: build_dir / f"{token}-{suffix}"
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"

    build_command = ["cmake", "--build", str(build_dir), "--parallel", "1"]
    build = run(build_command, env=environment)
    artifact("build-command.json").write_text(
        json.dumps(build_command, indent=2) + "\n", encoding="utf-8"
    )
    artifact("build-stdout.log").write_text(build.stdout, encoding="utf-8")
    artifact("build-stderr.log").write_text(build.stderr, encoding="utf-8")
    output(build)
    if build.returncode != 0:
        return fail(f"build failed with exit status {build.returncode}")

    temporary = build_dir / "Testing/Temporary"
    last_test = temporary / "LastTest.log"

    def retain_last_test(label: str) -> None:
        destination = artifact(label)
        if last_test.is_file():
            shutil.copy2(last_test, destination)
        else:
            destination.write_text("LastTest.log absent\n", encoding="utf-8")

    retain_last_test("before-discovery-LastTest.log")
    discovery_command = ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"]
    discovery = run(discovery_command, env=environment)
    retain_last_test("after-discovery-LastTest.log")
    artifact("discovery-command.json").write_text(
        json.dumps(discovery_command, indent=2) + "\n", encoding="utf-8"
    )
    artifact("discovery-stdout.log").write_text(discovery.stdout, encoding="utf-8")
    artifact("discovery-stderr.log").write_text(discovery.stderr, encoding="utf-8")
    if discovery.returncode != 0:
        output(discovery)
        return fail(f"CTest discovery failed with exit status {discovery.returncode}")
    try:
        discovered = json.loads(discovery.stdout)
        names = [test["name"] for test in discovered.get("tests", [])]
        if len(names) != len(set(names)):
            raise ValueError("CTest discovery contains duplicate names")
        missing = sorted(set(expected_ctests) - set(names))
        if missing:
            raise ValueError("registered CTest names are missing: " + ", ".join(missing))
        print(f"CTest discovery found {len(names)} unique registrations")
    except (json.JSONDecodeError, KeyError, TypeError, ValueError) as error:
        return fail(str(error))

    for relative in parsed_binaries:
        binary = build_dir / relative
        if not binary.is_file():
            return fail(f"case-list binary is missing after build: {binary}")
        listed = run([str(binary), "--list-cases"], env=environment)
        output(listed)
        if listed.returncode != 0:
            return fail(f"{relative} --list-cases failed")
        actual = listed.stdout.splitlines()
        if not actual or len(actual) != len(set(actual)):
            return fail(f"{relative} case list is empty or duplicated")
        expected = all_cases.get(relative)
        if actual != expected:
            return fail(
                f"{relative} source-derived cases differ from inventory: "
                f"expected {expected!r}, got {actual!r}"
            )

    if not args.run:
        print(f"validated nonempty {args.suite} phase09 case inventory")
        return 0

    checker_command = [
        sys.executable,
        "-B",
        str(ROOT / "scripts/check-rsp-no-hle.py"),
        "--build",
        str(build_dir),
    ]
    checker = run(checker_command, env=environment)
    artifact("source-body-check-command.json").write_text(
        json.dumps(checker_command, indent=2) + "\n", encoding="utf-8"
    )
    artifact("source-body-check-stdout.log").write_text(
        checker.stdout, encoding="utf-8"
    )
    artifact("source-body-check-stderr.log").write_text(
        checker.stderr, encoding="utf-8"
    )
    output(checker)
    if checker.returncode != 0:
        return fail(f"source/body/object inventory failed with exit status {checker.returncode}")

    retain_last_test("before-suite-LastTest.log")
    junit_path = artifact(f"{args.suite}-junit.xml").resolve()
    regex = "^(" + "|".join(re.escape(name) for name in expected_ctests) + ")$"
    command = [
        "ctest",
        "--test-dir",
        str(build_dir),
        "--no-tests=error",
        "--parallel",
        "4",
        "--output-on-failure",
        "-R",
        regex,
        "--output-junit",
        str(junit_path),
    ]
    artifact("suite-command.json").write_text(
        json.dumps(command, indent=2) + "\n", encoding="utf-8"
    )
    result = run(command, env=environment)
    retain_last_test("after-suite-LastTest.log")
    artifact("suite-exit-status.txt").write_text(
        f"{result.returncode}\n", encoding="utf-8"
    )
    artifact("suite-stdout.log").write_text(result.stdout, encoding="utf-8")
    artifact("suite-stderr.log").write_text(result.stderr, encoding="utf-8")
    if result.returncode == 0 and not junit_path.is_file():
        return fail(f"{args.suite} passed without its unique JUnit archive")
    output(result)
    if result.returncode != 0:
        return fail(f"{args.suite} suite failed with exit status {result.returncode}")
    print(f"validated and ran nonempty {args.suite} phase09 case inventory")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

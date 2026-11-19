#!/usr/bin/env python3
"""Controls for the Phase 9 harness guards and the fail-closed qualifier stub.

Cases are listed with --list-cases, run with --selftest (every case, verbose)
or one at a time with --case NAME. No builds and no nested CTest run here.
"""

from __future__ import annotations

import ast
import builtins
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


_SCRIPT_ROOT = Path(__file__).resolve().parents[2]
_WORKING_ROOT = Path.cwd().resolve()
if (_SCRIPT_ROOT / "scripts/gate.sh").is_file():
    ROOT = _SCRIPT_ROOT
elif (_WORKING_ROOT / "scripts/gate.sh").is_file():
    ROOT = _WORKING_ROOT
else:
    ROOT = _SCRIPT_ROOT

# Every case of this file, in the order --list-cases prints them. The
# dispatcher below only runs names listed in this tuple.
CASE_NAMES: tuple[str, ...] = (
    "gate-junit-hook-private-phase09",
    "stub-modes-refuse",
    "zero-case-inventory-refused",
    "inventory-shape-refused",
    "public-members-exact-allowlist",
    "public-member-private-suffix-refused",
    "public-member-state-magic-refused",
    "public-member-symlink-or-nested-refused",
    "source-paths-exist-and-cover-guards",
    "no-undefined-global-names",
    "case-checker-selftest-and-mismatch",
    "claim-skeleton-has-no-real-block",
)

# Named subsets run by the dedicated CTest names (no new logic, only grouping).
SUITES: dict[str, tuple[str, ...]] = {
    "stub": ("stub-modes-refuse", "zero-case-inventory-refused",
             "inventory-shape-refused"),
    "allowlist": ("public-members-exact-allowlist",
                  "public-member-private-suffix-refused",
                  "public-member-state-magic-refused",
                  "public-member-symlink-or-nested-refused"),
}

QUALIFIER = ROOT / "scripts/qualify-phase09.py"
INVENTORY = ROOT / "docs/spec/phase09-cases.json"
_SPEC = importlib.util.spec_from_file_location("t64_qualify_phase09", QUALIFIER)
assert _SPEC is not None and _SPEC.loader is not None
qualify = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(qualify)


def _undefined_global_names(path: Path) -> dict[str, int]:
    """Names loaded somewhere in a script that no statement ever binds."""
    tree = ast.parse(path.read_text(encoding="utf-8"))
    bound = set(dir(builtins)) | {"__file__", "__name__"}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            bound.add(node.name)
            if not isinstance(node, ast.ClassDef):
                args = node.args
                for arg in (*args.posonlyargs, *args.args, *args.kwonlyargs):
                    bound.add(arg.arg)
                if args.vararg:
                    bound.add(args.vararg.arg)
                if args.kwarg:
                    bound.add(args.kwarg.arg)
        elif isinstance(node, ast.Lambda):
            args = node.args
            for arg in (*args.posonlyargs, *args.args, *args.kwonlyargs):
                bound.add(arg.arg)
            if args.vararg:
                bound.add(args.vararg.arg)
            if args.kwarg:
                bound.add(args.kwarg.arg)
        elif isinstance(node, ast.Import):
            bound.update((alias.asname or alias.name).split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom):
            bound.update(alias.asname or alias.name for alias in node.names)
        elif isinstance(node, ast.ExceptHandler) and node.name:
            bound.add(node.name)
        elif isinstance(node, ast.Name) and isinstance(node.ctx, (ast.Store, ast.Del)):
            bound.add(node.id)
    missing: dict[str, int] = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load) \
                and node.id not in bound:
            missing.setdefault(node.id, node.lineno)
    return missing

# A placeholder root: no real checkout path appears in this file.
PLACEHOLDER_ROOT = "/r"


class Phase09Qualification(unittest.TestCase):
    def test_gate_junit_hook_private_phase09(self) -> None:
        """The N2 arm admits one more private directory and nothing else."""
        gate = (ROOT / "scripts/gate.sh").read_text(encoding="utf-8")
        self.assertIn('if [ -n "${T64_GATE_JUNIT_OUT:-}" ]; then', gate)
        block = re.search(r'  case "\$T64_GATE_JUNIT_OUT" in\n.*?\n  esac\n', gate, re.S)
        self.assertIsNotNone(block)
        assert block is not None
        root = PLACEHOLDER_ROOT
        cases = (
            # Accepted: the Phase 9 private directory, and the Phase 8 one.
            (f"{root}/build/gate/09-11-phase09-private/run/junit.xml", 0),
            (f"{root}/build/gate/08-05-audio-private/run/junit.xml", 0),
            # Refused: a `..` component in the middle and at the end.
            (f"{root}/build/gate/09-11-phase09-private/../x.xml", 2),
            (f"{root}/build/gate/09-11-phase09-private/a/..", 2),
            # Refused: outside both private directories, including near misses.
            (f"{root}/build/gate/other/junit.xml", 2),
            (f"{root}/build/gate/09-11-phase09-privatex/junit.xml", 2),
            (f"{root}/build/gate/09-11-phase09-private", 2),
        )
        failures = []
        for value, expected in cases:
            script = 'ROOT="$1"; T64_GATE_JUNIT_OUT="$2"\n' + block.group(0)
            result = subprocess.run(["sh", "-c", script, "sh", root, value],
                                    capture_output=True, text=True, check=False)
            print(f"gate hook {value} -> exit {result.returncode} (want {expected})",
                  file=sys.stderr)
            if result.returncode != expected:
                failures.append((value, result.returncode, expected))
        self.assertEqual(failures, [])
        # Both private arms are present exactly once; the run lines of the
        # hook and the plain branch are unchanged.
        self.assertEqual(gate.count('"$ROOT"/build/gate/08-05-audio-private/*)'), 1)
        self.assertEqual(gate.count('"$ROOT"/build/gate/09-11-phase09-private/*)'), 1)
        self.assertIn(
            'step ctest ctest --preset gate --output-junit "$T64_GATE_JUNIT_OUT"', gate)
        self.assertIn('step ctest ctest --preset gate\nfi', gate)

    # ---- qualifier stub -----------------------------------------------------

    def test_stub_modes_refuse(self) -> None:
        """Every mode except --selftest refuses; none exits 0 without evidence."""
        script = str(QUALIFIER)
        accepted = subprocess.run([sys.executable, "-B", script, "--help"],
                                  capture_output=True, text=True, check=False)
        self.assertEqual(accepted.returncode, 0, accepted.stderr)
        modes = (["--native"], ["--emit-hosted"], ["--collect-hosted"],
                 ["--verify-evidence"], ["--verify-record", "private-run"],
                 ["--scan-pii"])
        for mode in modes:
            result = subprocess.run([sys.executable, "-B", script, *mode],
                                    capture_output=True, text=True, check=False,
                                    cwd=ROOT)
            self.assertEqual(result.returncode, 1, (mode, result.stdout, result.stderr))
            self.assertIn("refused original evidence", result.stderr, mode)
            self.assertIn("not yet present", result.stderr, mode)
            self.assertEqual(result.stdout, "", mode)
        # No mode at all is a usage error, never a success.
        none = subprocess.run([sys.executable, "-B", script],
                              capture_output=True, text=True, check=False)
        self.assertNotEqual(none.returncode, 0)

    def _inventory(self) -> dict:
        return json.loads(INVENTORY.read_text(encoding="utf-8"))

    def _write(self, directory: Path, value: object) -> Path:
        path = directory / "inventory.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def test_zero_case_inventory_refused(self) -> None:
        """An inventory with no suites, no cases or no binaries is refused."""
        summary = qualify.validate_case_inventory(INVENTORY)
        # Later plans add suites as data rows; the summary must count exactly
        # the suites of the committed inventory and keep the two original ones.
        suites = self._inventory()["suites"]
        self.assertEqual(summary["suites"], len(suites))
        self.assertTrue({"identity", "mechanism"} <= set(suites))
        self.assertGreater(summary["cases"], 0)
        with tempfile.TemporaryDirectory(prefix="t64-p09-inv.") as tmp:
            directory = Path(tmp)
            base = self._inventory()
            zero_suites = dict(base, suites={})
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.validate_case_inventory(self._write(directory, zero_suites))
            self.assertIn("zero suites", str(caught.exception))
            no_binaries = json.loads(json.dumps(base))
            no_binaries["suites"]["identity"]["binaries"] = []
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.validate_case_inventory(self._write(directory, no_binaries))
            self.assertIn("binaries", str(caught.exception))
            all_empty = json.loads(json.dumps(base))
            for suite in all_empty["suites"].values():
                for binary in suite["binaries"]:
                    binary["cases"] = []
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.validate_case_inventory(self._write(directory, all_empty))
            self.assertIn("cases", str(caught.exception))
            # The committed inventory is still accepted in the same setup.
            self.assertEqual(qualify.validate_case_inventory(INVENTORY), summary)

    def test_inventory_shape_refused(self) -> None:
        """Each malformed shape is refused with its own message."""
        self.assertGreater(qualify.validate_case_inventory(INVENTORY)["cases"], 0)

        def mutate(change) -> dict:
            value = json.loads(json.dumps(self._inventory()))
            change(value)
            return value

        cases = (
            ("wrong schema id", lambda v: v.__setitem__("schema", "t64.audio-case-inventory"),
             "schema"),
            ("wrong version", lambda v: v.__setitem__("schema_version", 2), "schema"),
            ("empty ctest list", lambda v: v["suites"]["identity"].__setitem__("ctest", []),
             "ctest"),
            ("empty cases list",
             lambda v: v["suites"]["identity"]["binaries"][0].__setitem__("cases", []),
             "cases"),
            ("duplicate case",
             lambda v: v["suites"]["identity"]["binaries"][0]["cases"].append(
                 v["suites"]["identity"]["binaries"][0]["cases"][0]),
             "duplicate"),
            ("duplicate ctest name",
             lambda v: v["suites"]["identity"]["ctest"].append(
                 v["suites"]["identity"]["ctest"][0]),
             "duplicate"),
            ("unsafe binary path",
             lambda v: v["suites"]["identity"]["binaries"][0].__setitem__(
                 "path", "../core/test_si_profile"),
             "path"),
            ("absolute binary path",
             lambda v: v["suites"]["identity"]["binaries"][0].__setitem__(
                 "path", "/core/test_si_profile"),
             "path"),
            ("suite is not an object", lambda v: v["suites"].__setitem__("identity", []),
             "suite"),
            ("non-string case",
             lambda v: v["suites"]["identity"]["binaries"][0]["cases"].append(7),
             "cases"),
        )
        with tempfile.TemporaryDirectory(prefix="t64-p09-inv.") as tmp:
            directory = Path(tmp)
            for label, change, needle in cases:
                path = self._write(directory, mutate(change))
                with self.assertRaises(qualify.Refusal, msg=label) as caught:
                    qualify.validate_case_inventory(path)
                self.assertIn(needle, str(caught.exception), label)
            # Not JSON at all is also a refusal, never an exception of another type.
            bad = directory / "inventory.json"
            bad.write_text("{not json", encoding="utf-8")
            with self.assertRaises(qualify.Refusal):
                qualify.validate_case_inventory(bad)

    # ---- public member allowlist -------------------------------------------

    MANIFEST = json.dumps({"schema": "t64.phase09-qualification",
                           "schema_version": 1}).encode("utf-8")

    def _public_dir(self, root: Path, name: str, manifest: bytes | None = None) -> Path:
        directory = root / name
        directory.mkdir()
        (directory / "manifest.json").write_bytes(
            self.MANIFEST if manifest is None else manifest)
        return directory

    def test_public_members_exact_allowlist(self) -> None:
        """Exactly one regular manifest.json is accepted; anything else is not."""
        with tempfile.TemporaryDirectory(prefix="t64-p09-pub.") as tmp:
            root = Path(tmp)
            accepted = self._public_dir(root, "accepted")
            value = qualify.scan_public_members(accepted)
            self.assertEqual(value["schema"], "t64.phase09-qualification")
            extra = self._public_dir(root, "extra")
            (extra / "notes.txt").write_text("x", encoding="utf-8")
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(extra)
            self.assertIn("unexpected member", str(caught.exception))
            missing = root / "missing"
            missing.mkdir()
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(missing)
            self.assertIn("no manifest.json", str(caught.exception))
            renamed = root / "renamed"
            renamed.mkdir()
            (renamed / "other.json").write_bytes(self.MANIFEST)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(renamed)
            self.assertIn("unexpected member", str(caught.exception))
            absent = root / "absent"
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(absent)
            self.assertIn("absent or unsafe", str(caught.exception))
            # The baseline is still accepted after the refusals.
            self.assertEqual(qualify.scan_public_members(accepted), value)

    def test_public_member_private_suffix_refused(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-p09-pub.") as tmp:
            root = Path(tmp)
            baseline = self._public_dir(root, "baseline")
            self.assertEqual(qualify.scan_public_members(baseline)["schema_version"], 1)
            for index, name in enumerate(("x.sta", "y.rpl", "z.sav", "e.eep", "s.sra",
                                          "r.z64", "UPPER.STA")):
                directory = self._public_dir(root, f"case{index}")
                (directory / name).write_bytes(b"private")
                with self.assertRaises(qualify.Refusal, msg=name) as caught:
                    qualify.scan_public_members(directory)
                self.assertIn("private-suffix member", str(caught.exception), name)

    def test_public_member_state_magic_refused(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-p09-pub.") as tmp:
            root = Path(tmp)
            baseline = self._public_dir(root, "baseline")
            self.assertEqual(qualify.scan_public_members(baseline)["schema_version"], 1)
            magic = self._public_dir(root, "magic", b"T64STATE" + self.MANIFEST)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(magic)
            self.assertIn("state container magic", str(caught.exception))
            # Local paths and personal identifiers are built at run time so no
            # literal of either shape is committed in this file.
            home = "/" + "Users" + "/" + "someone" + "/run"
            path_manifest = json.dumps({"schema": "t64.phase09-qualification",
                                        "where": home}).encode("utf-8")
            pathdir = self._public_dir(root, "path", path_manifest)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(pathdir)
            self.assertIn("local path", str(caught.exception))
            address = "someone" + "@" + "example" + "." + "org"
            email_manifest = json.dumps({"schema": "t64.phase09-qualification",
                                         "who": address}).encode("utf-8")
            emaildir = self._public_dir(root, "email", email_manifest)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(emaildir)
            self.assertIn("personal identifier", str(caught.exception))
            oversize = self._public_dir(root, "oversize", b"{" + b" " * (17 * 1024) + b"}")
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(oversize)
            self.assertIn("bounded", str(caught.exception))

    def test_public_member_symlink_or_nested_refused(self) -> None:
        with tempfile.TemporaryDirectory(prefix="t64-p09-pub.") as tmp:
            root = Path(tmp)
            baseline = self._public_dir(root, "baseline")
            self.assertEqual(qualify.scan_public_members(baseline)["schema_version"], 1)
            nested = self._public_dir(root, "nested")
            (nested / "sub").mkdir()
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(nested)
            self.assertIn("nested directory", str(caught.exception))
            linked = root / "linked"
            linked.mkdir()
            target = root / "elsewhere.json"
            target.write_bytes(self.MANIFEST)
            (linked / "manifest.json").symlink_to(target)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(linked)
            self.assertIn("symlink", str(caught.exception))
            link_dir = root / "link-dir"
            link_dir.symlink_to(baseline)
            with self.assertRaises(qualify.Refusal) as caught:
                qualify.scan_public_members(link_dir)
            self.assertIn("absent or unsafe", str(caught.exception))

    # ---- bound sources and names -------------------------------------------

    def test_source_paths_exist_and_cover_guards(self) -> None:
        paths = qualify.SOURCE_PATHS
        self.assertEqual(len(paths), len(set(paths)), "duplicate source path")
        for guard in ("scripts/check-rsp-no-hle.py", "scripts/check-test-inventory.sh",
                      "scripts/check-ci-yml.sh", "scripts/rights-scan.sh",
                      "scripts/pii-scan.sh", "scripts/check-provenance.sh",
                      "scripts/check-claims.sh", "scripts/gate.sh",
                      "scripts/check-phase09-cases.py", "scripts/qualify-phase09.py",
                      "scripts/tests/test_phase09_qualification.py",
                      "scripts/list-membership-sites.py",
                      "docs/spec/profile-membership-sites.txt",
                      "docs/spec/phase09-cases.json", ".github/workflows/ci.yml",
                      "core", "runner", "tools/rights", "tools/manifest", "fixtures"):
            self.assertIn(guard, paths)
        in_git = subprocess.run(["git", "rev-parse", "--is-inside-work-tree"], cwd=ROOT,
                                capture_output=True, text=True, check=False)
        for relative in paths:
            self.assertFalse(Path(relative).is_absolute() or ".." in Path(relative).parts,
                             relative)
            self.assertTrue((ROOT / relative).exists(), f"{relative} is missing")
            if in_git.returncode == 0:
                tracked = subprocess.run(["git", "ls-files", "--", relative], cwd=ROOT,
                                         capture_output=True, text=True, check=False)
                self.assertTrue(tracked.stdout.strip(), f"{relative} is not tracked in git")

    def test_no_undefined_global_names(self) -> None:
        for relative in ("scripts/qualify-phase09.py", "scripts/check-phase09-cases.py",
                         "scripts/tests/test_phase09_qualification.py"):
            self.assertEqual(_undefined_global_names(ROOT / relative), {},
                             f"{relative} loads names it never defines")

    def test_case_checker_selftest_and_mismatch(self) -> None:
        Phase09CaseChecker().check(self)

    def test_claim_skeleton_has_no_real_block(self) -> None:
        """The claim file is a skeleton: example fences only, no real block."""
        path = ROOT / "docs/claims/phase09.md"
        lines = path.read_text(encoding="utf-8").splitlines()
        self.assertNotIn("```t64-claim", lines)
        self.assertGreaterEqual(lines.count("```t64-claim-example"), 6)
        checked = subprocess.run(
            ["sh", str(ROOT / "scripts/check-claims.sh"), "--files",
             str(path.relative_to(ROOT))],
            capture_output=True, text=True, check=False, cwd=ROOT)
        self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)


def _run_checker(build: Path, inventory: Path, suite: str) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ)
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    return subprocess.run(
        [sys.executable, "-B", str(ROOT / "scripts/check-phase09-cases.py"),
         "--build", str(build), "--suite", suite, "--inventory", str(inventory)],
        capture_output=True, text=True, check=False, env=env)


class Phase09CaseChecker:
    """The inventory checker, driven against a tiny generated build directory."""

    SUITE = "stubsuite"
    CASES = ["alpha", "beta", "gamma"]

    def _write_stub(self, path: Path, cases: list[str]) -> None:
        body = "#!/bin/sh\n" + "".join(
            f'[ "$1" = --list-cases ] && echo {name}\n' for name in cases) + "exit 0\n"
        path.write_text(body, encoding="utf-8")
        path.chmod(0o755)

    def _write_inventory(self, path: Path, cases: list[str]) -> None:
        path.write_text(json.dumps({
            "schema": "t64.phase09-case-inventory",
            "schema_version": 1,
            "suites": {self.SUITE: {
                "ctest": ["stub.one"],
                "binaries": [{"path": "core/test_stub", "cases": cases}],
            }},
        }, indent=2), encoding="utf-8")

    def check(self, tc: unittest.TestCase) -> None:
        checker = ROOT / "scripts/check-phase09-cases.py"
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        selftest = subprocess.run(
            [sys.executable, "-B", str(checker), "--selftest"],
            capture_output=True, text=True, check=False, env=env)
        tc.assertEqual(selftest.returncode, 0, selftest.stderr)
        with tempfile.TemporaryDirectory(prefix="t64-p09-chk.") as tmp:
            root = Path(tmp)
            source = root / "src"
            build = root / "build"
            (source).mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.25)\nproject(stubs NONE)\n"
                "enable_testing()\nadd_test(NAME stub.one COMMAND true)\n",
                encoding="utf-8")
            configured = subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build)],
                capture_output=True, text=True, check=False)
            tc.assertEqual(configured.returncode, 0, configured.stderr)
            (build / "core").mkdir()
            stub = build / "core/test_stub"
            inventory = root / "inventory.json"

            def attempt(stub_cases: list[str], inventory_cases: list[str]):
                self._write_stub(stub, stub_cases)
                self._write_inventory(inventory, inventory_cases)
                return _run_checker(build, inventory, self.SUITE)

            # Accepted baseline in the same setup.
            baseline = attempt(self.CASES, self.CASES)
            tc.assertEqual(baseline.returncode, 0, baseline.stdout + baseline.stderr)
            tc.assertIn("validated nonempty stubsuite", baseline.stdout)
            prefix = "Phase09 case inventory error"
            # One case added to the inventory: the stub no longer matches.
            added = attempt(self.CASES, self.CASES + ["delta"])
            tc.assertEqual(added.returncode, 1)
            tc.assertIn(prefix, added.stderr)
            tc.assertIn("source-derived cases differ from inventory", added.stderr)
            tc.assertIn("'delta'", added.stderr)
            # One case removed from the stub.
            removed = attempt(self.CASES[:-1], self.CASES)
            tc.assertEqual(removed.returncode, 1)
            tc.assertIn("source-derived cases differ from inventory", removed.stderr)
            tc.assertIn("got ['alpha', 'beta']", removed.stderr)
            # Two cases swapped: the order is part of the contract.
            swapped = attempt(["beta", "alpha", "gamma"], self.CASES)
            tc.assertEqual(swapped.returncode, 1)
            tc.assertIn("source-derived cases differ from inventory", swapped.stderr)
            tc.assertIn("got ['beta', 'alpha', 'gamma']", swapped.stderr)
            # A duplicate in the stub's own list.
            duplicate = attempt(["alpha", "beta", "beta"], ["alpha", "beta", "gamma"])
            tc.assertEqual(duplicate.returncode, 1)
            tc.assertIn("case list is empty or duplicated", duplicate.stderr)
            # A wrong schema id and a missing suite are refused with their own text.
            self._write_stub(stub, self.CASES)
            wrong = json.loads(inventory.read_text(encoding="utf-8"))
            self._write_inventory(inventory, self.CASES)
            wrong = json.loads(inventory.read_text(encoding="utf-8"))
            wrong["schema"] = "t64.audio-case-inventory"
            inventory.write_text(json.dumps(wrong), encoding="utf-8")
            schema = _run_checker(build, inventory, self.SUITE)
            tc.assertEqual(schema.returncode, 1)
            tc.assertIn("unsupported phase09 case inventory schema", schema.stderr)
            self._write_inventory(inventory, self.CASES)
            unknown = _run_checker(build, inventory, "no-such-suite")
            tc.assertEqual(unknown.returncode, 1)
            tc.assertIn("unknown suite: no-such-suite", unknown.stderr)
            # The accepted baseline still passes after the refusals.
            again = attempt(self.CASES, self.CASES)
            tc.assertEqual(again.returncode, 0, again.stdout + again.stderr)


def main() -> int:
    args = sys.argv[1:]
    if args == ["--list-cases"]:
        print("\n".join(CASE_NAMES))
        return 0
    if args == ["--selftest"]:
        for name in CASE_NAMES:
            print(f"case reached: {name}")
        sys.argv = [sys.argv[0], "-v"]
        unittest.main(verbosity=2)
        return 0
    selected: tuple[str, ...] | None = None
    if len(args) == 2 and args[0] == "--case" and args[1] in CASE_NAMES:
        selected = (args[1],)
    elif len(args) == 2 and args[0] == "--suite" and args[1] in SUITES:
        selected = SUITES[args[1]]
    if selected is not None:
        suite = unittest.TestSuite(
            [Phase09Qualification("test_" + name.replace("-", "_")) for name in selected])
        for name in selected:
            print(f"case reached: {name}")
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        return 0 if result.wasSuccessful() else 1
    print("usage: test_phase09_qualification.py "
          "--list-cases | --selftest | --case NAME | --suite stub|allowlist",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())

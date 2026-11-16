#!/usr/bin/env python3
"""Validate the retained BASE evidence without launching guest code."""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
import os
import pathlib
import re
import shlex
import subprocess
import stat
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE_ROOT = pathlib.Path("build/private-base-diagnostic")
AUTHORED_BINDING = pathlib.Path(
    "build/gate/06-05-final-authored-comparisons/inputs/binding.json")
AUTHORED_MANIFEST = pathlib.Path("fixtures/manifest.json")
AUTHORED_EXECUTION = pathlib.Path("build/gate/06-05-final-authored-comparisons")
AUTHORED_RUN_ROOT = pathlib.Path("build/gate")
AUTHORED_CLAIMS = pathlib.Path("docs/claims/phase06.md")
HEX40 = re.compile(r"[0-9a-f]{40}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
MAX_JSON = 64 * 1024 * 1024
AUTHORED_EXCLUSIONS = (
    "any other build, profile, ROM or fixture set; general ROM compatibility; "
    "hardware behavior or clock accuracy; RDP, audio, peripheral or 8 MiB behavior; "
    "T2 timing; external diagnostic results; commercial software"
)
CONTEXT_BASE_KEYS = {
    "bindings", "inventory", "profile", "schema", "schema_version",
    "source_authority", "source_projection", "source_revision",
}
CONTEXT_V2_KEYS = CONTEXT_BASE_KEYS | {"rsp_source_projection"}
BINDING_KEYS = {
    "context_sha256", "inventory_sha256", "schema", "schema_version",
    "source_authority_sha256", "source_projection_sha256",
}
AUTHORITY_KEYS = {"members", "schema", "schema_version", "source_revision"}
INVENTORY_KEYS = AUTHORITY_KEYS | {"source_authority_sha256"}
PUBLIC_RECEIPT_KEYS = {
    "artifact_binding", "core_build_id", "execution_record",
    "fixture_manifest", "profiles",
    "project_version", "schema", "schema_version", "source_annotation",
    "source_annotation_dirty",
}
PUBLIC_BINDING_KEYS = {"artifacts", "path", "sha256"}
PUBLIC_MANIFEST_KEYS = {"fixtures", "path", "sha256"}
PUBLIC_FIXTURE_KEYS = {"expected_kind", "id", "oracle_revision", "profile_id", "rom_sha256"}
PUBLIC_PROFILE_KEYS = {
    "boot_fail", "evidence_tier", "fixture_id", "guest_fail", "harness_error",
    "header_entry_tick", "last_retired_opcode", "last_retired_pc",
    "missing", "not_claimed", "oracle_kind", "oracle_revision",
    "output_bytes", "output_sha256", "output_text", "pass", "profile_id",
    "profile_sha256", "retired_instructions", "rom_sha256", "skipped",
    "terminal_opcode", "terminal_pc", "terminal_tick", "timeout", "total",
}
PUBLIC_ARTIFACT_KEYS = {"name", "sha256"}
PUBLIC_EXECUTION_KEYS = {
    "attempts", "binding_sha256", "build_id", "compiler_policy",
    "ctest_log_sha256", "last_test_log_sha256", "log_manifest_sha256",
    "path", "source_revision",
}
PUBLIC_ATTEMPT_KEYS = {"command_sha256", "outcome_sha256", "path", "test"}
CLAIM_KEYS = {
    "build_id", "profile_id", "tier", "fixture_set", "manifest_digest",
    "pass", "total", "guest_fail", "boot_fail", "timeout", "missing",
    "harness_error", "skipped", "oracle_kind", "oracle_revision",
    "not_claimed", "statement",
}
PRIVATE_ALIAS = re.compile(
    r"selected[\s_-]*(?:member|rom)|external[\s_-]*(?:count|result)|"
    r"registration[\s_-]*index|value[\s_-]*id|member[\s_-]*results|"
    r"expected[\s_-]*members|source[\s_-]*authority|private[\s_-]*(?:count|result|identity)",
    re.IGNORECASE,
)


class Refusal(ValueError):
    """Evidence is missing or inconsistent."""


def refuse() -> None:
    raise Refusal("BASE evidence is missing or inconsistent")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def parse_canonical(data: bytes) -> dict:
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                refuse()
            result[key] = value
        return result

    try:
        value = json.loads(data, object_pairs_hook=unique_pairs)
    except (UnicodeError, json.JSONDecodeError, RecursionError):
        refuse()
    if not isinstance(value, dict) or canonical(value) != data:
        refuse()
    return value


def parse_json(data: bytes) -> dict:
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                refuse()
            result[key] = value
        return result
    try:
        value = json.loads(data, object_pairs_hook=unique_pairs)
    except (UnicodeError, json.JSONDecodeError, RecursionError):
        refuse()
    if not isinstance(value, dict):
        refuse()
    return value


def exact(value: object, keys: set[str]) -> dict:
    if not isinstance(value, dict) or set(value) != keys:
        refuse()
    return value


def _public_file(path: pathlib.Path, maximum: int = MAX_JSON) -> bytes:
    if path.is_symlink():
        refuse()
    descriptor = -1
    try:
        descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) |
                             getattr(os, "O_NONBLOCK", 0))
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode) or before.st_size <= 0 or before.st_size > maximum:
            refuse()
        chunks = []
        total = 0
        while True:
            chunk = os.read(descriptor, min(1024 * 1024, maximum + 1 - total))
            if not chunk:
                break
            total += len(chunk)
            if total > maximum:
                refuse()
            chunks.append(chunk)
        after = os.fstat(descriptor)
        if ((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) !=
                (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) or
                total != before.st_size):
            refuse()
        data = b"".join(chunks)
    except OSError:
        refuse()
    finally:
        if descriptor >= 0:
            os.close(descriptor)
    return data


def _privacy_scan(value: object) -> None:
    if isinstance(value, dict):
        for key, child in value.items():
            if not isinstance(key, str) or PRIVATE_ALIAS.search(key):
                refuse()
            _privacy_scan(child)
    elif isinstance(value, list):
        for child in value:
            _privacy_scan(child)
    elif isinstance(value, str) and PRIVATE_ALIAS.search(value):
        refuse()


def _render_claim(profile: dict, receipt: dict) -> dict[str, str]:
    versioned_build = receipt["project_version"] + "+" + receipt["core_build_id"]
    values = {
        "build_id": versioned_build,
        "profile_id": profile["profile_id"],
        "tier": profile["evidence_tier"],
        "fixture_set": profile["fixture_id"],
        "manifest_digest": "sha256:" + receipt["fixture_manifest"]["sha256"],
        "pass": str(profile["pass"]), "total": str(profile["total"]),
        "guest_fail": str(profile["guest_fail"]), "boot_fail": str(profile["boot_fail"]),
        "timeout": str(profile["timeout"]), "missing": str(profile["missing"]),
        "harness_error": str(profile["harness_error"]), "skipped": str(profile["skipped"]),
        "oracle_kind": profile["oracle_kind"], "oracle_revision": profile["oracle_revision"],
        "not_claimed": profile["not_claimed"],
    }
    phrase = {"self-checking-fixture": "passes self-checking fixtures"}.get(values["tier"])
    if phrase is None:
        refuse()
    values["statement"] = (
        f"Tendies64 {values['build_id']} on profile {values['profile_id']}: {phrase} "
        f"for {values['fixture_set']} @ {values['manifest_digest']} "
        f"({values['pass']}/{values['total']}; missing {values['missing']}, "
        f"skipped {values['skipped']}, timeout {values['timeout']}, "
        f"harness-error {values['harness_error']}, guest-fail {values['guest_fail']}, "
        f"boot-fail {values['boot_fail']}). Oracle: {values['oracle_kind']} "
        f"{values['oracle_revision']}. Not claimed: {values['not_claimed']}."
    )
    return values


def _record_fields(line: str) -> tuple[str, dict[str, str]]:
    tokens = line.strip().split()
    if not tokens:
        refuse()
    fields = {}
    for token in tokens:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if not key or key in fields:
            refuse()
        fields[key] = value
    return tokens[0].split("=", 1)[0], fields


def _last_test_sections(data: bytes) -> dict[str, dict]:
    try:
        lines = data.decode("utf-8").splitlines()
    except UnicodeError:
        refuse()
    sections = {}
    current = None

    def finish() -> None:
        nonlocal current
        if current is not None:
            if current["name"] in sections:
                refuse()
            sections[current["name"]] = current
            current = None

    for line in lines:
        match = re.match(r'^\s*\d+/\d+ Test: "?([^"\s]+)"?\s*$', line)
        if match:
            finish()
            current = {"name": match.group(1), "output": [], "passed": False,
                       "command": None, "directory": None, "collecting": False}
            continue
        if current is None:
            continue
        command = re.match(r"^\s*Command: (.*)$", line)
        directory = re.match(r"^\s*Directory: (.*)$", line)
        if command:
            current["command"] = command.group(1)
        elif directory:
            current["directory"] = directory.group(1)
        elif line.strip() == "Output:":
            current["collecting"] = True
        elif current["collecting"] and line.startswith("Test time ="):
            current["collecting"] = False
        elif current["collecting"]:
            current["output"].append(line)
        elif line.strip() == "Test Passed.":
            current["passed"] = True
    finish()
    return sections


def _attempt_command(command_bytes: bytes, test_name: str) -> None:
    try:
        command_text = command_bytes.decode("utf-8")
        argv = shlex.split(command_text.strip())
    except (UnicodeError, ValueError):
        refuse()
    if not command_text.endswith("\n"):
        refuse()
    if test_name == "integration.cold-hello":
        if (len(argv) != 8 or argv[1:7] != [
                "--compare", "--profile-a", "ntsc-cart-4m-openipl3-rdram-a-t1-v1",
                "--profile-b", "ntsc-cart-4m-openipl3-rdram-b-t1-v1", "--fixture"]):
            refuse()
        runner, fixture = pathlib.Path(argv[0]), pathlib.Path(argv[7])
    else:
        if (len(argv) != 6 or argv[1:5] != [
                "--probe", "--profile", "ntsc-cart-4m-openipl3-rdram-b-t1-v1",
                "--fixture"]):
            refuse()
        runner, fixture = pathlib.Path(argv[0]), pathlib.Path(argv[5])
    if (not runner.is_absolute() or runner.parts[-4:] !=
            ("build", "gate", "runner", "t64-cold-boot-probe") or
            not fixture.is_absolute() or fixture.parts[-3:] !=
            ("build", "fixtures", "hello.z64")):
        refuse()


def _find_original_attempt(repository_root: pathlib.Path, test_name: str,
                           section: dict) -> dict:
    mode = "hello" if test_name == "integration.cold-hello" else "startup"
    pattern = re.compile(rf"06-05-cold-{mode}-attempt-[0-9]{{2}}\Z")
    section_output = ("\n".join(section["output"]) + "\n").encode("utf-8")
    matches = []
    for directory in (repository_root / AUTHORED_RUN_ROOT).glob(
            f"06-05-cold-{mode}-attempt-*"):
        if (not pattern.fullmatch(directory.name) or directory.is_symlink() or
                not directory.is_dir()):
            continue
        command_path, outcome_path = directory / "command.txt", directory / "outcome.txt"
        try:
            command_bytes = _public_file(command_path, maximum=16 * 1024)
            outcome_bytes = _public_file(outcome_path, maximum=2 * 1024 * 1024)
        except Refusal:
            continue
        if not outcome_bytes.startswith(b"exit_code=0\n"):
            continue
        child_output = outcome_bytes[len(b"exit_code=0\n"):]
        if not child_output or child_output not in section_output:
            continue
        _attempt_command(command_bytes, test_name)
        relative = directory.relative_to(repository_root).as_posix()
        matches.append({
            "command": command_bytes,
            "outcome": outcome_bytes,
            "receipt": {
                "command_sha256": sha256(command_bytes),
                "outcome_sha256": sha256(outcome_bytes),
                "path": relative,
                "test": test_name,
            },
            "child_output": child_output,
        })
    if len(matches) != 1:
        refuse()
    return matches[0]


def _validate_original_coldboot_evidence(repository_root: pathlib.Path,
                                         receipt: dict, manifest: dict) -> tuple[dict, list[dict]]:
    execution = exact(receipt.get("execution_record"), PUBLIC_EXECUTION_KEYS)
    if execution.get("path") != AUTHORED_EXECUTION.as_posix():
        refuse()
    for key in ("binding_sha256", "ctest_log_sha256", "last_test_log_sha256",
                "log_manifest_sha256"):
        if not isinstance(execution.get(key), str) or not HEX64.fullmatch(execution[key]):
            refuse()
    if (not isinstance(execution.get("build_id"), str) or
            not re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", execution["build_id"]) or
            not isinstance(execution.get("source_revision"), str) or
            not HEX40.fullmatch(execution["source_revision"]) or
            execution.get("compiler_policy") != "bound-by-build-id-v1"):
        refuse()

    execution_root = repository_root / AUTHORED_EXECUTION
    binding_bytes = _public_file(repository_root / AUTHORED_BINDING, maximum=1024 * 1024)
    binding = parse_json(binding_bytes)
    if (sha256(binding_bytes) != execution["binding_sha256"] or
            binding.get("source_head") != receipt.get("source_annotation") or
            binding.get("dirty_source") is not receipt.get("source_annotation_dirty") or
            set(binding) != {"artifacts", "caps", "dirty_source", "intent", "source_head"} or
            not isinstance(binding.get("artifacts"), dict)):
        refuse()
    artifact_hashes = binding["artifacts"]
    expected_artifacts = {
        "hello.z64", "t64-cold-boot-probe", "test_boot_determinism", "test_cold_boot",
    }
    if set(artifact_hashes) != expected_artifacts:
        refuse()
    artifact_rows = receipt["artifact_binding"]["artifacts"]
    if {row["name"]: row["sha256"] for row in artifact_rows} != artifact_hashes:
        refuse()
    artifact_bytes = {}
    for name, digest in artifact_hashes.items():
        if not isinstance(digest, str) or not HEX64.fullmatch(digest):
            refuse()
        data = _public_file(execution_root / "inputs" / name,
                            maximum=512 * 1024 * 1024)
        if sha256(data) != digest:
            refuse()
        artifact_bytes[name] = data
    if sha256(binding_bytes) != receipt["artifact_binding"]["sha256"]:
        refuse()

    log_manifest_bytes = _public_file(execution_root / "log-sha256.json",
                                      maximum=16 * 1024)
    log_manifest = parse_json(log_manifest_bytes)
    if set(log_manifest) != {"LastTest.log", "ctest.log"}:
        refuse()
    last_test_bytes = _public_file(execution_root / "LastTest.log", maximum=1024 * 1024)
    ctest_bytes = _public_file(execution_root / "ctest.log", maximum=64 * 1024)
    if (log_manifest.get("LastTest.log") != sha256(last_test_bytes) or
            log_manifest.get("ctest.log") != sha256(ctest_bytes) or
            execution.get("last_test_log_sha256") != sha256(last_test_bytes) or
            execution.get("ctest_log_sha256") != sha256(ctest_bytes) or
            execution.get("log_manifest_sha256") != sha256(log_manifest_bytes)):
        refuse()
    try:
        ctest_text = ctest_bytes.decode("utf-8")
    except UnicodeError:
        refuse()
    test_names = set(re.findall(
        r"^\s*\d+/\d+ Test #\d+: ([^\s]+) .*?\s+Passed\b",
        ctest_text, re.MULTILINE))
    expected_tests = {"integration.cold-hello", "integration.cold-startup"}
    if (test_names != expected_tests or
            not re.search(r"100% tests passed(?:, 0 tests failed)? out of 2", ctest_text)):
        refuse()
    sections = _last_test_sections(last_test_bytes)
    if set(sections) != expected_tests or not all(row["passed"] for row in sections.values()):
        refuse()
    attempts = []
    original_attempts = {}
    for test_name in ("integration.cold-hello", "integration.cold-startup"):
        section = sections[test_name]
        try:
            wrapper_argv = shlex.split(section["command"])
        except (TypeError, ValueError):
            refuse()
        if (len(wrapper_argv) != 4 or pathlib.Path(wrapper_argv[0]).name != "test_cold_boot" or
                pathlib.Path(wrapper_argv[2]).name != "t64-cold-boot-probe" or
                pathlib.Path(wrapper_argv[3]).name != "tendies64" or
                not isinstance(section.get("directory"), str) or
                not pathlib.Path(section["directory"]).is_absolute()):
            refuse()
        expected_mode = "--integration" if test_name.endswith("hello") else "--startup"
        if wrapper_argv[1] != expected_mode:
            refuse()
        attempt = _find_original_attempt(repository_root, test_name, section)
        attempts.append(attempt["receipt"])
        original_attempts[test_name] = attempt
    if execution.get("attempts") != attempts:
        refuse()

    hello_output = original_attempts["integration.cold-hello"]["child_output"]
    try:
        hello_lines = hello_output.decode("utf-8").splitlines()
        startup_lines = original_attempts["integration.cold-startup"]["child_output"].decode(
            "utf-8").splitlines()
    except UnicodeError:
        refuse()
    identities = [_record_fields(line)[1] for line in hello_lines
                  if line.startswith("comparison-identity ")]
    if len(identities) != 1:
        refuse()
    identity = identities[0]
    if (identity.get("fixture-a") != "hello-openipl3-rdram-a-t1" or
            identity.get("fixture-b") != "hello-openipl3-rdram-b-t1" or
            identity.get("build-id") != execution["build_id"] or
            identity.get("source-revision") != execution["source_revision"] or
            identity.get("compiler-policy") != execution["compiler_policy"] or
            not isinstance(identity.get("rom-sha256"), str) or
            not HEX64.fullmatch(identity["rom-sha256"]) or
            not isinstance(identity.get("ipl3-payload-sha256"), str) or
            not HEX64.fullmatch(identity["ipl3-payload-sha256"]) or
            identity["rom-sha256"] != binding["artifacts"]["hello.z64"] or
            execution["build_id"].encode() not in artifact_bytes["t64-cold-boot-probe"]):
        refuse()
    startup_rows = [_record_fields(line)[1] for line in startup_lines
                    if line.startswith("classification=observed ")]
    if len(startup_rows) != 1:
        refuse()
    startup = startup_rows[0]
    required_startup = {
        "profile": "ntsc-cart-4m-openipl3-rdram-b-t1-v1",
        "operation": "read", "address": "0x0470000c", "width": "4",
        "value": "0x00000000", "pc": "0xa4000094", "opcode": "0x8c42000c",
        "stop": "guest-tick-bound", "output_bytes": "30", "dropped": "0",
    }
    if any(startup.get(key) != value for key, value in required_startup.items()):
        refuse()
    policies = [line for line in hello_lines if line.startswith("terminal-policy=")]
    summaries = [_record_fields(line)[1] for line in hello_lines
                 if line.startswith("classification=compare-pass ")]
    if (policies != ["terminal-policy=authenticated-architectural-break-v1"] * 2 or
            len(summaries) != 1 or summaries[0].get("comparison") != "equal"):
        refuse()
    summary = summaries[0]
    expected_profiles = {
        "ntsc-cart-4m-openipl3-rdram-a-t1-v1": "hello-openipl3-rdram-a-t1",
        "ntsc-cart-4m-openipl3-rdram-b-t1-v1": "hello-openipl3-rdram-b-t1",
    }
    profile_hashes = {}
    for line in hello_lines:
        if line.startswith("profile-identity "):
            fields = _record_fields(line)[1]
            if fields.get("profile") in profile_hashes:
                refuse()
            profile_hashes[fields.get("profile")] = fields.get("sha256")
    profile_records = {}
    boot_records = {}
    cpu_records = {}
    pi_records = {}
    retirement_records = {}
    for line in hello_lines:
        prefix, fields = _record_fields(line)
        profile_id = fields.get("profile")
        if line.startswith("profile="):
            profile_id = line.split(None, 1)[0].split("=", 1)[1]
            fields["profile"] = profile_id
            if profile_id in profile_records:
                refuse()
            profile_records[profile_id] = fields
        elif prefix == "boot-observation":
            if profile_id in boot_records:
                refuse()
            boot_records[profile_id] = fields
        elif prefix == "cpu-state":
            if profile_id in cpu_records:
                refuse()
            cpu_records[profile_id] = fields
        elif prefix == "pi-state":
            if profile_id in pi_records:
                refuse()
            pi_records[profile_id] = fields
        elif prefix == "retirement-tail":
            if profile_id in retirement_records:
                refuse()
            retirement_records[profile_id] = fields
    if (set(profile_records) != set(expected_profiles) or
            set(profile_hashes) != set(expected_profiles) or
            set(boot_records) != set(expected_profiles) or
            set(cpu_records) != set(expected_profiles) or
            set(pi_records) != set(expected_profiles) or
            set(retirement_records) != set(expected_profiles)):
        refuse()
    hello_hex = summary.get("hello-hex")
    if (not isinstance(hello_hex, str) or not re.fullmatch(r"(?:[0-9a-f]{2})+", hello_hex)):
        refuse()
    output_bytes = bytes.fromhex(hello_hex)
    if output_bytes != b"Tendies64 hello via open IPL3\n":
        refuse()
    if (summary.get("terminal") is None or summary.get("last-retired") is None or
            summary.get("terminal-in-delay-slot") != "0"):
        refuse()
    terminal_pair = summary["terminal"].split("/")
    last_pair = summary["last-retired"].split("/")
    if len(terminal_pair) != 2 or len(last_pair) != 2:
        refuse()
    profiles = []
    fixture_rows = {row["id"]: row for row in receipt["fixture_manifest"]["fixtures"]}
    manifest_rows = {row.get("id"): row for row in manifest.get("entries", [])
                     if isinstance(row, dict)}
    for profile_id, fixture_id in expected_profiles.items():
        runtime = profile_records[profile_id]
        boot = boot_records[profile_id]
        cpu = cpu_records[profile_id]
        tail = retirement_records[profile_id]
        fixture = fixture_rows.get(fixture_id)
        manifest_row = manifest_rows.get(fixture_id)
        profile_hash = profile_hashes[profile_id]
        if (fixture is None or manifest_row is None or
                manifest_row.get("profile") != profile_id or
                fixture.get("profile_id") != profile_id or
                fixture.get("expected_kind") != "self-checking" or
                fixture.get("oracle_revision") != manifest_row.get("expected", {}).get("revision") or
                fixture.get("rom_sha256") != identity["rom-sha256"] or
                manifest_row.get("sha256") != identity["rom-sha256"] or
                runtime.get("stop") != "qualified-terminal" or
                runtime.get("completion-seen") != "1" or
                runtime.get("terminal-in-delay-slot") != "0" or
                runtime.get("protocol-trace-complete") != ("1" if "-b-t1-" in profile_id else "0") or
                runtime.get("protocol-state-complete") != ("1" if "-b-t1-" in profile_id else "0") or
                runtime.get("output-exact") != "1" or runtime.get("output-lost") != "0" or
                runtime.get("hello-hex") != hello_hex or
                runtime.get("terminal") != summary.get("terminal") or
                runtime.get("last-retired") != summary.get("last-retired") or
                boot.get("header-entry-retired") != "1" or
                cpu.get("profile") != profile_id or pi_records[profile_id].get("profile") != profile_id or
                not isinstance(profile_hash, str) or not HEX64.fullmatch(profile_hash)):
            refuse()
        try:
            terminal_tick = int(runtime["stop_tick"])
            retired = int(runtime["retired"])
            header_tick = int(boot["tick"])
            output_count = int(runtime["output-bytes"])
            terminal_pc, terminal_op = (int(value, 16) for value in terminal_pair)
            last_pc, last_op = (int(value, 16) for value in last_pair)
            tail_count = int(tail["count"])
            tail_values = [token for token in next(
                line for line in hello_lines if line.startswith("retirement-tail profile=" + profile_id)
            ).split()[3:]]
            tail_pairs = [tuple(int(value, 16) for value in token.split("/"))
                          for token in tail_values]
        except (KeyError, StopIteration, ValueError):
            refuse()
        if (terminal_tick <= 0 or retired <= 0 or header_tick <= 0 or
                output_count != len(output_bytes) or not tail_count or
                tail_count != len(tail_pairs) or tail_pairs[-1] != (last_pc, last_op) or
                int(runtime["terminal-in-delay-slot"]) != 0 or
                int(runtime["completion-seen"]) != 1 or

                int(runtime["output-exact"]) != 1 or int(runtime["output-lost"]) != 0 or
                cpu.get("stopped") != "0" or cpu.get("attempt-outcome") != "2" or
                cpu.get("exception") != "1/9/badva:0x0/epc:0xffffffff80000478/bd:0" or
                terminal_pc != 0x80000478 or terminal_op != 0x0000000d or
                last_pc != 0x80000474 or last_op != 0xa50e0016):
            refuse()
        profiles.append({
            "boot_fail": 0, "evidence_tier": "self-checking-fixture",
            "fixture_id": fixture_id, "guest_fail": 0, "harness_error": 0,
            "header_entry_tick": header_tick, "last_retired_opcode": last_op,
            "last_retired_pc": last_pc, "missing": 0,
            "oracle_kind": "authored-literal-transcript",
            "oracle_revision": fixture["oracle_revision"],
            "output_bytes": output_count, "output_sha256": sha256(output_bytes),
            "output_text": output_bytes.decode("utf-8"), "pass": 1,
            "profile_id": profile_id, "profile_sha256": profile_hash,
            "retired_instructions": retired, "rom_sha256": identity["rom-sha256"],
            "skipped": 0, "terminal_opcode": terminal_op, "terminal_pc": terminal_pc,
            "terminal_tick": terminal_tick, "timeout": 0, "total": 1,
        })
    if (summary.get("terminal") != "/".join(
            f"0x{int(value, 16):08x}" for value in terminal_pair) or
            summary.get("last-retired") != "/".join(
                f"0x{int(value, 16):08x}" for value in last_pair) or
            summary.get("hello-hex") != hello_hex or
            output_bytes.hex() != hello_hex or
            sha256(output_bytes) != profiles[0]["output_sha256"] or
            profiles[0]["output_sha256"] != profiles[1]["output_sha256"]):
        refuse()

    source_paths = (
        "fixtures/hello/hello.S", "fixtures/hello/hello.ld",
        "tools/fixture/t64_rom_pack.c", "scripts/build-boot-fixtures.sh",
    )
    oracle_digest = hashlib.sha256()
    for relative in source_paths:
        oracle_digest.update(_public_file(repository_root / relative, maximum=1024 * 1024))
    for fixture in receipt["fixture_manifest"]["fixtures"]:
        source_row = manifest_rows.get(fixture["id"])
        if (source_row is None or source_row.get("binary_rights") != "cleared" or
                source_row.get("expected", {}).get("revision") !=
                    "source-content-sha256:" + oracle_digest.hexdigest()):
            refuse()
    if (len(artifact_bytes["t64-cold-boot-probe"]) == 0 or
            output_bytes not in artifact_bytes["t64-cold-boot-probe"]):
        refuse()
    execution_expected = {
        "attempts": attempts,
        "binding_sha256": sha256(binding_bytes),
        "build_id": identity["build-id"],
        "compiler_policy": identity["compiler-policy"],
        "ctest_log_sha256": sha256(ctest_bytes),
        "last_test_log_sha256": sha256(last_test_bytes),
        "log_manifest_sha256": sha256(log_manifest_bytes),
        "path": AUTHORED_EXECUTION.as_posix(),
        "source_revision": identity["source-revision"],
    }
    return execution_expected, profiles


def validate_public_schema(receipt: object) -> None:
    """Reject unapproved public fields and aliases before any evidence is read."""
    exact(receipt, PUBLIC_RECEIPT_KEYS)
    _privacy_scan(receipt)
    if (receipt.get("schema") != "t64.phase06.final-evidence" or
            type(receipt.get("schema_version")) is not int or receipt["schema_version"] != 1 or
            not isinstance(receipt.get("project_version"), str) or
            not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", receipt["project_version"]) or
            not isinstance(receipt.get("core_build_id"), str) or
            not re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", receipt["core_build_id"]) or
            not isinstance(receipt.get("source_annotation"), str) or
            not HEX40.fullmatch(receipt["source_annotation"]) or
            type(receipt.get("source_annotation_dirty")) is not bool):
        refuse()
    binding = exact(receipt.get("artifact_binding"), PUBLIC_BINDING_KEYS)
    manifest = exact(receipt.get("fixture_manifest"), PUBLIC_MANIFEST_KEYS)
    for reference, expected_path in ((binding, AUTHORED_BINDING.as_posix()),
                                     (manifest, AUTHORED_MANIFEST.as_posix())):
        if (reference.get("path") != expected_path or
                not isinstance(reference.get("sha256"), str) or
                not HEX64.fullmatch(reference["sha256"])):
            refuse()
    artifact_rows = binding.get("artifacts")
    if not isinstance(artifact_rows, list) or not artifact_rows:
        refuse()
    artifact_names = set()
    for row in artifact_rows:
        exact(row, PUBLIC_ARTIFACT_KEYS)
        if (not isinstance(row.get("name"), str) or row["name"] in artifact_names or
                pathlib.PurePosixPath(row["name"]).name != row["name"] or
                not isinstance(row.get("sha256"), str) or not HEX64.fullmatch(row["sha256"])):
            refuse()
        artifact_names.add(row["name"])
    fixture_rows = manifest.get("fixtures")
    expected_ids = {"hello-openipl3-rdram-a-t1", "hello-openipl3-rdram-b-t1"}
    if (not isinstance(fixture_rows, list) or len(fixture_rows) != len(expected_ids) or
            {row.get("id") for row in fixture_rows if isinstance(row, dict)} != expected_ids):
        refuse()
    for row in fixture_rows:
        exact(row, PUBLIC_FIXTURE_KEYS)
        if (row.get("expected_kind") != "self-checking" or
                not isinstance(row.get("profile_id"), str) or
                not isinstance(row.get("oracle_revision"), str) or
                not isinstance(row.get("rom_sha256"), str) or
                not HEX64.fullmatch(row["rom_sha256"])):
            refuse()
    profiles = receipt.get("profiles")
    if not isinstance(profiles, list) or len(profiles) != 2:
        refuse()
    profile_ids = set()
    for profile in profiles:
        exact(profile, PUBLIC_PROFILE_KEYS)
        if (not isinstance(profile.get("profile_id"), str) or
                profile["profile_id"] in profile_ids or
                not isinstance(profile.get("fixture_id"), str) or
                not isinstance(profile.get("profile_sha256"), str) or
                not HEX64.fullmatch(profile["profile_sha256"]) or
                not isinstance(profile.get("rom_sha256"), str) or
                not HEX64.fullmatch(profile["rom_sha256"]) or
                not isinstance(profile.get("output_text"), str) or
                not isinstance(profile.get("output_sha256"), str) or
                not HEX64.fullmatch(profile["output_sha256"]) or
                not isinstance(profile.get("evidence_tier"), str) or
                not isinstance(profile.get("oracle_kind"), str) or
                not isinstance(profile.get("oracle_revision"), str) or
                profile.get("not_claimed") != AUTHORED_EXCLUSIONS or
                any(type(profile.get(key)) is not int for key in (
                    "header_entry_tick", "terminal_opcode", "terminal_pc",
                    "last_retired_opcode", "last_retired_pc", "terminal_tick", "retired_instructions",
                    "output_bytes", "pass", "total", "guest_fail", "boot_fail",
                    "timeout", "missing", "harness_error", "skipped"))):
            refuse()
        profile_ids.add(profile["profile_id"])
    if profile_ids != {row["profile_id"] for row in fixture_rows}:
        refuse()
    _privacy_scan(receipt)


def validate_public_receipt(repository_root: pathlib.Path = ROOT) -> dict:
    """Check the closed authored receipt against its exact manifest and archive."""
    receipt_path = repository_root / "docs/evidence/phase06-final.json"
    receipt = parse_json(_public_file(receipt_path, maximum=1024 * 1024))
    validate_public_schema(receipt)
    if (receipt.get("schema") != "t64.phase06.final-evidence" or
            type(receipt.get("schema_version")) is not int or
            receipt["schema_version"] != 1 or
            not isinstance(receipt.get("project_version"), str) or
            not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", receipt["project_version"]) or
            not isinstance(receipt.get("core_build_id"), str) or
            not re.fullmatch(r"t64-build-v1-[0-9a-f]{64}", receipt["core_build_id"]) or
            type(receipt.get("source_annotation_dirty")) is not bool):
        refuse()
    binding_reference = exact(receipt.get("artifact_binding"), PUBLIC_BINDING_KEYS)
    manifest_reference = exact(receipt.get("fixture_manifest"), PUBLIC_MANIFEST_KEYS)
    if (binding_reference.get("path") != AUTHORED_BINDING.as_posix() or
            manifest_reference.get("path") != AUTHORED_MANIFEST.as_posix() or
            not isinstance(binding_reference.get("sha256"), str) or
            not HEX64.fullmatch(binding_reference["sha256"]) or
            not isinstance(manifest_reference.get("sha256"), str) or
            not HEX64.fullmatch(manifest_reference["sha256"])):
        refuse()

    binding_bytes = _public_file(repository_root / AUTHORED_BINDING, maximum=1024 * 1024)
    binding = parse_json(binding_bytes)
    if (sha256(binding_bytes) != binding_reference["sha256"] or
            set(binding) != {"artifacts", "caps", "dirty_source", "intent", "source_head"} or
            binding.get("dirty_source") is not receipt["source_annotation_dirty"] or
            binding.get("source_head") != receipt["source_annotation"]):
        refuse()
    artifact_rows = binding_reference.get("artifacts")
    if not isinstance(artifact_rows, list):
        refuse()
    actual_artifacts = binding.get("artifacts")
    if not isinstance(actual_artifacts, dict) or len(artifact_rows) != len(actual_artifacts):
        refuse()
    seen_artifacts = set()
    for item in artifact_rows:
        exact(item, PUBLIC_ARTIFACT_KEYS)
        name, digest = item.get("name"), item.get("sha256")
        if (not isinstance(name, str) or name in seen_artifacts or
                pathlib.PurePosixPath(name).name != name or
                not isinstance(digest, str) or not HEX64.fullmatch(digest) or
                actual_artifacts.get(name) != digest):
            refuse()
        path = repository_root / AUTHORED_BINDING.parent / name
        if _file_sha256(path) != digest:
            refuse()
        seen_artifacts.add(name)
    if seen_artifacts != set(actual_artifacts):
        refuse()

    manifest_bytes = _public_file(repository_root / AUTHORED_MANIFEST, maximum=4 * 1024 * 1024)
    if sha256(manifest_bytes) != manifest_reference["sha256"]:
        refuse()
    manifest = parse_json(manifest_bytes)
    if manifest.get("schema") != "t64.manifest" or not isinstance(manifest.get("entries"), list):
        refuse()
    expected_ids = {"hello-openipl3-rdram-a-t1", "hello-openipl3-rdram-b-t1"}
    manifest_rows = {row.get("id"): row for row in manifest["entries"] if isinstance(row, dict)}
    fixture_rows = manifest_reference.get("fixtures")
    if not isinstance(fixture_rows, list) or len(fixture_rows) != len(expected_ids):
        refuse()
    if {row.get("id") for row in fixture_rows if isinstance(row, dict)} != expected_ids:
        refuse()

    execution, observed_profiles = _validate_original_coldboot_evidence(
        repository_root, receipt, manifest)
    if (execution != receipt["execution_record"] or
            execution["build_id"] != receipt["core_build_id"] or
            execution["source_revision"] != receipt["source_annotation"]):
        refuse()
    if not re.search(rf"project\(tendies64 VERSION {re.escape(receipt['project_version'])} LANGUAGES C\)",
                     (repository_root / "CMakeLists.txt").read_text()):
        refuse()
    observed = {row["profile_id"]: row for row in observed_profiles}
    claim_values = {}
    for profile in receipt["profiles"]:
        original = observed.get(profile["profile_id"])
        if (original is None or
                {k: v for k, v in profile.items() if k != "not_claimed"} != original):
            refuse()
        claim_values[profile["profile_id"]] = _render_claim(profile, receipt)

    claims_text = _public_file(repository_root / AUTHORED_CLAIMS, maximum=256 * 1024).decode("utf-8")
    blocks = re.findall(r"^```t64-claim\n(.*?)^```$", claims_text, re.MULTILINE | re.DOTALL)
    if len(blocks) != 2:
        refuse()
    checked_claims = set()
    for block in blocks:
        values = {}
        for line in block.splitlines():
            if ":" not in line:
                refuse()
            key, value = line.split(":", 1)
            if key in values:
                refuse()
            values[key] = value.lstrip()
        exact(values, CLAIM_KEYS)
        expected = claim_values.get(values.get("profile_id"))
        if expected is None or values != expected or values["profile_id"] in checked_claims:
            refuse()
        checked_claims.add(values["profile_id"])

    _privacy_scan(receipt)
    return {"schema": "t64.phase06-authored-evidence", "schema_version": 1,
            "status": "verified"}


def read_private(path: pathlib.Path, *, maximum: int = MAX_JSON,
                 minimum: int = 1) -> bytes:
    flags = (os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) |
             getattr(os, "O_NONBLOCK", 0))
    try:
        descriptor = os.open(path, flags)
    except OSError:
        refuse()
    try:
        before = os.fstat(descriptor)
        if (not stat.S_ISREG(before.st_mode) or
                stat.S_IMODE(before.st_mode) != 0o600 or
                before.st_uid != os.geteuid() or before.st_size < minimum or
                before.st_size > maximum):
            refuse()
        chunks = []
        total = 0
        while True:
            chunk = os.read(descriptor, min(1024 * 1024, maximum + 1 - total))
            if not chunk:
                break
            total += len(chunk)
            if total > maximum:
                refuse()
            chunks.append(chunk)
        after = os.fstat(descriptor)
        if ((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) !=
                (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) or
                total != before.st_size):
            refuse()
        return b"".join(chunks)
    finally:
        os.close(descriptor)


def _load_verifier(repository_root: pathlib.Path):
    path = repository_root / "scripts/verify-base-diagnostic-qualification.py"
    spec = importlib.util.spec_from_file_location("t64_base_evidence_verifier", path)
    if spec is None or spec.loader is None:
        refuse()
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _context_ref(value: object, path: str) -> str:
    if (not isinstance(value, dict) or set(value) != {"path", "sha256"} or
            value.get("path") != path or not isinstance(value.get("sha256"), str) or
            not HEX64.fullmatch(value["sha256"])):
        refuse()
    return value["sha256"]


def _independent_source_authority(context: dict, repository_root: pathlib.Path,
                                 projection_bytes: bytes) -> tuple[bytes, bytes]:
    """Derive historical BASE membership from the fixed CPU/RSP providers.

    This authenticates the guest-source closure and projection without requiring
    old host-runner bindings to match the current checkout.
    """
    if context.get("schema_version") != 2:
        refuse()
    verifier = _load_verifier(repository_root)
    projection = parse_canonical(projection_bytes)
    cpu_provider_path = repository_root / "scripts/verify-cpu-diagnostic-qualification.py"
    if cpu_provider_path.is_symlink() or not cpu_provider_path.is_file():
        refuse()
    spec = importlib.util.spec_from_file_location("t64_base_evidence_cpu_provider",
                                                  cpu_provider_path)
    if spec is None or spec.loader is None:
        refuse()
    cpu_provider = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(cpu_provider)
    except Exception:
        refuse()
    body = {key: value for key, value in projection.items()
            if key != "projection_sha256"}
    if (projection.get("schema") != "t64.cpu-source-projection" or
            projection.get("schema_version") != 2 or
            projection.get("source_revision") != context.get("source_revision") or
            projection.get("source_revision") != cpu_provider.SOURCE_REVISION or
            projection.get("projection_sha256") != cpu_provider.SOURCE_PROJECTION_SHA256 or
            sha256(canonical(body)) != cpu_provider.SOURCE_PROJECTION_SHA256):
        refuse()

    # Only the guest source closure is compared with live source bytes. The
    # historical host/compiler/tool bindings remain authenticated by the saved
    # context/binding/snapshot tuple, not reinterpreted as current inputs.
    reviewed = projection.get("reviewed_source_file_sha256")
    testlist_sha = projection.get("testlist_sha256")
    source_rows = context.get("bindings", {}).get("source")
    if (not isinstance(reviewed, dict) or not isinstance(testlist_sha, str) or
            not HEX64.fullmatch(testlist_sha) or not isinstance(source_rows, list) or
            not source_rows):
        refuse()
    expected_source = dict(reviewed)
    expected_source.setdefault("src/tests/testlist.rs", testlist_sha)
    source_root = pathlib.PurePosixPath("build/private-cpu-diagnostic/nemu64-test")
    actual_source = {}
    for row in source_rows:
        if (not isinstance(row, dict) or set(row) != {"path", "sha256"} or
                not isinstance(row.get("path"), str) or
                not isinstance(row.get("sha256"), str) or
                not HEX64.fullmatch(row["sha256"]) or "\\" in row["path"]):
            refuse()
        path = pathlib.PurePosixPath(row["path"])
        if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
            refuse()
        try:
            relative = path.relative_to(source_root).as_posix()
        except ValueError:
            refuse()
        if relative in actual_source:
            refuse()
        actual_source[relative] = row["sha256"]
        source_path = verifier._project_path(
            repository_root, row["path"], "historical CPU source")
        if verifier._regular_digest(source_path) != row["sha256"]:
            refuse()
    if actual_source != expected_source:
        refuse()

    # The RSP provider independently derives its source membership, while the
    # existing component-closure helper binds CPU and RSP files to the saved
    # qualifications. Capture its generic status line; it is not evidence data.
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        rsp_projection = verifier._verify_rsp_source_authority(
            repository_root, context, projection, projection_bytes)
    deriver = verifier._load_deriver(repository_root)
    cpu_files, rsp_files = verifier._component_source_files(
        repository_root, context, deriver)
    members = deriver.derive_members(projection, rsp_projection, cpu_files, rsp_files)
    authority_bytes = canonical({
        "members": members,
        "schema": "t64.base-source-authority",
        "schema_version": 1,
        "source_revision": context["source_revision"],
    })
    inventory_bytes = canonical({
        "members": members,
        "schema": "t64.base-inventory",
        "schema_version": 1,
        "source_authority_sha256": sha256(authority_bytes),
        "source_revision": context["source_revision"],
    })
    return authority_bytes, inventory_bytes


def validate_artifact_pair(context_root: pathlib.Path,
                           repository_root: pathlib.Path,
                           *, require_live_executable: bool = False,
                           snapshot_root: pathlib.Path | None = None) -> dict:
    """Check the exact retained executable/context/binding and bound inventories."""
    context_root = pathlib.Path(context_root)
    try:
        root_info = context_root.lstat()
    except OSError:
        refuse()
    if stat.S_ISLNK(root_info.st_mode) or not stat.S_ISDIR(root_info.st_mode):
        refuse()
    context_root = context_root.resolve(strict=True)
    repository_root = repository_root.resolve(strict=True)
    root_info = context_root.stat(follow_symlinks=False)
    if (not stat.S_ISDIR(root_info.st_mode) or stat.S_IMODE(root_info.st_mode) != 0o700 or
            root_info.st_uid != os.geteuid()):
        refuse()

    context_bytes = read_private(context_root / "context.json", maximum=2 * 1024 * 1024)
    binding_bytes = read_private(context_root / "binding.json", maximum=64 * 1024)
    context = parse_canonical(context_bytes)
    binding = parse_canonical(binding_bytes)
    version = context.get("schema_version")
    if (type(version) is not int or version not in {1, 2} or
            set(context) != (CONTEXT_BASE_KEYS if version == 1 else CONTEXT_V2_KEYS) or
            context.get("schema") != "t64.base-qualification-context" or
            not isinstance(context.get("source_revision"), str) or
            not HEX40.fullmatch(context["source_revision"]) or
            not isinstance(context.get("profile"), (str, dict))):
        refuse()
    if (set(binding) != BINDING_KEYS or
            binding.get("schema") != "t64.base-qualification-binding" or
            binding.get("schema_version") != 1 or
            binding.get("context_sha256") != sha256(context_bytes)):
        refuse()

    authority_sha = _context_ref(
        context.get("source_authority"), "build/private-base-diagnostic/source-authority.json")
    inventory_sha = _context_ref(
        context.get("inventory"), "build/private-base-diagnostic/inventory.json")
    projection_sha = _context_ref(
        context.get("source_projection"), "build/private-cpu-diagnostic/source-projection.json")
    if (binding.get("source_authority_sha256") != authority_sha or
            binding.get("inventory_sha256") != inventory_sha or
            binding.get("source_projection_sha256") != projection_sha):
        refuse()

    authority_path = context_root / "source-authority.json"
    inventory_path = context_root / "inventory.json"
    authority_bytes = read_private(authority_path)
    inventory_bytes = read_private(inventory_path)
    authority = parse_canonical(authority_bytes)
    inventory = parse_canonical(inventory_bytes)
    if (set(authority) != AUTHORITY_KEYS or
            authority.get("schema") != "t64.base-source-authority" or
            authority.get("schema_version") != 1 or
            set(inventory) != INVENTORY_KEYS or
            inventory.get("schema") != "t64.base-inventory" or
            inventory.get("schema_version") != 1 or
            authority.get("source_revision") != context["source_revision"] or
            inventory.get("source_revision") != context["source_revision"] or
            authority.get("members") != inventory.get("members") or
            sha256(authority_bytes) != authority_sha or
            sha256(inventory_bytes) != inventory_sha or
            inventory.get("source_authority_sha256") != authority_sha or
            binding.get("source_authority_sha256") != sha256(authority_bytes) or
            binding.get("inventory_sha256") != sha256(inventory_bytes)):
        refuse()

    if repository_root == ROOT.resolve(strict=True):
        projection_path = repository_root / pathlib.Path(
            context["source_projection"]["path"])
        projection_bytes = read_private(projection_path, maximum=16 * 1024 * 1024)
        expected_authority, expected_inventory = _independent_source_authority(
            context, repository_root, projection_bytes)
        if authority_bytes != expected_authority or inventory_bytes != expected_inventory:
            refuse()

    verifier = _load_verifier(repository_root)
    verifier._verify_retained_execution_pair(
        snapshot_root or context_root, context, context_bytes, binding_bytes)
    executable_rows = context["bindings"].get("executable")
    if not isinstance(executable_rows, list) or len(executable_rows) != 1:
        refuse()
    executable = executable_rows[0]
    if (not isinstance(executable, dict) or set(executable) != {"path", "sha256"} or
            not isinstance(executable.get("path"), str) or
            not isinstance(executable.get("sha256"), str) or
            not HEX64.fullmatch(executable["sha256"])):
        refuse()
    if require_live_executable:
        live = repository_root / executable["path"]
        if live.is_symlink() or not live.is_file():
            refuse()
        try:
            live_bytes = live.read_bytes()
        except OSError:
            refuse()
        if sha256(live_bytes) != executable["sha256"]:
            refuse()

    return {"schema": "t64.base-evidence-artifacts", "schema_version": 1,
            "status": "verified"}


def _private_path(root: pathlib.Path, value: object) -> pathlib.Path:
    if (not isinstance(value, str) or not value or "\\" in value or
            any(part in {".", ".."} for part in value.split("/"))):
        refuse()
    path = pathlib.Path(value)
    if not path.is_absolute():
        refuse()
    root = root.resolve(strict=True)
    try:
        relative = path.relative_to(root)
    except ValueError:
        refuse()
    if not relative.parts:
        refuse()
    cursor = root
    for part in relative.parts:
        cursor = cursor / part
        if cursor.is_symlink():
            refuse()
    try:
        resolved = path.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError):
        refuse()
    return resolved


def _file_sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            while True:
                chunk = stream.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
    except OSError:
        refuse()
    return digest.hexdigest()


def _validate_batch(batch_root: pathlib.Path, repository_root: pathlib.Path,
                    expected_lane: str) -> None:
    batch_root = batch_root.resolve(strict=True)
    batch_info = batch_root.stat(follow_symlinks=False)
    if (not stat.S_ISDIR(batch_info.st_mode) or stat.S_IMODE(batch_info.st_mode) != 0o700 or
            batch_info.st_uid != os.geteuid()):
        refuse()
    validate_artifact_pair(
        batch_root, repository_root,
        snapshot_root=repository_root / PRIVATE_ROOT)
    context_bytes = read_private(batch_root / "context.json", maximum=2 * 1024 * 1024)
    binding = parse_canonical(read_private(batch_root / "binding.json", maximum=64 * 1024))
    context_sha = sha256(context_bytes)
    executable = batch_root / "consumer"
    try:
        executable_info = executable.lstat()
    except OSError:
        refuse()
    if (stat.S_ISLNK(executable_info.st_mode) or not stat.S_ISREG(executable_info.st_mode) or
            stat.S_IMODE(executable_info.st_mode) != 0o700 or
            executable_info.st_uid != os.geteuid()):
        refuse()
    context = parse_canonical(context_bytes)
    executable_rows = context.get("bindings", {}).get("executable")
    if (not isinstance(executable_rows, list) or len(executable_rows) != 1 or
            _file_sha256(executable) != executable_rows[0].get("sha256")):
        refuse()

    batch_receipt = parse_canonical(read_private(batch_root / "batch.json", maximum=64 * 1024))
    batch_keys = {
        "classification_exit_code", "classification_input", "classification_report",
        "context_sha256", "interrupted", "lane", "schema", "schema_version",
        "stop_reason", "stopped",
    }
    if (set(batch_receipt) != batch_keys or
            batch_receipt.get("schema") != "t64.base-batch" or
            batch_receipt.get("schema_version") != 1 or
            batch_receipt.get("context_sha256") != context_sha or
            batch_receipt.get("lane") != expected_lane or
            batch_receipt.get("interrupted") is not False or
            batch_receipt.get("stopped") is not False or
            batch_receipt.get("classification_exit_code") not in {0, 3}):
        refuse()
    for input_key in ("classification_input", "classification_report"):
        name = batch_receipt[input_key]
        if (not isinstance(name, str) or pathlib.PurePosixPath(name).name != name or
                name in {"", ".", ".."}):
            refuse()
    input_path = batch_root / batch_receipt["classification_input"]
    report_path = batch_root / batch_receipt["classification_report"]
    input_bytes = read_private(input_path, maximum=MAX_JSON)
    expected_report = read_private(report_path, maximum=MAX_JSON)
    manifest = parse_canonical(input_bytes)
    if (set(manifest) != {"members", "schema", "schema_version"} or
            manifest.get("schema") != "t64.base-classification-input" or
            manifest.get("schema_version") != 1 or
            not isinstance(manifest.get("members"), list)):
        refuse()
    record_root = (repository_root / PRIVATE_ROOT).resolve(strict=True)
    for row in manifest["members"]:
        if (not isinstance(row, dict) or set(row) != {
                "expected_rom_sha256", "original", "outcome", "reason", "record",
                "registration_index", "value_index"}):
            refuse()
        original = _private_path(record_root, row["original"])
        record = _private_path(record_root, row["record"])
        read_private(original, maximum=4096, minimum=0)
        read_private(record, maximum=8192)

    authority = batch_root / "source-authority.json"
    inventory = batch_root / "inventory.json"
    if (_file_sha256(authority) != binding.get("source_authority_sha256") or
            _file_sha256(inventory) != binding.get("inventory_sha256")):
        refuse()
    authority_sha = binding["source_authority_sha256"]
    before = _file_sha256(executable)
    private_root = (repository_root / PRIVATE_ROOT).resolve(strict=True)
    verification_manifest = {
        "members": [dict(row, outcome="record", reason="")
                    for row in manifest["members"]],
        "schema": "t64.base-classification-input", "schema_version": 1,
    }
    with tempfile.TemporaryDirectory(prefix="check-", dir=private_root) as temporary:
        temporary_root = pathlib.Path(temporary)
        manifest_path = temporary_root / "classification-input.json"
        manifest_path.write_bytes(canonical(verification_manifest))
        manifest_path.chmod(0o600)
        regenerated = temporary_root / "classification.json"
        command = [str(executable), "--classify-manifest", str(authority), authority_sha,
                   str(inventory), context_sha, str(manifest_path), str(regenerated)]
        try:
            completed = subprocess.run(
                command, cwd=batch_root, stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60,
                check=False, start_new_session=True,
                env={"CI": "", "T64_REQUIRE_FIXTURES": "0", "LC_ALL": "C",
                     "PATH": os.environ.get("PATH", "")},
            )
        except (OSError, subprocess.TimeoutExpired):
            refuse()
        if (_file_sha256(executable) != before or
                completed.returncode != batch_receipt["classification_exit_code"] or
                not regenerated.is_file()):
            refuse()
        reproduced = regenerated.read_bytes()
    reproduced_report = parse_canonical(reproduced)
    if (reproduced != expected_report or
            reproduced_report.get("schema") != "t64.base-classification" or
            reproduced_report.get("schema_version") != 1 or
            reproduced_report.get("context_sha256") != context_sha or
            reproduced_report.get("inventory_sha256") != binding["inventory_sha256"] or
            reproduced_report.get("source_complete") is not True):
        refuse()


def check_current(repository_root: pathlib.Path = ROOT) -> dict:
    """Reclassify every retained original batch using its exact archived C consumer."""
    private_root = repository_root / PRIVATE_ROOT
    summary = parse_canonical(read_private(
        private_root / "plan06-06-execution-summary.json", maximum=2 * 1024 * 1024))
    lanes = summary.get("lanes")
    if not isinstance(lanes, list):
        refuse()
    found = set()
    for row in lanes:
        if not isinstance(row, dict) or not isinstance(row.get("lane"), str):
            refuse()
        lane = row["lane"]
        if lane not in {"required", "informational", "stress"} or lane in found:
            refuse()
        relative = row.get("batch")
        relative_path = pathlib.PurePosixPath(relative) if isinstance(relative, str) else None
        if (relative_path is None or relative_path.is_absolute() or
                relative_path.parts[:2] != ("build", "private-base-diagnostic") or
                any(part in {"", ".", ".."} for part in relative_path.parts)):
            refuse()
        batch = repository_root / pathlib.Path(*relative_path.parts)
        private_root = (repository_root / PRIVATE_ROOT).resolve(strict=True)
        try:
            batch.resolve(strict=True).relative_to(private_root)
        except (OSError, ValueError):
            refuse()
        _validate_batch(batch, repository_root, lane)
        found.add(lane)
    if found != {"required", "informational", "stress"}:
        refuse()
    return {"schema": "t64.base-evidence-current", "schema_version": 1,
            "status": "verified"}


def check_preuse(repository_root: pathlib.Path = ROOT) -> dict:
    """Verify the live executable against its saved pre-use binding."""
    validate_artifact_pair(repository_root / PRIVATE_ROOT, repository_root,
                           require_live_executable=True)
    return {"schema": "t64.base-evidence-preuse", "schema_version": 1,
            "status": "verified"}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--check-current", action="store_true")
    modes.add_argument("--check-artifacts", action="store_true")
    modes.add_argument("--check-preuse", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.check_artifacts:
            validate_artifact_pair(ROOT / PRIVATE_ROOT, ROOT,
                                   require_live_executable=False)
            result = validate_public_receipt(ROOT)
        elif args.check_preuse:
            result = check_preuse()
        else:
            result = check_current()
        print(json.dumps(result, sort_keys=True, separators=(",", ":")))
        return 0
    except (OSError, Refusal, ValueError):
        print("BASE evidence refused", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Exercise the qualification gate with a local, synthetic accepted receipt."""

from __future__ import annotations

import hashlib
import importlib.util
import copy
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import tomllib

sys.dont_write_bytecode = True


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: pathlib.Path, value: object) -> None:
    path.write_text(json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n")
    path.chmod(0o600)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load_verifier(path: pathlib.Path):
    spec = importlib.util.spec_from_file_location("t64_qualification_verifier", path)
    require(spec is not None and spec.loader is not None, "cannot load production verifier")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def make_fake_compiler(path: pathlib.Path, log: pathlib.Path) -> None:
    source = r'''#!/usr/bin/env python3
import os
import pathlib
import sys

output = pathlib.Path(sys.argv[sys.argv.index("-o") + 1])
output.write_text(r"""#!/usr/bin/env python3
import pathlib
import sys

if "--selftest" in sys.argv:
    raise SystemExit(0)
args = sys.argv[1:]
output_path = pathlib.Path(args[args.index("--output") + 1])
payload_path = pathlib.Path(args[args.index("--payload") + 1])
output_path.write_bytes(bytes(4096) + payload_path.read_bytes())
""")
output.chmod(0o700)
with open(os.environ["T64_FAKE_TOOL_LOG"], "a", encoding="utf-8") as stream:
    stream.write("host-compiler\n")
'''
    path.write_text(source)
    path.chmod(0o700)


def make_fake_binutil(path: pathlib.Path) -> None:
    source = r'''#!/usr/bin/env python3
import os
import pathlib
import sys

name = pathlib.Path(sys.argv[0]).name.removeprefix("mips64-elf-")
args = sys.argv[1:]
with open(os.environ["T64_FAKE_TOOL_LOG"], "a", encoding="utf-8") as stream:
    stream.write(name + " " + " ".join(args) + "\n")
if "--version" in args:
    print(f"mips64-elf-{name} (GNU Binutils) 2.45")
elif name == "ld" and "-V" in args:
    print("elf32ebmip")
elif name in {"objcopy", "objdump"} and "--info" in args:
    print("elf32-bigmips")
elif name == "objdump" and "-f" in args:
    print("start address 0x80000400")
elif name in {"as", "ld"}:
    output = pathlib.Path(args[args.index("-o") + 1])
    output.write_bytes(b"authored tool fixture\n")
elif name == "objcopy":
    pathlib.Path(args[-1]).write_bytes(b"authored payload\n")
else:
    raise SystemExit(2)
'''
    path.write_text(source)
    path.chmod(0o700)


def sha_for_manifest(manifest: dict, name: str, target: str) -> str:
    return manifest["pkg"][name]["target"][target]["xz_hash"]


def main() -> int:
    repository = pathlib.Path(sys.argv[1]).resolve(strict=True)
    private_root = repository / "build/private-cpu-diagnostic"
    source_checkout = private_root / "nemu64-test"
    projection_source = private_root / "source-projection.json"
    manifest_path = private_root / "rust-channel/channel-rust-nightly.toml"
    if not source_checkout.is_dir() or not projection_source.is_file():
        raise RuntimeError("pinned private source projection is unavailable")

    verifier_path = repository / "scripts/verify-cpu-diagnostic-qualification.py"
    verifier = load_verifier(verifier_path)
    manifest = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory(prefix="qualification-control.", dir=private_root) as temp_text:
        fixture = pathlib.Path(temp_text)
        checkout = fixture / "overlay-checkout"
        subprocess.run(
            ["git", "clone", "--local", "--quiet", str(source_checkout), str(checkout)],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        require(
            subprocess.run(
                ["git", "-C", str(checkout), "rev-parse", "HEAD"],
                check=True,
                text=True,
                stdout=subprocess.PIPE,
            ).stdout.strip()
            == verifier.SOURCE_REVISION,
            "synthetic overlay does not preserve the pinned source base",
        )
        with (checkout / "Cargo.toml").open("a", encoding="utf-8") as stream:
            stream.write("\n# Authored qualification-gate fixture only.\n")

        projection_path = fixture / "source-projection.json"
        shutil.copyfile(projection_source, projection_path)
        overlay_path = fixture / "authored-overlay-fixture.patch"
        overlay_path.write_bytes(b"authored fixture bytes; no upstream source context\n")
        fake_compiler = fixture / "fake-host-compiler"
        tool_log = fixture / "tool-invocations.log"
        make_fake_compiler(fake_compiler, tool_log)
        fake_tools: dict[str, pathlib.Path] = {}
        for name in sorted(verifier.REQUIRED_TARGET_TOOLS):
            tool_path = fixture / f"mips64-elf-{name}"
            make_fake_binutil(tool_path)
            fake_tools[name] = tool_path

        overlay_sha = digest(overlay_path)
        changed_files = [{"path": "Cargo.toml", "sha256": digest(checkout / "Cargo.toml")}]
        closure_path = fixture / "prebuild-closure.json"
        closure = {
            "changed_files": changed_files,
            "excluded_before_compile": sorted(verifier.REQUIRED_ASSETS),
            "overlay_checkout": str(checkout),
            "overlay_sha256": overlay_sha,
            "source_revision": verifier.SOURCE_REVISION,
            "status": "PASS_PREBUILD",
        }
        write_json(closure_path, closure)

        lock_path = checkout / "Cargo.lock"
        lock = tomllib.loads(lock_path.read_text(encoding="utf-8"))
        dependencies = []
        for package in lock["package"]:
            source = package.get("source")
            if source is None:
                continue
            require(source.startswith("registry+"), "fixture lock contains a non-registry source")
            archive = private_root / "crates" / f"{package['name']}-{package['version']}.crate"
            dependencies.append(
                {
                    "archive": {"path": str(archive), "sha256": digest(archive)},
                    "build_hook_reviewed": True,
                    "checksum": package["checksum"],
                    "legitimacy": "verified",
                    "license_reviewed": True,
                    "name": package["name"],
                    "proc_macro_reviewed": True,
                    "source_reviewed": True,
                    "version": package["version"],
                }
            )

        compiler = {
            "path": str(fake_compiler),
            "sha256": digest(fake_compiler),
        }
        binutils = [
            {"name": name, "path": str(path), "sha256": digest(path)}
            for name, path in sorted(fake_tools.items())
        ]
        archives = []
        archive_paths = {
            "rustc": private_root / "root-host-tool-audit/rustc-nightly-aarch64-apple-darwin.tar.xz",
            "cargo": private_root / "root-host-tool-audit/cargo-nightly-aarch64-apple-darwin.tar.xz",
            "rust-std": private_root / "root-host-tool-audit/rust-std-nightly-aarch64-apple-darwin.tar.xz",
            "rust-src": private_root / "root-rust-src-audit/rust-src-nightly.tar.xz",
        }
        for name, path in archive_paths.items():
            target = "*" if name == "rust-src" else "aarch64-apple-darwin"
            official_sha = sha_for_manifest(manifest, name, target)
            archives.append(
                {
                    "archive": {"path": str(path), "sha256": digest(path)},
                    "manifest_sha256": official_sha,
                    "name": name,
                    "target": target,
                }
            )

        receipt = {
            "assets": {
                "closure_report": {"path": str(closure_path), "sha256": digest(closure_path)},
                "excluded_before_compile": sorted(verifier.REQUIRED_ASSETS),
                "prebuild_closure_reviewed": True,
            },
            "checks": {name: "pass" for name in sorted(verifier.REQUIRED_CHECKS)},
            "dependencies": dependencies,
            "project": {
                "locked": True,
                "lockfile": {"path": str(lock_path), "sha256": digest(lock_path)},
                "manifest": {
                    "path": str(checkout / "Cargo.toml"),
                    "sha256": digest(checkout / "Cargo.toml"),
                },
                "resolution_sha256": digest(lock_path),
            },
            "schema": "t64.cpu-qualification",
            "schema_version": 1,
            "source": {
                "checkout": str(source_checkout),
                "membership_sealed": True,
                "overlay": {"path": str(overlay_path), "sha256": overlay_sha},
                "overlay_checkout": str(checkout),
                "overlay_sha256": overlay_sha,
                "projection": {
                    "path": str(projection_path),
                    "projection_sha256": verifier.SOURCE_PROJECTION_SHA256,
                    "sha256": digest(projection_path),
                },
            },
            "source_revision": verifier.SOURCE_REVISION,
            "status": "qualified",
            "toolchain": {
                "archives": archives,
                "channel": "nightly-2026-07-16",
                "host_target": "aarch64-apple-darwin",
                "manifest": {"path": str(manifest_path), "sha256": digest(manifest_path)},
            },
            "tools": {"host_c_compiler": compiler, "target_binutils": binutils},
        }
        receipt_path = fixture / "qualification.json"
        write_json(receipt_path, receipt)

        env = os.environ.copy()
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        env["CC"] = str(fake_compiler)
        env["T64_CPU_DIAGNOSTIC_QUALIFICATION_ROOT"] = str(fixture)
        env["T64_FAKE_TOOL_LOG"] = str(tool_log)
        for name, path in fake_tools.items():
            env[f"T64_MIPS_{name.upper()}"] = str(path)

        build_script = repository / "scripts/build-cpu-diagnostic.sh"
        packer_path = private_root / "t64_rom_pack"
        original_packer = packer_path.read_bytes() if packer_path.exists() else None
        original_packer_mode = packer_path.stat().st_mode & 0o777 if packer_path.exists() else None
        try:
            accepted = subprocess.run(
                ["/bin/sh", str(build_script), "--qualify"],
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                env=env,
            )
        finally:
            if original_packer is None:
                packer_path.unlink(missing_ok=True)
            else:
                packer_path.write_bytes(original_packer)
                packer_path.chmod(original_packer_mode)
        require(accepted.returncode == 0, f"accepted authored baseline failed: {accepted.stdout}")
        require("pre-use qualification accepted" in accepted.stdout, "baseline did not pass the gate")
        invocations = tool_log.read_text(encoding="utf-8").splitlines()
        for name in fake_tools:
            require(any(row.startswith(name + " ") for row in invocations), f"accepted baseline did not reach {name}")
        require(any("--version" in row for row in invocations), "accepted baseline did not reach version probes")

        refusal_cases = [
            (
                "qualification status",
                "qualification status is not qualified",
                lambda item: item.update(status="unqualified"),
            ),
            (
                "wrong Rust archive SHA",
                "rustc archive SHA-256 differs from the dated manifest",
                lambda item: next(
                    row for row in item["toolchain"]["archives"] if row["name"] == "rustc"
                ).update(manifest_sha256="0" * 64),
            ),
            (
                "missing Cargo.lock",
                "packaged Cargo.lock is missing or outside the repository",
                lambda item: item["project"]["lockfile"].update(
                    path=str(fixture / "missing-Cargo.lock")
                ),
            ),
            (
                "changed Cargo resolution",
                "resolved dependency graph differs from the packaged Cargo.lock",
                lambda item: item["project"].update(resolution_sha256="0" * 64),
            ),
            (
                "unreviewed build hook",
                "a registry dependency is unverified or has an unreviewed hook",
                lambda item: item["dependencies"][0].update(build_hook_reviewed=False),
            ),
            (
                "uncleared font",
                "uncleared font, graphics, framebuffer, or non-CPU RSP startup remains",
                lambda item: item["assets"].update(
                    excluded_before_compile=[
                        asset
                        for asset in item["assets"]["excluded_before_compile"]
                        if asset != "font"
                    ]
                ),
            ),
            (
                "source projection seal",
                "source projection digest differs from the source-derived prebuild seal",
                lambda item: item["source"]["projection"].update(
                    projection_sha256="0" * 64
                ),
            ),
            (
                "host compiler hash",
                "host C compiler SHA-256 does not match the reviewed executable",
                lambda item: item["tools"]["host_c_compiler"].update(sha256="0" * 64),
            ),
        ]
        git_lock_path = fixture / "git-Cargo.lock"
        git_lock_path.write_bytes(
            lock_path.read_bytes()
            + b'\n[[package]]\nname = "unreviewed-git"\nversion = "1.0.0"\n'
            + b'source = "git+https://example.invalid/repo#0123456789abcdef"\n'
        )
        refusal_cases.append(
            (
                "unreviewed git lock source",
                "Cargo.lock contains an unreviewed git or non-registry source",
                lambda item: item["project"].update(
                    lockfile={"path": str(git_lock_path), "sha256": digest(git_lock_path)},
                    resolution_sha256=digest(git_lock_path),
                ),
            )
        )
        reached_refusals = []
        for name, expected_reason, mutate in refusal_cases:
            variant = copy.deepcopy(receipt)
            mutate(variant)
            write_json(receipt_path, variant)
            tool_log.unlink(missing_ok=True)
            result = subprocess.run(
                ["/bin/sh", str(build_script), "--qualify"],
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                env=env,
            )
            refusal_prefix = "pre-use qualification refused: "
            observed_reason = next(
                (line[len(refusal_prefix):] for line in result.stdout.splitlines() if line.startswith(refusal_prefix)),
                "",
            )
            passed = (
                result.returncode == 4
                and expected_reason in observed_reason
                and "pre-use qualification accepted" not in result.stdout
                and not tool_log.exists()
            )
            require(
                passed,
                f"{name} did not reach its isolated refusal without tool use: {result.stdout}",
            )
            reached_refusals.append(
                {
                    "name": name,
                    "expected_reason": expected_reason,
                    "observed_reason": observed_reason,
                    "reached": True,
                    "tool_side_effect": False,
                }
            )
        require(len(reached_refusals) == len(refusal_cases), "private receipt refusal matrix was incomplete")

        target_hash_refusals = []
        for name in sorted(fake_tools):
            bad_receipt = json.loads(json.dumps(receipt))
            row = next(item for item in bad_receipt["tools"]["target_binutils"] if item["name"] == name)
            row["sha256"] = "0" * 64
            write_json(receipt_path, bad_receipt)
            tool_log.unlink(missing_ok=True)
            marker = fixture / "compiler-invoked"
            marker.unlink(missing_ok=True)
            bad_env = env.copy()
            bad_env["T64_FAKE_TOOL_LOG"] = str(tool_log)
            bad = subprocess.run(
                ["/bin/sh", str(build_script), "--qualify"],
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                env=bad_env,
            )
            require(bad.returncode == 4, f"tampered {name} did not refuse: {bad.stdout}")
            require(f"target {name} SHA-256 does not match the qualified bytes" in bad.stdout, f"tampered {name} reached a different refusal: {bad.stdout}")
            require(not tool_log.exists(), f"tampered {name} was invoked before refusal")
            require("pre-use qualification accepted" not in bad.stdout, f"tampered {name} passed the gate")
            target_hash_refusals.append(
                {
                    "name": name,
                    "observed_reason": f"target {name} SHA-256 does not match the qualified bytes",
                    "reached": True,
                    "tool_side_effect": False,
                }
            )

        write_json(receipt_path, receipt)
        fixture_marker = fixture / "compiler-invoked"
        fixture_marker.unlink(missing_ok=True)
        symlink_root = fixture / "symlink-checkout"
        target_build = fixture / "symlink-target/build"
        (target_build / "private-cpu-diagnostic").mkdir(parents=True)
        (target_build / "private-cpu-diagnostic/qualification.json").write_text("{}\n")
        (target_build / "private-cpu-diagnostic/qualification.json").chmod(0o600)
        (symlink_root / "scripts").mkdir(parents=True)
        shutil.copyfile(build_script, symlink_root / "scripts/build-cpu-diagnostic.sh")
        shutil.copyfile(verifier_path, symlink_root / "scripts/verify-cpu-diagnostic-qualification.py")
        (symlink_root / "scripts/build-cpu-diagnostic.sh").chmod(0o700)
        (symlink_root / "build").symlink_to(target_build)
        symlinked = subprocess.run(
            ["/bin/sh", str(symlink_root / "scripts/build-cpu-diagnostic.sh"), "--qualify"],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
        )
        require(symlinked.returncode == 4, f"symlinked build ancestor did not refuse: {symlinked.stdout}")
        require("private qualification root traverses a symlink" in symlinked.stdout, f"symlink refusal reason changed: {symlinked.stdout}")
        require(not tool_log.exists(), "symlinked build ancestor reached an external tool")

    report_path = private_root / "local-qualification-control.json"
    write_json(
        report_path,
        {
            "accepted_baseline": "production pre-use verifier accepted the synthetic local receipt",
            "binutils_version_and_format_probes": sorted(verifier.REQUIRED_TARGET_TOOLS),
            "command": (
                "PYTHONDONTWRITEBYTECODE=1 python3 -B "
                f"{pathlib.Path(__file__).resolve()} {repository}"
            ),
            "external_diagnostic_invoked": False,
            "host_compiler_invoked": "authored fixture wrapper only",
            "schema": "t64.local-qualification-control.v1",
            "status": "PASS",
            "receipt_mutations": reached_refusals,
            "target_tool_hash_mutations_refused_before_invocation": sorted(
                verifier.REQUIRED_TARGET_TOOLS
            ),
            "target_tool_hash_mutation_cases": target_hash_refusals,
            "symlinked_build_ancestor_refused_before_tool_use": True,
        },
    )
    print("PASS: accepted synthetic authored receipt reaches all four hash-bound tool probes")
    print("PASS: four target-tool hash mutations refuse before any tool invocation")
    print(f"PASS: {len(reached_refusals)} isolated receipt mutations reach their own refusal with no tool side effect")
    print("PASS: symlinked build ancestor refuses before private output or tool use")
    print(f"PRIVATE REPORT: {report_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError, KeyError, TypeError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)

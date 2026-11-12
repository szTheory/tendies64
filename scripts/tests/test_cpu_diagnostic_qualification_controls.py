#!/usr/bin/env python3
"""Portable authored controls for the CPU diagnostic qualification gate."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_verifier(path: pathlib.Path):
    spec = importlib.util.spec_from_file_location("t64_qualification_verifier", path)
    require(spec is not None and spec.loader is not None, "cannot load production verifier")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def executable(path: pathlib.Path, body: str = "#!/bin/sh\nexit 0\n") -> None:
    path.write_text(body)
    path.chmod(0o700)


def call_build_script(repository: pathlib.Path, environment: dict[str, str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["/bin/sh", str(repository / "scripts/build-cpu-diagnostic.sh"), "--qualify"],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=environment,
    )


def main() -> int:
    source_root = pathlib.Path(sys.argv[1]).resolve(strict=True)
    verifier_path = source_root / "scripts/verify-cpu-diagnostic-qualification.py"
    verifier = load_verifier(verifier_path)
    reached = 0

    with tempfile.TemporaryDirectory(prefix="t64-qualification-controls.") as temp_text:
        root = pathlib.Path(temp_text).resolve(strict=True)
        repository = root / "repo"
        private_root = repository / "build/private-cpu-diagnostic"
        private_root.mkdir(parents=True)

        compiler = private_root / "fake-cc"
        executable(compiler)
        target_paths: dict[str, pathlib.Path] = {}
        target_rows = []
        env_names = {
            "as": "T64_MIPS_AS",
            "ld": "T64_MIPS_LD",
            "objcopy": "T64_MIPS_OBJCOPY",
            "objdump": "T64_MIPS_OBJDUMP",
        }
        for name in sorted(verifier.REQUIRED_TARGET_TOOLS):
            path = private_root / f"mips64-elf-{name}"
            executable(path)
            target_paths[name] = path
            target_rows.append({"name": name, "path": str(path), "sha256": digest(path)})

        saved_environment = {
            key: os.environ.get(key)
            for key in ["CC", *env_names.values()]
        }
        try:
            os.environ["CC"] = str(compiler)
            for name, key in env_names.items():
                os.environ[key] = str(target_paths[name])

            accepted = verifier.check_tools(
                repository,
                {
                    "host_c_compiler": {"path": str(compiler), "sha256": digest(compiler)},
                    "target_binutils": target_rows,
                },
                private_root,
            )
            require(accepted["host_c_compiler"] == str(compiler), "valid authored tool set was refused")
            require(set(accepted["target_binutils"]) == verifier.REQUIRED_TARGET_TOOLS, "valid tool set was incomplete")
            reached += 1
            print("PASS: accepted authored compiler and target-tool hash baseline")

            cases = []
            for name in sorted(target_paths):
                mutated = copy.deepcopy(target_rows)
                next(row for row in mutated if row["name"] == name)["sha256"] = "0" * 64
                cases.append((name, mutated, f"target {name} SHA-256 does not match the qualified bytes"))
            for name, mutated, expected in cases:
                refused = False
                try:
                    verifier.check_tools(
                        repository,
                        {
                            "host_c_compiler": {"path": str(compiler), "sha256": digest(compiler)},
                            "target_binutils": mutated,
                        },
                        private_root,
                    )
                except verifier.Refusal as error:
                    refused = str(error) == expected
                reached += 1
                require(refused, f"{name} target-tool hash mutation missed its refusal")

            registry_fixture = {
                "package": [
                    {
                        "name": "authored-dependency",
                        "version": "1.0.0",
                        "source": "registry+https://example.invalid/index",
                        "checksum": "a" * 64,
                    }
                ]
            }
            require(
                verifier.lock_registry_packages(registry_fixture)
                == {("authored-dependency", "1.0.0", "a" * 64)},
                "authored registry lock baseline was not accepted",
            )
            reached += 1
            refused = False
            try:
                verifier.lock_registry_packages(
                    {
                        "package": [
                            *registry_fixture["package"],
                            {
                                "name": "unreviewed-git",
                                "version": "1.0.0",
                                "source": "git+https://example.invalid/repo#0123456789abcdef",
                            },
                        ]
                    }
                )
            except verifier.Refusal as error:
                refused = str(error) == "Cargo.lock contains an unreviewed git or non-registry source"
            reached += 1
            require(refused, "unreviewed Git-source mutation missed its refusal")

            bad_compiler = {
                "host_c_compiler": {"path": str(compiler), "sha256": "0" * 64},
                "target_binutils": target_rows,
            }
            refused = False
            try:
                verifier.check_tools(repository, bad_compiler, private_root)
            except verifier.Refusal as error:
                refused = str(error) == "host C compiler SHA-256 does not match the reviewed executable"
            reached += 1
            require(refused, "host compiler hash mutation missed its refusal")

            wrong_selected_tool = private_root / "unreviewed-ld"
            executable(wrong_selected_tool)
            os.environ["T64_MIPS_LD"] = str(wrong_selected_tool)
            refused = False
            try:
                verifier.check_tools(
                    repository,
                    {
                        "host_c_compiler": {"path": str(compiler), "sha256": digest(compiler)},
                        "target_binutils": target_rows,
                    },
                    private_root,
                )
            except verifier.Refusal as error:
                refused = str(error) == "selected target ld path differs from the reviewed executable"
            reached += 1
            require(refused, "target tool path substitution missed its refusal")
        finally:
            for key, value in saved_environment.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value

        # Run the production shell entry point in isolated scratch roots so the
        # checks require neither the private receipt nor private input archives.
        for kind in ("missing-receipt", "symlinked-build-ancestor"):
            scratch = root / kind
            scripts = scratch / "scripts"
            scripts.mkdir(parents=True)
            shutil.copyfile(source_root / "scripts/build-cpu-diagnostic.sh", scripts / "build-cpu-diagnostic.sh")
            shutil.copyfile(verifier_path, scripts / "verify-cpu-diagnostic-qualification.py")
            (scripts / "build-cpu-diagnostic.sh").chmod(0o700)
            marker = root / f"{kind}.compiler-invoked"
            fake_cc = scratch / "fake-cc"
            executable(fake_cc, "#!/bin/sh\nprintf invoked > \"$T64_CONTROL_MARKER\"\nexit 73\n")
            env = os.environ.copy()
            env["CC"] = str(fake_cc)
            env["T64_CONTROL_MARKER"] = str(marker)
            env["PYTHONDONTWRITEBYTECODE"] = "1"
            env.pop("T64_CPU_DIAGNOSTIC_QUALIFICATION_ROOT", None)
            if kind == "missing-receipt":
                (scratch / "build/private-cpu-diagnostic").mkdir(parents=True)
                expected = "qualification receipt is missing or malformed"
            else:
                target_private_root = root / "symlink-target/build/private-cpu-diagnostic"
                target_private_root.mkdir(parents=True)
                (target_private_root / "qualification.json").write_text("{}\n")
                (target_private_root / "qualification.json").chmod(0o600)
                (scratch / "build").symlink_to(target_private_root.parent)
                expected = "private qualification root traverses a symlink"
            result = call_build_script(scratch, env)
            reached += 1
            require(result.returncode == 4 and expected in result.stdout, f"{kind} missed its refusal: {result.stdout}")
            require(not marker.exists(), f"{kind} reached the fake compiler")

    require(reached == 1 + 4 + 2 + 1 + 1 + 2, "not every portable qualification control was reached")
    print("PASS: compiler/binutils hashes, selected path, registry lock, missing receipt and symlink controls")
    print("PASS: all 11 portable qualification-control assertions were reached")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError, KeyError, TypeError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)

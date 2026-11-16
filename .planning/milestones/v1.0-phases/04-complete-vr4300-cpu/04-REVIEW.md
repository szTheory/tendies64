---
phase: 04-complete-vr4300-cpu
reviewed: 2026-11-13T09:05:58Z
depth: standard
files_reviewed: 135
files_reviewed_list:
  - core/CMakeLists.txt
  - core/softfloat/f32_add.c
  - core/softfloat/f32_div.c
  - core/softfloat/f32_eq.c
  - core/softfloat/f32_le.c
  - core/softfloat/f32_lt.c
  - core/softfloat/f32_mul.c
  - core/softfloat/f32_sqrt.c
  - core/softfloat/f32_sub.c
  - core/softfloat/f32_to_f64.c
  - core/softfloat/f32_to_i32.c
  - core/softfloat/f32_to_i64.c
  - core/softfloat/f64_add.c
  - core/softfloat/f64_div.c
  - core/softfloat/f64_eq.c
  - core/softfloat/f64_le.c
  - core/softfloat/f64_lt.c
  - core/softfloat/f64_mul.c
  - core/softfloat/f64_sqrt.c
  - core/softfloat/f64_sub.c
  - core/softfloat/f64_to_f32.c
  - core/softfloat/f64_to_i32.c
  - core/softfloat/f64_to_i64.c
  - core/softfloat/i32_to_f32.c
  - core/softfloat/i32_to_f64.c
  - core/softfloat/i64_to_f32.c
  - core/softfloat/i64_to_f64.c
  - core/softfloat/internals.h
  - core/softfloat/platform.h
  - core/softfloat/primitiveTypes.h
  - core/softfloat/primitives.h
  - core/softfloat/s_addMagsF32.c
  - core/softfloat/s_addMagsF64.c
  - core/softfloat/s_approxRecip32_1.c
  - core/softfloat/s_approxRecipSqrt32_1.c
  - core/softfloat/s_approxRecipSqrt_1Ks.c
  - core/softfloat/s_approxRecip_1Ks.c
  - core/softfloat/s_countLeadingZeros32.c
  - core/softfloat/s_countLeadingZeros64.c
  - core/softfloat/s_countLeadingZeros8.c
  - core/softfloat/s_mul64To128.c
  - core/softfloat/s_normRoundPackToF32.c
  - core/softfloat/s_normRoundPackToF64.c
  - core/softfloat/s_normSubnormalF32Sig.c
  - core/softfloat/s_normSubnormalF64Sig.c
  - core/softfloat/s_roundPackToF32.c
  - core/softfloat/s_roundPackToF64.c
  - core/softfloat/s_roundToI32.c
  - core/softfloat/s_roundToI64.c
  - core/softfloat/s_shiftRightJam32.c
  - core/softfloat/s_shiftRightJam64.c
  - core/softfloat/s_shiftRightJam64Extra.c
  - core/softfloat/s_shortShiftRightJam64.c
  - core/softfloat/s_subMagsF32.c
  - core/softfloat/s_subMagsF64.c
  - core/softfloat/softfloat.h
  - core/softfloat/softfloat_types.h
  - core/softfloat/sources.cmake
  - core/softfloat/specialize.h
  - core/t64_bus.c
  - core/t64_cache.c
  - core/t64_cop1.c
  - core/t64_cp0.c
  - core/t64_cpu.c
  - core/t64_digest.c
  - core/t64_exception.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_memory.c
  - core/t64_mmu.c
  - core/t64_pi.c
  - core/t64_scheduler.c
  - core/t64_softfloat.c
  - core/t64_sp.c
  - core/tests/test_boot_entry.c
  - core/tests/test_cop1.c
  - core/tests/test_cpu_cache.c
  - core/tests/test_cpu_cp0.c
  - core/tests/test_cpu_exceptions.c
  - core/tests/test_cpu_integer.c
  - core/tests/test_cpu_link.c
  - core/tests/test_cpu_memory.c
  - core/tests/test_cpu_mmu.c
  - core/tests/test_cpu_path.c
  - core/tests/test_cpu_time.c
  - core/tests/test_digest_trace.c
  - core/tests/test_profile.c
  - core/tests/test_softfloat.c
  - docs/spec/cpu-cache.md
  - docs/spec/cpu-compatibility.md
  - docs/spec/cpu-cop1.md
  - docs/spec/cpu-cp0.md
  - docs/spec/cpu-diagnostic.md
  - docs/spec/cpu-memory.md
  - fixtures/manifest.json
  - runner/CMakeLists.txt
  - runner/t64_cpu_diag_protocol.c
  - runner/t64_cpu_diag_protocol.h
  - runner/t64_cpu_diagnostic.c
  - runner/tests/test_cpu_diag_protocol.c
  - runner/tests/test_cpu_diagnostic.c
  - scripts/apply-cpu-diagnostic-overlay.py
  - scripts/build-cpu-diagnostic.sh
  - scripts/build-qualified-cpu-diagnostic.py
  - scripts/check-test-inventory.sh
  - scripts/prepare-cpu-diagnostic-vendor.py
  - scripts/private-tests/accepted-qualification-baseline.py
  - scripts/run-cpu-diagnostic.py
  - scripts/tests/test_cpu_diagnostic_build_controls.py
  - scripts/tests/test_cpu_diagnostic_overlay_controls.py
  - scripts/tests/test_cpu_diagnostic_qualification_controls.py
  - scripts/tests/test_cpu_diagnostic_run_controls.py
  - scripts/verify-cpu-diagnostic-build.py
  - scripts/verify-cpu-diagnostic-qualification.py
  - scripts/verify-cpu-diagnostic-source-closure.py
  - scripts/verify-cpu-diagnostic-unit-graph.py
  - tests/cpu-diagnostic/overlay.patch
  - tests/cpu-diagnostic/replacements/allocator.rs
  - tests/cpu-diagnostic/replacements/exception_imports.rs
  - tests/cpu-diagnostic/replacements/exception_park.rs
  - tests/cpu-diagnostic/replacements/graphics_mod.rs
  - tests/cpu-diagnostic/replacements/linker.ld
  - tests/cpu-diagnostic/replacements/main.rs
  - tests/cpu-diagnostic/replacements/panic.rs
  - tests/cpu-diagnostic/replacements/print.rs
  - tests/cpu-diagnostic/replacements/randomized_imports.rs
  - tests/cpu-diagnostic/replacements/randomized_signature.rs
  - tests/cpu-diagnostic/replacements/rsp_mod.rs
  - tests/cpu-diagnostic/replacements/tests_prefix.rs
  - tests/cpu-diagnostic/replacements/tests_runner.rs
  - tests/cpu-diagnostic/replacements/text_out.rs
  - tools/fixture/t64_rom_pack.c
  - tools/lint/core-lint.sh
  - tools/lint/lint-objects.sh
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 04: Code Review Report

**Reviewed:** 2026-11-13T09:05:58Z
**Depth:** standard  
**Files Reviewed:** 135
**Status:** clean

## Summary

The historical baseline and prior correction supplements remain intact; their reviewed union contained 126 paths and their recorded findings are resolved as documented. The diagnostic-pipeline supplement adds four paths to the union. CR-10, WR-13, and WR-14 are resolved. This bounded re-review verified the deadline guard refuses success after timeout even when cleanup observes exit code zero and an empty process group. The hosted run-control fixture now uses an authored private directory under the ignored build root, so it no longer requires local qualification artifacts; no new findings were found. Executor-reported controls and builds were not rerun by this reviewer. No package build hook, diagnostic ROM, or guest execution is established by this review.

The historical baseline reviewed the explicit tracked native source and test scope for Phase 4 Plans 01–15 and 17. It re-reviewed the final `runner/t64_cpu_diagnostic.c`, `runner/tests/test_cpu_diagnostic.c`, and `runner/CMakeLists.txt` changes. The Git-unavailable fallback now checks canonical parent ancestors for `.git` markers and refuses repository destinations; the added regression covers this path with an empty `PATH`. The earlier missing-group concern remains a false positive: Plan 14 permits reduced inventories for group-level inspection while refusing whole-inventory completion. A supplemental review of the portability-only change in `runner/t64_cpu_diagnostic.c` confirmed that it uses `S_ISVTX` when visible and the POSIX sticky-bit mask `01000` otherwise; this preserves the existing sticky-directory predicate and its confinement behavior. No source-grounded findings remained in that historical baseline.

## Narrative Findings (AI reviewer)

The historical baseline files meet quality standards. The later incremental findings are recorded below.

---

_Reviewed: 2026-11-12T19:46:52Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_

## Incremental Review Supplement: Plan 16 Inventory v3 Correction

**Reviewed:** 2026-11-12T21:54:44Z
**Frozen source revision:** `c6f0b6a` (diff base `8ed6e14`)
**Current incremental scope:** `runner/t64_cpu_diag_protocol.c`, `runner/t64_cpu_diag_protocol.h`, `runner/tests/test_cpu_diag_protocol.c`, and `runner/tests/test_cpu_diagnostic.c`

The original 101-path baseline and its prior review notes above are retained as historical coverage; they were not counted as newly re-reviewed here. These four incremental paths already occur in that baseline list, so the frontmatter continues to report 101 unique paths. This supplement records the current review of those four paths for the v3 correction.

The parser maps `Weird` only to `required-base` alongside `BasicFunctionality` and `RarelyUsed`; the other CPU lanes remain separate. Required values are counted from that lane, the aggregate still refuses whole-inventory completion when any required group is absent or any value is missing, and member iteration remains in registration/value order. Batch identity continues to bind the exact inventory digest, selector, and selected-ROM hash for each member. The header and serializers retain batch v2 and guest-run v1 while inventory parsing requires v3. The authored tests cover stale inventory-v2 refusal, v3 `Weird` membership, the full required-group/value path, and unchanged batch/guest wire versions. The authored inventory fixture's expected canonical digest was independently recomputed and matched.

No source-grounded findings were found in this v3 correction. No native builds or tests were run by this reviewer; the executor owns those checks. The known fake-compiler receipt-refusal RED in the separate build-script gate remains outside this v3 finding scope and is not attributed to this correction. No external diagnostic run, SC4 result, or overall Plan 16 completion is claimed.

_Incremental review: 2026-11-12T21:54:44Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Receipt Gate

**Reviewed:** 2026-11-12T22:28:54Z
**Frozen source revision:** `1f7f42d9ec611ed575bdb4d31ae0a6125b4899b9` (diff base `c6f0b6a`)
**Current incremental scope:** `runner/tests/test_cpu_diagnostic.c`, `scripts/build-cpu-diagnostic.sh`, and `scripts/verify-cpu-diagnostic-qualification.py`

The earlier 101-path baseline and v3 correction supplement are retained without re-reviewing unchanged CPU core or protocol code. The C test and shell script already occur in the baseline list; the new verifier adds one unique path, bringing the report's unique reviewed-path count to 102. The source inspection below is pinned to the frozen receipt-gate revision. No native build or test was run for this review.

### Critical Issues

#### CR-01: Receipt does not bind the executables that the gate runs

**Severity:** BLOCKER
**File:** `scripts/build-cpu-diagnostic.sh:9-12,37-43,75-89`; `scripts/verify-cpu-diagnostic-qualification.py:116-119`
**Issue:** `CC` and the target-tool paths are environment-overridable. After receipt acceptance, `compile_packer` executes `CC` based only on `command -v`, and `verify_target_tools` executes the assembler, linker, objcopy, and objdump after checking only their self-reported version and format strings. The closed receipt schema records Rust archives but has no path or digest binding for these actual executables. A substituted compiler or target tool can therefore execute while the receipt's `host_toolchain`/`target_tools` flags say `pass`.
**Fix:** Add resolved path and SHA-256 entries for each executable to the receipt, verify each executable's bytes before invocation, and use only the verified paths. Do not treat a version string or a caller-controlled environment override as tool identity.
**Disposition:** Resolved at `79cc519`: the verifier checks the absolute host compiler path and digest and all four selected target-tool paths and digests before the shell invokes the compiler or probes binutils (`scripts/verify-cpu-diagnostic-qualification.py:175-254`; `scripts/build-cpu-diagnostic.sh:48-65,102-128`).

#### CR-02: A symlinked build parent can move the private trust root outside the repository

**Severity:** BLOCKER
**File:** `scripts/build-cpu-diagnostic.sh:49-55`; `scripts/verify-cpu-diagnostic-qualification.py:73-80`
**Issue:** The shell rejects a symlink only at the final `qualification_root` component. The verifier resolves `repository/build/private-cpu-diagnostic` and then treats that result as the trust root without checking that it remains beneath `repository` or that its ancestors are not symlinks. If `build` is a symlink, a receipt under the redirected directory can satisfy the receipt-relative check, and subsequent packer/temp outputs use the same redirected lexical path.
**Fix:** Resolve and require the private root to be beneath the resolved repository, and reject symlinks in every existing component of the build/output path before accepting the receipt or writing files. Add a refusal control for a symlinked `build` ancestor.
**Disposition:** Resolved at `79cc519`: validation walks each component of the canonical repository build/private path and refuses a symlink ancestor before loading the receipt; the public authored control asserts that refusal and checks the fake compiler marker remains absent (`scripts/verify-cpu-diagnostic-qualification.py:70-81,492-501`; `scripts/tests/test_cpu_diagnostic_qualification_controls.py:194-225`).

#### CR-03: Git-sourced Cargo packages are omitted from the audited dependency set

**Severity:** BLOCKER
**File:** `scripts/verify-cpu-diagnostic-qualification.py:183-189,191-240`
**Issue:** `locked_packages` includes only Cargo.lock rows whose source starts with `registry+`. A `git+` package is not added to either `locked_packages` or `reviewed_packages`, so the final equality check can pass without any source, revision, checksum, license, or build-hook review for that dependency. Cargo can later fetch and build this unreviewed code despite the receipt claiming the dependency check passed.
**Fix:** Fail closed on unsupported non-registry remote sources, or extend the receipt and verifier to bind and review every remote source type and require exact equality with all external packages in Cargo.lock.
**Disposition:** Resolved at `79cc519` for remote non-registry sources: every nonempty Cargo.lock `source` not beginning with `registry+` now refuses, and the public control specifically exercises the Git-source refusal (`scripts/verify-cpu-diagnostic-qualification.py:257-285`; `scripts/tests/test_cpu_diagnostic_qualification_controls.py:123-156`). A separate source-less path-package gap is recorded as the current CR-04 below.

### Warnings

#### WR-01: The six refusal controls exit before writing or checking any receipt

**Severity:** WARNING
**File:** `runner/tests/test_cpu_diagnostic.c:576-590,1179-1182,1336`
**Issue:** `receipt_path` is only formatted; the new fixture no longer creates that file. `write_file` opens with `O_WRONLY | O_TRUNC` and no `O_CREAT`, so the first loop iteration fails to write and jumps to cleanup without recording a failed assertion. None of the six refusal cases run, and the missing fake-compiler marker is not evidence that the gate refused them.
**Fix:** Create the initial receipt with `create_file` before the loop or use a test writer that creates the file with restrictive permissions. Convert every setup/write failure into a failed assertion so the control cannot silently skip.
**Disposition:** Resolved at `79cc519`: the local-only integration helper creates its receipt with `write_json`, overwrites each mutation using the same checked writer, asserts each specific refusal, and fails if a tool log appears (`scripts/private-tests/accepted-qualification-baseline.py:254-255,361-399`). This repair does not rewrite the earlier invalid-coverage record. The public CTest uses a separate authored-only helper and does not invoke this local-only integration helper.

#### WR-02: The synthetic receipt's Rust standard-library archive name disagrees with the verifier

**Severity:** WARNING
**File:** `runner/tests/test_cpu_diagnostic.c:1273`; `scripts/verify-cpu-diagnostic-qualification.py:25,145-163`
**Issue:** The fixture emits the archive name `rust-std`, but the verifier requires `host-rust-std`. The archive-name set check refuses this otherwise synthetic receipt before project-lock, dependency-hook, or asset validation. After WR-01 is fixed, the later cases still cannot reach their expected refusal reasons.
**Fix:** Align the required archive name and fixture with the exact component key in the pinned channel manifest. Add a positive receipt-acceptance check before mutating it into the six refusal variants, then assert each variant reaches its own reason.
**Disposition:** Resolved at `79cc519`: both the verifier's required archive set and the local-only authored receipt use the manifest component key `rust-std`; the helper runs an accepted baseline before its isolated refusal mutations (`scripts/verify-cpu-diagnostic-qualification.py:35,569-595`; `scripts/private-tests/accepted-qualification-baseline.py:194-211,254-289`).

No real toolchain install, Cargo discovery, diagnostic build/run, or private qualification was performed or inferred here. The review does not claim an external diagnostic result, SC4 completion, or overall Plan 16 completion; remaining projection, overlay, actual-resolution, and layout qualification stays outstanding.

_Incremental review: 2026-11-12T22:28:54Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Receipt Controls and Source Bindings

**Reviewed:** 2026-11-12T23:25:54Z
**Frozen source revision:** `79cc519` (diff base `1f7f42d`)
**Current incremental scope:** `runner/tests/test_cpu_diagnostic.c`, `scripts/build-cpu-diagnostic.sh`, `scripts/verify-cpu-diagnostic-qualification.py`, `scripts/tests/test_cpu_diagnostic_qualification_controls.py`, and `scripts/private-tests/accepted-qualification-baseline.py`

This review reuses prior coverage for the C test, build script, and verifier; it newly reviews the public portable-control helper and the local-only receipt integration helper. The public `runner.cpu-diagnostic` path calls only the self-contained helper under `scripts/tests/`, whose fixtures are authored in temporary directories and whose direct production-verifier controls cover the accepted tool baseline, individual tool hash/path refusals, registry/Git lock behavior, missing receipt, and symlinked build ancestor. It does not call or read the helper under `scripts/private-tests/`. The latter remains explicitly local-only and depends on ignored audited inputs; it is not a public CTest prerequisite. No private qualification result or external diagnostic execution was reviewed or inferred.

The five findings from the preceding receipt-gate supplement are resolved in this frozen source. The validator now binds the selected compiler and target tools to reviewed paths and SHA-256 values before the shell uses them; the private-root check rejects symlinked ancestors; Git and other non-registry lock sources refuse; the local-only receipt controls now create and rewrite their fixture with a checked writer and assert individual expected outcomes; and the `rust-std` archive key is aligned between verifier and synthetic baseline. These dispositions update current issue counts while preserving the earlier findings and their invalid-coverage history.

The added source checks bind the source-projection file to its canonical digest, separately hash the pinned test-list file, verify the pinned HEAD for both source checkouts, require the packaged Cargo manifest and lockfile to be the exact files in the overlay checkout, hash each declared changed file, and compare the declared set with Git's reported working-tree changes. The shell/Python boundaries use argument arrays or quoted paths for tool invocation; no command interpolation issue was found in the reviewed paths.

### Critical Issues

#### CR-04: Cargo path packages are omitted from the reviewed dependency set

**Severity:** BLOCKER
**File:** `scripts/verify-cpu-diagnostic-qualification.py:257-285,621-670`
**Issue:** `lock_registry_packages` skips every Cargo.lock package row with no `source` (`:264-266`), then compares only registry packages against the receipt's `dependencies` list. Cargo lockfiles omit `source` for path dependencies as well as the root package ([Cargo lockfile format](https://doc.rust-lang.org/stable/nightly-rustc/cargo/resolver/encode/index.html)). Consequently a local path package can be absent from `reviewed_packages` while the set equality still passes; its files, license, build script, and proc-macro effects are not required by this verifier. A path can also name a source tree outside the checked overlay, which the changed-file closure does not bind. Such code may be compiled and run as a build hook under a receipt that reports dependency checks passed.
**Fix:** Resolve the exact root/workspace packages from the manifest and fail closed on any other source-less lock row unless the verifier recursively binds and reviews that path package's complete source tree, license, build script, and proc-macro inputs. Require any accepted path to remain inside the private qualified checkout and add a control for an omitted source-less package.
**Disposition:** Resolved at `395131b`: the verifier derives the package identity from the hash-checked manifest, refuses workspaces, requires exactly one source-less row matching the root, and accepts only fully classified crates.io rows for the remaining entries. The public controls cover a valid root-plus-registry baseline and a source-less path-package refusal (`scripts/verify-cpu-diagnostic-qualification.py:257-312,627-663`; `scripts/tests/test_cpu_diagnostic_qualification_controls.py:123-180`).

At the time of this supplement, the frontmatter counted this one open follow-up finding; the five preceding findings are retained with explicit resolved dispositions. No external toolchain installation, Cargo resolution/build, diagnostic build/run, private data inspection, native build, or test was performed by that reviewer. The implementation remains a pre-use receipt gate; the actual overlay/module closure, resolved Cargo graph/ELF-ROM layout/startup, and full private suite execution remain later qualification steps. This review does not claim an external diagnostic result, SC4 completion, or overall Plan 16 completion.

_Incremental review: 2026-11-12T23:25:54Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Root Classification and Overlay Applier

**Reviewed:** 2026-11-12T23:43:38Z
**Frozen source revision:** `395131b` (diff base `79cc519`)
**Current incremental scope:** `runner/tests/test_cpu_diagnostic.c`, `scripts/verify-cpu-diagnostic-qualification.py`, `scripts/apply-cpu-diagnostic-overlay.py`, `scripts/tests/test_cpu_diagnostic_qualification_controls.py`, and `scripts/tests/test_cpu_diagnostic_overlay_controls.py`

This review reuses historical coverage for the C test, verifier, and qualification-control helper; the applier and its authored-control helper are newly reviewed paths. CR-04 is resolved: the verifier reads the hash-checked package identity from `Cargo.toml`, refuses workspace manifests, requires one source-less lock row matching that root, rejects any other source-less row, and admits only exact crates.io registry sources for the fully audited remaining packages. The public helper exercises the root-plus-registry baseline and a path-row refusal (`scripts/verify-cpu-diagnostic-qualification.py:257-312,627-663`; `scripts/tests/test_cpu_diagnostic_qualification_controls.py:123-180`).

The public CTest path invokes the portable helper under `scripts/tests/`; that helper invokes the overlay control helper in a temporary report location. Neither helper references `scripts/private-tests/accepted-qualification-baseline.py` or reads private qualification data. The five overlay controls execute the production applier as a subprocess and check exact replacement, bad file hash, overlapping ranges, target symlink, and `..` traversal. No native builds or tests were run by this reviewer. The executor's supplied build and test evidence is not a claim of Cargo resolution, diagnostic build, or guest execution.

### Critical Issues

#### CR-05: Overlay targets can rewrite Git metadata

**Severity:** BLOCKER
**File:** `scripts/apply-cpu-diagnostic-overlay.py:100-115`; `scripts/verify-cpu-diagnostic-qualification.py:371-387,831-861`
**Issue:** The applier rejects absolute and `..` paths, then only checks that the resolved target is a regular file beneath the source root. It does not reject `.git` paths. An overlay directive can therefore replace `.git/config`, `.git/info/exclude`, or another regular Git metadata file. The qualification verifier later trusts `git rev-parse` and `git status` output to establish the checkout revision and declared changed-file set, so the applier can alter metadata that those checks rely on.
**Fix:** Restrict overlay targets to the explicitly approved upstream source paths and reject any path with a `.git` component. Add a control that targets a regular file under `.git` and asserts refusal before changing its bytes.
**Disposition:** Resolved at `53dba86`: `safe_relative_path` rejects `.git` path components before target resolution, and the control targets `.git/config`, requires refusal, and verifies the bytes remain unchanged (`scripts/apply-cpu-diagnostic-overlay.py:81-91,232-240`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:176-193`).

### Warnings

#### WR-03: A later file-write failure leaves earlier overlay files replaced

**Severity:** WARNING
**File:** `scripts/apply-cpu-diagnostic-overlay.py:126-145`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:97-125`
**Issue:** The applier validates every source/range before writes, but then replaces files sequentially. If creating, writing, or replacing a later file fails, the exception handler removes only that file's temporary path; it does not restore earlier targets already replaced. The overlap control only exercises a validation failure before this write loop, so it does not cover this partial-application path. The command can fail while leaving a checkout that no longer matches the patch's original file hashes, preventing a clean retry.
**Fix:** Apply into a fresh private checkout and promote it only after all writes succeed, or keep rollback copies and restore every replaced target on failure. Add a deterministic injected failure after the first replacement and assert all original files remain unchanged.
**Disposition:** Resolved at `53dba86` for a later ordinary publish failure: staged outputs retain each source's original bytes and permission bits, and the rollback path restores them; the control injects failure on the second publication and checks both source byte sequences and temporary-file cleanup (`scripts/apply-cpu-diagnostic-overlay.py:276-318,325-328`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:217-260`).

#### WR-04: Boolean and floating schema versions pass the integer version check

**Severity:** WARNING
**File:** `scripts/apply-cpu-diagnostic-overlay.py:84-90`
**Issue:** `patch["schema_version"] != 1` accepts JSON `true` and `1.0` because Python considers both equal to integer `1`. The overlay's closed schema therefore accepts malformed version types even though the byte-range validator explicitly excludes booleans from integer fields.
**Fix:** Require `type(patch["schema_version"]) is int` and value `1` before processing edits. Add controls for `true` and `1.0` that refuse without modifying the checkout.
**Disposition:** Resolved at `53dba86`: the schema check requires the exact `int` type, and the controls verify `true` and `1.0` are refused without changing source bytes (`scripts/apply-cpu-diagnostic-overlay.py:207-212`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:196-214`).

No source-derived external diagnostic result, SC4 completion, or overall Plan 16 completion is claimed. Actual overlay/module closure, Cargo unitgraph, ELF/ROM composition and startup, and full private-suite execution remain later qualification steps.

_Incremental review: 2026-11-12T23:43:38Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Overlay Rollback Failure Handling

**Reviewed:** 2026-11-12T23:52:24Z
**Frozen source revision:** `53dba86` (diff base `395131b`)
**Current incremental scope:** `scripts/apply-cpu-diagnostic-overlay.py` and `scripts/tests/test_cpu_diagnostic_overlay_controls.py`

This follow-up reuses historical coverage for both paths and rechecks only the frozen applier and its public control helper. The three prior overlay findings are resolved as described above. The helper covers `.git` target refusal, exact integer schema-version refusal, and a one-shot later-publish failure with byte restoration and staged-file cleanup. No tests or builds were run by this reviewer, and no private qualification inputs were inspected. No real toolchain installation, Cargo resolution/build, diagnostic execution, guest run, or Plan 16 completion is claimed.

### Warnings

#### WR-05: Failed rollback deletes the original recovery copy

**Severity:** WARNING
**File:** `scripts/apply-cpu-diagnostic-overlay.py:301-318`
**Issue:** If a staged publication fails after earlier files were replaced and a subsequent rollback `replace_operation` also raises `OSError`, the handler records the rollback error but unconditionally unlinks `backup_path` in `finally`. The target remains patched and the only on-disk copy of its original bytes is discarded before the caller receives “rollback was incomplete.” The injected control fails only the second publication and allows the rollback operation, so it does not exercise this double-failure path (`scripts/tests/test_cpu_diagnostic_overlay_controls.py:238-260`).
**Fix:** Do not delete a rollback backup when its replacement fails. Preserve it as a recovery file and include its path in the incomplete-rollback error (or use a verified fallback restore); add a control that injects both failures and asserts the original bytes remain recoverable and the error identifies the recovery artifact.
**Disposition:** Resolved at `999681b` for the failed restore-publication path: after backup bytes are written, flushed, synced, and chmod succeeds, a failed replacement retains the recovery file and names it in the incomplete-rollback error. The deterministic control injects both publication and rollback failures and checks the retained bytes, mode, error path, and disposable staging cleanup (`scripts/apply-cpu-diagnostic-overlay.py:303-331`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:263-321`).

No crash-atomicity guarantee is inferred. The finding concerns the explicitly handled ordinary `OSError` path when rollback cannot publish its saved original.

_Incremental review: 2026-11-12T23:52:24Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Rollback Recovery Copy Follow-up

**Reviewed:** 2026-11-13T00:18:19Z
**Frozen source revision:** `999681b` (diff base `53dba86`)
**Current incremental scope:** `scripts/apply-cpu-diagnostic-overlay.py` and `scripts/tests/test_cpu_diagnostic_overlay_controls.py`

This follow-up reuses historical coverage for both paths and rechecks only the frozen rollback implementation and its control. WR-05 is resolved for the reported failed-rename case: the new control forces the later publication and recovery replacement to fail, then verifies the retained original bytes and mode, the error's recovery filename, and removal of disposable staged files. The fixer report records successful controls and native gate checks; this reviewer did not rerun them. No private qualification inputs were inspected. No real toolchain installation, Cargo resolution/build, diagnostic execution, guest run, or Plan 16 completion is claimed.

### Warnings

#### WR-06: A mode-setting failure still discards a complete recovery copy

**Severity:** WARNING
**File:** `scripts/apply-cpu-diagnostic-overlay.py:306-331`
**Issue:** `recovery_ready` is set only after `backup_path.chmod(mode)` succeeds (`:314-315`). If creating and writing the backup, flushing, and `fsync` all succeed but `chmod` raises `OSError`, the exception branch treats the original copy as not ready, and `finally` unlinks it (`:317-331`). The target remains patched even though the temporary file already contains the complete original bytes; the caller receives an incomplete-rollback error without a recovery path. The new control injects failure at `replace_operation`, after `recovery_ready` is true, so it does not cover this preparation failure (`scripts/tests/test_cpu_diagnostic_overlay_controls.py:288-321`).
**Fix:** Track successful completion of the original-byte write/flush/fsync separately from mode restoration. If mode application fails, preserve the complete recovery bytes and report both the recovery path and that its mode could not be restored; add a control that injects a recovery-file chmod failure after the write and verifies recoverability and the error details.
**Disposition:** Resolved at `04be456`: byte readiness is marked only after write, flush, and `fsync`, before chmod. A chmod failure retains the recovery bytes and reports their path, the unverified mode, and that the target remains unrestored; the control injects that failure after confirming the bytes are complete and checks the retained copy and error (`scripts/apply-cpu-diagnostic-overlay.py:303-340`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:324-400`). The outer cleanup removes only staged `.cpu-overlay.*` files; recovery copies use the separate `.cpu-overlay-recovery.*` prefix (`scripts/apply-cpu-diagnostic-overlay.py:307-308,347-350`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:307-321`).

No crash-atomicity guarantee is inferred. This finding concerns a caught ordinary `OSError` during rollback preparation.

_Incremental review: 2026-11-13T00:18:19Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Recovery-Mode Failure Handling

**Reviewed:** 2026-11-13T00:27:31Z
**Frozen source revision:** `04be456` (diff base `999681b`)
**Current incremental scope:** `scripts/apply-cpu-diagnostic-overlay.py` and `scripts/tests/test_cpu_diagnostic_overlay_controls.py`

This follow-up reuses historical coverage for both paths and checks the frozen recovery-mode change and its control. WR-06 is resolved: the control confirms the original bytes have been written before injecting the chmod error; the applier retains that copy, reports that its mode is unverified and the target remains unrestored, and the outer `finally` cleans disposable staged files without deleting recovery-prefixed files. The fixer report records successful controls and native gate checks; this reviewer did not rerun them. No private inputs were inspected. No real toolchain installation, Cargo resolution/build, diagnostic execution, guest run, or Plan 16 completion is claimed.

No new source-grounded findings were found in this bounded follow-up. No crash-atomicity guarantee is inferred.

_Incremental review: 2026-11-13T00:27:31Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Registration Renderer

**Reviewed:** 2026-11-13T00:42:14Z
**Frozen source revision:** `a02ade6` (diff base `04be456`)
**Current incremental scope:** `scripts/apply-cpu-diagnostic-overlay.py` and `scripts/tests/test_cpu_diagnostic_overlay_controls.py`

This follow-up reuses historical coverage for both paths and reviews only the frozen source-registration renderer and its public controls. The renderer recognizes the source's two supported constructor shapes (empty-braced and unit), permits multiline formatting and trailing commas, and rejects field-bearing, tuple-style, mismatched, or otherwise unknown expressions. It compares projection and audit metadata for every registration, verifies the pinned registration/source digests, and requires the generated required-base indices to equal the projection's ordered required indices. Generation occurs while preparing the edit outputs, before any files are staged or replaced. The new public controls use synthetic `super::sample` constructor names and small authored snippets; they do not include upstream source excerpts, private files, or private fixture data.

The executor reports successful local controls and native gate checks; this reviewer did not rerun them. This source-only review does not establish actual private-source closure, a qualified receipt, Cargo build, ROM composition, or guest execution. No private inputs or results were inspected, and no crash-atomicity guarantee is inferred.

No new source-grounded findings were found in this bounded follow-up.

_Incremental review: 2026-11-13T00:42:14Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Authored Guest Overlay

**Reviewed:** 2026-11-13T01:26:43Z
**Frozen source revision:** `6c8b4ef9cff79e27fafb34068c6870ebf5ac9d0e` (diff base `c71a5ca5f57f5ef63a993181baf177eacc12c107`)
**Current incremental scope:** `tests/cpu-diagnostic/overlay.patch` and the 14 files under `tests/cpu-diagnostic/replacements/`

This supplement adds 15 new unique paths to the existing 106-path historical review; the frontmatter lists the exact 121-path union. I reviewed the authored overlay directives and Rust/linker replacements, reusing prior coverage of the native protocol, runner, and applier contracts. The manifest's replacement-file digests, allowed relative paths, edit shapes, and non-overlapping byte ranges were checked statically. Source-side file and range digests remain enforced by the production applier; I did not inspect the excluded upstream checkout. The replacement snippets contain authored harness code and synthetic public controls, with no upstream source excerpts or private assets observed.

The guest transcript matches the native `t64.cpu-run` v1 parser: root/record keys are canonical and closed, record order and status names match, indices are selector-derived, the completion marker is a string count of three, and the line ends in the canonical newline. The native runner captures output exactly and refuses missing entry, park, or transcript completion. Exception/panic paths park without emitting a completed transcript and remain bounded by the runner's tick/wall supervision. The actual Rust `isviewer::text_out` implementation and other upstream test semantics were not available for this public-slice review, so no claims about them are made. No new dependency is introduced.

No private qualification inputs or results were inspected. The executor reports successful local controls and native gate checks; this reviewer did not rerun them. The report does not claim actual package/source qualification, a qualified receipt, Rust/Cargo build, final ELF/ROM layout, or guest execution.

### Warnings

#### WR-07: Entry and park sentinels are not pinned by the linker

**Severity:** WARNING
**File:** `tests/cpu-diagnostic/replacements/main.rs:48-60`; `tests/cpu-diagnostic/replacements/tests_runner.rs:1-3`; `tests/cpu-diagnostic/replacements/linker.ld:13-16`; `runner/t64_cpu_diagnostic.c:37-38,406-408`
**Issue:** The native runner only accepts an observed entry retirement at `0x80000400` and a park retirement at `0x80000420` with a specific jump opcode. The linker script places broad `.text`/`.text.*` inputs in link order but does not assign or assert addresses for `entrypoint` or `park`. The Rust functions have no dedicated link sections; `park` is also eligible for inlining. Thus the runner's completion test depends on incidental compiler/linker layout and can reject an otherwise valid transcript as missing entry/park after a layout change.
**Fix:** Give the entry and park routines dedicated retained linker sections at explicit addresses and add linker assertions, or make the runner consume and verify their actual addresses from the linked artifact instead of hard-coding them.

#### WR-08: BSS is zeroed before the heap/stack boundary is checked

**Severity:** WARNING
**File:** `tests/cpu-diagnostic/replacements/main.rs:62-72`; `tests/cpu-diagnostic/replacements/linker.ld:38-47`; `tests/cpu-diagnostic/replacements/allocator.rs:15-36`
**Issue:** `rust_entrypoint` clears the full linker-defined BSS range before calling `init_allocator`. The only check that BSS ends below the fixed heap boundary occurs later in `init_allocator`, and the linker has no assertion that `__bss_end` stays below `__cpu_heap_limit`/`__cpu_stack_bottom`. If added static storage crosses that boundary, startup first clears heap or reserved stack memory (and could clear beyond the 4 MiB RDRAM window) before the guard refuses allocator initialization.
**Fix:** Add a link-time assertion that `__bss_end <= __cpu_heap_limit` (and that the stack bounds fit the profile's RDRAM), with a pre-clear runtime guard as defense in depth.

No crash-atomicity, external-source semantics, package closure, final artifact layout, or guest execution is inferred by this review.

_Incremental review: 2026-11-13T01:26:43Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Entrypoint and Pre-clear Bounds Follow-up

**Reviewed:** 2026-11-13T01:51:00Z
**Frozen source revision:** `c3c48440479a714f1b5d6458dee13a7a9fd92ba7` (diff base `6c8b4ef9cff79e27fafb34068c6870ebf5ac9d0e`)
**Current incremental scope:** `tests/cpu-diagnostic/overlay.patch`, `tests/cpu-diagnostic/replacements/linker.ld`, `tests/cpu-diagnostic/replacements/main.rs`, and `tests/cpu-diagnostic/replacements/tests_runner.rs`

This follow-up reuses historical coverage for all four paths; the reviewed union remains 121 unique paths. WR-07 is resolved: entrypoint and park are retained in dedicated sections at the native runner's required addresses, and linker assertions bind their symbols and instruction sizes. The authored park emits the expected jump opcode `0x08000108`. WR-08 is resolved at link time and before the BSS clear: linker assertions bound BSS and stack to profile RDRAM, and startup refuses invalid BSS/heap/stack ranges before writing memory. All 14 replacement-file digests in the corrected overlay manifest match the committed replacement bytes.

The fix report records that a fully authored MIPS linker fixture confirmed the entry and park addresses/opcode and rejected oversized BSS. That does not verify the Rust startup guard or an actual Rust ELF. No Rust/Cargo build or guest execution was run or claimed here; no private inputs were inspected.

### Critical Issues

#### CR-06: Pre-clear guard compares different MIPS64 address representations

**Severity:** BLOCKER
**File:** `tests/cpu-diagnostic/replacements/main.rs:36-38,74-93`; `tests/cpu-diagnostic/replacements/linker.ld:53-57,72-91`
**Issue:** The linker assigns BSS and profile addresses in canonical sign-extended MIPS64 form (`0xffffffff80000000` through `0xffffffff80400000`), while `CPU_PROFILE_RDRAM_BASE` and `CPU_PROFILE_RDRAM_END` are zero-extended `usize` constants (`0x80000000` and `0x80400000`). `bss_end_address` is cast from the 64-bit linker symbol, so the check `bss_end_address > CPU_PROFILE_RDRAM_END` is true for every normal linked BSS range. Startup therefore calls `park()` before clearing BSS or entering the tests, and the guest emits no completed transcript.
**Fix:** Keep both operands in the same address representation: define the runtime RDRAM bounds as canonical sign-extended 64-bit addresses matching the linker symbols, or normalize the linker-derived addresses and bounds to 32-bit offsets before comparing them.
**Disposition:** Resolved at `8aa0921`: `CPU_PROFILE_RDRAM_BASE` is now `0xffff_ffff_8000_0000`, and `CPU_PROFILE_RDRAM_END` is derived from it, so the runtime guard compares linker-derived canonical MIPS64 addresses against bounds in the same representation. The committed `main.rs` replacement digest matches `overlay.patch`.

No actual Rust ELF, private source projection, qualification receipt, or guest result is inferred by this review.

_Incremental review: 2026-11-13T01:51:00Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Canonical Runtime Bounds

**Reviewed:** 2026-11-13T01:56:40Z
**Frozen source revision:** `8aa0921` (diff base `c3c4844`)
**Current incremental scope:** `tests/cpu-diagnostic/replacements/main.rs` and `tests/cpu-diagnostic/overlay.patch`

This follow-up reuses historical coverage for both paths; the reviewed union remains 121 unique paths. The runtime RDRAM base and end are now canonical sign-extended `usize` addresses matching the linker-defined BSS, heap, stack, and RDRAM symbols. The pre-clear guard compares those addresses directly, without truncating or masking high bits. The `main.rs` replacement digest matches the manifest value in `overlay.patch`, resolving CR-06. WR-07 and WR-08 remain resolved as recorded above.

This static review does not establish an actual Rust ELF or guest startup. No Rust/Cargo build, guest execution, or private input inspection was performed.

No new source-grounded findings were found in this bounded follow-up.

_Incremental review: 2026-11-13T01:56:40Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Post-build Composition Helper

**Reviewed:** 2026-11-13T02:11:37Z
**Frozen source revision:** `f7aa2fc` (diff base `8aa0921`)
**Current incremental scope:** `scripts/verify-cpu-diagnostic-build.py` and `scripts/tests/test_cpu_diagnostic_build_controls.py`

This supplement adds two new public paths to the retained 121-path review; the frontmatter lists the exact 123-path union. The helper checks the actual ELF header, section table and symbols, reconstructs the loaded flat payload from file-backed allocated sections and zero gaps, checks entry/park instructions, selector placement/initializer, and BSS/heap/stack/RDRAM bounds. It checks the ROM header, cleared boot bytes, payload, and excluded-font bytes without writing that asset. The controls use authored ELF-like byte arrays and text tables and patch the expected asset hashes only within their fixtures; they do not contain private assets. The helper invokes the receipt-validated objdump path and marks guest execution unverified.

The successful orchestration path contains a result-shape mismatch: the production qualification validator returns tool paths but not the tool digest map later indexed by this helper. The controls exercise layout/composition functions directly and do not call `main()` through report creation. No builds or tests were run by this reviewer. No actual Rust ELF, ROM, private input, or guest startup is claimed.

### Critical Issues

#### CR-07: Composition success always fails while recording the objdump digest

**Severity:** BLOCKER
**File:** `scripts/verify-cpu-diagnostic-build.py:251-262`; `scripts/verify-cpu-diagnostic-qualification.py:175-254,869`; `scripts/tests/test_cpu_diagnostic_build_controls.py:132-160`
**Issue:** `qualification.validate()` returns `build_tools` with only `host_c_compiler` and `target_binutils` keys (`check_tools()` at lines 175–254 constructs that shape; `validate()` returns it at line 869). After valid artifacts pass verification and objdump succeeds, the helper constructs its report using `tools["target_binutils_sha256"]["objdump"]` at line 256. That key is absent, so every successful composition reaches a caught `KeyError`, exits with refusal status, and writes no report. The current controls test the pure layout/composition functions but not this report-creation path.
**Fix:** Make the returned validator result expose the validated objdump digest (and consume that documented key), or derive the digest from the already validated receipt data retained by `main()`. Add an authored orchestration control that drives a valid composition through report creation and asserts the report includes the bound digest.

**Disposition:** RESOLVED in the `5f01201` follow-up. The helper now hashes the actual objdump path for report creation, and the added authored main-path control exercises report creation using the validator's committed two-key return shape.

_Incremental review: 2026-11-13T02:11:37Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: CR-07 Result-Shape Follow-up

**Reviewed:** 2026-11-13T02:18:25Z
**Frozen source revision:** `5f01201` (diff base `f7aa2fc`)
**Current incremental scope:** `scripts/verify-cpu-diagnostic-build.py` and `scripts/tests/test_cpu_diagnostic_build_controls.py`

This review reuses coverage for both paths; the reviewed union remains 123 unique paths. The missing digest-map lookup is gone, and the authored orchestration control reaches report creation using the validator's actual return shape. CR-07 is resolved. The new digest handling still does not establish that the executable invoked is the one whose digest the validator checked.

### Critical Issues

#### CR-08: The invoked objdump can differ from the receipt-qualified executable

**Severity:** BLOCKER
**File:** `scripts/verify-cpu-diagnostic-build.py:244-254`; `scripts/verify-cpu-diagnostic-qualification.py:233-254,869`
**Issue:** `qualification.validate()` verifies each target tool's on-disk SHA-256 against the receipt at lines 233-250, then returns only its path at line 254. After that check and other file reads, the helper hashes the path at line 245, invokes that path, and only compares its post-invocation hash to this new local digest at lines 253-254. It never compares the local digest to the receipt-authenticated digest. If the executable is replaced after `validate()` checks it but before line 245, the replacement is invoked and a `PASS_POSTBUILD_COMPOSITION` report records the replacement's hash as though it were qualified. The new control checks successful report creation and digest formatting, but does not exercise this mismatch.
**Fix:** Preserve the expected objdump SHA-256 from the exact validated receipt in the validator result, then require the pre-invocation digest to equal it. Keep the post-invocation comparison against the same expected digest; ensure the invoked executable is the same verified file (for example, run from an immutable verified snapshot or a verified open file descriptor where supported). Add an authored control that changes the objdump after validation and asserts refusal before invocation and no success report.

**Disposition:** RESOLVED in `cb23a95`. The helper snapshots the receipt before validation, refuses if its bytes change during validation, binds the selected objdump path and expected SHA-256 from that validated receipt, and checks the executable hash before invocation and after inspection. The authored replacement control proves refusal occurs before invocation and without creating a report.

_Incremental review: 2026-11-13T02:18:25Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: CR-08 Digest-Binding Follow-up

**Reviewed:** 2026-11-13T02:24:01Z
**Frozen source revision:** `cb23a95` (diff base `5f01201`)
**Current incremental scope:** `scripts/verify-cpu-diagnostic-build.py` and `scripts/tests/test_cpu_diagnostic_build_controls.py`

This review reuses coverage for both paths; the reviewed union remains 123 unique paths. The helper now reads the qualification receipt before validation, rejects a receipt that changes during validation, and compares the receipt-approved objdump path and SHA-256 with the selected executable before invoking it. It rechecks the same approved SHA after inspection and reports that identity. The new authored control mutates the tool at the qualification seam and asserts refusal before subprocess invocation with no report; the success control still exercises the committed validator return shape. CR-08 is resolved. This bounded review found no new source-grounded findings. No builds or tests were run by this reviewer, and no actual Rust ELF, ROM, or guest startup is claimed.

_Incremental review: 2026-11-13T02:24:01Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Prebuild Gate, Vendor Sources, and Unit Graph

**Reviewed:** 2026-11-13T02:50:53Z
**Frozen source revision:** `ad33214` (diff base `cb23a95`)
**Current incremental scope:** `runner/tests/test_cpu_diagnostic.c`, `scripts/build-cpu-diagnostic.sh`, `scripts/prepare-cpu-diagnostic-vendor.py`, `scripts/tests/test_cpu_diagnostic_overlay_controls.py`, `scripts/tests/test_cpu_diagnostic_qualification_controls.py`, `scripts/verify-cpu-diagnostic-qualification.py`, `scripts/verify-cpu-diagnostic-source-closure.py`, and `scripts/verify-cpu-diagnostic-unit-graph.py`

This supplement reuses prior coverage of the five already reviewed paths and adds three new public paths; the unique review union is 126 paths. The receipt gate rebinds the host compiler and target binutils to paths and hashes, and the toolchain install verifies the pinned component archives and fresh-prefix executables. The vendor preparer compares registry archives, source trees, lock checksums, and package identities before publishing the offline vendor tree. Source closure and unit-graph checks bind the pinned overlay, required CPU modules, reviewed package identities, target panic strategy, and build-hook roles. The authored public controls use temporary fixtures and exercise the baseline plus their intended refusals; private audit inputs and results were not inspected. This review does not claim a Cargo package build, build-hook execution, diagnostic ROM, or guest run.

### Critical Issues

#### CR-09: Source preparation follows a symlinked build ancestor outside the repository

**Severity:** BLOCKER
**File:** `scripts/build-cpu-diagnostic.sh:52-84`; `scripts/verify-cpu-diagnostic-source-closure.py:510-543`
**Issue:** `verify_private_directory()` checks only whether the final private directory is a symlink. If `build` is a symlink to an external directory and its `private-cpu-diagnostic` child exists, `--prepare-source` accepts that path, then `git clone` writes the overlay checkout through it. The source-closure verifier repeats the gap: it rejects a symlink at the final `private_root_input`, but resolves that path without requiring the result to stay under the canonical repository or checking each ancestor. It then accepts the redirected root and writes the private closure report there. This pre-source step does not invoke the qualification verifier that separately rejects symlinked ancestors. The same checks also accept a group/world-readable private root, and source preparation does not set a restrictive umask before `git clone`; an exposed root can make the private checkout readable before the later qualification gate refuses it.
**Fix:** Before cloning or reading/writing private artifacts, walk and reject symlinks from the canonical repository through `build/private-cpu-diagnostic`, require the resolved root to equal the expected in-repository path, reject group/world access, and set `umask 077` before creating private files. Add authored controls for a symlinked `build` ancestor and an exposed private root, asserting refusal before clone or report creation.

**Disposition:** Resolved in `ec76c39`: the shell build gate walks each path component from canonical `ROOT`, rejects symlinks and missing directories, checks private-root mode, and sets `umask 077`; the vendor and source-closure entry points independently verify canonical, non-symlinked ancestors and private-root mode before reading receipts or writing reports (`scripts/build-cpu-diagnostic.sh:3-5,53-75,77-84`; `scripts/prepare-cpu-diagnostic-vendor.py:29-40,387-402`; `scripts/verify-cpu-diagnostic-source-closure.py:385-396,523-555`). The authored controls assert refusal before Git invocation or report/tree creation.

### Warnings

#### WR-09: Unit-graph input paths can escape the private root with `..`

**Severity:** WARNING
**File:** `scripts/verify-cpu-diagnostic-unit-graph.py:422-436,473-483`
**Issue:** `private_file()` checks `path.relative_to(root)` before normalizing the path. A path lexically under the private root with `..` components therefore passes that check; the loop does not reject `..`, and `path.is_file()` follows the traversal. The subsequent JSON and TOML readers can consume files outside private diagnostic storage, despite the helper's stated private-input boundary.
**Fix:** Resolve each input strictly, reject symlinks in every component, and require the resolved file to be beneath the canonical private root before reading it. Add a control passing a `..` path for each input class and assert refusal before parsing or writing a review report.

**Disposition:** Resolved in `ec76c39`: unit-graph input/output paths reject `..`, resolve beneath the validated canonical private root, and reject symlink components; output additionally requires an existing contained parent. The public authored control exercises traversal for every graph-input label and the output (`scripts/verify-cpu-diagnostic-unit-graph.py:424-469,483-529`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:693-718`).

#### WR-10: Source-closure stdout exposes private derived fields

**Severity:** WARNING
**File:** `scripts/verify-cpu-diagnostic-source-closure.py:544-551`
**Issue:** After writing the mode-restricted report, the CLI prints the private closure status, projection-derived module/registration/value counts, and overlay digest to stdout. `--prepare-source` passes this output through, so callers that capture command output also capture private audit details outside the private report.
**Fix:** Keep stdout to a generic success message and retain the detailed fields only in the private report. Add a control asserting the CLI output contains no private status, count, digest, or path fields.

**Disposition:** Resolved in `ec76c39`: source-closure success prints only a fixed generic line, and the authored control injects sentinel private status, digest, and counts then checks the exact stdout (`scripts/verify-cpu-diagnostic-source-closure.py:552-557`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:650-692`).

#### WR-11: Boolean values pass strict unit-graph integer checks

**Severity:** WARNING
**File:** `scripts/verify-cpu-diagnostic-unit-graph.py:185-186,273-274,388-389`
**Issue:** Python treats `True == 1` and `False == 0`. The graph schema check therefore accepts `version: true`, and a `roots` list containing `false` compares equal to the collected root index `[0]`. A malformed graph can reach the success report despite the verifier's closed version/root contract.
**Fix:** Require `type(graph["version"]) is int` before comparing it with `1`, and validate each root index with `type(index) is int` and an in-range check before comparing roots. Add Boolean-version and Boolean-root refusal controls.

**Disposition:** Resolved in `ec76c39`: graph version and root indices require exact integer types, root indices are range-checked, and dependency indices also retain exact type/range checks. Authored controls reject Boolean and floating-point versions and roots (`scripts/verify-cpu-diagnostic-unit-graph.py:185-186,271-276,365-370`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:478-491`).

No private source files, ignored audit artifacts, raw logs, or local result counts were inspected or copied into this report. No builds or tests were run by this reviewer.

_Incremental review: 2026-11-13T02:50:53Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Private-Path and Pre-use Gate Follow-up

**Reviewed:** 2026-11-13T03:13:41Z
**Frozen source revision:** `ec76c39` (diff base `ad33214`)
**Current incremental scope:** `scripts/build-cpu-diagnostic.sh`, `scripts/prepare-cpu-diagnostic-vendor.py`, `scripts/tests/test_cpu_diagnostic_overlay_controls.py`, `scripts/verify-cpu-diagnostic-source-closure.py`, `scripts/verify-cpu-diagnostic-unit-graph.py`, `scripts/verify-cpu-diagnostic-build.py`, and `scripts/tests/test_cpu_diagnostic_build_controls.py`

This supplement reuses prior coverage of all seven paths; the reviewed union remains 126 unique paths. It verifies the resolutions of CR-09 and WR-09 through WR-11, including that vendor/source-closure entry points check the private root before receipt reads and that the post-build helper checks the root before opening the qualification receipt or invoking the validator. Unit-graph inputs and outputs are confined to the canonical private root, graph versions and root and dependency indices reject non-integer values, and source-closure success output no longer carries private-derived fields. The new controls exercise these refusal and output boundaries using authored fixtures. No new source-grounded findings were found. Executor-reported controls and normal/ASan checks were not rerun by this reviewer. No real package build hook, diagnostic ROM, or guest execution is claimed.

_Incremental review: 2026-11-13T03:13:41Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Exception-Park Overlay Correction

**Reviewed:** 2026-11-13T03:37:33Z
**Frozen source revision:** `5862cd3` (diff base `3e59240`)
**Current incremental scope:** `scripts/build-cpu-diagnostic.sh`, `scripts/tests/test_cpu_diagnostic_overlay_controls.py`, and `tests/cpu-diagnostic/overlay.patch`

This supplement reuses prior coverage for all three paths; the reviewed union remains 126 unique paths. The new manifest edit is bound to one exact source-file/range digest and has an empty replacement. Its authored control checks the manifest directive, applies a synthetic deletion through the production applier, and confirms that the following permanent park loop remains. The RED evidence labels this a manifest-control failure and explicitly does not establish CPU startup. No actual package compilation, ROM, or guest execution is claimed.

### Warnings

#### WR-12: Existing overlay checkout can bypass the new guarded deletion

**Severity:** WARNING
**File:** `scripts/build-cpu-diagnostic.sh:10,93-109`; `scripts/verify-cpu-diagnostic-source-closure.py:455-469,478-505`; `tests/cpu-diagnostic/overlay.patch:1`
**Issue:** When `overlay-checkout-8aa0921` already exists, `prepare_source()` skips applying the current manifest. The new deletion edits `src/exception_handler.rs`, which is already one of that manifest's changed paths, so the old checkout has the same expected changed-path set. Source-closure validation compares only path membership and then records the current file hashes; it does not compare those bytes with the exact result of applying the current manifest to the pinned base. A stale checkout lacking the new park correction can therefore be sealed under the newly named closure report and proceed toward Cargo with source that does not match the current overlay.
**Fix:** Before sealing a reused checkout, derive expected output bytes for every overlay target from the exact pinned base and current manifest, including generated files, and refuse any mismatch. Add an authored stale-checkout control whose changed-path set matches but whose target bytes omit the new deletion; require refusal before report creation or compilation.

**Disposition:** Resolved in `212fc16`: the production applier now exposes a no-write planner, and source-closure validation derives all expected target bytes from the pinned base and current manifest, checks each applied target through the confined path resolver, and compares bytes before generating a closure report. The authored controls cover accepted bytes, stale content, one-byte drift, and a symlinked target (`scripts/apply-cpu-diagnostic-overlay.py:225-316`; `scripts/verify-cpu-diagnostic-source-closure.py:364-382,489-496`; `scripts/tests/test_cpu_diagnostic_overlay_controls.py:732-809`). File mode differences are intentionally excluded from that byte comparison; private-root and regular-file checks remain in place.

_Incremental review: 2026-11-13T03:37:33Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: WR-12 Exact Applied-Source Binding

**Reviewed:** 2026-11-13T03:51:15Z
**Frozen source revision:** `212fc16` (diff base `5862cd3`)
**Current incremental scope:** `scripts/apply-cpu-diagnostic-overlay.py`, `scripts/verify-cpu-diagnostic-source-closure.py`, and `scripts/tests/test_cpu_diagnostic_overlay_controls.py`

This supplement reuses prior coverage for all three paths; the reviewed union remains 126 unique paths. The extracted planner derives exact output bytes from the pinned base and guarded manifest without writing. The production source-closure path compares every derived target with the applied checkout using the existing confined, no-symlink path resolver and refuses mismatches before report creation. The authored controls cover matching bytes and stale, mutated, and symlinked targets. The report does not rely on file mode equality; the private root remains owner-only. WR-12 is resolved. Executor-reported controls were not rerun by this reviewer, and no actual package build or guest execution is claimed here.

_Incremental review: 2026-11-13T03:51:15Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Plan 16 Volatile Selector Read

**Reviewed:** 2026-11-13T04:25:50Z
**Frozen source revision:** `03ab52b` (diff base `212fc16`)
**Current incremental scope:** `tests/cpu-diagnostic/replacements/tests_runner.rs` and `tests/cpu-diagnostic/overlay.patch`

This supplement reuses prior coverage for both paths; the reviewed union remains 126 unique paths. The runner now reads the selector through `read_volatile(addr_of!(...))`, which forces a memory read without first forming a reference to the immutable static. The selector remains a retained `u32` initialized to zero in `.t64_selector`; the linker still aligns and keeps that section, and the updated replacement digest matches the manifest. Selector decoding, bounds checks, test dispatch, transcript, and park lifecycle are unchanged. No source-grounded findings were found. The reported controls were not rerun; this source review does not establish the generated ELF's load instruction, final ROM composition, or guest startup.

_Incremental review: 2026-11-13T04:25:50Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: CPU Diagnostic Unsupported Stop Observations

**Reviewed:** 2026-11-13T05:01:12Z
**Frozen source revision:** `03ab52b` (diff base)
**Current incremental scope:** `runner/t64_cpu_diagnostic.c` and `runner/tests/test_cpu_diagnostic.c`

This supplement reviews only the current uncommitted changes in these two paths; both are already included in the retained 126-path review union. The observer now records the CPU unsupported trace event as the last attempted PC/opcode while continuing to mark entry and park only from retirement events. The incomplete stdout branch exposes the stop outcome and bounded execution observations, then exits before batch encoding or output creation. The authored control replaces the second guest instruction after one retirement and checks that the refusal instruction is reported and that no incomplete batch is written. The source-grounded review found no new issue. No tests were rerun by this reviewer, and no private guest input or execution result is reported here.

_Incremental review: 2026-11-13T05:01:12Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Frozen Phase 04 Diagnostic Pipeline

**Reviewed:** 2026-11-13T05:55:11Z
**Frozen baseline:** `a3afef3`
**Current incremental scope:** `runner/t64_cpu_diagnostic.c`, `runner/tests/test_cpu_diagnostic.c`, `scripts/build-cpu-diagnostic.sh`, `scripts/build-qualified-cpu-diagnostic.py`, `scripts/run-cpu-diagnostic.py`, `scripts/tests/test_cpu_diagnostic_build_controls.py`, `scripts/tests/test_cpu_diagnostic_run_controls.py`, `tests/cpu-diagnostic/overlay.patch`, and `docs/spec/cpu-diagnostic.md`

This supplement reuses the historical 126-path review and adds four paths to the exact 130-path union in the frontmatter. I statically reviewed the changed runner and tests, both new orchestration scripts and their controls, the build-script dispatch, the overlay edit, and the specification update. Previously saved source, lock, tool, graph, and asset qualification was reused without reopening private inputs. The handler adaptation is limited to the two guarded CP0 snapshot mnemonics; required test bodies, assertions, membership, and core policy remain unchanged. The completion fast path requires a parsed complete transcript, entry retirement, exact capture accounting, and two observed park jump/NOP cycles; authored controls cover incomplete transcripts and a non-NOP delay slot. No build or test command was run by this reviewer, and no private diagnostic output or external corpus details are included here.

### Critical Issues

#### CR-10: Batch runner trusts an adapter that is not bound to the qualified build

**Severity:** BLOCKER
**File:** `scripts/run-cpu-diagnostic.py:334-336,426-433,718-727`; `scripts/build-qualified-cpu-diagnostic.py:148-159,178-190`
**Issue:** The required `--adapter` argument is accepted if it names any non-symlink file. The loader then trusts that executable's self-reported core and profile identities, but the qualified-build seal does not record the adapter digest: the builder computes `adapter_sha` only to detect changes during its own inventory-sealing step, and omits the adapter from the sealed artifacts. During completion verification, the Python runner accepts the adapter's exit code and outer JSON framing without independently validating the report. A substituted executable can therefore return the expected runtime identity and validation responses, then emit a fabricated complete report with exit code zero; the CLI persists it as a successful whole-inventory result.
**Fix:** Record the selected adapter's path and SHA-256 in the qualified-build seal, verify the digest before every adapter invocation, and refuse a substituted or changed binary. Add a control that replaces the adapter with one that reports matching core/profile fields but fabricates completion.
**Disposition:** Resolved at `ce87869`: the qualified-build seal records the canonical adapter path and digest; the loader rejects alternate paths and mismatched bytes; and guarded invocation checks the seal and adapter before and after every real adapter call. Run manifests and the current-run pointer also bind the adapter identity. The authored controls exercise matching-identity fabricated completion, alternate-path refusal, and changed-binary refusal.

### Warnings

#### WR-13: Successful build commands can leave descendants running

**Severity:** WARNING
**File:** `scripts/build-qualified-cpu-diagnostic.py:59-83`
**Issue:** `run()` waits only for the process leader. It kills the process group on timeout or nonzero exit, but returns normally when the leader exits zero without checking whether its process group still contains children. Because stdout and stderr are regular files, a forked child can inherit them without keeping `wait()` blocked, then continue running after the build step is recorded successful. This applies to the Cargo build and its build-script subprocesses, so a child can continue writing to the private build tree or logs while later steps copy and seal artifacts.
**Fix:** Ensure the whole process group has exited before returning success, or terminate remaining descendants after the leader exits. Add a control where a successful parent leaves a child alive and assert that the child is gone before the helper reports success.
**Disposition:** Resolved at `ce87869`: `run()` observes the leader without reaping it, signals its process group after leader exit on both success and failure paths, reaps the leader, and accepts success only after the group disappears. A saved authored control covers a successful parent with a surviving child; the existing timeout descendant control remains. These controls were not rerun by this reviewer.

No source changes were made for this review.

_Incremental review: 2026-11-13T05:55:11Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Diagnostic Pipeline Fixes and Completion Classification

**Reviewed:** 2026-11-13T06:29:46Z
**Frozen source revision:** `ce878697da543a64d7cf94ecb830118acdddca59`
**Current incremental scope:** `scripts/build-qualified-cpu-diagnostic.py`, `scripts/run-cpu-diagnostic.py`, `scripts/tests/test_cpu_diagnostic_build_controls.py`, `scripts/tests/test_cpu_diagnostic_run_controls.py`, `runner/t64_cpu_diagnostic.c`, and `runner/tests/test_cpu_diagnostic.c`

This follow-up reuses the existing 130-path union; all reviewed paths are already listed in the frontmatter. CR-10 is resolved: the qualified seal and per-run manifests bind the production adapter's path and digest, and the runner refuses a different path or changed bytes. `invoke_adapter()` checks that binding before and after each production adapter call, including the startup qualification path. WR-13 is resolved: the build helper now kills the command's process group after leader exit and refuses success until the group is gone. The authored controls include both a fake adapter that reports matching identities while fabricating completion and a successful command whose child outlives its leader. Those saved controls were not rerun by this reviewer.

The bound-hit path now classifies missing entry or the expected park retirement as incomplete when the tick limit is reached. `command_run()` exits before batch encoding for that protocol status. The authored guest control emits a complete transcript while redirecting the park jump away from the required PC and asserts incomplete classification with no batch output. Output-accounting and loss failures still take the harness-error path before this incomplete classification. No new private inputs or diagnostic outputs were inspected.

### Warnings

#### WR-14: A command that crossed its deadline can still be reported successful

**Severity:** WARNING
**File:** `scripts/build-qualified-cpu-diagnostic.py:68-75,92-95`
**Issue:** `run()` sets `timed_out = True` when the deadline expires, then signals the process group and waits for the leader. Its final success condition checks only `returncode` and `group_empty`; it ignores `timed_out`. If the leader exits with status zero after the deadline but before the signal takes effect, and the group is then empty, the helper records the expired command as successful and continues to copy or seal its output.
**Fix:** Include `timed_out` in the failure condition so any expired command fails even if it exits zero during cleanup. Add a deterministic control for the deadline/exit race and assert that no success result is returned.
**Disposition:** Resolved in the current worktree correction based on `ce87869`: the final success condition now rejects `timed_out` independently of exit status and process-group cleanup. The authored regression controls the OS outcome and clock, confirms timeout, exit-zero, and empty-group preconditions, and asserts the helper refuses success.

_Incremental review: 2026-11-13T06:29:46Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: WR-14 Deadline Race Correction

**Reviewed:** 2026-11-13T06:44:14Z
**Frozen baseline:** `ce878697da543a64d7cf94ecb830118acdddca59`
**Current incremental scope:** `scripts/build-qualified-cpu-diagnostic.py` and `scripts/tests/test_cpu_diagnostic_build_controls.py`

This bounded follow-up reuses the exact 130-path union in the frontmatter. `run()` now fails when its deadline expires even if the process exits with status zero and process-group cleanup succeeds, closing WR-14. The deterministic authored control reaches all three conditions and asserts refusal; the saved pre-fix RED identified the intended assertion. I did not rerun tests or builds. No new source-grounded findings remain in this follow-up.

_Incremental review: 2026-11-13T06:44:14Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Incremental Review Supplement: Hosted Run-Control Fixture Root

**Reviewed:** 2026-11-13T09:05:58Z
**Comparison base:** `e69c41c`
**Current incremental scope:** `scripts/tests/test_cpu_diagnostic_run_controls.py`

This source change reuses the existing 130-path review union. The authored controls now require the existing `build` directory and refuse it if it is missing or a symlink, then create their unique fixture directory there with owner-only permissions. The fixture content remains synthetic, the existing class teardown removes the created directory, and no private qualification directory or external input is required. The diff leaves the authored control cases and production runner unchanged. No source-grounded findings were found. I did not rerun tests.

_Incremental review: 2026-11-13T09:05:58Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_


## Plan19 compatibility delta

The configured deep review and focused re-review cover the additional profile,
state, CPU/memory and sealed-selection changes. Three new warning-level
coverage findings were fixed; no runtime blocker was confirmed. The final
annul side-effect assertion was verified by the orchestrator in normal and
ASan CPU-control runs. [Review and dispositions](04-19-REVIEW.md) preserve
the original findings and exact follow-up scope; current open findings are
zero. The file list above is the union with the saved prior phase review.

## Plan20 compatibility v2 delta

The configured review covers the added profile/state, coprocessor, CP0/MMU,
cache/data and sealed-selector delta. It found one conversion destination
scope defect. A real pre-fix regression, operand-specific correction, normal
and instrumented owning checks, and focused independent re-review resolve it.
[Plan20 review](04-20-REVIEW.md) and [correction evidence](04-20-REVIEW-FIX.md)
preserve the initial finding and exact scope. Current open findings: zero.
The union above retains all earlier phase review paths.

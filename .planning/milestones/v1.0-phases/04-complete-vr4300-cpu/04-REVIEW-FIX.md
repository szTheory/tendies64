---
phase: 04-complete-vr4300-cpu
fixed_at: 2026-11-13
review_path: .planning/phases/04-complete-vr4300-cpu/04-REVIEW.md
iteration: 3
findings_in_scope: 24
fixed: 24
skipped: 0
status: all_fixed
---

# Phase 04: Code Review Fix Report

**Fixed at:** 2026-11-13T01:45:28Z
**Source review:** `.planning/phases/04-complete-vr4300-cpu/04-REVIEW.md`
**Iteration:** 3

**Cumulative summary after the incremental pipeline fixes:**
- Findings in scope: 24
- Fixed: 24
- Skipped: 0

## Fixed Issues

### WR-05: Failed rollback deletes the original recovery copy

**Files modified:** `scripts/apply-cpu-diagnostic-overlay.py`, `scripts/tests/test_cpu_diagnostic_overlay_controls.py`  
**Commit:** `999681b`  
**Applied fix:** A rollback copy is now kept under a recovery-specific name when restoring the original file fails, and the incomplete-rollback error identifies that recovery copy. The new deterministic control injects both a later-file publication failure and a rollback failure, then checks the original bytes and mode, the reported recovery filename, and cleanup of disposable staging files.

### WR-06: A mode-setting failure still discards a complete recovery copy

**Files modified:** `scripts/apply-cpu-diagnostic-overlay.py`, `scripts/tests/test_cpu_diagnostic_overlay_controls.py`  
**Commit:** `04be456`  
**Applied fix:** The rollback path now tracks completion of the original-byte write separately from mode restoration. If `chmod` fails after the bytes have been flushed and synced, it retains the recovery copy and reports its path, the failed mode restoration, the unverified recovery-file mode, and that the target remains unrestored. The control confirms the original bytes exist immediately before the injected mode failure, then checks the retained bytes and the reported failure details.

### WR-07: Entry and park sentinels are not pinned by the linker

**Files modified:** `tests/cpu-diagnostic/overlay.patch`, `tests/cpu-diagnostic/replacements/main.rs`, `tests/cpu-diagnostic/replacements/tests_runner.rs`, `tests/cpu-diagnostic/replacements/linker.ld`
**Commit:** `2420399`
**Applied fix:** The entry and park routines now use naked assembly in retained dedicated sections. The linker fixes them at the runner's entry and park addresses and asserts the symbols, addresses, and four-instruction/eight-byte layouts. Park emits the runner-required self-jump opcode.

### WR-08: BSS is zeroed before the heap/stack boundary is checked

**Files modified:** `tests/cpu-diagnostic/overlay.patch`, `tests/cpu-diagnostic/replacements/main.rs`, `tests/cpu-diagnostic/replacements/linker.ld`
**Commit:** `c3c4844`
**Applied fix:** Startup checks the BSS range, heap boundary, stack bounds, and 4 MiB RDRAM limit before clearing BSS. An invalid layout parks without producing a completed transcript. Linker assertions reject BSS that overlaps the heap or stack and reject heap/stack bounds outside profile RDRAM.

## Verification

Verification ran from the main checkout.

- Before the WR-05 production change, its double-failure control reached the recovery-copy assertion and observed that no recovery copy remained.
- Before the WR-06 production change, its mode-failure control confirmed the original bytes were complete at the injected `chmod` failure, then reached the recovery-copy assertion and observed that no recovery copy remained.
- After WR-06, `PYTHONDONTWRITEBYTECODE=1 python3 -B scripts/tests/test_cpu_diagnostic_overlay_controls.py` passed all ten overlay controls.
- After WR-06, the gate preset configured and built all targets; the exact three diagnostic CTests passed: `runner.cpu-diagnostic`, `runner.cpu-diagnostic-protocol`, and `runner.cpu-diagnostic-inventory`.
- After WR-06, the ASan/UBSan preset configured and built all targets; the same three CTests passed with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. These native checks were not rerun for the Rust/linker-only WR-07/08 changes.
- A small fully authored MIPS assembly fixture linked with the updated linker script using the already-qualified MIPS binutils. The linked entry and park symbols matched the runner PCs, park encoded opcode `0x08000108`, and an oversized BSS was rejected by the heap assertion.
- All 14 public replacement files match their final SHA-256 values in `overlay.patch`; `git diff --check` passed for the two WR-07/WR-08 commits.

**Limitations:** No Rust/Cargo build or guest diagnostic was run, and no final Rust ELF or private qualification input was inspected. The already-qualified MIPS binutils were used only for the fully authored linker fixture, which verifies linker assertions and instruction encodings but not compiled replacement functions. The executor owns the existing overlay controls and source-closure/qualification checks; those checks were not rerun here.

---

_Fixed: 2026-11-13T01:45:28Z_
_Fixer: the agent (gsd-code-fixer)_  
_Iteration: 3_

## CR-07: Composition report uses the committed qualification API

The helper depended on an additional digest field present in the executor's
uncommitted qualifier work, although the committed API returns only compiler
and binutils paths. The new authored main-path control reached report creation
and failed with `KeyError: target_binutils_sha256` before the correction.

The helper now hashes the receipt-validated objdump directly, records that hash,
and checks it remains unchanged after inspection. It no longer depends on an
uncommitted return field. The complete control executes argument handling,
production main-path orchestration with an authored qualification seam, all
layout/composition checks, and private report creation; it asserts report
identity, mode and the absence of guest-execution qualification.

`python3 -B scripts/tests/test_cpu_diagnostic_build_controls.py` passes all
13 authored controls after the correction. `git diff --check` passes. These
controls use authored ELF/payload/boot/font bytes and an injected objdump
response; no real Rust diagnostic build or guest startup is established.

## CR-08: Match the invoked objdump against the receipt digest

The added main-path replacement control changes the authored tool immediately
after the qualification seam returns. Before the fix it reached report creation
and failed because the expected refusal never occurred. The corrected helper
retains the receipt bytes across validation, refuses any receipt change, obtains
the approved objdump path/digest from those same bytes, and compares the local
executable with that approved identity before invocation and after inspection.

All 14 authored controls now pass. The replacement case verifies no objdump
invocation and no report creation on mismatch. The successful case still creates
the private bound report using only the committed qualification API. This uses
the existing owned-host tool path/hash policy; it does not introduce a sandbox
for hostile concurrent processes with the same host authority.

## CR-09 adjacent post-build caller: Refuse unsafe roots before receipt reads

The post-build helper now requires its existing private root to equal its strict
canonical path, including ancestors, and rejects group/world permissions before
reading the receipt. Two complete main-path controls cover a symlinked `build`
ancestor and an exposed private root; both assert that qualification and external
tool invocation remain unreached and no report is created.

All 16 authored helper controls pass. As a current regression check, the two new
controls were also exercised against the exact committed `cb23a95` helper loaded
in memory: both reached the assertion that qualification was invoked before the
required early refusal. This isolated check followed implementation and is not
original TDD history. Its first attempt omitted the qualifier exception type in
the authored seam; that fixture error was corrected before accepting the two
behavioral failures. No external source/tool/guest execution occurred.

## Prebuild boundary findings closed at ec76c39

CR-09 and WR-09 through WR-11 are resolved in the combined prebuild fix, including
the adjacent vendor and post-build callers. Private ancestors and permissions
are checked before effects, creation uses restrictive permissions, graph inputs
cannot traverse out of private storage, graph integer fields reject Boolean and
floating values, and closure stdout contains no detailed private audit fields.

The 22 authored overlay/gate controls pass with accepted baselines and isolated
refusal cases. Normal and ASan/UBSan all-target builds and the exact three
diagnostic CTests pass after these changes. The existing qualification receipt
and saved offline dependency graph also pass the corrected gates without
reinstallation, renewed acquisition, or another Cargo graph generation. The
root-owned composition helper's 16 authored controls pass separately.

The configured combined review at ec76c39 is clean. These are prebuild and
authored-control facts; actual diagnostic compilation, ELF/ROM composition and
guest startup remain separate required evidence.

## WR-12: Verify reused checkout contents before sealing

The applier now exposes its existing no-write output plan; source-closure
validation compares every applied target with that exact plan through the
confined path resolver. A matching path list alone cannot seal stale or edited
bytes. File owner permission masks may differ under private creation permissions;
the content and symlink checks remain strict.

All 24 authored controls and the bounded registered normal diagnostic control
pass. The actual prepared source, existing qualification receipt and saved graph
pass the corrected checks. The configured review at 212fc16 is clean. Native C,
CTest registration and core semantics are unchanged; their sanitizer/core/fuzz
evidence is reused. This closes the source-fidelity finding, not guest execution.


## Pipeline execution, adapter identity and command lifecycle

The owned pipeline now uses the existing qualification, source-closure and
composition checks before building or running. It derives inventory membership
from the sealed projection and delegates all batch validation and group
calculation to the production C implementation. The qualified seal binds the
adapter's canonical path and binary digest before its self-reported identity is
trusted; each invocation is checked before and after execution. Fresh private
attempts preserve their own inputs, observations and canonical records.

CR-10 is closed by accepted real-adapter baselines and refusals for a substitute
that fabricates matching identities/completion and for a changed bound binary.
WR-13 is closed by job-group cleanup even after a successful leader exit. Its
regression first failed because the descendant retained its FIFO writer. The
final helper retains the leader PID until cleanup and requires group
disappearance. A transient signal error for an exited group is not treated as
proof of cleanup. WR-14 is closed by independently refusing an observed timeout;
its RED control reached timeout=true, exit=0 and completed cleanup before failing
the intended acceptance assertion.

The completion path accepts an early result only through the shared guest parser
and retired jump/NOP loop observations. Missing entry/park at the tick bound is
incomplete and produces no authenticated batch; capture loss and invalid output
remain refused. A complete transcript with the wrong terminal loop first
reached the intended pre-fix classification failure, then passed the incomplete
control after the fix. Direct required test instructions and current CPU refusal
policies are unchanged.

Final owning checks: normal all-target build and the exact three diagnostic
CTests passed in 14.88 seconds; ASan configure/all-target build and the same
selection passed in 36.47 seconds with the inherited ASan/UBSan environment.
The selection executes 20 authored build controls and 11 authored batch controls,
plus the existing qualification/overlay and C protocol controls. The first
instrumented substitution fixture hit an earlier target-path refusal; the
fixture was corrected to prove an accepted instrumented target before reaching
the intended substitute-path refusal. No control was weakened to accept an
unreached case. The configured final review is clean. These are authored
verification facts and contain no external diagnostic result.

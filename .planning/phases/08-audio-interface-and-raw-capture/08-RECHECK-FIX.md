---
phase: 08
fixed_at: 2026-11-19T01:57:00Z
review_path: orchestrator-verified recheck findings W1, W2, I3, I5, I6 (no REVIEW.md); rechecked fixes are in .planning/phases/08-audio-interface-and-raw-capture/08-REVIEW-FIX.md
iteration: 2
findings_in_scope: 5
fixed: 5
skipped: 0
status: all_fixed
---

# Phase 08: Recheck Fix Report

**Fixed at:** 2026-11-19T01:57:00Z
**Source:** independent recheck of commits `cc18852` (CR-01) and `6a956aa` (WR-01), findings verified by the orchestrator
**Iteration:** 2 (follows `08-REVIEW-FIX.md`; that report and `08-REVIEW.md` are unchanged)

**Summary:**
- Findings in scope: 5 (W1, W2, I3, I5 documentation sentence, I6)
- Fixed: 5. W1 and W2 are logic changes and are marked `fixed: requires human verification`.
- Documented only, not changed: I4 (see below)

**Commits (all on `main`, unpushed):**

| Commit | Findings | Files |
|--------|----------|-------|
| `747aa64` | W1, I3, I5 (docs) | `runner/t64_raw_capture.c`, `runner/tests/test_audio_capture.c`, `docs/spec/raw-capture.md`, `docs/spec/audio-cases.json`, `scripts/qualify-audio.py` (inventory digest pin only) |
| `0cdb725` | W2 | `scripts/qualify-audio.py`, `scripts/tests/test_audio_qualification.py` |
| `caac95f` | I6 | `runner/t64_audio_observation.h` |

Verification ran in the main checkout (`workflow.use_worktrees=false`), using the approved `gate` and `asan` test presets (4 jobs). Logs are local and git-ignored under `build/`; every CTest batch has its own `--output-junit` and copied `LastTest.log` files from both before and after the run.

## Fixed Issues

### W1: Staging-parent cleanup gate leaves residue in group-writable parents (fixed: requires human verification)

**Files modified:** `runner/t64_raw_capture.c`, `runner/tests/test_audio_capture.c`, `docs/spec/raw-capture.md`, `docs/spec/audio-cases.json`, `scripts/qualify-audio.py`
**Commit:** `747aa64`

**Reasoning re-checked.** I re-derived the reviewer's claim before removing the gate. Cleanup is an identity-checked (`dev`, `ino`, owner, plus mode 0700 on the success path) `unlinkat(parent_fd, name, AT_REMOVEDIR)` relative to the retained parent descriptor. `rmdir` semantics give three outcomes for a principal that swaps the visible entry between the check and the call: a symlink is refused (`ENOTDIR`, never followed), a non-empty directory is refused (`ENOTEMPTY`), and an empty directory is removed. The third case gives the other principal nothing. Moving a directory into the stage name needs write access to its source parent, and in a non-sticky destination parent the same principal can already delete any empty entry. A sticky parent was already allowed by the gate. A parent not owned by the runner's effective user can be written only through group, other or ACL access, and the same argument applies. I found no exploitable consequence, so I kept the identity checks, dropped `destination_allows_safe_stage_cleanup`, and did not take the "keep the gate and document" alternative. The remaining windows only leave an empty mode-0700 container behind, if another principal renamed the entry away first. `docs/spec/raw-capture.md` now says so.

**Applied fix.** Removed the gate helper and its two uses (`t64_raw_capture_abort`, `t64_raw_capture_finalize`). The abort path keeps an explicit `parent_fd >= 0` check, which the helper used to provide.

**Test first.** New case `group-writable-parent-no-residue`, in `controls.real.raw-capture-target-errors`, builds a mode-0775, non-sticky destination parent and asserts three things. A late rename failure leaves the parent empty. A mid-stream write failure leaves it empty. An accepted capture leaves exactly its target. All three scenarios run before the case returns, so the RED run shows every observed residue.
- RED against the previous code: `build/gate/08-recheck-w1-red-ctest.log`, `build/gate/08-recheck-w1-red.junit.xml`, `build/gate/08-recheck-w1-red-before-LastTest.log`, `build/gate/08-recheck-w1-red-after-LastTest.log`. Each scenario left one `.t64-stage-*` entry; the accepted capture showed 4 entries instead of 1. The setup was valid: the baseline cases earlier in the same test passed and the accepted bundle was complete.
- GREEN after the fix: `build/gate/08-recheck-w1-green-ctest.log`, `build/gate/08-recheck-w1-green.junit.xml` and its two `LastTest.log` copies. The new case's "case reached" line is in `08-recheck-w1-green-after-LastTest.log`.
- The 8 raw-capture CTests (without `harness.audio-qualification`) also passed before the commit: `build/gate/08-recheck-w1-controls-ctest.log`, `.junit.xml`, and the `LastTest.log` copies.

**Registration.** The new case is in the `--list-cases` output and in the `capture-errors` suite of `docs/spec/audio-cases.json`. That file is digest-pinned by `EXPECTED_INVENTORY_SHA256` in `scripts/qualify-audio.py`, so the pin was updated in the same commit. Without it, 9 of the unit tests errored in `_projection()` at the inventory validation, which I observed before updating the pin.

### I3: Stage-parent and child identity recorded after the early-return checks

**Files modified:** `runner/t64_raw_capture.c`
**Commit:** `747aa64`

`create_stage` and `create_child_directory` now record the created entry's `dev`/`ino` immediately after `fstatat` succeeds, before the checks that can return early. Abort's identity-gated `rmdir` can then clean up an attempt that a later check refuses. Cleanup still removes only a directory owned by the runner's effective user whose identity matches the record.

**Control:** none. The early refusals require `fstatat` to fail or a foreign owner or device to appear right after the runner's own `mkdirat`. The only way to reach them from the CLI is a new `--test-fault` name threaded through `t64_real.c` and `t64_supervisor.c`, which is not a cheap, small change. The 8 existing raw-capture CTests and the stage-replacement and interrupted-stage controls pass, so the success and cleanup paths are covered, but the refusal branches themselves are not exercised.

### I5: Mode 0700 not representable on FAT/exFAT

**Files modified:** `docs/spec/raw-capture.md`
**Commit:** `747aa64`

Added one sentence stating the fail-closed limit. I did not run this on a FAT/exFAT volume; the statement follows from the staging-parent mode checks in `create_stage`, which refuse any filesystem that cannot report `0700` after `fchmod`. Capture opens before guest execution (`t64_real.c`), so such a refusal is a host-side `capture-*` error before the guest runs.

### W2: Verifier did not inspect the observation tail (fixed: requires human verification)

**Files modified:** `scripts/qualify-audio.py`, `scripts/tests/test_audio_qualification.py`
**Commit:** `0cdb725`

**New checks** (`_verify_audio_observation_tail`, called at the end of `_verify_audio_observation`). All literals come from `fixtures/audio/pcm.S`, written out independently of any retained record. I decoded the store opcodes (`sw rt,off(rs)`) and counted instruction addresses by hand, then cross-checked them against the C test's `fixture_observations_match` and against real records:
- Bus writes 0-7: the 8 PCM words at `0x3400 + 4i`. Bus writes 8-14: the 7 AI stores with exact addresses and values. Bus write 15: `0x3210` carrying the MI snapshot. All have kind `BUS_WRITE32` (4) and strictly increasing ticks.
- Retirements: 8 at `0x8000045c` (`0xad2b0000`), then the 7 AI stores at `0x8000047c...0x800004ac` with opcodes `0xad09xxxx`. Each shares its tick with its bus write. Slot 15 is all zero.
- `guest_first_escape_*` and all `guest_first_exception_*` fields are zero. `guest_mi_snapshot` has the AI bit (`0x04`), fits the 6-bit MI register, and equals the write to `0x3210`.
- `mi_ai_read_tick >= packets[1].last_tick`. Packet 0 starts after the last AI register write, and the snapshot write comes after packet 1 completes. `guest_last_cpu_tick` is at least the MI read and the snapshot write, and at most the stop tick. `guest_last_cpu_pc == 0x800005e4`, the hello-transcript `sh` before `finish: break`.
- `trace_records >= result.instruction_count + bus_write_count`, because every retirement and every captured write is a distinct drained trace record. This is a bound, not an exact count.
- Yields: the first `yield_count` entries have `before <= reached <= target`, `reached <= stop`, monotone video/audio packet counts and at most 2 audio packets. Entries beyond `yield_count` must be zero. The C writer guarantees this: `trace_observation observations = {0}` and entries are filled only through `yield_count++` (`t64_real.c` ~1246).
- Removed the dead `if field in {...}` filter in the packet metadata loop.

I did not pin exact `trace_records`, exact `yield_count` or exact stop/read ticks beyond the existing 551001 check, because they are not derivable from the authored fixture without running the machine. All 201 retained passing records agree on them, but pinning them would just copy a retained record.

**Test first.** `_audio_observation_baseline()` in `scripts/tests/test_audio_qualification.py` now builds the accepted record with the real tail (authored literals; ticks and trace count are observed values). The existing truncated/forged/resealed test uses it. New `test_tampered_audio_observation_tail_rejected` applies 37 one-variable mutations to that baseline, each asserted to change the bytes and to raise `Refusal`: zeroed bus writes, wrong PCM word/address, wrong AI address/value/kind, snapshot address/AI bit/consistency/width, write-tick order, zeroed retirements, garbage retirement PC/opcode, non-empty slot 15, retirement-versus-write tick, stale and counted-but-empty yields, yield count 66, guest exception seen/epc, escape pc/tick, zero MI read tick, MI read before the last packet, zero last CPU tick, wrong last CPU PC, zero and overflowed trace counts, tampered packet byte/tick/profile/build identity, a changed raw bundle packet, and an inflated result instruction count. I also printed the refusal message for each mutation to confirm the intended check fired, not an incidental one.
- RED against the previous verifier: `build/gate/08-recheck-w2-red.log`. 30 of the 37 mutations were accepted. The 7 already rejected (yield count 66, trace overflow, packet byte/tick/profile/build, bundle packet) remain as regression controls. The baseline itself and the existing forged-record test passed.
- GREEN: `build/gate/08-recheck-w2-green.log`; full unit selftest (22 tests): `build/gate/08-recheck-w2-unit-selftest.log`; `python3 -B scripts/qualify-audio.py --selftest`: `build/gate/08-recheck-w2-qualify-selftest.log`.
- Not new: `CASE_NAMES` gained `tampered-audio-observation-tail-rejected` for discovery.

**No false rejection.** `build/gate/08-recheck-w2-real-record-sweep.log`: the new verifier accepted every retained real passing `audio-pcm-literal` record that has a full 3,976-byte observation. That is 122 records with a raw bundle (real manifest, packets and result) and 79 more checked against bundle inputs synthesised from the observation. That is 201 accepted and 0 rejected. The 4 retained records with a 1,988-byte observation (the WR-01 partial-write controls) are correctly refused by the size check and were not swept. I could not reproduce the orchestrator's count of 113; the tree holds 122 such records now, and the difference may come from the selection filter. Python syntax checks use `ast.parse`; no bytecode files were written.

### I6: Pin the observation layout

**Files modified:** `runner/t64_audio_observation.h`
**Commit:** `caac95f`

Added 64-bit-guarded (`UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu`) `_Static_assert` checks for `sizeof(t64_audio_observation) == 3976`, `sizeof(t64_audio_packet_info) == 280` and the offsets of `packets` (144), `bus_writes` (736), `sw_retirements` (1120) and `yields` (1376). I chose the header over `t64_real.c` because it sits beside the type and is compiled by both the producer (`t64_real.c`) and the test consumer; the existing `<= 4096` assert in `t64_real.c` is unchanged. It compiles in both the normal and ASan builds on this host. A scratch copy of the header with an extra field after `yield_count` tripped the five asserts that the shifted layout affects (record size and the four region offsets). This is a compile-time guard over existing behaviour, so there is no RED run.

**Sealed-body check.** `runner/t64_real.c` itself is unchanged. `scripts/check-rsp-no-hle.py` has no seal for any `runner/` source or header (it inventories `core/`), so no hash needed review and nothing was resealed. It passes on both trees (81 reviewed sources): `build/gate/08-recheck-final-source-body-normal.log`, `build/asan/08-recheck-final-source-body-asan.log`. The runner build identity does change, as for any runner production edit (`runner/t64_raw_capture.c` already changed it), so earlier retained runner binaries are not interchangeable with the rebuilt ones.

## Documented only, not changed

- **I4 (swap window around the `fchmod` in `create_stage`):** skipped as directed. I did not re-analyse it in this pass.

## Verification

- Normal nine-test batch (`ctest --preset gate -R '^(runner\.raw-capture|controls\.real\.raw-capture-.*|harness\.audio-qualification)$'`): 9/9 passed. `build/gate/08-recheck-final-normal-ctest.log`, `build/gate/08-recheck-final-normal.junit.xml`, `build/gate/08-recheck-final-normal-before-LastTest.log`, `build/gate/08-recheck-final-normal-after-LastTest.log`.
- The same nine tests under ASan/UBSan (`ctest --preset asan`): 9/9 passed. `build/asan/08-recheck-final-asan-ctest.log`, `build/asan/08-recheck-final-asan.junit.xml`, `build/asan/08-recheck-final-asan-before-LastTest.log`, `build/asan/08-recheck-final-asan-after-LastTest.log`. Both builds: `build/gate/08-recheck-final-build.log`, `build/asan/08-recheck-final-build.log`.
- `harness.audio-qualification`: passed in both batches. `python3 -B scripts/qualify-audio.py --selftest` and `python3 -B scripts/tests/test_audio_qualification.py --selftest`: passed (22 tests).
- Source/body gate: passed on both trees (see I6). `git diff --check`: clean. `sh scripts/rights-scan.sh`: 0 findings. `sh scripts/pii-scan.sh`: 0 findings. Both scans were also run on the tree after the commits.
- Hygiene: `scripts/__pycache__/qualify-audio.cpython-314.pyc` was present untracked when I started. It is a generated bytecode cache that fails both scanners, and I removed it. It is created again by running `build/gate/runner/test_audio_qualification` (a Python script) outside `python3 -B`; I used `PYTHONDONTWRITEBYTECODE=1` afterwards and no cache remains.
- Local checks used the AUTONOMY pattern: unique `--output-junit` per batch, and `LastTest.log` copied before and after each batch. I did not run `check-audio-cases.py` (its discovery replaces `LastTest.log`). Instead I compared `--list-cases` output to the inventory directly in Python, which is the same comparison.

## Not run

- Full qualification (`qualify-audio.py --native`), pushing, CI and the hosted Linux job: not started, as directed.
- A FAT/exFAT filesystem for I5, and Linux runtime: not exercised. The new W1 control is a mode-0775 non-sticky parent on this macOS host.
- Hosted-style `-Werror` GCC check: run, with the findings below.

## New findings outside the assigned scope (not fixed)

These predate this pass and block the hosted Linux job and Phase 08 qualification. I did not change them because they are outside the requested findings and the commit limit.

1. **GCC 13 `-Werror=type-limits` in `runner/t64_raw_capture.c:835`**, `valid_audio_info`: `(uint64_t)info->frame_count > UINT64_MAX / UINT64_C(4)` is always false because `frame_count` is `uint32_t`. Introduced in `b588325`. Safe fix: drop that comparison. `expected_bytes = frame_count * 4` cannot overflow `uint64_t`. Evidence: `build/gate/08-recheck-final-gcc13-raw-capture-unchanged-flags.log`. With only `-Wno-type-limits` relaxed, my version of the file compiles cleanly under the gcc:13 closure flags, so my changes add no new GCC warnings there.
2. **GCC 13 `-Werror=format-truncation` in `runner/tests/test_audio_capture.c:1855`**, `run_existing_target_case`: `snprintf(directory /*512*/, ..., "%s", pattern /*1024*/)`. Pre-existing code, not touched by this pass. GCC reports no other diagnostics in that unit, so my new code in the file is clean. `runner/t64_real.c` (with the new header asserts) compiled cleanly. Evidence: `build/gate/08-recheck-final-gcc13-changed-units.log`.
3. **`check-audio-cases.py --suite qualification` would fail**, and `_native_command_plan` runs it. `runner/test_audio_qualification --list-cases` prints 20 names (21 now, with the case I added), but the digest-pinned `docs/spec/audio-cases.json` lists only 9 for that binary, and the checker requires an exact match. I compared the lists directly in Python; the 11 unregistered names were added by the 08-05 commits. Fixing it means registering those names in the inventory and re-pinning `EXPECTED_INVENTORY_SHA256`. I did not do this because it is a policy decision about what the denominator should contain.

## Disagreements and cautions

- The orchestrator's W2 brief said to mirror the C assertions in `fixture_observations_match`. That function does not assert the exception, escape, `guest_last_cpu_*` or `trace_records` fields. I derived those expectations from the fixture's structure instead (it runs entirely inside `[0x80000400, 0x80000600)` and ends at `break`, which is a completion stop, not an exception), and cross-checked them against the 201 retained records. They deserve the human check noted above.
- `mi_ai_read_tick` is overwritten on every MI read that has the AI bit set, so it is the last such read (after the snapshot store), not the read captured by the snapshot. I therefore only lower-bound it by the second packet and did not tie it to the snapshot tick.
- The tick-equality checks (each store's retirement and its bus write share a tick; the packet and snapshot ordering) are observed invariants, not authored literals. They hold in all 201 retained records and are the strongest cross-binding between the independent regions. If a later machine change legitimately moves those ticks, this verifier must be updated in step.

---

_Fixed: 2026-11-19T01:57:00Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 2_

## Orchestrator follow-up: pre-existing blockers found by the fixer

The fixer reported three problems outside its assigned scope. The orchestrator
verified each in the source and fixed them before qualification, because each
would have failed `--native` or the hosted Linux job.

- **GCC 13 `-Werror=type-limits`, `runner/t64_raw_capture.c` `valid_audio_info`:**
  removed the always-false `frame_count > UINT64_MAX / 4` comparison
  (`frame_count` is 32 bits, so `frame_count * 4` cannot overflow 64 bits).
- **GCC 13 `-Werror=format-truncation`, `runner/tests/test_audio_capture.c`
  `run_existing_target_case`:** widened `directory` to the 1024 bytes of
  `pattern` and now checks the `snprintf` return value.
- **Stale pinned case inventory:** `runner/test_audio_qualification --list-cases`
  prints 21 cases but `docs/spec/audio-cases.json` registered 9, and
  `check-audio-cases.py` requires an exact ordered match. `--native` would have
  failed at that step after the long shared gate. All 21 are now registered in
  binary order and `EXPECTED_INVENTORY_SHA256` is re-pinned. No check was relaxed.

**Evidence (orchestrator):** the pinned `gcc:13` read-only offline route compiled
`t64_raw_capture.c`, `test_audio_capture.c`, `t64_real.c` and `t64_supervisor.c`
with the repository warning flags and `-Werror`: rc 0. `check-audio-cases.py
--suite qualification --run` passed 4/4 under both normal and ASan builds
(`build/gate/08-orch/inventory-normal.log`, `inventory-asan.log`).
`qualify-audio.py --selftest`, the source/body check, `git diff --check` and the
rights/PII scans pass. Not run: full `--native`, push, hosted CI.

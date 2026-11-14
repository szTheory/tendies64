---
phase: 05-rsp-low-level-emulation
plan: "09"
subsystem: rsp-authored-integration
requires:
  - phase: 05-08
    provides: Supported opcode ledger, fetched reference/invariant coverage and no-HLE controls.
provides:
  - Rights-cleared authored CPU/SP/RSP smoke and five meaningful negative ROMs.
  - Checked source/tool/boot/composition/install/manifest identity closure.
  - Authenticated production-route integration with partitioned-step comparison.
affects: [05-10, rsp-qualification]
requirements-completed: []
completed: 2026-11-14
status: locally-verified-hosted-pending
---

# Plan09 — Authored CPU/SP/RSP guest proof

A rights-cleared authored ROM boots the functional profile, loads IMEM/DMEM
through CPU-driven SP DMA, clears HALT and executes the planned fetched RSP
instructions. CPU bus-visible checks cover saturation/flags, scalar load-use,
partial memory, VMOV, multiply/accumulator slices, divider high/low, status,
PC and semaphore. Only the exact transcript and CPU terminal branch/NOP delay,
with required RSP retirements and the specific load-use stall, qualify success.
Baseline results agree across boot advance partitions1 and16; the DMA/RSP
stage remains single-tick observed.

All five distinct authored negatives reach their intended classifications:
persistent CPU-visible mismatch, exact reserved-opcode refusal, fetched missing
BREAK, altered CPU completion delay, and same-bank-wrap CPU check failure.
Known cleared image identities, boot payload and profile are enforced before
emulation; caller-provided hashes cannot authorize another input. Generated ROMs
use the existing build-only channel for local/CI checks and are never committed
or uploaded. No protected external material enters this integration.

Two production integration bugs were fixed: the inherited warm SP overlay
shadowed functional SP status, and the output-window predicate omitted the new
profile. Both fixes retain historical profile behavior. These results establish
the stated software profile, not hardware equivalence or general compatibility.

## Verification and review

- Final stable-source canonical gate passes127 checks, exact inventories,
  lint86/0 and authored evidence tail in65s:
  build/gate/05-09-final-gate.log.
- Fresh all-target normal/ASan builds precede integration and bus/output/warm
  policy regressions; all pass in build/{gate,asan}/05-09-review-ctest.log.
  Final updated integration passes normally and under ASan/UBSan, with the final
  sanitizer run17.73s in build/asan/05-09-publication-ctest.log. Inventories
  remain127/129 with the single planned registration.
- Optimized GCC13 objects pass owning warning flags for both changed core units
  and the consumer: build/gate/05-09-review-gcc13.log. Diff, shell syntax,
  rights and PII checks pass; owner instruction/config edits retain their hashes.
- Parent review reproduces the unqualified-tool execution gap and corrects
  executable qualification order, packer receipt binding, admitted image set,
  child deadlines and exact independent field/classification assertions.
- Individual guests retain10M ticks, maxstep16, child10s, outer12s and4096
  capture. The serial batch has its own120s timeout. Prior aggregate12s
  timeouts are preserved as harness-scope failures, not guest results.

[Fixture spec](../../../docs/spec/rsp-smoke.md),
[executor evidence](../../../docs/evidence/phase05-authored-rsp.md) and
[parent review](05-09-review-evidence.md) preserve exact source/fixture/artifact
identities, original attempts, setup/encoding corrections, an ineffective initial
negative and its persistent replacement, and the disclosed Task1 TDD process
deviation. Task2's original formal RED is the actual pre-fix SP_STATUS refusal.
No synthetic or setup-only result is relabeled as guest evidence.

Required hosted Linux/macOS verification is pending on the reviewed checkpoint.
Full configured phase code/security review and current-core private qualification
remain Plan10 work. No actual external diagnostic, Cargo rebuild, installation,
physical test or binary publication ran in this plan.

## Next action

Finish the hosted checkpoint, then execute Plan10 current-source qualification,
exact component completion/classification and configured final reviews under the
original rights and resource limits. Existing private receipts keep their original
identities until that deliberate requalification. No user action is needed.

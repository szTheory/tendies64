# Phase 06 — Implementation Research

**Status:** complete for planning; first-use unknowns are execution probes.
**Method:** orchestrator fallback after the configured researcher completed
acquisition but stalled without writing an artifact. Its checkpoint confirmed
the manual's MDU values. The three completed decision reports and canonical
CONTEXT remain the detailed evidence; this report does not claim new runs.

## Scope and source authority

Deliver BUS-03, CPU-08 and VER-02 under CONTEXT D-01..D-12. Read the three
decision reports alongside this summary. No new dependency or tool install is
needed. Existing C17 core, CTest, shell/Python qualification and fixture tooling
are the stack. Preserve the existing profiles and private external-data policy.

## Concrete implementation seams

| Work | Owning seam | Planning consequence |
|---|---|---|
| Two cold/T1 profiles | `core/t64_machine.c`, `core/t64_internal.h`, `core/t64_digest.c` | Explicit policy predicates; inherit compat-v2 and functional RSP; new state domain; keep old descriptor/state bytes frozen. |
| Cold boot | Existing HLE-PIF seed, `fixtures/boot-contract.json`, authored hello and cleared open IPL3 | First execute zero-RI_SELECT path under existing bounds; record first reached operation before expanding the device model. |
| RDRAM protocol | `core/t64_warm_devices.c`, `core/t64_bus.c`, `core/t64_mi.c` | Existing RI_SELECT owner is the narrow insertion point. Add protocol state only for new profiles, complete-span access/refusal rules and independent authored sequences. |
| T1 intervals | `core/t64_cpu.c`, `core/t64_exception.c`, `core/t64_scheduler.c` | Replace fixed next-attempt interval only for new profiles; preflight cost/successor/resource overflow before architectural effects. |
| Timer/Random | `core/t64_cp0.c`, `core/tests/test_cpu_time.c`, `core/tests/test_cpu_cp0.c` | Reuse elapsed-time Count/half-phase and once-per-retirement Random; actual Wired/TLBWR guest ordering must be pinned. |
| Whole BASE evidence | Existing CPU/RSP C diagnostic primitives and independent source qualifiers | A CPU-only projection is incomplete. New C-owned aggregate must authenticate full source membership and original member outputs for one Tier B/T1 target. |
| Context retention | Existing pre-use verifiers, artifact/binding archives | Fresh immutable contexts per attempt; no arbitrary caller hash authority; full verifier before guest directory creation. |

## Timing facts and model policy

The acquired NEC manual check confirms Table 3-12 (printed p.76) integer MDU
whole-pipeline stalls: MULT/MULTU 5, DIV/DIVU 37, DMULT/DMULTU 8,
DDIV/DDIVU 69 PCycles. See `06-CPU-TIMING-DECISION.md` for pinned primary
reference and limits. Base issue interval 1 plus the named stall is selected
T1 software policy, not a hardware latency measurement. Other instructions
retain base cost; an architectural delay slot is its own instruction.
Exceptions and sampled interrupts consume the base boundary without retirement
or Random stepping. Count follows elapsed guest time through intervening events.
Preserve compat-v2's six-bit Wired/modulo64 policy and pre-step TLBWR selection.
No dynamic cache/bus timing or T2 workstream is added.

## First-use probes and unknowns

1. With concrete source/tool/asset pre-use checks complete, execute the exact
   cleared IPL3 plus authored hello through zero RI_SELECT. Preserve its first
   reached RI/RDRAM operation and original bounded result. The source branch
   alone is not proof the compiled image takes it.
2. Nintendo programming §6.10 does not establish analog calibration/readback
   values. Use the actual cold source path and trace to define a versioned,
   deterministic software protocol. Record exact register/address/width,
   selection/broadcast/repeat transitions and refusal effects. No guessed
   silicon readings or ROM/PC-specific success shortcut.
3. Determine minimum logical slots from that path, preserving 4 MiB backing,
   compat-v2's selected absent data-read policy and distinct fetch/store/DMA
   owners. Serialize every future-affecting protocol state.
4. Qualify an early actual member before long native expansion. Derive complete
   default features, levels, aliases, source ordinals and empty-value semantics
   from the pinned executor; exact inventory stays private. Test whether legacy
   original records can be joined losslessly. If not, use a separate phase-wide
   C member protocol over shared primitives; never weaken legacy contracts.

These probes are tasks, not reasons to repeat discussion or block on optional
wrapper abstractions. A missing device/asset remains an identity-bound blocked
member in the denominator; it does not authorize RDP or uncleared asset use.

## Validation Architecture

- Existing infrastructure: CMake presets `gate` and `asan`, CTest four-job
  policy, deterministic gate and admission fuzz/negative control. No install.
- Build all owning normal/instrumented targets before focused CTest. Existing
  baseline inventories are 127 normal and 129 instrumented; additions must be
  explicit. Never build into a tree while its gate is running.
- Native owners: profile/digest/reset/boot/bus/MI, CPU time/CP0/scheduler,
  RSP/DMA interleave. Add accepted setup and reached-case assertions to every
  refusal test. Test full state preservation, partitions and separate instances.
- Timing tests use literal event ticks/Count/retirement/Random expectations,
  guest Wired/TLBWR sequences, exceptions, overflow atomicity and equal-tick FIFO.
- Authored evidence controls invoke complete qualifier entry and normal dispatch;
  reject shrunk independently sealed membership, changed source/tools/artifact,
  duplicate/missing/foreign records, forged pass, unqualified assets and private
  identities in public receipts. Keep one C completeness authority.
- Serialize wall-capped actual diagnostics after sanitizer/fuzz workloads.
  Preserve existing CPU/RSP limits, capture bounds, failed contexts and no-retry
  directions. Hash and archive exact executable plus binding before rebuild.
- Before broad retest use `scripts/verification-scope.mjs` against the last
  tested source. Reuse unchanged semantic evidence without relabeling binaries.
- Final authored-only Linux/macOS CI, scoped source/security review and original
  artifact checks establish the selected software contract. Private external
  lanes remain local; lane definitions alone are not execution evidence.

## Planning direction

Lead with a production tracer: explicit profiles, minimal T1 boundary and the
first authored cold-path observation, then cohesive protocol/timing expansion,
complete C evidence inventory/context, required lane execution/classification,
claims and final reviews. Keep actual qualified first use early. Parent owns
routine closeout; no redundant broad gates for documentation commits.

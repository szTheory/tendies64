# Phase 4 adversarial decision check

**Current verdict: READY FOR PLANNING.** The bounded recheck below confirms all
four findings are resolved in the final context. No implementation or hardware
qualification is claimed.

**Initial verdict: NEEDS CHANGES before the phase plan is final.** The canonical
interpreter, separate CPU profile, functional caches, context-threaded SoftFloat
and private diagnostic overlay are a coherent direction. Correct the four
contract issues below; none requires owner acknowledgment or a broader research
restart. This is an agent review, not owner acceptance or execution evidence.

## Original findings and required corrections

| Issue and consequence | Evidence | Concrete correction |
|---|---|---|
| Status mask permits a reserved write. `0xFF5FFFFF` includes bit 19 despite requiring that bit to read zero. RP is also variant-dependent. | `04-DECISION-CPU.md`, CP0 table; pinned NEC manual pp.166–167, Figures 6-5/6-6, including the p.166 RP footnote. DS bits 23 and 19 are RFU; TS21 is read-only. | The defined-field software mask is `0xFF57FFFF` when RP27 is writable. Preserve hardware-owned TS separately and force RFU zero. Record the profile's RP support policy: the manual limits low-power support to particular VR4300/VR4305 models and fixes RP zero on others; a NUS target label alone does not establish which rule applies. If RP is fixed zero, remove bit27 too. Test individual fields independently of the aggregate mask. |
| Nested exception wording protects EPC but can overwrite its paired BD indication. The saved branch PC would then be interpreted incorrectly on return. | `04-DECISION-CPU.md`, Execution and faults; pinned NEC manual p.201 Figure 6-14 and p.203 Figure 6-15. Both flowcharts bypass EPC **and BD** capture when EXL is already set. | Gate EPC/BD capture together on old EXL=0. With old EXL=1 preserve both; update the applicable exception code/coprocessor/address-context fields and take the common vector. Include nested fault cases with an existing BD=1/EPC pair and a new non-delay fault, and the converse. |
| Free-running Count lacks a complete boundary-order contract. Merely retaining the current CPU scheduling tail ties handler progress to retirement; same-tick Compare writes and interrupts can otherwise depend on event insertion order. | NEC pp.164–165; `core/t64_cpu.c:857`, `core/t64_cpu.c:891`, `core/t64_cpu.c:1027`; `core/t64_scheduler.c:10`, `core/t64_scheduler.c:282`, `core/t64_scheduler.c:367`; `04-RECOMMENDATION.md` item4 and CPU Count section. | Keep the explicit model policy: one guest tick is one model RSP cycle, model CPU:RSP=1:1, Count increments every two ticks from a serialized initial phase. Specify a fixed order for clock synchronization, equality/IP7 update, instruction-boundary interrupt sampling, and CP0 writes at that tick; keep CPU/device FIFO order explicit. A guest exception consumes its scheduled attempt and continues to a later CPU boundary without retiring; guest time and Count still advance. Test equality coincident with Count/Compare writes, delay-slot interruption, idle jumps and partitioned advance. This defines deterministic policy, not physical timing. |
| The local adapter says to apply the existing named profile, but the existing warm profile deliberately lacks the new semantics. Renaming its descriptor alone also does not prepare a running machine. | `04-DECISION-TESTS.md`, sequence step4; CPU identity recommendation; `core/t64_machine.c:157`, `core/t64_machine.c:166`, `core/t64_machine.c:397`, `core/t64_machine.c:467`, `core/t64_machine.c:508`, `core/t64_machine.c:699`; `fixtures/boot-contract.json` warm seed and policy digest. NEC pp.151–153 defines Config fixed/read-only fields and BE initialization. | Bind the local suite to the **new CPU-enabled profile**, not warm-v1. Its reviewed descriptor must select executable entry/reset preparation, explicit CP0/Config/PRId/Count-phase seed, invalid cache seed, qualified boot/image admission and a boot-policy digest. Zero-filled CP0 cannot be silently reused as the complete architectural seed: Config fixed bits and prepared big-endian behavior must agree. Include boot metadata in descriptor v3 serialization rather than inheriting the current v2-only branch. Reuse interpreter/device primitives; dispatch only the named semantic policies. Preserve warm-v1 descriptor bytes, v8 serialization and fixed vectors under their original identity. |

## Retained guards

| Boundary | Adversarial result and planning guard |
|---|---|
| SoftFloat closure and no-host-FP | No decision contradiction found. `04-DECISION-FPU.md` correctly keeps generic arithmetic separate from VR4300 behavior and requires explicit operation state. `core/CMakeLists.txt` currently builds/hashes only top-level `t64_*.c` and selected headers, while `tools/lint/core-lint.sh` defaults to the core tree. Any copied closure outside those sets must enter the production build identity and both source/object lint sets explicitly. No helper, specialization, TLS/global wrapper or runtime-helper allowlist exception may bypass the existing policy. Complete the exact per-file provenance/closure audit before copying; it remains a use prerequisite, not an already completed result. |
| Exact external inventory and SC4 | Keep `04-DECISION-TESTS.md`'s complete per-test/value statuses, source pin, overlay hash, private records and separate adapter. Anchor the allowlist to the pinned upstream CPU membership and account for every CPU registration/value before execution; arbitrary allowlist reductions cannot redefine completeness. Non-CPU and later timing lanes may be reported separately, but omitted/unreachable required CPU members leave SC4 unmet. Failed executed cases can have `fail` status; missing/reduced cases cannot count as a completed inventory. The 758 constructor observation is not a runtime denominator. |
| Diagnostic rights and tools | Keep D2 and the pre-use tool/asset audit exactly as written. No ROM, captures or derived diagnostic results in commits, public CI or uploads. Removing assets must be composition-verified; selecting the already cleared IPL3 still needs format/startup qualification. Root MIT does not resolve compiled binary/asset rights. Native vectors proceed independently but cannot substitute for SC4. |
| Manual uncertainty | Keep p.181's table/prose conflict explicit and the proposed bootstrap-vector choice labeled policy. Invalid PageMask, SC link consumption/invalidation granule and DMA non-snooping remain selected deterministic policies pending stronger evidence. Do not turn those selections into hardware facts or silently weaken unsupported-operation refusal. |

## Evidence and limits

Reviewed the three Phase 4 decision reports and synthesis, current roadmap and
requirements, autonomy/copy policy, and the concrete CPU, scheduler, profile,
digest, bus, build and lint seams. Checked complete relevant pages of the already
hash-verified NEC manual, including Status footnotes and exception flowcharts.
The manual source/digest is the one recorded in `04-DECISION-CPU.md`; no document
bytes, extracts or screenshots are included here. No prohibited emulator source
was read. No packages, builds, CI, diagnostic ROMs or guest execution were used.

## Bounded resolution recheck — 2026-11-11

Read only the final `04-CONTEXT.md` and the corresponding CPU report corrections
and TESTS sequence step4. The original findings above remain as review history.

| Original finding | Resolved disposition |
|---|---|
| Status mask and RP | Context D-04 and the corrected CPU table use defined-field mask `0xFF57FFFF`, then fix unmodeled RP27 zero as explicit profile policy, yielding `0xF757FFFF`. TS remains preserved and RFU zero; this does not assert NUS silicon qualification. Resolved. |
| Nested EPC/BD | Context D-03 and corrected CPU fault section gate both captures on old EXL=0 and preserve both with old EXL=1. Resolved. |
| Count order and exception scheduling | Context D-05 fixes synchronization/new equality/IP7 before eligible interrupt sampling, with CP0 effects only when the instruction executes and commits. Compare commit clears IP7; Count commit preserves phase and checks written-value equality. An unchanged equality after Compare clear does not arbitrarily re-latch, and new pending interrupts are observed at the next eligible CPU boundary. D-03 schedules a later boundary after non-retiring exceptions. The CPU report explicitly defers final ordering to this context. These are coherent deterministic policies; no physical timing claim is inferred. Resolved. |
| Executable new profile | Context D-01/D-02 require a separately prepared CPU profile, explicit Config/PRId/Status/Random/Wired/Count seed, invalid TLB/cache state, qualified guest entry and boot identity in descriptor v3 serialization. Corrected TESTS step4 selects this new CPU profile. Exact seed values and independent seed checks must be completed before first admission; warm-v1 remains preserved. Resolved for planning. |

No remaining contradiction in the four reviewed decisions. The parent can
proceed to planning without user action. Existing per-copy, seed, asset and
toolchain qualification prerequisites still apply before their affected use;
they are not represented as completed checks. This recheck performed no new
research, builds, installs, tests or guest execution.

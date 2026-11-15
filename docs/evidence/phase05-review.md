---
status: issues_found
unresolved_blockers: 0
---

# Phase 05 configured code review

The configured deep Codex code review completed against source through
`0b8786b`, using the project-resolved `gsd-code-reviewer` model
`gpt-6-luna` with `xhigh` effort. Its explicit scope was the 63 phase source,
test, fixture, workflow and provenance paths listed in the
[original report](../../.planning/phases/05-rsp-low-level-emulation/05-REVIEW.md).
The review read implementation and call chains; it did not rerun broad tests.

The initial verdict is **issues found**. It remains preserved rather than
being rewritten as a passing review after fixes.

| Finding | Concrete consequence | Disposition |
|---|---|---|
| CR-01, blocker | A fetched MTC0 DMA request can refuse after the current DMA beat changes memory or queue state, violating the software profile's atomic-refusal contract. | Resolved by projected-queue admission before either effect commits; the configured scoped review confirms closure. Fetched acceptance and span/queue/token refusal controls compare future-affecting state and memory. |
| WR-01, warning | Anonymous content-conditioned effects in decode helpers can escape the no-HLE entry-route digest guard. | The scoped review confirms closure for all cited RSP helpers. The later SP entry-path gap is separately closed below. The guard is bounded, not a proof about arbitrary C behavior. |

## Subsequent parent scope review

The configured reviewer explicitly stated that it had not independently
traced private nonpass ownership against the declared instruction scope.
A separate read-only scope review therefore checked those saved records;
the detailed mapping remains private. It identified a documented synchronous
RSP self-HALT instruction rejected by the asynchronous CPU/host status-write
policy. This is an additional in-scope implementation defect, owned by
`core/t64_rsp.c` and `core/t64_sp.c`. The origin-aware fix now retires guest
self-HALT alone, advances PC, permits DMA while halted and resumes at the saved
PC. Malformed commands, in-flight single-step and asynchronous host requests
retain their refusal contracts. This is selected software policy, without a
simultaneous SU/VU hardware-arbitration claim. Native sanitizer success before
the scope review did not establish the missing behavior.

## Focused fix evidence

All-target normal and instrumented builds precede the affected selections.
Both final selections pass in `build/{gate,asan}/05-10-self-halt-{build,ctest}.log`.
The earlier semantic DMA and self-HALT REDs and their GREEN controls are retained
under `build/gate/05-10-{dma,self-halt}-{red,green}.log`. Fixture/observation
mismatches and pre-pair-guard results remain separately named; they are not
relabeled as final passes. Optimized GCC13 owning-warning checks pass for all
changed C units, with a final affected-unit check after self-HALT.

The complete source/object guard includes all RSP execution helpers and SP
functions, including both host MMIO entry points. The final mutation controls
pass in `build/gate/05-10-no-hle-host-entry-green.log`. The final-evidence
authored tests and selftest pass in
`build/gate/05-10-evidence-commit-{tests,selftest}.log`.
The checker binds original inventory/member/attempt artifacts rather than two
summary records, rejects a dropped original row with recomputed summaries and
refusal-to-pass relabeling, and requires explicit passing CTest rows. A parent
synthetic RED additionally exposed JSON-quoted private identifiers escaping the
privacy pattern; its correction and control are retained separately.

The [configured scoped follow-up](../../.planning/phases/05-rsp-low-level-emulation/05-10-REVIEW-FOLLOWUP.md)
completed and its original findings remain preserved. The parent implemented
the accepted corrections rather than rewriting that verdict:

| Finding | Correction and authored control |
|---|---|
| CR-02 | Fixed canonical native/ASan executables are hashed before identity queries; actual source, build and profile identities must match. An arbitrary valid-looking build ID is rejected. |
| CR-03 | Native evidence requires every registered test exactly once as Passed, with exact inventory and gate/lint markers. Marker-only, dropped, duplicate and failed rows are rejected. |
| CR-04 | The denominator is now checked through the existing pinned-source qualification verifier before C metadata validation. A control reseals both reduced snapshots and all dependent outputs, proves internal consistency, then rejects the reduced set against the original qualified anchor. The actual saved private evidence also passes the new read-only source-anchor integration. |
| CR-05 | Original batches, captured output, attempt receipts and explicit complete-verification output are cross-checked through the existing C protocol. Refusal relabeling is rejected. A correctly authenticated complete partition containing nonpassing members remains nonpassing and is valid classification evidence. |
| CR-06 | Quoted private identity/result fields and selected-ROM identity aliases are rejected, including a selected identity equal to the approved authored digest. Direct and rehashed-report controls pass. Only the single verified authored composition field is admitted in the closed public receipt. |
| WR-02 | All SP functions, including DMA dispatch and host register-write entry points, are guarded. Anonymous entry-path effects and unreviewed helpers are rejected. |

The reviewed core and checker source at `578e86a` passed the complete local
canonical gate, the owning instrumented selection including admission fuzz
and its negative control, and both required hosted jobs. Exact artifacts and
logs are retained. The previous diagnostic binding intentionally pins older
production source, requiring an additive host-only qualification update.
That update is committed at `3a3c8ca`. Its
[configured scoped review](../../.planning/phases/05-rsp-low-level-emulation/05-10-V6-REVIEW.md)
identified one evidence-log blocker. Both producer and independent verifier
now reuse the existing strict registered-test parser; authored marker-only,
failed, duplicate and unknown-name mutations reject. The parent verified the
accepted saved logs and focused qualification, build, runner and evidence
suites after all source corrections. The full instrumented inventory and
owning selected-run totals remain distinct. No other v6 blocker was found by
the configured review. Original reviewer verdicts remain preserved; these are
parent fix dispositions, not a retroactively rewritten clean review.

The combined source at `3a3c8ca` passes the canonical gate, instrumented owning
selection and both hosted jobs. Actual pre-use integration then exposed a host
verifier setup error: the full v6 function referenced undefined digest-pattern
names. Its accepted fixture had exercised helpers rather than the complete
entry point. The runner stopped before adapter or guest use; no private outcome
was produced by that attempt.

The correction at `61542f9` defines those patterns, adds an accepted full-entry
and normal-dispatch regression, and checks undefined global names with the
Python standard library. Fixed refresh paths and schemas preserve the original
sealed archive and failed setup. The producer, independent full preflight and
single current-core run then completed; the exact source-anchored membership
and original C result records have been audited. The focused qualification,
runner and evidence suites pass,
including clean authored setup without private roots. No core or build-policy
input changed; refreshed exact artifacts retain the tested build and profile
identities. The earlier native/instrumented semantic evidence is reused with
its original source identity. Both hosted jobs pass on the correction. The
actual final artifact check passes, and the configured scoped security
follow-up returned SECURED. Phase-goal verification remains separate.

Other source/expectation conflicts remain explicit limits. An authenticated
guest-failure completion without a retained failing assertion establishes the
failure classification, not a localized semantic cause or hardware fact.
Historical results retain their original source and recorded executable hash;
the exact original executable was not retained before a later rebuild. That
artifact-retention limit is disclosed rather than replacing it with a newer
binary or treating a later authored check as a private rerun.

The planned final evidence checker and final-source gates were absent at the
initial review snapshot; their later evidence is distinguished above. A
reviewer invocation is not test evidence; a complete classification partition
is not a passing diagnostic suite. Private diagnostic membership, identities
and results stay outside this public report. Hardware accuracy, optimized
differential execution and general compatibility are not established here.

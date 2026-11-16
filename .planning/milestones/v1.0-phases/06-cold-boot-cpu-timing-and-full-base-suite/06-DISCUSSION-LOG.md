# Phase 6: Cold Boot, CPU Timing and Full Base Suite — Discussion Log

> Audit trail only. Downstream planning uses `06-CONTEXT.md`.

**Date:** 2026-11-15
**Mode:** Autonomous discussion under the owner's standing policy
**Areas:** RDRAM boot profiles; instruction costs; full BASE membership and evidence

Three independent `gsd-advisor-researcher` tasks used the project-resolved
Codex `gpt-6-luna` / `xhigh` configuration. Each returned a bounded comparison
and actual source/document anchors. The parent synthesized the recommendations
and performed the adversarial checks below once. No implementation, build,
guest run, installation, physical measurement or new asset use occurred.

## RDRAM preset and register-protocol boot

| Option | Consequence | Selected |
|---|---|---|
| Separate Tier A/B profiles over the same cleared IPL3 | Preserves existing identities; actual cold branch can be tested before another fixture is introduced | Yes |
| Switch the existing warm profile between modes | Changes behavior under a frozen identity | No |
| Create another boot variant immediately | Adds bytes/provenance before the same-byte path is disproven | No |

**Auto-selection:** Separate new profiles, under AUTONOMY. The exact binary
cold branch remains a first-use experiment, not a fact inferred solely from
source. Logical device topology and calibration readback are named software
policies until stronger evidence exists. No physical calibration claim follows.

## T1 instruction costs

| Option | Consequence | Selected |
|---|---|---|
| Compact versioned static class costs | Meets the accepted T1 definition while reusing timer and retirement ownership | Yes |
| Relabel the fixed step as T1 | Leaves ADR-0003's class table missing | No |
| Build a T2 pipeline now | Adds deferred hazards/stage timing and unbounded evidence scope | No |

**Auto-selection:** The two new boot profiles share one named T1 table and
approved compat-v2 CPU behavior. Check the primary MDU table before literal
weights; label issue-plus-stall conversion as model policy. Preserve Count
guest-time phase, retirement-only Random and the approved high-bit case. Do
not add a dynamic cache/bus timing project or a third timing-only profile.

## Full BASE inventory and qualification

| Option | Consequence | Selected |
|---|---|---|
| Phase-owned C join and immutable contexts | Preserves component contracts and checks every original required member | Yes, subject to lossless authentication |
| Separate direct phase-wide C member protocol | Use shared primitives if existing component records cannot be joined losslessly | Bounded fallback |
| Monolithic full image and run | Risks uncleared asset compilation and losing later member outcomes after one failure | No |

**Auto-selection:** Independently seal the actual complete default/BASE source
set, not the CPU subset. Use one new Tier B/T1 target for the required phase
partition; old warm outputs are historical. Inputs can be reused after closure
checks, results cannot be relabeled. Blocked assets/devices retain individual
source identities and reasons privately. C owns membership/aggregation;
Python owns preparation and original-record retention. Runtime fixes create
fresh immutable contexts instead of another hard-coded protocol version.

## Adversarial synthesis

| Counterexample | Required resolution |
|---|---|
| Source has a cold branch but the pinned artifact does not reach it | Bounded exact-binary startup first; only then consider another cleared fixture. |
| Strict Random or blanket absent-memory refusal is copied into compat-derived profiles | Carry the approved compat-v2 policies and exclusions explicitly; test old and new profiles separately. |
| A static cost is presented as hardware latency or double-counts a branch delay | Version the model, check raw primary facts, keep delay slot as its own instruction and defer T2. |
| A larger cost overflows a successor deadline after mutation | Preflight time/event arithmetic and failure effects before commit; assert complete future-state refusal. |
| The existing CPU denominator hides non-CPU BASE members | Derive full source selection and exact set; keep blocked members with reasons rather than filtering. |
| An aggregate mixes old warm-profile results into cold-profile evidence | Require the chosen target tuple for new results; preserve historical identity separately. |
| Mutable context fields authorize arbitrary builds or source | Independent source authority, actual tested artifact binding, closed schemas and full-entry negative controls. |
| Scheduled authored stress is mislabeled as an external run | Separate actual execution, local qualification, rights blockers and lane definitions; no inferred recurring service or larger window. |
| Different A/B profile digests are treated as a failed equality test | Compare authored output across A/B; check state determinism within each profile. |

No genuine new owner rights, authority, physical, paid or destructive decision
was needed to choose these defaults. Existing stops are preserved. The
discussion produced one canonical context; further work moves to research and
planning rather than another discussion pass.

## Delegated implementation detail

Exact profile identifiers, new state layout/version numbers, bounded helper
seams and CLI spelling remain with implementation research/planning. Their
choices cannot weaken source membership, old profiles, accepted compatibility,
rights or resource limits. The parent retains auto-advance intent in memory;
it does not rewrite the owner's dirty configuration for an ephemeral chain flag.

## Deferred

T2, dynamic cache/bus timing, 8 MiB expansion, RDP implementation, new physical
measurements, public external diagnostics and D6 release-channel decisions.

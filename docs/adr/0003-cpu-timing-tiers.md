---
status: accepted
date: 2026-11-09
decision-makers: szTheory
consulted: none
informed: none
---

# Define CPU timing as three tiers that are never named after test groups

**Status:** accepted

## Context and Problem Statement

Hardware test fixtures probe CPU timing at different depths: instruction ordering, per-instruction cost and
pipeline or hazard effects. A single "timing accurate" label would hide which depth a profile implements and
which fixture groups it can honestly be measured against. How should CPU timing be tiered so that a profile
states exactly what it models?

## Decision Drivers

- A support statement ties to an exact profile, build and evidence tier, and a fixture group name alone is
  not a tier (AGENTS.md).
- The first profile must be small enough to verify, and deeper accuracy must stay possible later.
- A cost model needs a version, because a change to the table changes results.
- Results from different fixture groups are different evidence and must not be merged.

## Considered Options

- Name tiers after the test groups that exercise them.
- Three numbered tiers, T0, T1 and T2, defined by what the core models.

## Decision Outcome

Chosen option: "Three numbered tiers defined by what the core models", because the tier then describes the
implementation and not the test suite.

| Tier | Models | Status |
|------|--------|--------|
| T0 | instruction ordering only; no per-instruction cost | defined |
| T1 | a per-instruction-class cost table with a named version | the first profile |
| T2 | pipeline and hazard accuracy | deferred (ACC-01) |

Rules:

- Tiers are never named after test groups. A group name may appear as evidence, never as a tier.
- `timing`, `cycle` and `cop0hazard` are separate evidence fields. A result for one never implies another.
- Profile identity includes the tier and the cost-table version. A new cost-table version is a new profile.
- Whether T1 can pass the `timing` group is an unknown. It is not claimed until a result exists.

### Consequences

- Good: a profile states its modeling depth, and a claim cannot borrow a deeper tier's evidence.
- Good: the cost table is versioned, so a regression is attributable to a table change.
- Bad: the first profile may fail fixture groups that need T2, and the project reports that as scope, not as a
  defect.

### Confirmation

The profile identity fields in `docs/spec/result-protocol.md` and the claims gate (plan 01-12) carry the tier
and cost-table version. Phase 3 records per-group results in the separate evidence fields.

## Reversal evidence

This decision is wrong if hardware-derived or primary-document evidence shows that a fixture set the project
claims to cover needs T2 behavior, so that the T1 table cannot be made to match it by adjusting costs. The
tier definitions would then change, and affected profiles would get a new identity. A convenience argument
for collapsing the tiers is not evidence.

## Pros and Cons of the Options

### Name tiers after test groups

- Good: familiar to readers of existing test suites.
- Bad: couples the tier definition to a third-party suite, and invites claims of the form "passes group X
  therefore models Y".

### Three numbered tiers defined by what the core models

- Good: the tier is a property of the implementation.
- Good: deferral of T2 is explicit.
- Bad: needs a mapping from tier to fixture groups, kept as evidence and not as a definition.

## More Information

- Related: ADR-0005 for outcome classification, and requirement ACC-01 for T2.
- Unknown: whether T1 passes the `timing` group. Phase 3 measures it.

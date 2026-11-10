---
status: accepted
date: 2026-11-09
decision-makers: szTheory
consulted: none
informed: none
---

# Record architecture decisions with MADR headings and a mandatory reversal section

**Status:** accepted

## Context and Problem Statement

The project makes decisions that are cheap to reverse early and expensive later: language and ABI, timing
tiers, outcome classification, the lint mechanism and the copy policy. Without a written record, a later
contributor cannot tell whether a decision was reasoned, who may change it, or what observation would
change it. How should decisions be recorded so that the record is checkable and reversible?

## Decision Drivers

- A decision must name the evidence that would reverse it, so that reversal is a measurement and not a
  preference contest (AGENTS.md: separate decisions, facts, inferences and unknowns).
- The record must be machine-checkable, so that a missing section fails the gate instead of relying on review.
- The format must be small enough to write for a one-person decision.
- Decision-makers are logins or roles, never emails, because the repository is public.

## Considered Options

- MADR 4.0.0 headings plus a mandatory Reversal evidence section, written fresh (headings only).
- A lighter four-part note (context, decision, status, consequences).
- No decision records; keep rationale in commit messages and planning files.

## Decision Outcome

Chosen option: "MADR 4.0.0 headings plus a mandatory Reversal evidence section", because it is a known,
documented layout and the one added section forces every decision to state what would overturn it.

Rules (enforced by `scripts/check-docs.sh`):

- Files are `docs/adr/NNNN-slug.md` with a four-digit number and a lowercase slug. Numbers are unique.
  `0000-template.md` is the template and is not a decision.
- The front matter carries `status`, `date`, `decision-makers`, `consulted` and `informed`. The status is one
  of `proposed`, `accepted`, `rejected`, `deprecated` or `superseded`.
- A body line `**Status:**` repeats the front-matter status exactly (first word compared).
- The headings appear in the order of the template: Context and Problem Statement, Decision Drivers,
  Considered Options, Decision Outcome with Consequences and Confirmation, Reversal evidence, Pros and Cons
  of the Options, More Information. Extra subheadings are allowed.
- The Reversal evidence section is not empty.
- The set of required decisions is a list at the top of the checker. A required decision that is absent, or
  present with a status other than `accepted`, fails the gate. ADR-0002 was exempt from the acceptance rule
  until plan 01-08 accepted it after its pre-registered spike, so no exemption remains.
- An owner-visible decision is flagged `[owner-visible]` in its text. The flag informs the owner; it does not
  block work.

### Consequences

- Good: every decision states how it can be wrong, and the gate refuses a record that does not.
- Good: the rule that no core code lands before ADR-0002 is accepted is checked by the same tool.
- Bad: small decisions cost a short document; the template keeps that cost low.

### Confirmation

`sh scripts/check-docs.sh` runs in every gate run as the CTest test `repo.docs`, and its `--selftest` runs as
`repo.docs-selftest`. The selftest builds a temporary tree for each rule and asserts the rule id and a
non-zero exit.

## Reversal evidence

This decision is wrong if records written in this format are routinely skipped or filled with placeholder
text that the checker cannot detect, shown by decisions found in commit messages with no record, or by
reversal sections that name no observation. It is also wrong if a required field turns out to block a real
decision, shown by a decision that could not be recorded without inventing content. Either outcome is a
reason to change the template and the checker together. Dislike of the format is not evidence.

## Pros and Cons of the Options

### MADR 4.0.0 headings plus a mandatory Reversal evidence section

- Good: a documented layout with a stable heading order the checker can verify.
- Good: the Reversal evidence section makes reversal criteria explicit.
- Bad: more headings than a four-part note.

### A lighter four-part note

- Good: shortest to write.
- Bad: no place for the reversal criteria, and no stable structure to check.

### No decision records

- Good: no upkeep.
- Bad: rationale scatters across history, and the ordering rules between decisions cannot be checked.

## More Information

- Template: `docs/adr/0000-template.md`. The template headings follow MADR 4.0.0 (MIT OR CC0-1.0); only
  headings are used and no template prose is copied. Plan 01-10 records the structural adoption in the
  provenance record.
- Required decisions in Phase 1: ADR-0002 language, build and ABI; ADR-0003 CPU timing tiers; ADR-0004
  emulator extension opcodes; ADR-0005 outcome and wall-clock classification; ADR-0006 determinism lint
  mechanism; ADR-0007 copy and reading policy.

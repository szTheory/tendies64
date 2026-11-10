---
status: accepted
date: 2026-11-09
decision-makers: szTheory
consulted: none
informed: none
---

# Copy only from four permissive licenses, and ban reading copyleft emulator source while implementing

**Status:** accepted

## Context and Problem Statement

The project is public and licensed under Apache-2.0. It may benefit from small helpers and from existing
emulator knowledge, but copied code carries license obligations, and exposure to copyleft or unlicensed
emulator source can taint an independent implementation. Which licenses may code be copied from, and what may
a contributor read while implementing hardware behavior? This decision is [owner-visible]: it is surfaced to
the owner and does not block work.

## Decision Drivers

- The project's own license must stay clean (Apache-2.0), and every copied file must be traceable.
- A small, understandable copied helper can beat a dependency when its provenance and owner are clear
  (AGENTS.md).
- The reading rule must be simple enough to follow and to audit, and strict enough that a later challenge to
  independent derivation can be answered.
- The owner can relax a policy later with counsel advice, so the stricter option is the reversible default.

## Considered Options

- Allow permissive licenses broadly and allow clean-room reading of any source.
- Allow exactly four licenses, ban reading copyleft, MAME-licensed or unlicensed emulator source, and allow
  reading permissive code.
- Copy nothing and read nothing.

## Decision Outcome

Chosen option: "Allow exactly four licenses, ban reading copyleft, MAME-licensed or unlicensed emulator
source, and allow reading permissive code".

Copy allowlist (D4), exactly: ISC, MIT, BSD-3-Clause and Unlicense.

- [owner-visible] BSD-2-Clause, MIT-0, 0BSD, Zlib and Apache-2.0 are pre-vetted but not yet allowed, pending
  an owner decision. MPL-2.0 stays out.
- GPL-licensed code and Angrylion-Plus code are never copied.
- Reading rule [owner-visible]: a flat ban on reading the source of GPL, MAME-licensed or unlicensed emulator
  projects while implementing. Black-box runs, documentation, issues, wikis and permissive source (the ares
  emulator under ISC and the cen64 emulator under BSD-3-Clause) may be read. Accidental exposure is logged in
  the exposure log, and a reviewer may require re-derivation from tests.
- Per-copy record, the copied-looking definition and the enforcement tool are specified in
  `docs/policy/copy-provenance.md`.

### Consequences

- Good: copied code is limited to four licenses that need only notice preservation, and every copy is
  recorded.
- Good: the reading rule is a bright line that a reviewer can check against the exposure log.
- Bad: the reading rule is stricter than a clean-room allowance, so some reference behavior must be
  re-derived from tests and documents.

### Confirmation

`docs/policy/copy-provenance.md` states the rule, and `t64-provenance-check` (plan 01-10) enforces the
per-copy record, the allowlist and the copied-looking definition in every gate run. `scripts/check-docs.sh`
checks that this record and the policy headings exist.

## Reversal evidence

This decision changes if the owner makes a different D4 decision, for example by allowing another pre-vetted
license, or if counsel advises that the reading ban can be relaxed to a documented clean-room process. A
contributor's preference for faster implementation is not evidence.

## Pros and Cons of the Options

### Allow permissive licenses broadly and allow clean-room reading of any source

- Good: fastest access to reference behavior.
- Bad: harder to audit, and a clean-room claim is easier to challenge.

### Allow exactly four licenses and ban reading copyleft source

- Good: simple, auditable and reversible.
- Bad: slower in places, and stricter than the research's clean-room allowance.

### Copy nothing and read nothing

- Good: nothing to record.
- Bad: gives up small, well-understood permissive helpers and available permissive references.

## More Information

- Related: `docs/policy/copy-provenance.md`, `docs/policy/rights-and-fixtures.md`, requirement GOV-02.
- Pre-vetted licenses are documented so the owner can accept them quickly; nothing in the gate allows them.

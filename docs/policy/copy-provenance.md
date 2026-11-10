# Copy and provenance policy

Status: normative. Decision record: `docs/adr/0007-copy-and-reading-policy.md`. Requirement: GOV-02.

This policy decides what code may be copied into the repository, what a contributor may read while
implementing hardware behavior, and what record every copy carries. The repository is public and its own
license is Apache-2.0 (see `LICENSE` and `NOTICE`).

A small, understandable copied helper can be better than a dependency when its provenance and maintenance
owner are clear. Every copy is recorded; an unrecorded copy fails the gate.

## Allowed licenses

Code may be copied only from a project licensed under exactly one of these:

- ISC
- MIT
- BSD-3-Clause
- Unlicense

Copying keeps the upstream copyright and license notice verbatim.

## Never copied

- GPL-licensed code of any version, and anything derived from it.
- Angrylion-Plus code.
- MAME-licensed code.
- Code with no license, and code whose license cannot be established from the upstream repository.
- Code from a source that restricts redistribution or use.

## Reading rule

[owner-visible] While implementing a hardware behavior, do not read the source of GPL, MAME-licensed or
unlicensed emulator projects. This is a flat ban, stricter than a clean-room allowance.

Allowed:

- Black-box runs of other emulators and test ROMs.
- Documentation, issues, wikis and papers.
- Reading permissive source: the ares emulator (ISC) and the cen64 emulator (BSD-3-Clause).

If a contributor is exposed by accident, they record it in the exposure log and tell a reviewer. A reviewer
may require the affected behavior to be re-derived from tests and documents by someone not exposed. The rule
can be relaxed later on counsel advice, as a new decision record.

## Pre-vetted, not yet allowed

[owner-visible] These licenses were reviewed and look compatible with the project's license, but they are
not on the allowlist until the owner decides:

- BSD-2-Clause
- MIT-0
- 0BSD
- Zlib
- Apache-2.0

MPL-2.0 stays out: its file-level copyleft is not wanted. The gate rejects any copy under a license that is
not on the allowlist, including the pre-vetted ones.

## Per-copy record

Every copied file carries all three of these:

1. A license header in the copied file: an SPDX license identifier line and an SPDX copyright text line that
   names the upstream holder (REUSE 3.3 minimum). Only copied files carry these headers.
2. The license text at `LICENSES/<SPDX-id>.txt`. That directory holds only licenses in use.
3. An entry in `provenance/copies.json` with these fields: the paths in this repository, upstream repository
   and path, the 40-hex upstream commit SHA, the retrieval date, the SPDX id, the license text path, the
   verbatim upstream notice, `verbatim` or `adapted`, the modifications, the id of the independent
   verification test, `maintenance_owner`, `upstream_status` and the exposure log.

`maintenance_owner` is a GitHub login or a role, never an email address. When an upstream project is
archived, the owner is the project itself, written `Tendies64`.

A file is copied-looking when any of these holds:

- it is under `third_party/`;
- its first 20 lines carry an SPDX license identifier tag with a value other than `Apache-2.0`;
- its first 20 lines name an SPDX copyright holder other than `Tendies64 contributors`.

A copied-looking file with no entry in `provenance/copies.json` fails the gate. A missing SHA, a missing owner
or a license that is not allowlisted also fails.

No literal SPDX tag appears in this document, so that this file is not itself copied-looking.

## Exposure log

Each entry in `provenance/copies.json` has an `exposure` list. Each item names the person (a GitHub login or
role), the date, what was read, and what was done about it. An empty list means nobody recorded exposure. A
separate exposure for a behavior that has no copied file is recorded in the pull request that implements the
behavior and linked from the review.

## REUSE scope

This is not full REUSE compliance in v1. Only copied files carry SPDX headers; project-authored files carry
none, and the root `LICENSE` stays the Apache-2.0 notice. `LICENSES/Apache-2.0.txt` is required only if the
project later opts into full REUSE compliance. That would be a new decision.

## Automated enforcement

`t64-provenance-check` (plan 01-10) runs in every gate run. It enforces the allowlist, the copied-looking
definition above, the 40-hex SHA, the owner format and the license text path, and it has seeded negative
cases. `scripts/check-docs.sh` checks that this policy and its headings exist.

# Local whole-BASE diagnostic protocol

Status: implementation contract for the private, local-only Phase 06 BASE
diagnostic. Authored controls contain synthetic members only. External source
identities, assets, tool receipts, inventories, and results remain in ignored
owner-only build storage.

## One phase-wide source authority

`runner/t64_base_diag_protocol.c` owns the complete phase-wide member
denominator and outcome completeness. CPU and RSP protocols keep their existing
schemas and component contracts; neither is used to infer the whole-BASE
member set. A member is identified by its source registration index and value
index. Display names are descriptive and may repeat.

The private source derivation uses the accepted CPU source projection. Required
default CPU members preserve their source value order. Non-CPU members remain
in the denominator as required blocked entries until their source and asset
closure is authorized for execution. A zero-value registration has one
`once` member with an empty value identifier. Optional CPU lanes outside the
default BASE dispatch are not promoted into the required denominator.

The canonical `t64.base-source-authority` v1 document contains the complete
ordered `members` array and `source_revision`. The canonical
`t64.base-inventory` v1 document repeats the exact member array, source
revision, and `source_authority_sha256`. The parser accepts an independently
checked expected authority digest from the immutable context. It refuses
reduced source and inventory pairs even when both are resealed. Exact private
counts and member identities are not published.

Each member has exactly `execution`, `group`, `index`, `lane`, `level`, `name`,
`reason`, `required`, `value_id`, and `value_index`. `execution` is one of
`values`, `once`, or `blocked`. Arrays preserve source order; canonical JSON
uses closed keys and a trailing newline. Inventory and authority inputs are
bounded to 4 MiB, 131,072 members, and 512 bytes per text field.

## Original member records

The legacy canonical `t64.base-member-record` v1 document binds the inventory digest,
source registration/value indices, one outcome (`pass`, `fail`, `incomplete`,
`not-run`, `guest-stop`, or `harness-error`), and the SHA-256 of the original
member bytes. Records are bounded to 8 KiB. The C aggregate verifies the exact
original bytes before accepting a result, rejects unknown or duplicate
members, and poisons itself after malformed or mismatched input. A claimed pass
or fail additionally requires the canonical shared guest transcript, matching
source/value ordinals and verdict. Legacy records cannot establish a bound
whole-suite pass.

Version 2 binds the immutable context SHA-256, actual core build identity,
profile ID and profile SHA-256, selected ROM SHA-256, original capture SHA-256,
and execution observations. The expected selector image digest comes from the
independently qualified image closure, separately from the record. Capture is
bounded to 4,096 bytes. Tick and attempted capture total are canonical unsigned
decimal strings; tick cannot exceed the existing ten-million-tick case limit.
A pass/fail requires exact, lossless capture, observed guest entry, and at least
two consecutive observed terminal jump/NOP pairs at the qualified park address.
Changed delay-slot instructions and incomplete sequences are refused.

The C aggregate exposes three independent predicates:

- `source_complete`: every source row, including informational and blocked
  members, has one valid identity-bound disposition.
- `authenticated_execution_complete`: source-complete, with every required
  member producing a genuine authenticated pass or fail.
- `all_pass`: authenticated execution is complete and every required member passes.

The closed version-2 outcomes add `timeout`, `blocked`, `unexecuted` and
`refused`; legacy `not-run` is refused in version 2. Blocked receipts require
the exact source-owned reason and no guest progress or selected image.
Unexecuted receipts require a bounded reason (`not-selected`, `interrupted` or
`window-exhausted`) and no guest progress. Attempted refusals, incomplete
results, timeouts and harness errors retain their separate reasons and expected
selected-image identity. Missing records remain missing; they are never silently
converted into unexecuted results. Invalid, duplicate or foreign additions poison
completion.

`t64.base-classification` v1 is a C-produced canonical report with source-order
member rows, group/lane counts, target identities and the three predicates.
Arrival order cannot change its bytes. `reached` counts attempted records,
excluding blocked/unexecuted receipts; it does not assert guest-entry execution.
Metadata reports have a separate 64 MiB bound, without increasing guest capture.
Python derives source membership and prepares private files; C alone calculates
counts and completeness.

`--classify-manifest SOURCE SHA INVENTORY CONTEXT_SHA INPUT REPORT` consumes
canonical `t64.base-classification-input` v1. Each `members` row has exactly
`expected_rom_sha256`, `original`, `outcome`, `reason`, `record`,
`registration_index` and `value_index`. `record` authenticates existing v2 bytes;
`blocked`, `unexecuted`, `timeout` and `harness-error` produce and retain C-encoded
receipts plus empty original captures. Output paths must be fresh private files.
Exit zero means source-complete classification, including honest blockers; the
private report states execution completeness and passing separately. Exit three
retains a source-incomplete report; malformed inputs exit four.

## Immutable context and pre-use ordering

The private context is fixed under `build/private-base-diagnostic` with
owner-only directory and file permissions. A separate binding records the
canonical context, inventory, source authority, and accepted source projection
digests. The verifier independently checks the source projection seal and, in
the production repository, revalidates it through the existing CPU
qualification authority. It then derives the expected BASE authority and
checks the exact source, tool, lock, runtime, boot, image, composition, profile,
build, compiler, asset, executable, and verifier file bindings. Missing files,
symlinks, unsafe write permissions, changed bytes, an unapproved profile, or an
already-created run directory stop dispatch.

Before activating a context, the builder retains an immutable owner-only pair
under `build/private-base-diagnostic/execution-snapshots/<context-sha256>/`:
the exact executable bytes and
the exact context and binding bytes. The verifier derives that location from
the active context bytes, checks the pair against the active binding, and
requires the retained executable to match the active executable hash before
dispatch. Retention preserves build evidence; it does not authorize an older
context or executable after the live binding changes. Existing snapshots are
never repaired or overwritten.

`verify_and_dispatch` completes every check before invoking normal dispatch.
The callback can receive the already verified context and exact input bytes,
so dispatch need not reopen a pathname after verification. The caller supplies
a fixed in-process dispatch path; context data cannot name
an arbitrary command. Executable bytes are hashed before use. Refusal and
setup failure occur before a guest run directory exists.

## Scope and evidence

Phase 6 authorizes the permitted required partition on the selected Tier B/T1
target and separate informational/stress selections, under the existing CPU/RSP
bounds and local rights controls. Original output, captures, source/tool bindings,
member identities/counts and exact consumer artifacts remain private. An
unsupported target or blocked member is an honest retained observation. A lane
definition or authored harness control does not establish external execution.
This protocol does not authorize another tier, physical hardware, uncleared
graphics assets or Phase 7 device implementation, and establishes no hardware
accuracy claim.

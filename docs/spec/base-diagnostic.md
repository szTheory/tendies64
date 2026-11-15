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

The canonical `t64.base-member-record` v1 document binds the inventory digest,
source registration/value indices, one outcome (`pass`, `fail`, `incomplete`,
`not-run`, `guest-stop`, or `harness-error`), and the SHA-256 of the original
member bytes. Records are bounded to 8 KiB. The C aggregate verifies the exact
original bytes before accepting a result, rejects unknown or duplicate
members, and poisons itself after malformed or mismatched input.

The aggregate reports completeness only when each required source member has
one authenticated result. `all_pass` is a separate predicate: a complete
denominator can contain blocked, failed, incomplete, or refused members and
must not be described as passing. Python derives the source set and prepares
private files; it does not calculate aggregate completeness.

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
The caller supplies a fixed in-process dispatch path; context data cannot name
an arbitrary command. Executable bytes are hashed before use. Refusal and
setup failure occur before a guest run directory exists.

## Scope and evidence

The only external attempt authorized by this phase is one qualified member on
the selected Tier B/T1 target, under the existing CPU/RSP bounds and local
rights controls. Its original output, capture, source/tool bindings, and exact
consumer artifact remain private. An unsupported target or blocked member is
an honest retained observation, not a pass. This protocol does not expand
execution to another tier, physical hardware, RDRAM, or any uncleared graphics
asset, and it does not claim whole-suite completion or hardware accuracy.

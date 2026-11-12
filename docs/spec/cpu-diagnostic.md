# Local CPU diagnostic protocol

Status: implementation contract for the local-only Phase 4 CPU diagnostic.
This document describes authored protocol behavior; it does not contain an
upstream source inventory, ROM identity, tool receipt, or diagnostic result.

## Ownership and evidence boundary

`runner/t64_cpu_diag_protocol.c` is the single implementation of the
`t64.cpu-inventory` v2 and `t64.cpu-batch` v2 schemas. The same CMake target
serves authored controls and the later production adapter. The schemas remain
separate from public `t64.result` and the public hello-fixture collector.

The protocol validates a supplied inventory and every batch against that
inventory. Its declared counts and SHA-256 bind the bytes together; they do not
prove that the inventory covers the pinned diagnostic source. Plan 16 must
derive and seal the exact registration and value denominator from the pinned
source before this aggregate can support SC4. That pre-build source-denominator
seal records which values are required; it does not invent output hashes that
do not exist yet. After each selector-patched ROM has been built and hashed,
Plan 16 binds that value's selector and selected-ROM SHA-256 into the final
canonical inventory before any run. The final inventory retains the pre-build
denominator and global source/build/profile identities while recording each
member's actual built-ROM identity. No caller-selected reduced
allowlist, README example count, constructor count, synthetic summary, or
native-only result can serve as that source seal. If the complete source set
exceeds a format limit, qualification stops instead of truncating it.

Plan 16 also owns source, tool, asset, and rights qualification. This public
protocol and its controls use authored-only data. External diagnostic inputs,
ROMs, captures, receipts, and derived results remain local and private.

## `t64.cpu-inventory` v2

The root has exactly `declared_counts`, `identity`, `registrations`, `schema`,
and `schema_version`. The schema string is `t64.cpu-inventory`; the version is
integer `2`. JSON is canonical: object keys use the shared writer's bytewise
order, arrays retain source order, and one newline terminates the document.
Unknown keys, duplicate keys, reordered or gapped indices, and noncanonical
bytes are refused.

`identity` has exactly these fields:

- `source_revision`: 40 lowercase hexadecimal characters.
- `overlay_sha256`, `tool_receipt_sha256`, `core_build_sha256`,
  `profile_sha256`, and `base_rom_sha256`: 64 lowercase hexadecimal
  characters each. These values describe the shared source/build/profile and
  base-ROM inputs for the complete inventory.

`declared_counts` has exactly `registrations`, `required_values`, and `values`.
Each is a canonical unsigned decimal string. The parser compares these values
with the parsed rows. `values` counts every listed CPU value across lanes;
`required_values` counts only `required-base` values. These checks catch count
drift inside the file. Plan 16 must separately establish that the sealed rows
match the pinned source.

Each registration has exactly `execution`, `group`, `index`, `lane`, `level`,
`name`, `reason`, and `values`. Registration indices start at zero and follow
source order. Names are unique, printable UTF-8 strings of at most 256 bytes.
Each value has exactly `id`, `index`, `selected_rom_sha256`, and `selector`.
Value indices start at zero within their registration; value identifiers are
unique there, printable UTF-8 and at most 128 bytes. `selected_rom_sha256` is
64 lowercase hexadecimal characters. `selector` is 1–128 ASCII letters,
digits, `.`, `_`, `+`, or `-`. This binding is per value, so selector-patched
images in one diagnostic inventory retain distinct hashes without weakening
the global source identity.

The required group map and order are fixed:

1. `address_error_exception`
2. `arithmetic`
3. `cache`
4. `cop0`
5. `cop1`
6. `cop_unusable`
7. `endian_re`
8. `exception_instructions`
9. `jumps`
10. `overflow_exception`
11. `privilege`
12. `tlb`
13. `tlb64`
14. `traps`

`BasicFunctionality` and `RarelyUsed` CPU registrations use lane
`required-base`, group from that map, reason `required`, and one or more exact
values. Other CPU levels stay explicit and out of the required denominator:
`Weird`/`weird`, `Timing`/`timing`, `Cycle`/`cycle`, `COP0Hazard`/`cop0-hazard`,
`PoorlyUnderstoodQuirk`/`quirk`, and `StressTest`/`stress`. Each such row needs
a nonempty reason. Non-CPU registrations use group `non-cpu`, lane
`skipped-non-cpu`, execution `omitted`, an empty value list, and a reason.

Execution is `values` for an explicit per-value list. `once` has exactly one
value with identifier `once`; Plan 16 may use it only after confirming from the
pinned executor that a registration with no `values()` entries runs once.
`omitted` is reserved for `skipped-non-cpu`. The protocol does not infer the
upstream zero-value convention.

## `t64.cpu-batch` v2

The root has exactly `identity`, `records`, `schema`, and `schema_version`.
The schema string is `t64.cpu-batch`; the version is integer `2`. Batch
identity repeats the shared source/build/profile/base identity, adds
`inventory_sha256` (the SHA-256 of the complete canonical inventory bytes),
and carries the exact `selector` and `selected_rom_sha256` registered for the
start record's member. The aggregator checks both global identities and the
member-specific built-ROM binding before it records an outcome.

One value is reported per fresh canonical-machine run. The three records must
appear in this order:

1. `start` with the bounded `registration_index` and `value_index` from the
   inventory.
2. `outcome` with one of `pass`, `fail`, `incomplete`, `not-run`, `guest-stop`,
   or `harness-error`.
3. `done` with `complete: true` and decimal-string `record_count: "3"`.

The adapter owns fresh-machine construction and the actual guest run. A batch
is only one transcript for one inventory member; it does not itself assert
whole-suite completion. Missing or truncated terminators are incomplete.
Identity drift, unknown members, malformed records, and duplicate members
invalidate aggregation.

## Complete aggregation and bounds

The shared aggregator stores outcomes by pinned registration/value index, so
arrival order cannot change output order. Its member iterator returns required
values in source registration order and then value order. Every one of the 14
groups must have at least one required value, and each required value must be
reached exactly once. A group is `incomplete` when any value is absent or did
not reach pass/fail; it is `fail` when all expected members were reached and
one or more failed; it is `pass` only when all expected members were reached
and none failed. Whole-inventory completeness is distinct from a passing
result: a complete diagnostic may contain failed groups.

Limits are fixed: inventory input at most 1 MiB; at most 65,536 total
registration and value rows; at most 256 bytes per registration name; at most
128 bytes per value identifier and selector; and at most 4,096 bytes per batch
transcript. Batch registration/value indices are bounded to 16 bits. Input
over the limits is refused, never truncated. Authored controls cover maximum
legal field lengths and one byte over the transcript and metadata limits.

Both schemas use closed keys and canonical JSON. All protocol-relevant counts
that may require unsigned 64-bit range are decimal strings; bounded array
indices use integers. Batch identity and the exact inventory digest are
validated before a member enters the aggregate. Invalid or duplicate batches
poison that aggregate, so ignoring an error cannot leave it complete.

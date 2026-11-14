# Private RSP diagnostic qualification

This document describes the local qualification boundary for the source-derived
RSP diagnostic guest. It does not claim that the emulator passes the guest's
tests or that the profile represents broad cartridge compatibility.

## Component identity

The private qualification receipt uses component `rsp` and binds:

- the pinned source revision, audited source projection, exact registration and
  value partition, selected source modules, and generated selector overlay;
- the default Cargo feature closure and the per-test level lane mapping;
- the reused, previously accepted toolchain and lock-partition receipts, with
  their current byte digests checked before use;
- the RSP profile identifier, native core build identity, memory layout, and
  the exact boot asset used to compose the guest ROM;
- a separate asset disposition for inputs not included by the selected source
  closure.

The inventory records source registration indices and value indices. Tests with
no source values use a single `once` selector. Repeated display names retain
their source identities and are disambiguated by registration index. The exact
source rows, names, counts, hashes, tool paths, and build results stay in
owner-only ignored build evidence.

## Asset and rights boundary

The default selected RSP modules are checked independently from the CPU
overlay. The selected closure contains no font, framebuffer, graphics data, or
new boot bytes. Explicit nondefault stress modules that reference an
unqualified bitmap are excluded from this profile and remain unavailable for
execution until their asset rights are established. Exclusion is recorded in
the private receipt and is not represented as a passing test.

The existing audited open IPL3 fixture may be reused only when its exact bytes
still match the accepted private receipt. A source or file hash establishes
identity; it does not grant rights.

## Lifecycle and limits

1. Prepare source/value membership from the pinned executor, features, level
   semantics, and source audit.
2. Apply a headless RSP selector overlay, then seal the RSP-specific source,
   asset, tool, lock, and build-identity closure before invoking Cargo.
3. Run the local guest build offline, locked, and frozen with the reviewed
   resource limits. Save each attempted command and its bounded logs in a new
   owner-only attempt directory.
4. Inspect the actual ELF, flattened payload, selector, boot bytes, and ROM
   composition against the sealed receipt.

The build and composition check do not execute the guest ROM. A successful
composition record is not a startup result, an opcode score, or an accuracy
claim. Private source-derived evidence is not uploaded by tests or CI.

## Run and result contract

The RSP adapter uses its own closed `t64.rsp.inventory` schema version 1 and
`t64.rsp.batch` schema version 1. Each inventory and member result is bound to
the RSP component, source projection, profile descriptor, core build, ROM,
boot asset, registration, and exact selector. The RSP schemas do not change
the frozen CPU inventory or result formats.

Member outcomes are `pass`, `guest-fail`, `refused`, `incomplete`, `timeout`,
and `harness-error`. A refusal preserves the operation and reason; it is not a
passing result. A pass requires the C adapter's authenticated terminal
retirement evidence, including a retired delay slot. The C adapter owns exact
membership aggregation and rejects missing, duplicate, extra, cross-component,
or identity-mismatched member records. Python orchestrates private local runs
and delegates result completeness to that C adapter.

`--run-local --local-only` runs one selector in a fresh headless machine. It
uses a 10,000,000 guest-tick bound, 16-tick stepping, a 10-second child wall
limit, a 12-second outer deadline, and a 4,096-byte output capture limit. If
the outer deadline expires, the attempt remains a timeout even if process
cleanup observes a zero exit. The private runner records actual reached stage
and outcome; building or composing an image alone never counts as guest use.

The local wrapper supports `--run-one`, `--run-all`, and
`--verify-complete`, with explicit adapter and private run-directory paths.
Guest execution is refused in CI or required-fixture mode. Qualified ROMs,
selectors, identities, and per-member outcomes remain in owner-only ignored
evidence.

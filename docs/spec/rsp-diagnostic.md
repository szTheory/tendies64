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

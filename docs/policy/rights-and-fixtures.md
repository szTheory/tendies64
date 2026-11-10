# Rights and fixtures policy

Status: normative. Requirements: GOV-02 (policy half), GOV-03 and GOV-07.

This policy decides which material may enter the repository, a build artifact or public CI, and how a test
fixture records its rights. A hash proves identity, not rights.

## Protected material

These classes are rights-bearing or untrusted. They are never committed, never uploaded from CI and never
redistributed without documented permission:

- ROMs and ROM images, in any extension or encoding.
- IPL, PIF and CIC boot data.
- 64DD media and disk images.
- Save data of any kind.
- Firmware, manuals and diagnostic assets.
- Generated ROMs that have not been reviewed for rights.

A hash, size or name of protected material identifies it; it does not make it permitted.

## Rights statuses

Each fixture records `source_rights` and `binary_rights` separately. Each is one of:

- `authored-here`: written in this repository by its authors.
- `cleared`: permission is documented in `rights_evidence`.
- `pending`: not yet decided. This is the default.
- `forbidden`: must not be used, with a recorded reason.

## Distribution channels

A fixture's `distribution` is a list drawn from `commit`, `artifact`, `build-only` and `local-only`. Bytes may
exist in the tree or in an artifact only when `binary_rights` is `authored-here` or `cleared` and the channel
is listed. `build-only` means the bytes are produced during a build and not stored. `local-only` means the
bytes stay on a maintainer's machine and are never committed or uploaded.

## Fixture manifest

`fixtures/manifest.json` lists fixtures, validated against `fixtures/manifest.schema.json`. An entry records
an id, source repository and 40-hex commit SHA, the build recipe and toolchain pins, `sha256`, size, profile,
the two rights statuses, `distribution`, `rights_evidence`, notices, and `expected` with a kind and source
and revision. The kinds are `self-checking`, `spec-derived`, `hardware-derived`, `differential` and
`regression-pin`.

`forbidden` entries record a reason and carry no digest, so the manifest never becomes a list of
protected-file hashes. Phase 1 entries are `pending` or `forbidden` only. No protected-file hash is
committed anywhere in the repository.

## Automated enforcement

The GOV-07 classes (ROM magic at offset 0, NUL or invalid UTF-8 blobs not listed in the manifest, denied
extensions, opaque containers, LFS pointers) are checked over the working tree, the history in a pull
request range and a staging directory by `scripts/rights-scan.sh` (plan 01-06). The manifest checker
(plan 01-09) validates the schema and the cross-field rules.

Phase 1 CI uploads nothing, so no CI step scans a staging directory yet. The staging mode is proven by the
seeded `staging-rom-magic` and `staging-nul` cases in `gov.rights.selftest`, which run in every gate run.
Scanning the staging directory becomes a CI step when a job first uploads. The artifact leg of ROADMAP
success criterion 4 is covered by these seeded staging cases in every gate run, and the phase summary
repeats that statement.

Decision D2: the nemu64-test suite runs local-only. Its ROMs are not committed and not used in public CI.

## Local-only material

A maintainer may keep an optional denylist of protected-file hashes outside the repository. It is never
committed, never uploaded and never referenced by a committed path.

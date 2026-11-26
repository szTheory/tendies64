# ABI freeze record

Status: bound: ABI 1.0 (package 1.0.0), final bind by plan 12-14 of Phase 12
(Minimal Host, Playable Homebrew and Final Bind). Plan 11-23 of Phase 11
(Installed Consumer and Freeze Candidate) bound this record as the freeze
candidate ABI 0.1 (package 0.1.0); the final bind confirmed that candidate with
no amendment and keeps the record at its candidate path.

Evidence tier: unit-tested, authored scope; a software-interface claim only. This
record announces nothing and makes no distribution, compatibility or accuracy
statement about any profile. `1.0.0` is the package's ABI number, not a release
version (section "Owner-review item (D6)" below). A later change to anything
bound here follows the rule after the final bind: a recorded amendment with its
cost, never a silent edit.

## What is bound

The bound ABI is the installed Development component of the `tendies64` package:
`include/tendies64/t64_machine.h`, `lib/libt64_core.a` and the CMake package
files (`docs/spec/api.md` section 2.3). Four goldens in this directory bind it.
`repo.abi-freeze-record` recomputes each one from the installed tree (the moved
prefix of `install.setup`), compares the result with the golden file, compares
each golden's own SHA-256 with the machine-read block below, and fails on any
difference.

| Element | Golden | Recomputed from the installed tree | Also held on every gate run by |
|---------|--------|------------------------------------|-------------------------------|
| the installed headers | `docs/abi/header.sha256` | SHA-256 of every file below `include/` after its line endings are normalised to LF, one line per file, sorted by path | `repo.abi-header`, `install.abi-header` |
| the public symbols (35 functions) | `docs/abi/public-symbols.txt` | the defined global names of the installed archive that do not start with `t64i_` | `install.abi-symbols`, `repo.abi-prefix` |
| the struct layouts (14 structs, 165 members, 190 constants) | `docs/abi/layout.c` | the pins regenerated from the installed header by a probe program | `repo.abi-layout`, `install.abi-layout` |
| the status classes (39 statuses, 8 stop reasons) | `docs/abi/status-classes.txt` | every `T64_STATUS_*` and `T64_STOP_REASON_*` of the installed header, value and class | `repo.abi-status-classes` |

The block also binds the ABI version (1.0, from `T64_ABI_VERSION_MAJOR` and
`T64_ABI_VERSION_MINOR` of the installed header), the package version (1.0.0, from
the installed package version file), the number of amendment rows below, the
minor at the bind (`bound_minor`) and the number of rows made before the bind
(`candidate_amendments`), the counts of public functions, statuses, stop reasons,
structs, members and constants, and the padding holes of the layout.
`repo.abi-freeze-selftest` shows the check failing on each kind of
incompatible change: a removed or reordered struct member, a renumbered,
unclassified or reclassified status, a removed or added public function, an
edited header line, a second installed header, an edited golden, an edited count
or hash of this record, and an ABI minor raised with no amendment row. A CRLF
copy of the header passes, because the hash is taken over the LF form.

## What is not bound

- `tendies64/t64_facade.h` (the synthetic-guest runner header; it is neither
  installed nor part of this record), the runner, the tools and the test
  executables.
- State blobs and container formats: a state serialization version may change
  with a build (`docs/spec/api.md` section 10.4).
- The internal `t64i_` names of the archive: they are held only as a prefix rule
  (`repo.abi-prefix`, `install.abi-symbols`), never by name.
- The behavior of any profile (digests, video and audio output): the machine
  tests and the digest sweep hold it, not this record.
- Any layout on a target whose pointers are narrower than 64 bits (below).
- `project(VERSION)`, the build identity and any release versioning (section
  "Owner-review item (D6)" below).

## Evidence for the bind

The bind is the commit `03ccb01c28179522d357d866c1e21aebfade7b0a` ("final ABI
bind at 1.0", parent `3cfcbec94d83f8e32e76e8bf76789fe7f59eafa3`), one coupled
commit of ten files made by plan 12-14. Its staged patch (SHA-256
`70ef069078c3decc7f7b55beb79fa1cfba6c4da20af4f9d34c0f3f100aa4eed2`) was proven in a
scratch clone of the parent before the commit, and the commit's own binary diff
hashes to the same value. What the evidence shows, in order:

- Clone gate: `sh scripts/gate.sh` in the scratch clone with the patch applied
  passed, 351 of 351 CTest names with none excluded and no limit widened (the
  names include the installed-consumer suite, the `install.*` and `api04.*`
  names and `repo.abi-freeze-record` against the clone's own installed tree).
- Behavior: the digest sweep against the Wave 0 base of Phase 12 printed 1599
  shared lines and 0 differences, and the no-HLE check printed the same output as
  before the patch, so the bind moved no behavior and no sealed digest.
- Installed consumer against the bound tree: after the commit the shared tree
  re-ran the `install.*`, `installed.*`, `api04.*`, `repo.abi*` and
  `unit.core.api.*` names (69 of 69). The two version controls request 1.1 and
  1.0.5 and each fail with the CMake version headline for their own request, and
  `doctored-header-abi` still answers `ABI_MISMATCH` (status 34) with a NULL
  machine.
- The title and the host, through the public API only: the TITLE wave (plans
  12-02 to 12-05) runs the authored title ROM through the runner
  (`runner.title-two-run`) and through the installed consumer
  (`installed.consumer.title`, `installed.consumer.title-persist`,
  `installed.consumer.title-refused`), and the HOST wave (plans 12-07 to 12-10)
  drives it through the host pump (`host.pump.*`, including
  `host.pump.title-two-speeds`) and the local SDL lane. The two hosted runs of
  those waves, 45887012740 (TITLE, pushed head `884153a`, gate-linux and
  gate-macos green, 336 of 336 names) and 45904452436 (HOST, pushed head
  `de21fcf`, both green, 351 of 351 names), ran against the candidate header 0.1;
  the host work found no defect of a bound postcondition.
- Amendment slot: the ledger of host-found API defects holds 15 rows, 13 accepted
  and 2 documentation rows (C-1 and H-03, both text of `docs/spec/api.md`, which
  is not a golden) and none with the disposition "amend", so plan 12-11 did not
  trigger and `candidate_amendments` is 0.

The installed header of the bind differs from the candidate's in the two ABI
constants and in settled comment text; the header hash moved by design, and the
freeze check holds the counts of the candidate (35 functions, 39 statuses, 8 stop
reasons, 14 structs, 165 members, 190 constants, 8 padding holes). Text-only commits
after the bind (the relabeled production comments and specification lines of plan
12-15) change the build identity string, because the production sources are build
inputs, and move no bound golden. The hosted pair for the bound head comes with
the next push. The evidence stays a software-interface claim; it says nothing about
a console or another emulator.

## Layout targets and padding holes (review item S-05)

The pins of `docs/abi/layout.c` hold for targets with 64-bit pointers: macOS arm64
and Linux x86_64, the two hosted gate operating systems, both of which compile
the same pins against the installed header (`install.abi-layout`). On any other
data model the file compiles and checks nothing, and this record binds no
layout there; in particular a target whose `uint64_t` has 4-byte alignment inside
a struct (i386 System V) would place members at other offsets.

Eight implicit 4-byte alignment holes exist on the bound targets. The block lists
each as `padding_hole: <struct> <member the hole follows> <byte offset> <bytes>`;
`repo.abi-freeze-record` measures them from the installed header and fails when
one appears, moves or disappears. They are fixed by the platform ABI, and copy-out
builds each record in a zeroed local, so no stack byte leaks through a hole. The
final bind kept them as they are (section "Accepted limits at the bind" below).

## The rule after the final bind

The bind is one-way: ABI 1.0 is the contract that every later check compares
against. From the bind on, a bound element changes only by an additive change in
the sense of `docs/spec/api.md` section 13 (trailing struct fields guarded by
`struct_size`, new functions, statuses and stop reasons appended after the last
value, additive profiles, devices and error subcodes); adding a profile, a device
or an error subcode is never an ABI break (G1). Each additive change needs

1. the evidence or reason for the change (a failing test, a review finding or a
   host finding),
2. a re-run of the installed-consumer suite against the changed installed tree,
3. a minor increment (`T64_ABI_VERSION_MINOR` and the package version: 1.1 and
   1.1.0 follow 1.0 and 1.0.0),
4. a dated row in the amendment log below: `date | golden | change | defect
   evidence | consumer suite run | new minor`.

Any incompatible change (the breaks that section 13 lists) is a new major with its
own record; it is never made under this record.

The check enforces the minor rule mechanically: with the block keys `bound_minor`
and `candidate_amendments` present, the ABI minor of the installed header must
equal `bound_minor` plus the amendment rows made after the bind (every dated row
of the log minus `candidate_amendments`), and the block must equal what is
recomputed. A pre-bind amendment, which only the conditional amendment plan 12-11
of Phase 12 could have made, folds into ABI 1.0: it is counted by
`candidate_amendments` together with its amendment-log rows and raises no minor.
Plan 12-11 did not trigger (the host work found no defect of a bound
postcondition), so `candidate_amendments` is 0 and the log is empty.

Change procedure, in order: (1) record the evidence; (2) regenerate the changed
golden with its generator (`layout --write`, `symbols --write`) or edit the class
table, and add 1 to the ABI minor and to the package version's minor; (3) rebuild
and install, and re-run the installed-consumer suite; (4) run
`python3 -B scripts/check-abi.py freeze --record docs/abi/FREEZE-CANDIDATE.md
--prefix <installed prefix> --write`, which rewrites `docs/abi/header.sha256` and
the block, keeps `bound_minor` and `candidate_amendments`, and refuses while
another golden does not match the tree; (5) add the amendment row. A commit that
changes a bound element without these steps fails `repo.abi-freeze-record`.

## Final bind (Phase 12)

Plan 12-14 of Phase 12 made the final bind on 2026-11-26. It confirmed the
candidate with no amendment: the host-found API defect ledger of the Phase 12
planning record holds no row with the disposition "amend", so plan 12-11 did not
trigger and nothing found in the host work needed amending. The bind set the
major to 1 and the minor to 0 (ABI 1.0; package 1.0.0 with `COMPATIBILITY
SameMajorVersion`), settled the header comments that called the API provisional
(the header text moved, so the header hash moved by design), regenerated the
three ABI-version pins of `docs/abi/layout.c` and rewrote `docs/abi/header.sha256`
and this block. The public symbols, the status classes, every struct layout and
the padding holes are the candidate's.

## Accepted limits at the bind

Both are stated so that no reader takes them for hidden behavior; neither is an
open amendment.

- S-05, padding holes: the eight implicit alignment holes of the block stay bound
  rows, pinned for targets with 64-bit pointers. Turning a hole into an explicit
  reserved member inserts a struct member, which is not an additive change
  (`docs/spec/api.md` section 13 appends new fields and never inserts them), so
  the holes stay as they are under ABI 1.0.
- I-05, debug bus read: the `value_out` span of `t64_machine_bus_read` is the
  caller's responsibility and is not judged against the instance (the header
  comment of the call and `docs/spec/api.md` section 10.1). Judging it would
  narrow the inputs the call accepts, which section 13 counts as a break, so it
  stays an accepted limit of ABI 1.0.

## Owner-review item (D6)

ABI 1.0 is an ABI identity constant: `T64_ABI_VERSION_MAJOR` 1 and
`T64_ABI_VERSION_MINOR` 0 name the binary interface a host compiles against, and
the CMake package version in the installed configuration (1.0.0) is that ABI
number, not a release version. `project(VERSION)` stays 0.0.0, and tags, the
release channel and release naming remain unselected (D6). No tag or release was
made with the bind. The owner confirmed the 1.0 numbers on 2026-11-26 as an ABI
identity. This is an owner-review item: the owner may later choose release
versioning independently, and that choice is separate from the ABI number bound
here.

## Machine-read block

```freeze-record
abi_version: 1.0
package_version: 1.0.0
amendments: 0
bound_minor: 0
candidate_amendments: 0
header_sha256: b4f5705c87e25f94ac56091f305cd2364acd432210914fc1093cb8e94f992dac
public_symbols_sha256: c45ff33c495f8c0935dd2b0362b528c631037483fb6affb42b7d2d908a252d39
layout_sha256: 7897b5f8b19b74cf1f141ce2cfebbb75abd0c9bb058d3c5be38b78c17dbd7520
status_classes_sha256: 570548fc95fb3997a0f750bbe8cfdf443d6eadf4c0e5a38e5fe5d12ee50e6e5e
public_function_count: 35
status_count: 39
stop_reason_count: 8
layout_structs: 14
layout_members: 165
layout_constants: 190
padding_holes: 8
padding_hole: t64_audio_packet_info encoding 76 4
padding_hole: t64_audio_packet_info frame_count 44 4
padding_hole: t64_audio_packet_info struct_size 4 4
padding_hole: t64_cpu_stop_detail struct_size 4 4
padding_hole: t64_host_allocator struct_size 4 4
padding_hole: t64_host_storage struct_size 4 4
padding_hole: t64_output_drain_result struct_size 4 4
padding_hole: t64_state_import_report rule 20 4
```

## Amendment log

Empty at the candidate bind and at the final bind. A row is added by an
amendment, never by hand without the steps above.

| date | golden | change | defect evidence | consumer suite run | new minor |
|------|--------|--------|-----------------|--------------------|-----------|

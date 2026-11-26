# Freeze-candidate record

Status: freeze candidate, ABI 0.1 (package 0.1.0). Bound by plan 11-23 of Phase 11
(Installed Consumer and Freeze Candidate). The final bind is the last plan of
Phase 12; until then this record is a candidate and nothing more.

Evidence tier: unit-tested, authored scope; a software-interface claim only. This
record announces nothing and makes no distribution, compatibility or accuracy
statement about any profile. `0.1.0` is the number of the candidate, not a
version a host may rely on keeping the same layout. A
later change to anything bound here is a recorded amendment with its cost, not a
silent edit.

## What is bound

The candidate is the installed Development component of the `tendies64` package:
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

The block also binds the ABI version (0.1, from `T64_ABI_VERSION_MAJOR` and
`T64_ABI_VERSION_MINOR` of the installed header), the package version (0.1.0, from
the installed package version file), the number of amendment rows below, the
counts of public functions, statuses, stop reasons, structs, members and
constants, and the padding holes of the layout. `repo.abi-freeze-selftest` shows
the check failing on each kind of incompatible change: a removed or reordered
struct member, a renumbered, unclassified or reclassified status, a removed or
added public function, an edited header line, a second installed header, an edited
golden, an edited count or hash of this record, and an ABI minor raised with no
amendment row. A CRLF copy of the header passes, because the hash is taken over
the LF form.

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

## Evidence for the candidate

The installed-consumer suite (`installed.consumer.*`, `installed.controls.*`, the
`install.*` and `api04.*` names) passed on both hosted operating systems in run
45775107983 (gate-linux and gate-macos, 324 of 324 names, pushed head `4b13a90`).
The installed header and archive of that tree are the ones bound here: the header
is byte-identical, and the only production-directory change since is a test
registration in `core/CMakeLists.txt`. The hosted pair for the commits of plan
11-23 comes with the next push. The candidate evidence stays a software-interface
claim; it says nothing about a console or another emulator.

## Layout targets and padding holes (review item S-05)

The pins of `docs/abi/layout.c` hold for targets with 64-bit pointers: macOS arm64
and Linux x86_64, the two hosted gate operating systems, both of which compile
the same pins against the installed header (`install.abi-layout`). On any other
data model the file compiles and checks nothing, and the candidate binds no
layout there; in particular a target whose `uint64_t` has 4-byte alignment inside
a struct (i386 System V) would place members at other offsets.

Eight implicit 4-byte alignment holes exist on the bound targets. The block lists
each as `padding_hole: <struct> <member the hole follows> <byte offset> <bytes>`;
`repo.abi-freeze-record` measures them from the installed header and fails when
one appears, moves or disappears. They are fixed by the platform ABI, and copy-out
builds each record in a zeroed local, so no stack byte leaks through a hole.
Whether to turn them into explicit reserved members, as `t64_output_status`
already has, is a Phase 12 final-bind question: doing so changes the layout and
is an amendment (below).

## The rule between this record and the final bind

From this record until the Phase 12 final bind, a change to any bound element is
an amendment, and only a defect-driven one is allowed. An amendment needs

1. evidence of the defect (a failing test, a review finding or a host finding),
2. a re-run of the installed-consumer suite against the amended installed tree,
3. a minor increment (`T64_ABI_VERSION_MINOR` and the package version; DR-02),
4. a row in the amendment log below: `date | golden | change | defect evidence |
   consumer suite run | new minor`.

The check enforces the last two mechanically: the ABI minor of the installed
header must equal 1 plus the number of amendment rows, and the block must equal
what is recomputed. The additive-extension rule of `docs/spec/api.md` section 13
says which kinds of change are additive (trailing struct fields guarded by
`struct_size`, new functions, statuses and stop reasons appended after the last
value, additive profiles and devices); before the final bind an additive change
is still an amendment with a row, because every golden is bound by hash.

Re-bind procedure, in order: (1) record the defect evidence; (2) regenerate the
changed golden with its generator (`layout --write`, `symbols --write`) or edit
the class table, and add 1 to the ABI minor and the package version; (3) rebuild
and install, and re-run the installed-consumer suite; (4) run
`python3 -B scripts/check-abi.py freeze --record docs/abi/FREEZE-CANDIDATE.md
--prefix <installed prefix> --write`, which rewrites `docs/abi/header.sha256` and
the block and refuses while another golden does not match the tree; (5) add the
amendment row. A commit that changes a bound element without these steps fails
`repo.abi-freeze-record`.

## Final bind (Phase 12)

The final bind is the last plan of Phase 12. It either confirms this candidate or
lists its amendments, each with evidence and a re-run installed-consumer suite,
and it may raise the major version only when no amendment remains open. This
record makes no promise that it will; it is the contract that every later check
compares against, and no stable-ABI statement exists before that plan.

## Machine-read block

```freeze-record
abi_version: 0.1
package_version: 0.1.0
amendments: 0
header_sha256: 538a6a92105025adcef0f8ef08fcfd46a6bca9d9bc39297dc8a0949d90294fda
public_symbols_sha256: c45ff33c495f8c0935dd2b0362b528c631037483fb6affb42b7d2d908a252d39
layout_sha256: 9637d530d28435ee619452d290b19c4a217c964d5c6d9883c50e492fbfd01931
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

Empty at the candidate bind. A row is added by an amendment, never by hand
without the steps above.

| date | golden | change | defect evidence | consumer suite run | new minor |
|------|--------|--------|-----------------|--------------------|-----------|

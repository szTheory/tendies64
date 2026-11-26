# Phase 12 pin-impact record

Planning record of Phase 12 (Minimal Host, Playable Homebrew and Final Bind), opened by plan
12-01 (phase-wave W0, no production edit). Each later plan appends its own rows; the sections
below are written in the order the plans fill them. Evidence tier: unit-tested, authored scope;
a software-interface claim only.

## Wave bases

Format and rule (standing rule "Base records" of the Phase 12 plans): a base is the 40-hex
output of `git rev-parse HEAD`, written here as exactly one line per label,
`- <label>: <40 lowercase hex>`, in a commit that changes only this file. The Wave 0 record
below is the one exception: it is committed with Task 1 of plan 12-01, whose commit has this
base as its parent, and it is taken only after `git status --porcelain -- .planning` printed
nothing, so the base commit contains every Phase 12 plan, `12-VALIDATION.md`, the research and
the context. Later labels ("Host base" by plan 12-06, "Bind base" by plan 12-12,
"Qualification base" by plan 12-18) are appended by their plans. No check uses the moving
reference HEAD as a base.

- Wave 0 base: 60b9e3c34fc0efc0ff64296cfa31415565b847b2

## Registration map

Written by plan 12-01 Task 2 at the Wave 0 base before any Phase 12 edit. Question: which sites
does a new authored fixture touch? Method: `git grep -n 'phase10-composite'` over the tree
(28 tracked files outside `.planning/` and the evidence documents; the sites below), widened by
`git grep -n -i composite` over the CMake, consumer and checker files and by the research
Pattern 3 list. The analogue column is the composite fixture's site measured at the Wave 0 base;
the title adds the same kind of site. The owner column names the Phase 12 plan that makes the
edit (rows start with `| R-`). The oracle-side sites, owned by plan 12-02, are listed
separately below the table (rows start with `| O-`) so that every `R-` row is owned by a plan
between 12-03 and 12-05 or by every plan.

| ID | Site (path and lines at the Wave 0 base) | Composite analogue measured | What the title adds | Owner |
|----|------------------------------------------|-----------------------------|---------------------|-------|
| R-01 | `fixtures/title/title.S`, `fixtures/title/title.ld` | `fixtures/composite/composite.S` (505 lines) and `composite.ld` (text at 0x80000400, `.rodata` after) | the title source (at most about 600 lines) and its linker script; the guest is frozen by the source commit before any pin (authoring-once rule) | 12-03 |
| R-02 | `scripts/build-title-fixture.sh` | `scripts/build-composite-fixture.sh`: stems line 30 (`COMPOSITE_STEMS`), source hash lines 84-86, completion symbol line 121 (`t64_save_completion`), schema string line 152 (`t64.phase10-composite-fixture-build.v1`), staging and defsym variants lines 175-182, usage line 207; `--qualify` adds `check_manifest_identities` | a copy with stems `phase12-title-s32k` and `phase12-title-refused-s32k` (defsym `T64_TITLE_REFUSED=1`), completion symbol `t64_title_completion`, schema `t64.phase12-title-fixture-build.v1` | 12-03 |
| R-03 | `core/CMakeLists.txt` after line 419 | stems 392-393, output loop 395-400, custom command 402-415 (builder, `composite.S`, `composite.ld` as dependencies), target `t64-phase10-composite-fixture` 417-419 with `add_dependencies(... t64-boot-fixtures)` | target `t64-phase12-title-fixture` with outputs `.z64 .elf .payload.bin .json` per stem | 12-03 |
| R-04 | `fixtures/manifest.json` after line 1203 (41 `"id"` rows, 1,205 lines) | rows `phase10-composite-s32k` (1147-1175) and `phase10-composite-wrong-s32k` (1176-1203): `source_rights` authored-here, `binary_rights` cleared, `distribution` build-only, toolchain GNU Binutils 2.45, `expected.kind` self-checking | two rows with the same rights state; `source.revision` names a commit that already holds the source, so the sources and the rows are two commits; appended rows need no oracle rebind (per-row pins), checked at once by `harness.rdp-oracle-authority` | 12-03 |
| R-05 | `runner/CMakeLists.txt` near line 641 | `add_dependencies(t64-phase10-composite-fixture t64-rdp-fill-fixture)`: the RDP fill target recompiles the shared packer, so every consumer builds after it (hosted run 44631218639) | the same `add_dependencies` line for `t64-phase12-title-fixture` | 12-03 |
| R-06 | `runner/t64_real_fixture.c` after line 165 (a production runner edit) | rows 154-165: id, SHA-256, size 5,496, profile, profile SHA-256, completion PC, pass-eligible, tick cap `T64_REAL_GUEST_TICKS_LONG_CAP`, transcript bytes and SHA-256 | two rows (title and refused-frame ROM); `pinned >= 6` and the generic row checks of `runner/tests/test_phase09_runner.c` lines 1604-1670 need no count edit | 12-04 |
| R-07 | `runner/tests/test_title_two_run.c` | `runner/tests/test_composite_two_run.c` (1,281 lines): ROM ids lines 59-60, script path line 65, the `T64_COMPOSITE_EXPECTED` read near line 506 | a copy with cases `two-run`, `wrong-input` and `refused-frame` reading `fixtures/title/title.expected.json` | 12-04 |
| R-08 | `runner/CMakeLists.txt` near lines 731-758 | executable `test_composite_two_run` 737-745 (dependencies on the supervisor, result checker, runner and fixture targets) and the helper `t64_phase10_composite_case` 746-755 with a literal `TIMEOUT 120` (`harness.phase09-qualification` scans every `CMakeLists.txt` for literal TIMEOUT numbers) | executable `test_title_two_run`, a helper and three names `runner.title-two-run`, `controls.real.title-wrong-input`, `controls.real.title-refused-frame`, labels `runner;controls;phase12`, COST and TIMEOUT per F-108 | 12-04 |
| R-09 | `fixtures/title/title.runner-pins.json` | `fixtures/composite/composite.runner-pins.json` (schema `t64.composite-runner-pins`), cited by `scripts/check-consumer-literals.py` lines 47 and 106-107 | the runner section (12-04, label "runner-recorded pin, not independent") and the consumer section (12-05, label "consumer-recorded pin, not independent") | 12-04, 12-05 |
| R-10 | `tests/installed-consumer/cases.txt` after line 12 (24 case rows) | row `composite` | rows `title` and `title-persist` (the closed list: `scripts/check-install.sh build` fails when the binary and the file disagree) | 12-05 |
| R-11 | `tests/installed-consumer/main.c` near line 44, `tests/installed-consumer/t64c_cases.h`, `tests/installed-consumer/CMakeLists.txt` line 20 (`T64C_SOURCES`) | the case table row `{"composite", t64c_case_composite}`, the shared helper header and `composite.c` in the source list | two table rows, the helper declarations and `title.c` in `T64C_SOURCES` | 12-05 |
| R-12 | `tests/installed-consumer/title.c`, `tests/installed-consumer/t64c_title.h` | `composite.c` and `t64c_composite.h` (generated by `scripts/check-consumer-literals.py`) | the title cases and the generated literals header | 12-05 |
| R-13 | `tools/abi/CMakeLists.txt` lines 394-396 and 422-464 | the fixture dependency line 394-395 (`t64-installed-consumer-fixtures`), `T64_CONSUMER_ROM` line 396, the cost variables from `T64_CONSUMER_COST_composite` line 422 (a case with no cost is a configure error, checked inside the loop) and the registration loop 445-464 passing `--rom "${T64_CONSUMER_ROM}"` | `t64-phase12-title-fixture` in the dependency line, a per-case ROM variable `T64_CONSUMER_ROM_<case>` that falls back to `T64_CONSUMER_ROM`, set to `build/fixtures/phase12-title-s32k.z64` for the two title cases, and `T64_CONSUMER_COST_title` and `T64_CONSUMER_COST_title-persist` | 12-05 |
| R-14 | `scripts/check-consumer-literals.py` | constants lines 45-48 (`EXPECTED_REL`, `INPUT_REL`, `PINS_REL`, `HEADER_REL`), schema checks 100-107, header writer 172-219, selftest edit 334 (`--check --selftest` is the CTest `installed.consumer.literals`) | a second literal set for the title (expected, input scripts, runner pins, header) with its seeded failures | 12-05 |
| R-15 | `scripts/check-installed-case-map.py` | generic over `cases.txt` and the `installed.consumer.*` names (3 allowlisted script names); the hard-coded case names at lines 147 and 158 are its own selftest data | no edit expected; `repo.installed-case-map` re-runs unchanged and its count row stays 3 (one name plus the fixture closure `installed.consumer.build` and `install.setup`) | 12-05 |
| R-16 | `scripts/check-test-inventory.sh` | block `PHASE11_TESTS` lines 279-353 and the concatenation `printf` at line 456 (the block is separate from `PHASE10_TESTS`, 245-278) | block `PHASE12_TESTS` (label `phase12` on every name) and its entry in the concatenation list; the count of `ctest -N -L phase12` must equal the block | every plan |
| R-17 | `scripts/preflight-phase12-regexes.txt` | the Phase 11 rows (`^installed\.consumer\.` 28 with the fixture closure) | the zero rows written by 12-01 are raised by the owning plan in the same commit as its registration; counts include the fixture closure that ctest adds to a `-R` match | every plan |
| R-18 | `scripts/check-install.sh` lines 295, 671, 713 | `$ROOT/build/fixtures/phase10-composite-s32k.z64` in the three-way, controls and sanitizer lanes | no edit planned: the title cases receive their ROM through the CTest `--rom` argument; the composite ROM stays the ROM of the three-way, controls and lanes | 12-05 (confirms no edit) |

Oracle-side sites (owned by plan 12-02, listed so that no site is unowned):

| ID | Site (path and lines at the Wave 0 base) | Composite analogue measured | What the title adds | Owner |
|----|------------------------------------------|-----------------------------|---------------------|-------|
| O-01 | `fixtures/title/title-spec.md`, `title_oracle.py`, the four `title.*.input.json` scripts and `title.expected.json` | `fixtures/composite/composite_oracle.py`, `composite.expected.json`, `composite.input.json` | the behavior authority, the stdlib-only oracle, the scenario scripts and the expected checkpoint digests (written before the guest) | 12-02 |
| O-02 | `runner/CMakeLists.txt` after line 768 | `harness.composite-oracle` 764-768 (`composite_oracle.py --check composite.expected.json`, labels `harness;phase10`, TIMEOUT 60) | `harness.title-oracle` (labels `harness;phase12`, COST, TIMEOUT) | 12-02 |

Not registration sites but later readers of the same names: `scripts/qualify-phase11.py` line 852
(`cases == ["composite", "smoke"]`) and its test (`scripts/tests/test_phase11_qualification.py`
line 973) are copied by plan 12-17 into the Phase 12 qualifier, which decides its own case set;
`docs/claims/phase10.md` and `docs/evidence/phase10-qualification.md` cite the composite ids
and stay untouched.

## Bind pin-site map

Written by plan 12-01 Task 2 at the Wave 0 base before any Phase 12 edit. Question: which sites
does the final bind (plans 12-13 to 12-16) have to move or re-check? Measured fresh with `git
grep` at the Wave 0 base; rows start with `| B-`. "Header" means the installed header
`core/include/tendies64/t64_machine.h`. The owner is the plan that edits the site: 12-13 is the
checker generalization (no ABI byte moves), 12-14 the one coupled bind commit (the last
ABI-moving plan), 12-15 the live-text settle (no bound hash moves), 12-16 the post-bind
controls and proofs. No scan path was narrowed to reach a count.

Measured counts at the Wave 0 base (the proof commands follow the table):

- Header lines holding "provisional": 13 (`grep -c -i provisional` on the header): 11 lines hold
  "provisional until the Phase 11" on one line (63, 131, 137, 142, 174, 190, 362, 677, 720, 877,
  963; line 174 and line 362 read "Provisional" with a capital), 1 wrapped site where the phrase
  continues on the next comment line (914-915) and 1 line that reads "(provisional)" (179).
- Header text sites to settle: 15, namely those 13 lines plus the banner (lines 2-4, "a 0.x freeze
  candidate bound by Phase 11 ... it is not a frozen ABI yet") and the sentence at line 713 ("the
  API is Phase 11"). Thirteen of the 15 sites (all except line 179 and line 713) also carry the
  clause "final ABI bind at the end of Phase 12" (13 header lines name Phase 12 and 12 hold the
  words "final ABI bind"), which the settle rewrites together with the provisional phrase.
- Measurement correction M-1: `git grep -n -i provisional -- core/include` prints 15 lines in two
  files, 13 in the installed header and 2 in `core/include/tendies64/t64_facade.h` (lines 2 and
  18: "PROVISIONAL, in-tree only"; the facade is the synthetic-guest runner header, it is not
  installed and sits outside the freeze record, DR-18 of Phase 11). The 15 header text sites above
  are in the installed header only; the facade lines are not bind sites.
- Lines holding "until the Phase 11" outside the header: 4 in `core` (`core/t64_internal.h` 1,
  `core/t64_machine.c` 1, `core/t64_state.c` 2) and 13 in `docs/spec` (`machine-contract.md` 5,
  `raw-capture.md` 1, `replay.md` 1, `result-protocol.md` 2, `save-media.md` 1,
  `state-container.md` 3; the wrapped `result-protocol.md` site at 546-547 is counted once);
  `runner` and `tools` hold 0. A multi-line aware scan of `core runner tools docs/spec tests
  scripts host` (phrase joined across comment and line breaks) finds the same per-file counts
  outside the header, so no wrapped site hides there. `scripts/check-stale-freeze-text.sh` holds
  1 more, in its own clean-relabel seed (line 122), which is intentional. Closed Phase 9 and
  Phase 10 records (`docs/claims/phase09.md` line 32, `docs/claims/phase10.md` line 35,
  `docs/evidence/phase10-qualification.md` 1) keep their historical wording and are not edited.
- Other `provisional` lines outside the installed header, for the 12-15 judgment (not all are
  freeze statements): `core/include/tendies64/t64_facade.h` 2, `core/tests/test_save_transfer.c`
  2, `docs/spec` 23 lines in 7 files (`machine-contract.md` 10, `state-container.md` 5, `rdp.md` 2,
  `result-protocol.md` 2, `save-media.md` 2, `raw-capture.md` 1, `replay.md` 1), `runner` headers 3
  and `runner/tests/test_state_runner.c` 1.

| ID | Site (path and lines at the Wave 0 base) | Current value or text | What the bind does | Owner |
|----|------------------------------------------|-----------------------|--------------------|-------|
| B-01 | Header line 41 | `#define T64_ABI_VERSION_MAJOR UINT32_C(0)` | becomes 1 | 12-14 |
| B-02 | Header line 42 | `#define T64_ABI_VERSION_MINOR UINT32_C(1)` | becomes 0 | 12-14 |
| B-03 | Header banner lines 2-4 | "a 0.x freeze candidate bound by Phase 11, with the final ABI bind at the end of Phase 12; it is not a frozen ABI yet" (wrapped over three lines) | settled text (FN-03) | 12-14 |
| B-04 | Header lines 63-64 | "provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12)", SI events | settled (FN-03) | 12-14 |
| B-05 | Header lines 131-132 | the same phrase, save device request refusals | settled | 12-14 |
| B-06 | Header lines 137-138 | the same phrase, save import and export refusals | settled | 12-14 |
| B-07 | Header lines 142-143 | the same phrase, state import refusals | settled | 12-14 |
| B-08 | Header lines 174-175 | "Provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12)" | settled | 12-14 |
| B-09 | Header line 179 | "Known and unsupported (provisional)" | settled (the "(provisional)" marker is dropped or replaced by the bound wording) | 12-14 |
| B-10 | Header lines 190-191 | "provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12)", byte-order policy | settled | 12-14 |
| B-11 | Header lines 362-363 | "Provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12)", descriptor policy | settled | 12-14 |
| B-12 | Header lines 677-678 | "State export (provisional until the Phase 11 freeze candidate, final ABI bind at the end of Phase 12: the name, the container layout and the format ..." | settled | 12-14 |
| B-13 | Header line 713 | "the string is not public API work (the API is Phase 11)" | settled | 12-14 |
| B-14 | Header lines 720-721 | "State import (plan 09-09; provisional until the Phase 11 freeze candidate, final ABI bind at the end of Phase 12)" | settled | 12-14 |
| B-15 | Header lines 877-878 | "Save device information (provisional until the Phase 11 freeze candidate, final ABI bind at the end of Phase 12)" | settled | 12-14 |
| B-16 | Header lines 914-915 | "provisional" / "until the Phase 11 freeze candidate, final ABI bind at the end of Phase 12)": the phrase wraps across the two comment lines | settled | 12-14 |
| B-17 | Header lines 963-964 | "(provisional until the Phase 11 freeze candidate, final ABI bind at the end of Phase 12)", input latch | settled | 12-14 |
| B-18 | `core/CMakeLists.txt` lines 141 and 160 | comment "(0.1.0, SameMinorVersion for 0.x)" and `VERSION 0.1.0 COMPATIBILITY SameMinorVersion` in `write_basic_package_version_file` | package 1.0.0 with `SameMajorVersion`, comment reworded | 12-14 |
| B-19 | `core/tests/test_api_surface.c` lines 21-22 | `T64_CHECK_INT(T64_ABI_VERSION_MAJOR, 0)` and `T64_CHECK_INT(T64_ABI_VERSION_MINOR, 1)` | expect 1 and 0 (the major-rule case is 12-16's) | 12-14 |
| B-20 | `docs/abi/layout.c` lines 245-247 | three pins: `T64_ABI_VERSION_MAJOR == 0ll`, `T64_ABI_VERSION_MINOR == 1ll`, `T64_ABI_VERSION == 1ll` | regenerated by `check-abi.py layout --write` (major 1 gives `T64_ABI_VERSION == 65536ll`) | 12-14 |
| B-21 | `docs/abi/header.sha256` and the block keys `header_sha256` and `layout_sha256` of `docs/abi/FREEZE-CANDIDATE.md` | header file hash `46d07b8f7658...`, block `header_sha256: 538a6a92...`, `layout_sha256: 9637d530...` | rewritten by `check-abi.py freeze --write`; the public-symbol and status-class goldens do not move | 12-14 |
| B-22 | `tests/installed-consumer/CMakeLists.txt` lines 17-18 | `set(T64C_REQUIRED_VERSION 0.1 CACHE STRING ...)` and `find_package(tendies64 ${T64C_REQUIRED_VERSION} REQUIRED CONFIG)` | request 1.0 | 12-14 |
| B-23 | `scripts/check-install.sh` lines 211 and 985 (message text), 446 | "consumer configure failed (find_package(tendies64 0.1 REQUIRED CONFIG))" twice; line 446 passes `-DT64C_REQUIRED_VERSION=$CTL_ARG` from the control row | messages say 1.0; the control mechanism is unchanged | 12-14 |
| B-24 | `scripts/check-install.sh` lines 477-491 and 512-515 | the `doctored-header-abi` doctoring (`T64_ABI_VERSION_MINOR` plus one, read back by `ctl_abi_minor`) | verify only: at 1.0 a caller minor 1 above the library's still answers ABI_MISMATCH | 12-14 (re-run), 12-16 |
| B-25 | `tests/installed-consumer/controls.tsv` lines 18, 19 and 22 | `version-mismatch-minor` (request 0.2), `version-mismatch-patch` (request 0.1.5, expected text `compatible with requested version "0.1.5"`), `doctored-header-abi` (status 34) | requests of a higher minor (1.1) and a higher major (2) plus one positive control; the major control is 12-16 (DR-27 `installed.controls.version-mismatch-major`) | 12-14, 12-16 |
| B-26 | `scripts/check-abi.py` lines 695, 702, 709, 719 and 941-962 | `FREEZE_CANDIDATE_MINOR = 1`, `SCALAR_KEYS`, `AMENDMENT_ROW_RE` (counts every dated row of the log), the advice text and the rule `minor != FREEZE_CANDIDATE_MINOR + rows` | generalized by `bound_minor` and `candidate_amendments` block keys with no ABI byte change (DR-25) | 12-13 |
| B-27 | `scripts/check-abi.py` lines 1572-1577 | seed `freeze-amendment-missing`: anchor `UINT32_C(1)` becomes `UINT32_C(2)` | generalized to read the minor from the header (the hard-coded anchor no longer exists at 1.0); must be done before the bind commit | 12-13 |
| B-28 | `docs/spec/api.md` lines 3-5 (status), 42-56 (2.1), 57-70 (2.2), 71-87 (2.3), 666-715 (13), 716-753 (14), 754-780 (15) and the section 12 `t64_machine_advance` row at line 638 | "0.x freeze candidate", `T64_ABI_VERSION_MAJOR` is 0 and `MINOR` is 1, 0x00000001, "0.1.0 with `COMPATIBILITY SameMinorVersion`", the 0.2 and 0.1.5 refusals, "Candidate, final bind and amendments", "Limits of this candidate"; FN-02: the advance row lacks the CP0 corrupt-phase `INTERNAL_DEFECT` source | rewritten for the bound 1.0 contract; FN-02 text added | 12-15 |
| B-29 | `docs/abi/FREEZE-CANDIDATE.md` lines 3-5 (status), 56-65 (evidence, FN-04), 85-113 (rule between record and bind), 114-120 (final bind), 124-147 (block: `abi_version: 0.1`, `package_version: 0.1.0`, `amendments: 0`), 149-155 (amendment log, empty) | candidate wording | status, block (`abi_version: 1.0`, `package_version: 1.0.0`, new keys `bound_minor: 0` and `candidate_amendments: N`), final bind text and the D6 statement in 12-14; the FN-04 evidence text, prose only, in 12-15 | 12-14, 12-15 |
| B-30 | `scripts/check-stale-freeze-text.sh` line 27 (`DENY`) and its seeds from line 82 | deny list holds only the Phase 10 patterns | adds the deny phrase for "provisional until the Phase 11" with a seeded case so the settle cannot regress | 12-15 |
| B-31 | `core/t64_internal.h` line 2111, `core/t64_machine.c` line 1062, `core/t64_state.c` lines 10 and 461 | "provisional until the Phase 11 freeze candidate" (4 lines in comments) | relabeled; a `core` comment edit moves `T64_BUILD_ID`, and no sealed body (`REVIEWED_BODY_DIGESTS` of `scripts/check-rsp-no-hle.py`) may be touched | 12-15 |
| B-32 | `docs/spec/machine-contract.md` lines 532, 574, 658, 775, 904; `raw-capture.md` 132; `replay.md` 11; `result-protocol.md` 546 (wrapped to 547) and 555; `save-media.md` 30; `state-container.md` 28, 416, 801 | "until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12)" (13 lines) | relabeled | 12-15 |
| B-33 | `host/CMakeLists.txt` (added by 12-07; not present at the Wave 0 base) | `find_package(tendies64 0.1 CONFIG REQUIRED)` until the bind | request 1.0 in the bind commit | 12-07 (adds), 12-14 (moves) |
| B-34 | `core/tests/test_api_struct_size.c` lines 112-127, `tests/installed-consumer/ops.c` lines 400-414, `tests/installed-consumer/sequence.c` line 1822 | create with `T64_ABI_VERSION + 1u`, `- 1u`, `+ (1 << 16)`: expectation ABI_MISMATCH; the comment "while the major is 0 the minor must match" and the label "the minor version one below the library's" | observation, no ABI byte: the expectations hold at 1.0 (`- 1u` is major 0 with minor 65,535, a lower major; `+ 1u` is minor 1 above the library's, refused by `caller_minor <= library minor`), but the two texts name a minor below that no longer exists; 12-14 Task 2 judges a relabel (test comment edit) | 12-14 |
| B-35 | `core/t64_machine.c` lines 2718-2729 (`abi_version_is_accepted`) | the live branch `caller_minor <= T64_ABI_VERSION_MINOR` runs only with equality at minor 0 | recorded gap: "older minor accepted" is untestable until a first minor exists; the major-rule unit case is 12-16 | 12-16 |

Proof commands (run at the Wave 0 base, `H` is the header path; each is multi-line aware where
a phrase wraps):

1. `grep -c -i provisional "$H"` prints 13.
2. `git grep -n -i provisional -- core/include | wc -l` prints 15 (13 in the header, 2 in the
   facade; M-1 above).
3. `tr '\n' ' ' < "$H" | sed 's/ \* / /g; s/  */ /g' | grep -c -i -E 'until the Phase 11|the API is
   Phase 11|not a frozen ABI yet'` prints 1: the comment text joined into one line holds a match.
   The count is one because the joined text is one line; the occurrence form of the same command
   (`grep -o ... | wc -l` in place of `grep -c`) prints 14: 12 of "provisional until the Phase 11"
   (11 one-line plus the wrapped site), 1 of "the API is Phase 11" and 1 of "not a frozen ABI
   yet" (the banner's "not a frozen" / "ABI yet" of lines 3-4 joins only after the collapse,
   which is why the plain line grep of the banner finds nothing).
4. `grep -c -i 'until the Phase 11' "$H"` prints 12: the 11 one-line phrases plus line 915, the
   second line of the wrapped site (line 914 holds only "provisional", so command 1 counts that
   site on line 914 and this command counts it on line 915).
5. `git grep -n -i -E 'until (the )?Phase 11' -- core runner tools docs/spec` prints 29 lines:
   12 in the header, 4 in `core` outside it and 13 in `docs/spec` (`runner` and `tools` print
   none); without the header the same command prints 17. The only other tracked line with the
   phrase, outside this pathspec, is the clean-relabel seed of
   `scripts/check-stale-freeze-text.sh` (line 122), which stays.
6. After the bind (plans 12-14 and 12-15) command 1 and the occurrence form of command 3 must
   print 0 for the header, the command of item 5 must print 0 outside the header and, with the
   header settled, 0 in total, and `repo.stale-freeze-text` carries the deny phrase (B-30).

## API-07 pin: LibFuturo

Written by plan 12-01 Task 2 (DR-22). The pin of the written API-07 evaluation (plan 12-10) is
re-taken here, at plan time, from committed objects only, as the Phase 11 paper read did
(section "API-07 input: LibFuturo paper read" of the Phase 11 record). The sibling checkout is
located through the `LIBFUTURO_DIR` environment variable or the repository's documented sibling
default; its path is deliberately not recorded. Commands run with `git -C "$LIBFUTURO_DIR"`:
`rev-parse HEAD`, `log -1 --format='%cI %s'`, `rev-list --count HEAD`, `remote -v | wc -l`,
`cat-file -t <sha>`, `rev-list --count <sha>..HEAD` and `show HEAD:<path>`. Nothing was written
to that checkout and its working tree (which carries uncommitted edits) was not read.

| Item | Value at 2026-11-26 |
|------|---------------------|
| Pinned commit | `4177a43e67312e2db88ab21cba13aa7350063105` (object type commit) |
| Commit date and subject | 2026-11-23T06:27:54-05:00, "docs: finalize browser-host v2.4 state" |
| History length | 1,169 commits |
| Configured remotes | 0 |
| Relation to the Phase 11 read | identical: the Phase 11 record read the same SHA, and 0 commits follow it, so the pin did not move |
| Committed files the evaluation will read | `include/libfuturo/libfuturo.h` (347 lines) and `docs/contract.md` (430 lines) via `git show`; the committed `LICENSE` is the Apache License |
| Published status | local, unpublished SHA: no remote is configured; the public repository of the same name had a different 2-commit history at the 2026-11-23 lens and was not re-queried (a request would carry project identifiers to an external service); the evaluation therefore pins a local SHA and says so |

Plan 12-10 re-verifies this pin at its start (a different HEAD is recorded as the new pin, never
silently). The expected conclusion stays "no adapter now", with no adapter code, no core API
change and no claim that LibFuturo support exists.

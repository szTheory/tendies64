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

## Decision record

Written by plan 12-01 Task 3 before any production edit. Every row binds each later Phase 12
plan: a later plan that needs a different name, value or shape records the change in this table
first. Columns: the row, the decision, its source (CTX locked, CTX autonomous, CTX settled,
research fact or discretion, with the CONTEXT decision or research finding it implements) and a
reversibility rating. Reversibility is costly for DR-01, DR-02, DR-04, DR-10 to DR-13 and DR-33
(later plans pin them), one-way for DR-25 (the bind; already locked by CONTEXT and confirmed by
the owner on 2026-11-26, so no checkpoint is raised) and reversible for the others
(REVERSIBILITY_GATES is false: the one-way decisions are already settled by CONTEXT, so no
`checkpoint:decision` task exists). Evidence tier: unit-tested, authored scope; a
software-interface claim only, with no compatibility, accuracy, hardware, real-time, latency or
throughput claim.

| ID | Decision | Source | Reversibility |
|----|----------|--------|---------------|
| DR-01 | Title identity: profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`; ROM stems `phase12-title-s32k` and `phase12-title-refused-s32k` (built with `--defsym T64_TITLE_REFUSED=1`); builder `scripts/build-title-fixture.sh` copied from `scripts/build-composite-fixture.sh` (research contradiction 2: the composite builder is the newer descendant of the pad builder and adds the manifest identity check), schema string `t64.phase12-title-fixture-build.v1`, CMake target `t64-phase12-title-fixture`, completion symbol `t64_title_completion`, entry `0x80000400`, clock `0xf`, crc2 0; manifest rows authored-here / cleared / build-only, toolchain GNU Binutils 2.45; the sources and the rows are two commits. | CTX locked (route A; one named composite profile shared with the RUN-07 fixture; copy-paste builder) refined by research contradiction 2 | costly |
| DR-02 | Screen: 128x96 RGBA8888, framebuffer at RDRAM 0x00100000, stride 512, packet 49,152 bytes; VI values of research Pattern 2 (WIDTH 128, X and Y scale 0x400, H_START 128, V_START 192, H_SYNC 3093, V_SYNC 525, STATUS 0x303, VI_INTR 2); standard NTSC field of 1,042,709 ticks. Reason: the runner raw-capture file limit of 69,632 bytes, under which a 320x240 packet of 307,200 bytes cannot be captured and one 128x96 packet can. | research fact (Pattern 2, Pitfall 1) and CTX autonomous (screen small enough for the raw capture) | costly |
| DR-03 | Frame lock: poll MI_INTERRUPT bit 0x08 (VI pending) with Status.IE cleared, as `fixtures/composite/composite.S` lines 148-152 do, and clear it by a VI_CURRENT write; no exception handler and no instruction-count delay loop. The ROADMAP wording "frame-locked on the VI interrupt" is met as the polled VI interrupt pending bit (research contradiction 3: no authored fixture takes a CPU interrupt, and a handler would be an unqualified idiom). | CTX locked (frame-locked, no delay loops) refined by research contradiction 3 | reversible (guest source; after the 12-03 source commit the authoring-once rule applies) |
| DR-04 | Title behavior: background 0x203060ff, square 0xf0c020ff, start position (56, 40), 2 pixels per field per direction, clamped to x 0..112 and y 0..80; the D-pad or the stick (an absolute axis value of at least 16, stick y positive is up) moves it; A on its press edge starts one fixed tone burst (two 2,048-byte descriptors of one square-wave table, DACRATE 1103, BITRATE 15) and adds 1 to the press count; B resets the position; Start writes the 16-byte SRAM slot at offset 0 (magic 0x54363454, x, y, press count), prints a short ISViewer transcript (at most 4096 bytes) and executes `break` at `t64_title_completion`; boot loads the slot when the magic matches. No Z, textures, RSP microcode, TLB or libultra; at most about 600 lines of assembly; one screen, one 16x16 filled square, one fixed AI tone burst on A, one SRAM save slot. The exact byte formats are fixed by `fixtures/title/title-spec.md` (plan 12-02), the single behavior authority that both the oracle and the guest implement. | CTX locked (scope cap) and discretion (colors, tone, byte formats) | costly |
| DR-05 | Scenarios: short (runner, at most 3 packets: poll 1 neutral, poll 2 Right+A, poll 3 B, poll 4 Start; completion below 4,000,000 ticks) `title.short.input.json`; long (consumer and pump, at least 10 fields: D-pad and stick moves, two A presses, B, Start last) `title.long.input.json`; short-wrong (the short events with Left in place of Right, the runner wrong-input control; it still completes) `title.short-wrong.input.json`; shifted (the long events one field later, the consumer and pump wrong-input check) `title.shifted.input.json`; persistence (the save exported after the long run, imported into a fresh instance that runs with no input events and must show the restored position). Stamps sit at field midpoints (about 521,000 ticks from each boundary); the oracle exits 2 for any stamp within 200,000 ticks of a field boundary (the margin 12-02 enforces, fixed here before 12-02 relies on it) and for a short script longer than 3 fields. | research fact (Pitfalls 2 and 6) and discretion | reversible |
| DR-06 | Expected values: `fixtures/title/title_oracle.py` (standard library only, never reads a ROM, the core, the runner or a build output) writes `title.expected.json` (checkpoint frame digests per scenario, the audio stream digest, the save bytes digest, the transcript bytes and the blank-frame digest of 49,152 zero bytes) with `--write` and `--check`; CTest `harness.title-oracle`; checkpoint digests, not long raw captures. | CTX autonomous (checkpoint digests; spec-derived oracle) | reversible |
| DR-07 | Recorded pins: `fixtures/title/title.runner-pins.json` with a section `runner` labeled "runner-recorded pin, not independent" (short scenario: completion tick, stop reason, final state digest, video packet count) and a section `consumer` labeled "consumer-recorded pin, not independent" (long scenario: the same four values); never called independent. | CTX locked (independent literals) following Phase 11 DR-32 | reversible |
| DR-08 | Runner: rows in `runner/t64_real_fixture.c` (a production runner edit; the tick cap is the long cap), `runner/tests/test_title_two_run.c` copied from `test_composite_two_run.c` with cases `two-run`, `wrong-input` and `refused-frame`; CTest names `runner.title-two-run`, `controls.real.title-wrong-input` and `controls.real.title-refused-frame`; the refused-frame run must end non-pass and its frames must equal the blank digest, reported as "blank". | CTX autonomous (wrong-input and refused-packet controls) | reversible |
| DR-09 | Consumer: binary cases `title` and `title-persist`, literals header `tests/installed-consumer/t64c_title.h`, `scripts/check-consumer-literals.py` extended, a per-case ROM variable `T64_CONSUMER_ROM_<case>` (default `T64_CONSUMER_ROM`) set to `build/fixtures/phase12-title-s32k.z64` for the two title cases, a COST per case. | research fact (Pattern 3, item 5) | reversible |
| DR-10 | Host layout: `host/CMakeLists.txt` is a standalone project `t64h` (C99, warnings as errors, `find_package(tendies64 0.1 CONFIG REQUIRED)` until the bind, option `T64H_SDL` default OFF); `host/pump` (`t64h_pump`, `t64h_input`, `t64h_script`, `t64h_frames`); `host/tests` (`t64h_tests.c` with `--case NAME`, `t64h_sha256` copied from `tests/installed-consumer/t64c_sha256`); `host/sdl` only when `T64H_SDL=ON`; the root `CMakeLists.txt` never references `host/`; CTest registrations in `tools/hostcheck/CMakeLists.txt` calling `scripts/check-host.sh` (`build`, `run CASE`, `sdl`); build tree `build/gate/host-check`; paths quoted because the moved prefix holds a space. | CTX autonomous (host layout under `host/`, not `tools/`) | costly |
| DR-11 | Pump API (discretion): `T64H_TICK_NS` 16 (62,500,000 ticks per second); `t64h_ticks_from_ns(elapsed_ns)` is `elapsed_ns / 16`; a clock callback returning monotonic ns; `t64h_sinks` (video, audio and text callbacks with a context); `t64h_pump_init`; `t64h_pump_step(pump, max_ticks, result)`, which reads the clock once, refuses a backward clock (`T64H_CLOCK_BACKWARD`, counted, no advance), queues due input lazily before the advance that contains it, runs the advance loop (`RESOURCE_LIMIT` with tick movement continues; without movement it drains video, audio, then text and retries once; a second no-move returns `T64H_STALL`, "stall without progress"), stops queueing at a terminal stop (`T64H_TERMINAL`) and returns `T64H_API_ERROR` carrying any other `t64_status`; counters for drains, backward clocks, stall retries and deferred events. | discretion (function names) and research fact (Pattern 4) | costly |
| DR-12 | Input mapping: `t64h_raw_pad` (two int16 axes, button bits of `t64h` names, no SDL type) is mapped by one pure table function to the logical pad (uint16 buttons, int8 x and y): the axis value divided by 256 and clamped to -128..127, a deadzone where an absolute value below 8 maps to 0, the vertical axis inverted (host down positive, controller up positive); A is bit 15, B 14, Z 13 (left trigger past half travel), Start 12, D-pad up 11, down 10, left 9, right 8; every other control is unmapped; port 0 only; hotplug unsupported (a removed pad gives neutral input); keyboard off. | CTX autonomous (input mapping; keyboard fallback settled off) | costly |
| DR-13 | Scripted source: `host/pump/t64h_script.c`, a strict copy of the `t64.input` v1 subset (keys `buttons`, `port`, `tick`, `x`, `y`; schema `t64.input`, `schema_version` 1; closed keys; `tick` a decimal string; port 0; axes -128..127; buttons 0..65535; file at most 65,536 bytes and 4,096 events), refusing on the first violation with a named reason (the runner parser is outside the include guard, research contradiction 5). | research fact (contradiction 5, Security V5) | costly |
| DR-14 | Frames and audio: `t64h_frame_copy` honors `stride_bytes` and refuses a packet larger than 320x240 RGBA or a short destination; buffer sizes come from `t64_machine_output_status` `video_next_bytes`; audio packets pass through as S16BE with a byte-swap helper; underrun policy (shell): silence, counted. | research fact (Pattern 4) and CTX locked (underrun is silence and counted) | reversible |
| DR-15 | Tick-rate text: one sentence in `docs/spec/api.md` section 8.3 (plan 12-07) stating that 62,500,000 ticks per second is one tick per 16 ns; api.md is not a golden, so no header change (research candidate C-1). | research fact (Pattern 4, C-1) | reversible |
| DR-16 | Guards: `scripts/check-host-includes.sh` (copy of `scripts/check-consumer-includes.sh` scanning `host/pump`, `host/tests` and `host/sdl`; SDL3 headers allowed in `host/sdl` only) with `--selftest`; the default roots of `scripts/check-api-init.py` gain `host`; `scripts/check-host-isolation.sh` (no `find_package(SDL3`, `SDL3::` target, `#include <SDL3/` or `SDL_` identifier in `core`, `runner`, `tools`, `tests` or the root `CMakeLists.txt`; the root `CMakeLists.txt` holds no `host/` path; no `SDL_` name in the installed archive's symbol table or in `docs/abi/installed-undefined.txt`; the bare token SDL is a MIPS store instruction in the CPU tests and is not a finding); `scripts/check-host-policy.sh` with `--selftest` (in `host/`: no `FetchContent`, `ExternalProject`, `third_party`, vendored SDL source, `install(` rule, `MACOSX_BUNDLE` or icon file; no C string literal holding the product name, case-insensitively; the window title literal is exactly "t64 minimal host (development)"). | research fact (contradiction 4) and CTX locked (D4, D7) | reversible |
| DR-17 | SDL lane: `sh scripts/check-host.sh sdl`, local and manual only, never a CTest name or a workflow step; `find_package(SDL3 3.2.0 CONFIG REQUIRED)` in `build/host-sdl`; `SDL_VIDEO_DRIVER=dummy` and `SDL_AUDIO_DRIVER=dummy` exported before init; a virtual gamepad drives the mapping; skip line "[host-sdl] SKIP: SDL3 >= 3.2.0 not found (link-only system package; this lane installs nothing)" when SDL3 is absent; recorded hosted reason "ubuntu-24.04 has no libsdl3-dev candidate"; sanitized output only. | CTX autonomous (SDL lane) and research fact (Pitfalls 3 and 4) | reversible |
| DR-18 | SDL presentation: window surface plus `SDL_BlitSurfaceScaled` nearest at an integer scale (default 4), no renderer, no GPU, no vsync dependence; `SDL_AudioStream` from S16BE stereo at the packet rate rounded to whole Hz (conversion only, no fidelity claim); clock `SDL_GetTicksNS`; at most 65,536 ticks per pump step between SDL event pumps; controller only. | CTX locked (window-surface blit, nearest or integer scale) | reversible |
| DR-19 | Host-side save file, fit test (CTX autonomous): the test is whether one CLI-provided path with an atomic write (a temporary file plus rename, refusal on a partial write, V12 path handling) fits one task of plan 12-09. Result: it does not fit, because the three tasks of 12-09 are full (audit and policy scan; shell, smoke and lane; measurement, lane record and gate) and the write would need its own task with its own tests; so the host-side save file is omitted (the headless runs prove the save bytes). Plan 12-09 re-confirms the result in "Host steps". | CTX autonomous (host-side save persistence omitted unless it fits one task) | reversible |
| DR-20 | Throwaway measurement: one fields-per-second figure of the pump with the real clock, recorded in the 12-09 SUMMARY only, information only, no claim made from it. | CTX autonomous (throwaway measurement) | reversible |
| DR-21 | Evidence tiers: the claim is scripted equality through the pump at two fake-clock speeds (byte-identical video, audio and save results to the headless run); the SDL smoke is self-checking host glue only and not rendering correctness; `docs/evidence/phase12-host-manual.md` is a human-needed, non-blocking, owner-attested checklist and never a must-have. | CTX locked (evidence tiers) | reversible |
| DR-22 | LibFuturo: the pin of section "API-07 pin: LibFuturo", the written evaluation `docs/evidence/phase12-libfuturo-fit.md` (plan 12-10), CTest `repo.api07-evaluation`, conclusion "no adapter now", no adapter code, no core API change and no claim that LibFuturo support exists. | CTX locked (API-07) | reversible |
| DR-23 | Amendment slot: plan 12-11 is reserved. Its trigger is at least one row of "Host-found API defects" with disposition "amend" (a postcondition defect of the bound API found by plans 12-02 to 12-10 with a failing test or recorded evidence; the ergonomics observations C-1 to C-6 of the research are not defects). With no such row 12-11 closes with a one-line SUMMARY and no code. A second amendment is a further numbered plan inserted before 12-12. | CTX locked (amendment rule; no decimal plans) | reversible |
| DR-24 | S-05 and I-05: keep the eight implicit padding holes (they are bound and pinned for 64-bit pointers), the bus-read `value_out` span stays an accepted limit, and an explicit-reserved amendment happens only if plan 12-11 is triggered anyway. Reversible only before the bind of plan 12-14; afterwards the holes are bound. | CTX settled (S-05 default) | reversible (until 12-14) |
| DR-25 | Final bind (OWNER CONFIRMED 2026-11-26, answer "Bind ABI 1.0 / package 1.0.0"): ABI 1.0 (`T64_ABI_VERSION_MAJOR` 1, `T64_ABI_VERSION_MINOR` 0), package 1.0.0 with `COMPATIBILITY SameMajorVersion`, `project(VERSION)` untouched at 0.0.0; the record keeps its path `docs/abi/FREEZE-CANDIDATE.md` and gains block keys `bound_minor: 0` and `candidate_amendments: N` (N amendment rows made before the bind); `scripts/check-abi.py` checks minor == `bound_minor` + (rows - `candidate_amendments`) when `bound_minor` is present and the candidate rule otherwise; the 15 header text sites are settled (FN-03), FN-02 and FN-04 text are fixed; the record states that 1.0 is an ABI identity and the package's ABI number, not a release version, with no tag, channel or release naming, as an owner-review item. Executed as four plans: 12-13 (checker generalization with no ABI byte change), 12-14 (the one coupled bind commit of 11 files, proven in a scratch clone first), 12-15 (text settle and the stale-text guard) and 12-16 (the major-rule unit case, the major-version control and the post-bind proofs). Recorded alternative only, not the plan: keep 0.x with a recorded reason if an amendment is still open at the start of 12-14. | CTX locked (one-way bind) and OWNER CONFIRMED 2026-11-26 | one-way |
| DR-26 | G4 position: new checks are CTest names with a COST inside the existing gate, no `ci.yml`, `check-ci-yml.sh` or `gate.sh` edit; sanitizer, TSan, fresh-archive, SDL and full-mutant runs stay local; a Phase 12 G4 exception for owner review with its reversal rule is recorded in "Gate-growth ledger" below. | CTX settled (Phase 12 G4 gate growth) | reversible |
| DR-27 | CTest names by plan (the `PHASE12_TESTS` block of `scripts/check-test-inventory.sh` is the exact list and the registering plan is authoritative for spelling): 12-02 `harness.title-oracle`; 12-04 `runner.title-two-run`, `controls.real.title-wrong-input`, `controls.real.title-refused-frame`; 12-05 `installed.consumer.title`, `installed.consumer.title-persist`; 12-07 `host.pump.build`, `host.pump.clock`, `host.pump.drain-order`, `host.pump.stall`, `repo.host-include-guard`, `repo.host-include-guard-selftest`, `repo.host-isolation`; 12-08 `host.pump.input-map`, `host.pump.script`, `host.pump.frames`, `host.pump.title-two-speeds`, `repo.p12-mutant-anchors`; 12-09 `repo.host-policy`, `repo.host-policy-selftest`; 12-10 `repo.api07-evaluation`; 12-16 `unit.core.api.abi-major-rule` and `installed.controls.version-mismatch-major` (existing families); 12-17 `harness.phase12-qualification`. Each carries LABELS `<kind>;phase12`, a COST equal to its measured native seconds rounded up to two decimals and a literal TIMEOUT of at least 3.5 times its native seconds and at least 2 times its COST (F-108). | discretion (names) and CTX locked (gate budget) | reversible |
| DR-28 | Order: oracle before guest, guest frozen before pins, title before pump equality, amendments before the bind; the 12-14 bind commit is the last ABI-moving change (12-13 before it and 12-15, 12-16 after it move no bound hash); the qualifier and the final audit bind the post-bind tree; the cold gate-macos run after the final push is orchestrator-only. "The final bind is the last plan" therefore means the last ABI-moving plan. | CTX settled (qualification versus bind ordering) | reversible |
| DR-29 | Carry-ins: every open carry-in of Phase 11 is triaged in section "Carry-in triage" below (done, deferred, accepted or no action, with a reason). | CTX locked (carry-ins triaged) | reversible |
| DR-30 | Decomposition: 18 linear plans (W0; four TITLE plans and their review; four HOST plans plus the conditional slot and their review; four BIND plans; the qualifier; the final audit and evidence). The per-wave reviews are mandated by AUTONOMY v1.2, the bind and the qualification are separated by the settled ordering, and the bind is split so that each plan has a safe resume point. 18 exceeds the G5 guide of about 10-15 plans; recorded in the ROADMAP G5 note. | discretion (plan decomposition) | reversible |
| DR-31 | SDL CI job (discretion): none; a documented manual command and the local script only (a workflow file would need a `scripts/check-ci-yml.sh` edit with its seeded case and a first hosted run, about one plan). | discretion (whether the SDL shell gets its own CI job) and research fact (Pattern 5) | reversible |
| DR-32 | Mutants: `scripts/p12-mutants.py` (a copy of the `scripts/p10-mutants.py` frame) with 8 pump mutants (divisor 15, queue after advance, skip the text drain, drop the first packet on a stall, ignore the stride, deadzone off by one, axis sign, completion treated as resumable) and 4 title mutants (D-pad bit swap, step 3, wrong tone sample, wrong save offset), each killed by a named host test; `--check-anchors` is CTest `repo.p12-mutant-anchors`; `--run` is local (the 12-12 review and the qualifier). | discretion (mutant set) | reversible |
| DR-33 | Route B (libdragon) is a documented non-goal for v1.2: no libdragon source, toolchain, build, fixture or manifest row; the title is route A only. Rated costly: a toolchain and rights choice that later fixtures build on; reversing it needs a per-file rights review and an owner D4 decision, which is why it is recorded rather than merely omitted. | CTX locked (API-06 route A; route B a non-goal) | costly |

## Carry-in triage

Written by plan 12-01 Task 3 (DR-29). Every open carry-in of Phase 11 (the Phase 11 "Carry-in
triage" and `11-REVIEW.md` rows FN-01 to FN-04 and S-05, and the 11-29 observation OIPL3) ends
in done, deferred, accepted or no action with one disposition per row and a reason, so the final
bind inherits nothing silently. Rows DEF-W1-01 and T1 are carried unchanged from the Phase 11
table. Evidence tier: code observations at the Wave 0 base; nothing here is a compatibility or
hardware claim.

| Item | Origin | Disposition and owning plan | Reason |
|------|--------|-----------------------------|--------|
| T2 | Phase 9 triage, deferred to Phase 12 by Phase 11 | deferred to v1.3 (no owning plan) | The replay-driver duplication in `runner/t64_replay.c` has no state or digest effect, and the pump of Phase 12 does not use `runner/t64_replay.c` (it carries its own strict `t64.input` reader, DR-13), so unifying the drivers now would be redone; no API effect. |
| T5 | Phase 9 triage, deferred to Phase 12 by Phase 11 | deferred to v1.3 (no owning plan) | Closed detail names for the recorder failures need a runner result-protocol vocabulary change and a checker change in the runner, which is not installed and not in the freeze record; no API effect. |
| T6 | Phase 9 triage, deferred to Phase 12 by Phase 11 | deferred to v1.3 (no owning plan) | `T64_REAL_KEEP_WORKSET` (`runner/t64_supervisor.c`) is a runner-internal production supervisor name (gating it broke two Phase 8 tests, FX-18); the runner is not installed and not in the freeze record, so the final bind does not own it. |
| DEF-W1-01 | Phase 10 Wave 1 review (carried by Phase 11) | deferred to v1.3 (no owning plan), unchanged | Three sibling copies of the perspective and major-edge floor in sealed RDP bodies are unpinned behaviorally; the fix is tests and mutants only, with no API effect. |
| T1 | Phase 9 triage (carried by Phase 11) | deferred beyond v1.2 (no owning plan), unchanged | Splitting `unit.core.state.import` changes a case list that `docs/spec/phase09-cases.json` inventories, and that file's digest is a closed pin of `harness.phase09-qualification`; the hosted cost fits and the Gate-growth ledger reversal rule, not a split, is the trigger. |
| FN-01 | `11-REVIEW.md` (info) | accepted limit (api.md rows stand) | The reset `INTERNAL_DEFECT` and the state-export `RESOURCE_EXHAUSTED` rows of `docs/spec/api.md` name defensive branches that no test produces (S-08 kind); the class of each status is tested elsewhere, so the rows stay as documented behavior. |
| FN-02 | `11-REVIEW.md` (info) | done in 12-15 | The `t64_machine_advance` row of `docs/spec/api.md` section 12 gains the CP0 corrupt-phase `INTERNAL_DEFECT` source (B-28); api.md is not one of the four goldens, so no amendment. |
| FN-03 | `11-REVIEW.md` (info) | done in 12-14 (header) and 12-15 (other live text) | The 15 header text sites (B-03 to B-17) are settled in the bind commit because they move the bound header hash by design; the 4 `core` comments and 13 `docs/spec` lines holding "until the Phase 11" (B-31, B-32) are relabeled by 12-15, and `repo.stale-freeze-text` gains the deny phrase (B-30). |
| FN-04 | `11-REVIEW.md` (info) | done in 12-15 | The "Evidence for the candidate" text of `docs/abi/FREEZE-CANDIDATE.md` is rewritten as "Evidence for the bind" with every block line and golden unchanged (prose only; `repo.abi-freeze-record` passes without `--write`). |
| S-05 | `11-REVIEW.md` (info) | per DR-24: accepted, the eight implicit padding holes stay | They are bound and pinned per target in `docs/abi/layout.c` for 64-bit pointers; an explicit-reserved amendment happens only if plan 12-11 is triggered anyway. |
| OIPL3 | 11-29 observation (documented in `docs/spec/machine-contract.md`, review I-02) | no action | The open IPL3 profile narrows raw ROM admission to the exact pinned asset or an authored big-endian z64 cartridge; the title ROM is an authored big-endian z64 cartridge, so it is admitted, and nothing in Phase 12 depends on byte-swapped or word-reversed dump orders. |

G3 (at most 5 carried warning-level gaps) after triage: the carried items are DEF-W1-01, T1, T2,
T5 and T6, that is 5, at the limit. FN-01 and OIPL3 are accepted observations and S-05 is a
bound, accepted layout fact; none is a warning-level gap carried into Phase 13, and FN-02 to
FN-04 close in this phase. No row is open: 11 rows, 3 done (FN-02, FN-03, FN-04), 5 deferred
(T2, T5, T6, DEF-W1-01, T1), 2 accepted (FN-01, S-05) and 1 no action (OIPL3), none dropped.

## Host-found API defects

Rows are added by plans 12-02 to 12-10 when a failing test or recorded evidence shows a
postcondition defect of the bound API (DR-23); a row with disposition "amend" triggers plan
12-11. Columns: ID, found by plan, postcondition (the `docs/spec/api.md` row), evidence,
disposition (amend, accept or defer).

| ID | Found by plan | Postcondition | Evidence | Disposition |
|----|---------------|---------------|----------|-------------|

none at Wave 0

## Owner questions

none

Owner-review items (recorded, non-blocking, no question open): (1) the ABI 1.0 and package 1.0.0
numbers are an ABI identity and not a release version, confirmed by the owner on 2026-11-26
(DR-25; D6: `project(VERSION)`, tags, release channel and release naming stay unselected); (2)
the Phase 12 G4 exception of the Gate-growth ledger below.

## Gate-growth ledger

Written by plan 12-01 Task 3 (DR-26). ROADMAP guardrail G4 says the required gate may not grow
without removing equal time, the Phase 10 and Phase 11 exceptions cover their own phases only,
and no equal-time offset exists in Phase 12, so this ledger starts from the measured Phase 11
cold run and measures growth instead of projecting it: one row per wave is appended by the review
plans (12-06 for TITLE, 12-12 for HOST) and by the final audit (12-18). Columns follow the Phase
11 ledger: the wave, the new CTest names, the measured added native seconds (from
`build/gate/Testing/Temporary/CTestCostData.txt`), the hosted macOS ctest delta against the
baseline, the offset or G4-exception reason, and the remaining margin to the 900 s gate-macos
limit.

| Wave | New CTest names | Measured added native seconds | Hosted macOS ctest delta | Offset or G4-exception reason | Margin to 900 s |
|------|-----------------|-------------------------------|--------------------------|-------------------------------|-----------------|
| Baseline (Phase 11 close; cold run 45795103375 at head `dc247fa`, F-136; warm pair run 45792136442) | none: 329 CTest names (75 `phase11`) | not applicable (the 75 `phase11` names carry 65.83 s of COST) | ctest 279 s warm on 329 names | not applicable: the Phase 11 exception covers Phase 11 only | measured cold gate-macos 729 s of the 900 s limit, margin 171 s (19.0 percent); warm gate-macos 487 to 592 s over the Phase 11 samples (spread 105 s), margin 308 to 413 s (F-136, F-134, F-135) |
| Projection of the Phase 12 names (RESEARCH "CI Budget"; not a measurement) | about 23 names (DR-27 lists 24, two of them, `unit.core.api.abi-major-rule` and `installed.controls.version-mismatch-major`, in existing families), first 12-02 | about 16 s native summed (`host.pump.build` 2.5, `installed.consumer.title*` 2.9 and 0.9, `host.pump.title-two-speeds` 1.8, `runner.title-two-run` 1.6, two controls 1.6, `repo.host-include-guard-selftest` 1.0, `host.pump.drain-order` 0.8, others under 0.7) | about +15 to +25 s of gate-macos wall including +5 to +6 s of inventory selftest (hosted multiplier 2.8 to 4.3, about 50 to 60 s summed over 4 jobs) | see the exception below | projected cold margin 171 s minus 15 to 25 s, about 146 to 156 s, above the 120 s warning level; the measured numbers replace this row at 12-06, 12-12 and 12-18 |

Phase 12 G4 position (DR-26) and exception for owner review. No offset is claimed and no
equal-time offset exists. The new CTest names are the required evidence of Phase 12 success
criteria SC1 to SC4 (the title through the runner and the installed consumer, the pump equality
at two fake-clock speeds, the host guards and the LibFuturo evaluation guard, and the bound
final record), so their cost is required Phase 12 gate evidence and not removable without
removing the evidence. Everything heavy stays outside the hosted gate: the SDL lane, sanitizer,
TSan, fresh-archive and full mutant runs are local or qualifier stages. No `ci.yml`,
`check-ci-yml.sh` or `gate.sh` edit is planned, and every new name carries a COST and a TIMEOUT
by the standing rule. This is a recorded exception for owner review, not an owner approval, and
it grants no oracle, rights or publication authority.

Reversal rule of the Phase 12 G4 exception: if a review-plan or qualification-plan hosted
measurement (12-06, 12-12 or 12-18) projects a cold gate-macos run above 900 s, or a hosted run
is cancelled at the limit, or the projected margin falls under 120 s (a warning, reported in the
ledger row), a new numbered plan moves non-essential Phase 12 names (`installed.consumer.title-persist`,
the selftests and the third pump speed if any) to a non-gating local lane before anything else,
and the measurement is repeated. The final cold gate-macos measurement after the last push is
orchestrator-only.

## Plan file-count exceptions

Written by plan 12-01 Task 3. The warning band is 10 files per plan (`files_modified` of the
plan frontmatter, measured on 2026-11-26 for all 18 plans). Seven plans are above it and one
(12-09) sits on it; each carries its reason. Three bookkeeping files appear in most plans and are
listed here once instead of per plan: `scripts/check-test-inventory.sh`,
`scripts/preflight-phase12-regexes.txt` and `PIN-IMPACT.md`. Plan size is capped by tasks (at
most 4) and minutes, not by files; no plan has more than 4 tasks.

| Plan | Files | Reason |
|------|-------|--------|
| 12-08 | 15 | three pump modules with headers plus their one test binary and registrations |
| 12-11 | 15 | the frontmatter lists every file an amendment may touch, none when untriggered |
| 12-07 | 14 | a new standalone project scaffold |
| 12-15 | 13 | one-phrase relabels in the files the pin-site map measured (B-28 to B-32) |
| 12-02 | 11 | new fixture or test files plus registrations (oracle, scenario scripts, expected values) |
| 12-05 | 11 | new fixture or test files plus registrations (consumer cases, literals, per-case ROM variable) |
| 12-14 | 11 | the coupled bind set, all hash-bound or version-coupled, committed once after proof in a scratch clone |
| 12-09 | 10 | on the band: the policy scan, the SDL shell and its smoke, the lane script and registrations |

Shared bookkeeping files counted in most plans once: `scripts/check-test-inventory.sh`,
`scripts/preflight-phase12-regexes.txt`, `.planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md`.

## Title expectations

Written by plan 12-02 Task 3 (DR-06). The oracle, the spec and the expectations exist before any
guest source, so no expected value comes from the producer under test (G6). Evidence tier:
authored, software-profile policy only; no compatibility, accuracy, hardware or real-time claim.

| Item | Value |
|------|-------|
| Behavior authority | `fixtures/title/title-spec.md`, 251 lines, SHA-256 `11c8bb00569fb7da567e88546a960bcf7829cbfb23eac4a0ce285dff4611753f` (TS-1 to TS-16) |
| Oracle | `fixtures/title/title_oracle.py`, 1,055 lines, SHA-256 `e0f262685b063e8ca329c398507426ad847a3ab1d7e2d73eecee4e3616245d16`; modes `--selftest`, `--print`, `--write`, `--check`, `--emit-inputs`, `--input`, `--max-fields`; 114 selftest cases with hand-written literals |
| Committed expectation | `fixtures/title/title.expected.json`, 14,266 bytes, SHA-256 `26ec0fb8702b2fa9cad0b3671361274e52a18beb7ef43a671d4f8b76ab347de8`; `harness.title-oracle` regenerates it byte for byte and also refuses a committed input script that differs from `--emit-inputs` |
| Input scripts (SHA-256) | short `90fff08d9bebfa06f4d61bdc3394533cbd09f4f1d3e7544ab6d3b64a23cf546e`, short-wrong `8b62331d3f01a722c4e1fe3255aae5c7723d86163e214181f82d685461b2cdaf`, long `48dcc3309d4f58a31a8ea4ccb2c244d4307e6be8cfa2d354c5a6a9756f733d17`, shifted `2a87f33f2f43d7240b76cddf5de22f36485cb7bf59124939fd6e010ad7855ab1` |
| Scenario field counts (video packets, polls to Start, audio packets) | short 3, 4, 2; short-wrong 3, 4, 2; long 12, 13, 4; shifted 13, 14, 4; persist 3 packets, 3 polls, no Start, 0 audio packets (boots from the long run's save image) |
| Blank digest | SHA-256 of 49,152 zero bytes `2aae7dc846aaf25f1cadf55f1666862046c6db9d65d84bdc07fa039dac405606`; no scenario frame equals it (selftest and document invariant); the refused-frame variant expects it for every packet; blank 32,768-byte save image `c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479` |
| Distinguishable controls | the short and short-wrong frame lists first differ at frame index 1 (poll 2), the long and shifted lists first differ at index 1, the persist frame differs from a fresh-boot frame |
| Independence statement | The oracle imports only the standard library (ast walk of its own source, closed list), reads only its own source (selftest), the input scripts beside it and the constants it carries (each cites its title-spec.md section), and writes only the expectation file and the scripts of `--emit-inputs`. It reads no ROM, no core or runner source, no build output, no manifest row and no output of any other emulator, and it was written before `title.S` exists. |
| Title source revision | 7b962961fcb8e1c1600dc84dae8a67f0c88dce26 (plan 12-03 Task 3: the commit that holds the final `fixtures/title/title.S`, 557 lines, 522 non-empty, SHA-256 `4c8ffcb967571c01f26d3014df66fcbd80de3c9bc979a6e59aa46b559befb6c9`; `fixtures/title/title.ld` SHA-256 `06dc5d6959df6499b90cef612d7598acc14a5b01cf042e8ad698e0eb0b700b9f`; builder source-content SHA-256 `0946b6391a5cdd56b2ffd8d594e5c4bb3d14f14d88266befb116813f5bd56c2a`); every later pin cites this revision (authoring-once rule) |
| Built identities (build-only, never committed) | `phase12-title-s32k` ROM SHA-256 `dfe77f5856aa2069d7f44671770c336e0958d03e7ec2bca31b0ad89383a7d684`, 7,568 bytes, payload SHA-256 `ab2f79b01e4e5776e62659916c81d9170a890af2cada15c5dd88c6603da22703` (3,472 bytes); `phase12-title-refused-s32k` ROM SHA-256 `7989af8ac44028bdb56a43202ad80ae50ccb768619438e5cab113984953a0271`, 7,568 bytes, payload SHA-256 `03f57652e0091a003fef53d46559a6637e800156415bc2e2b65747de30d791bc`; completion PC `0x80000820` for both |
| Manifest rows | `phase12-title-s32k` and `phase12-title-refused-s32k` appended after the composite rows by commit 54731f4c98cb0203567708eeec1ff0bc04644432 (plan 12-03 Task 4, a commit that changes only `fixtures/manifest.json`): authored-here, cleared, build-only, GNU Binutils 2.45, profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`, `source.revision` the title source revision above, `expected.kind` spec-derived (revision `sha256:` of `title.expected.json`); `sh scripts/build-title-fixture.sh --qualify` matched both identities and `harness.rdp-oracle-authority` passed at once with no rebind (per-row pins) |

Choices made in the spec that DR-02 to DR-06 leave open (no decision row changed; an addition, not a
different value): the tone table is 512 stereo frames of a square wave of period 64 frames and
amplitude 0x2000 (two identical 2,048-byte packets per burst, table SHA-256
`a485ccdcb209d568e43bb484c18ad62ba869ea017105cf694c3539671954f363`); the transcript has a boot line
`L xx yy cccccccc` and an end line `S xx yy cccccccc pp`; the slot restore validates x <= 112 and
y <= 80; poll `p` follows the VI pending bit of field `p - 1`, so poll 1 follows the VI enable and
the frame after poll `p` is the packet at B(p); event field `j` (a stamp at the midpoint of
B(j) to B(j + 1)) is first seen by poll `j + 2`; two A press edges must be at least 3 polls apart and
the last one at least 2 polls before Start (a burst plays about 1,451,000 ticks); a run models at
most 64 polls.

Open assumption for plan 12-03 (recorded, not a measurement): the field boundaries use a NOMINAL
VI enable tick T0 = 550,000 (the open IPL3 boot costs about 530,000 ticks; the research prototype's
first packet came at about 1.59M ticks, consistent with it). Every stamp is at least 200,000 ticks
from every boundary, so the expectations hold for any real T0 within 100,000 ticks of the nominal
value. Plan 12-03 measures the real T0 and the real poll delay; a value outside the band is a change
to `title-spec.md` TS-4 first, followed by `--emit-inputs` and `--write` (reversible before the
guest source commit of the authoring-once rule). The short scenarios end near
B(3) + 60,000 = 3,738,126 ticks, below the runner guest cap of 4,000,000; a fourth field would need
B(4) = 4,720,834.

Measured by plan 12-03 (a throwaway probe against the moved installed prefix, single-stepped by
instruction count with the trace enabled; observations of the authored software profile, not a
hardware or timing claim): the VI enable (the retired `STATUS` store, equal to the first video
packet's start tick) is at tick 546,614, 3,386 ticks before the nominal 550,000 and inside the
100,000-tick band, so TS-4, the scripts and the expectations stand unchanged. Poll `p` issues its
RD64B 3,984 or 3,985 ticks after the real B(p - 1) and the SI executes the state command (samples
the controller) 10,876 ticks later, so the sampling tick is B(p - 1) + 14,860 or 14,861 (polls 1 to
8 of the long scenario), inside the 4,000 to 60,000 delay of TS-4 and 11,475 ticks after the
nominal boundary, far inside the 200,000-tick stamp margin. The short scenario completes at tick
3,693,938 (below the runner guest cap of 4,000,000), the long one at 13,078,313 and the shifted
one at 14,121,022. The probe found every checkpoint frame digest, the audio packets and stream
digest, the save digest and the transcript of short, short-wrong, long and shifted, the persist
run (the long run's exported save imported into a fresh instance) and a slot image built from the
TS-12 layout equal to `title.expected.json`, and every packet of the refused-frame ROM equal to
the blank digest with no audio, no transcript, a blank save and no completion by tick 5,000,000.
No expected value was taken from the probe; the probe is not committed.

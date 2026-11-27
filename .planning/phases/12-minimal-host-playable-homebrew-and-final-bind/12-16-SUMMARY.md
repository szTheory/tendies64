---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 16
subsystem: abi-bind-proofs
tags: [abi, bind-proofs, major-rule, version-controls, digest-sweep, act-linux-skip, gate-growth, b-34]

requires:
  - phase: 12-15
    provides: the text-settled bound tree at ABI 1.0 (bind commit 03ccb01, goldens and record block byte-identical), the carry of the B-34 relabel
  - phase: 12-14
    provides: the one coupled bind commit, the moved version controls (1.1 and 1.0.5) and the Bind steps record
provides:
  - unit.core.api.abi-major-rule, the unit case of the live create-time rule at major 1 (verification-only; passed on its first run)
  - installed.controls.version-mismatch-major (request 2.0), its measured COST, its phase12 label and its seeded wrong-headline proof
  - PIN-IMPACT "Bind steps" post-bind proofs (sweep 0 differences, oracle authority blocked, installed suite 50 of 50, full gate 353 of 353, the act-linux SKIP line, the two names' seconds) and the BIND row stub of the Gate-growth ledger
  - the B-34 test relabels (a test comment and a refusal label that named a minor below the library's)
affects: [12-17, 12-18]

actuals:
  tokens: 8034
  tasks: 3
  commits: 4
plan_head_before: effc005ac407b241ca32cd719df7a1321aded366
plan_head_after: a2321c96d01f662e35d63b615a696249978b1842

tech-stack:
  added: []
  patterns:
    - "a unit case that pins a rule whose second half is unreachable at the current library minor branches on the constant and prints NOT EXERCISED, so the case starts testing the older-minor half by itself at the first 1.x minor"
    - "a control tied to its own argument is proven able to fail by a scratch copy of the driver and consumer directory whose row expects another argument's headline; the failing log shows the real refusal text"

key-files:
  created:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-16-SUMMARY.md
  modified:
    - core/tests/test_api_surface.c
    - core/CMakeLists.txt
    - tests/installed-consumer/controls.tsv
    - tools/abi/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase12-regexes.txt
    - core/tests/test_api_struct_size.c
    - tests/installed-consumer/ops.c
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "unit.core.api.abi-major-rule uses COST 0.02 (measured 0.003 s mean, 0.004 s max over 20 runs, CTest prints 0.00 s) and installed.controls.version-mismatch-major COST 0.38 (0.37 to 0.38 s standalone over five runs, 0.31 s inside the gate); no limit widened"
  - "The phase12 label of the new control is chosen by the id suffix -major in the controls foreach, not by the literal id, so the acceptance count (one occurrence of the id per file) holds and the earlier rows keep phase11"
  - "The act Linux run is recorded as one SKIP line (Docker engine unresponsive; the act image also has no CMake), not a PASS and not silently absent; the first Linux run of the new control is the hosted gate-linux job of the 12-17 push"

patterns-established:
  - "Post-bind proofs after a one-way bind move no bound hash: the guard is the plan's second verify (bind commit against the working tree over core/include and the four goldens, no key: value line of the record changed, repo.abi-freeze-record), run before every task commit"

requirements-completed: []

coverage:
  - id: D1
    description: "The create-time rule at the bound major is pinned: 1.1, 1.65535, 2.0, 2.1, 65535.0, 0.0 and 0.1 answer ABI_MISMATCH with machine_out NULL and the allocator never called; 1.0 creates and releases every allocation; the older-minor half prints NOT EXERCISED at library minor 0"
    requirement: "API-05"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.api\\.abi-(version|major-rule)$' 2 of 2; test_api_surface --case abi-major-rule: checks=44 failures=0"
        status: pass
    human_judgment: false
  - id: D2
    description: "A request for another major is refused with the headline tied to its own argument, and the control can fail (seeded wrong headline)"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "ctest -R '^installed\\.controls\\.version-mismatch-(minor|patch|major)$' 4 of 4 (with install.setup); each log prints expected text equal to observed text for 1.1, 1.0.5 and 2.0; scratch row expecting \"1.0\" fails with 'the expected text did not occur'"
        status: pass
    human_judgment: false
  - id: D3
    description: "The bind moved no behavior and no bound hash: digest sweep 0 differences, oracle authority blocked, bound-file diffs against the bind commit empty, repo.abi-freeze-record passes"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "digest-sweep.py --base 60b9e3c3 --head HEAD: 1599 shared lines, 0 differences, 0 head-only lines; check-rdp-oracle-authority.py --expect-blocked exit 0; the plan's bound-hash guard rc=0 at each task commit and at the final head"
        status: pass
    human_judgment: false
  - id: D4
    description: "The installed-consumer suite re-runs green against the bound installed tree and the full gate passes"
    requirement: "API-07"
    verification:
      - kind: integration
        ref: "ctest -R '^(install\\.|installed\\.|api04\\.)' 50 of 50 (35 s); sh scripts/gate.sh [gate] PASS 353 of 353, ctest 90 s, total 148 s"
        status: pass
    human_judgment: false
  - id: D5
    description: "The Linux run of the three version controls is recorded, never silently absent"
    requirement: "API-06"
    verification:
      - kind: judgment
        ref: "PIN-IMPACT Bind steps carries exactly one act-linux line: SKIP, reason Docker engine unresponsive and the act image has no CMake"
        status: pass
    human_judgment: false

duration: 26min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 16: BIND Proofs Summary

**The bound tree now carries a unit case for its own create-time major rule and a control for a request of another major, and the bind is proven to have moved no behavior: the digest sweep prints 0 differences over 1599 lines, the oracle request is still blocked, the installed suite passes 50 of 50 and the full gate passes 353 of 353. No ABI byte moved and no tag was made. The Linux run of the three version controls is recorded as a SKIP, because the Docker engine did not answer and the act image has no CMake.**

## Performance

- **Duration:** about 26 min for this executor (an earlier executor was interrupted inside Task 1; its uncommitted diff was reviewed and kept)
- **Started:** 2026-11-27T01:19Z
- **Completed:** 2026-11-27T01:45Z
- **Tasks:** 3 of 3 (Task 1 is the tracer; its verify passed and expansion went ahead)
- **Files modified:** 8 outside `.planning` plus PIN-IMPACT.md and this summary

## Accomplishments

- Task 1 (tracer, verification-only): `core/tests/test_api_surface.c` has the case `abi-major-rule`. The create-info versions are built as `(major << 16) | minor`. Seven refused versions (1.1, 1.65535, 2.0, 2.1, 65535.0, 0.0, 0.1) each answer `ABI_MISMATCH`, leave `*machine_out` NULL (it starts as a non-NULL sentinel) and never call the allocator; 1.0 creates a machine and releases every allocation. The older-minor half is guarded by `T64_ABI_VERSION_MINOR > 0` and prints `NOT EXERCISED` until the first 1.x minor. `unit.core.api.abi-major-rule` is registered beside `unit.core.api.abi-version` (LABELS `unit;phase12`, COST 0.02, TIMEOUT 30), listed in `PHASE12_TESTS`, and `^unit\.core\.api\.` is raised to 14. It passed on its first run (44 checks, 0 failures): no defect of the bound API, so no post-bind row in "Host-found API defects". The implementation already existed, so this is recorded as verification-only (AUTONOMY section 2).
- Task 2: the row `version-mismatch-major` (request 2.0, expected text `compatible with requested version "2.0"`) is in `controls.tsv`, with COST 0.38 and the label `phase12` in `tools/abi/CMakeLists.txt`, in `PHASE12_TESTS`, and `^installed\.controls\.` raised from 11 to 12. The three version controls print their own argument (`"1.1"`, `"1.0.5"`, `"2.0"`) as both expected and observed text. Seeded-failure proof: a scratch copy of the driver and consumer directory under the ignored `build/` tree, whose row expects `"1.0"`, fails; first failed assertion `control version-mismatch-major: expected text: compatible with requested version "1.0"` then `observed text: not found` and `FAIL ... the expected text did not occur`. The log tail it prints shows the real refusal, `Could not find a configuration file for package "tendies64" that is compatible with requested version "2.0".` and `The version found is not compatible with the version requested.` for the 1.0.0 package. So the control can fail, and it passes only for its own argument.
- Task 3: the sweep, oracle-authority and suite results and the act line are written into PIN-IMPACT "Bind steps" together with the seeds, the seconds of the two names and the full-gate line, plus the BIND row stub of the Gate-growth ledger. The full gate passed.
- B-34 (assigned by the orchestrator, no plan owned it): the comment of `case_handshake` in `core/tests/test_api_struct_size.c` and the refusal label for `T64_ABI_VERSION - 1u` in `tests/installed-consumer/ops.c` now state the bound rule (that value is major 0, minor 65535). No expectation, bound file or golden changed; `sequence.c` ("another ABI minor", a minor above the library's) stays true and is untouched.

## Task Commits

1. **Task 1 (tracer): unit.core.api.abi-major-rule** - `6601efc5c832a8be436851587f0d8d2fee9730f2` (test)
2. **Task 2: installed.controls.version-mismatch-major** - `6cecbd2036f49007e0f13df7da825d3d5420ac06` (test)
3. **B-34 relabel (deviation 1)** - `b06a1327564a6159997f79a710f18631096bef76` (test)
4. **Task 3: PIN-IMPACT post-bind proofs, act-linux SKIP, BIND ledger stub** - `a2321c96d01f662e35d63b615a696249978b1842` (docs, `[skip ci]`)

Plan metadata: the SUMMARY, STATE and ROADMAP commit follows this file.

## The two new names

| Name | Native seconds | COST | TIMEOUT | Label | Count row |
|------|----------------|------|---------|-------|-----------|
| `unit.core.api.abi-major-rule` | 0.003 mean over 20 direct runs, 0.004 max; CTest prints 0.00 | 0.02 | 30 | `unit;phase12` | `^unit\.core\.api\.` 13 to 14 |
| `installed.controls.version-mismatch-major` | 0.37 to 0.38 standalone (five runs), 0.31 in the gate | 0.38 | 120 | `installed;controls;phase12` | `^installed\.controls\.` 11 to 12 |

`ctest -N -L phase12` lists 24 names, equal to the `PHASE12_TESTS` block (preflight ctest-counts: 23 rows, 0 mismatches). The inventory prints `test inventory PASS preset=gate count=353`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Orchestrator assignment] B-34 test relabels**
- **Found during:** the 12-15 handoff ("not owned by any plan; the orchestrator can add it to 12-16") and the starting state of this run (an earlier executor had made the edits uncommitted)
- **Issue:** a test comment and a refusal label named "the minor version one below the library's", a minor that no longer exists at ABI 1.0.
- **Fix:** relabeled in their own commit, separate from the two task commits, so the plan's task commits stay as written. Touches no bound file, no golden and no expectation; `installed.consumer.build` and `installed.consumer.ops-handshake` and `unit.core.api.struct-size` pass after the edit.
- **Files modified:** `core/tests/test_api_struct_size.c`, `tests/installed-consumer/ops.c`
- **Commit:** b06a132

**2. [Rule 3 - Blocking] The control label and the acceptance count**
- **Found during:** Task 2
- **Issue:** the controls foreach in `tools/abi/CMakeLists.txt` gave every control the label `phase11`, but the plan (and the preflight label comparison with `PHASE12_TESTS`) needs `phase12` on the new name. Keying the label on the literal id would put the id on a second line of that file and break the acceptance count (`grep -c` prints 1 per file).
- **Fix:** the label is chosen by the id suffix: ids matching `-major$` get `installed;controls;phase12`, every earlier row keeps `phase11`. A comment says so.
- **Files modified:** `tools/abi/CMakeLists.txt`
- **Commit:** 6cecbd2

**3. [Rule 3 - Blocking] The plan's inventory command**
- **Found during:** Task 1 and Task 2 verify
- **Issue:** the verify commands run `sh scripts/check-test-inventory.sh` with no argument, which prints its usage and exits 2.
- **Fix:** run as `sh scripts/check-test-inventory.sh --preset gate` (exit 0, count 353). No script change.

### Environmental findings (not plan defects)

- **The act Linux run could not be done.** The Docker engine did not answer: five bounded `_ping` requests to its socket timed out after 8 s over 20 minutes, `docker info` and `docker image ls` hung for more than 10 minutes, while the CLI reports its version (another project's `docker run` was in flight in the same engine at one point). No engine was started or restarted. The Phase 11 record of the act image also says it has no CMake and no Ninja, so the controls could not have run there anyway. PIN-IMPACT carries the single line `- act-linux: SKIP: ...`. The same cause made the preflight step `linux-compile` fail with `docker is unavailable and a C change exists`: every other preflight step is ok. The preflight was repeated once with a stand-in `docker` that exits 1 at once, because the script's own `docker info` hung; that changes the wait, not the verdict. The Linux compile of `core/tests/test_api_surface.c` (public header and shared test header only) therefore has no local Linux evidence; the hosted gate-linux job of the 12-17 push compiles it with gcc 13.3.
- **The first full gate failed one name.** `gov.rights.repo` (352 of 353) reported R-NUL-HEAD and R-INVALID-UTF8 on four untracked `.DS_Store` files that macOS Finder wrote into the working tree at 20:28 (root, `.planning`, `tests`, `tools`); `rights-scan.sh` lists untracked non-ignored files and a hosted checkout has none. `.DS_Store` was added to the clone-local `.git/info/exclude` (no tracked file, no `.gitignore` change; to undo, delete that one line), the name passed alone and the full gate then passed 353 of 353 (ctest 90 s, total 148 s, host load 7 to 9; the `LastTest.log` of each run was copied under `build/` first). The `.DS_Store` files themselves were left in place.
- **Leftover process.** A `docker image ls` started by this run is still blocked on the engine (the attempt to end it was refused by the permission classifier and I did not retry). It holds nothing and exits when the engine answers or can be ended by hand.

### Other notes

- **Branch:** commits are on `main`, as in every earlier sequential plan. Nothing was pushed, no tag was made (`git tag --points-at HEAD` prints nothing), `project(VERSION)` is untouched.
- **COST 0.02 of the unit name** is a rounded-up measured figure (0.003 s mean), not an estimate; the named neighbour `unit.core.api.abi-version` keeps its COST 1.
- **Plan files not named in the frontmatter list:** `core/tests/test_api_struct_size.c` and `tests/installed-consumer/ops.c` (deviation 1).

## Authentication Gates

None.

## Issues Encountered

- Host load was 5 to 12 from unrelated processes (Docker Desktop and another project's Linux gate); no limit was widened and a native run was started only when the one-minute load was at most about 8.
- The plan's second verify of Tasks 1 and 2 (bound-hash guard) printed 0 differences each time and `repo.abi-freeze-record` passed.

## Verification Results

| Check | Result |
|-------|--------|
| Task 1 verify 1 (`cmake --build --preset gate`, `unit.core.api.abi-(version\|major-rule)`, inventory) | 2 of 2; inventory PASS count 353 (with `--preset gate`) |
| Task 1 acceptance (`ctest -N -R '^unit\.core\.api\.'`) | Total Tests: 14 |
| Task 1 case run directly | `checks=44 failures=0`, the NOT EXERCISED line printed |
| Task 2 verify 1 (`installed.controls.version-mismatch-(minor\|patch\|major)` with `install.setup`) | 4 of 4, each log shows expected equals observed |
| Task 2 acceptance (`grep -c version-mismatch-major` per file) | controls.tsv 1, tools/abi/CMakeLists.txt 1 |
| Seeded wrong headline (scratch copy, never committed) | exit 1: `the expected text did not occur`; log shows `compatible with requested version "2.0"` |
| Bound-hash guard (plan verify 2) at each task commit and at the final head | rc 0: 0 bound-file differences, no record key line changed, `repo.abi-freeze-record` passes |
| Digest sweep `--base 60b9e3c3... --head HEAD` (62 s) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` |
| `check-rdp-oracle-authority.py --expect-blocked` | exit 0, `authority record valid; blocked: no oracle operation is authorized` |
| Installed suite `^(install\.\|installed\.\|api04\.)` | 50 of 50 (35 s) |
| act Linux run | `- act-linux: SKIP: ...` (exactly one line in PIN-IMPACT) |
| `sh scripts/gate.sh` (second run) | `[gate] PASS`, ctest 353 of 353 in 90 s, total 148 s (first run 352 of 353, `.DS_Store`, see above) |
| `sh scripts/preflight-phase12.sh --no-replay` | all steps ok except `linux-compile` (Docker engine unresponsive) |
| `sh scripts/pii-scan.sh --tree`; `sh scripts/rights-scan.sh` | 0 findings; 0 findings |
| Doc readers after the PIN-IMPACT edit (`installed.consumer.literals`, `repo.api07-evaluation`, `repo.installed-case-map`, `repo.stale-freeze*`, `repo.abi-freeze*`, `repo.host*`, `repo.docs*`, `gov.pii*`, `gov.rights*`) | 20 of 20 |
| `git tag --points-at HEAD` | empty |
| Post-commit deletion check | no file deleted by any task commit |

## Handoff (12-17 and the orchestrator)

- `BIND_COMMIT` stays `03ccb01c28179522d357d866c1e21aebfade7b0a`; the bound-hash guard prints 0 differences at this plan's head. The qualifier (12-17) binds a tree that carries both new names.
- The first hosted run of `unit.core.api.abi-major-rule`, `installed.controls.version-mismatch-major`, the relabeled consumer, and the first Linux compile of the changed test is the push of 12-17. If gate-linux (CMake 3.31) shows the new control red, that is a finding for a numbered plan; the expected text is the headline both CMake majors print (11-28).
- The act Linux run of the three version controls is not done: if the orchestrator wants it before the push, it needs a working Docker engine and an image with CMake (acquiring one needs the package and provenance audit; do not start heavy services without that decision). The preflight `linux-compile` step fails the same way until the engine answers: run it again once Docker is back (`--no-replay` is enough).
- `docs/spec/api.md` section 2.3 still says the minor and patch controls "run the first two" requests; with the new control the third (another major, 2.0) is also exercised. The sentence is not wrong; an optional one-line addition is outside this plan's file list and is left for a text plan.
- The comment of the non-installed `t64_facade.h` still calls the public header a "0.x freeze candidate" (carried from 12-15; needs a plan that may edit `core/include`).
- Stray `.DS_Store` files appear in the working tree whenever Finder windows of the repository are open. The clone-local `.git/info/exclude` line keeps `gov.rights.repo` from failing locally; delete the line to undo it.
- BIND ledger row: the hosted columns stay empty until 12-17's push; the HR-04 projection of 12-12 stands (these two names add about 1.6 s hosted).

## Known Stubs

None.

## Threat Flags

None. T-12-56 holds: the case passed with no edit of the header, the record or a golden. T-12-57 holds: the act line is exactly one SKIP with a reason. T-12-58 holds: the 2.0 control's headline is tied to its own argument and the seeded wrong-headline copy fails.

## Self-Check: PASSED

Created files and the four commits above exist (checked with `git merge-base --is-ancestor`); `abi-major-rule` and `version-mismatch-major` are present in their files.

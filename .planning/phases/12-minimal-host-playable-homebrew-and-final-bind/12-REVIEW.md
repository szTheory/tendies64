# Phase 12 independent review record

Independent reviews of the Phase 12 waves (AUTONOMY v1.2: review per wave in a scratch clone, plus
the final audit). Each section is written by its review plan: "TITLE" by 12-06 (the W0 plan 12-01
with its Wave 0 checklist and the TITLE wave 12-02 to 12-05), "HOST" by 12-12 and "FINAL" by 12-18.
The reviewer fixes nothing: a finding gets a severity (blocking stops the plan before the push,
warning is a real gap that does not block the push, info is a recorded observation or accepted
limit) and a disposition; a fix beyond the plan's own list is a new numbered plan appended after
12-18. Evidence tier: unit-tested, authored scope; a software-interface claim only, with no
compatibility, accuracy, hardware, real-time, latency or throughput claim. Logs stay under the
ignored `build/12-review/`; only sanitized lines are quoted here.

## TITLE (12-01 Wave 0 checklist and 12-02 to 12-05)

| Item | Value |
|------|-------|
| Wave 0 base | `60b9e3c34fc0efc0ff64296cfa31415565b847b2` (extracted from PIN-IMPACT.md "Wave bases" with the base idiom: exactly one 40-hex line, an existing commit, an ancestor of the head) |
| Reviewed head | `9ea70f769ca90f69206eeb1cf5db1660bfde9272` (28 commits after the Wave 0 base; 37 commits after `origin/main` `dc247fa`, the Phase 11 closeout and Phase 12 planning commits included; 29 files changed outside `.planning/`) |
| Host | Darwin arm64, Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2; 1-minute load 6.8 at the start of the scratch gate and 12 to 17 at its end (unrelated processes on the same machine: a virtual machine and a desktop application), so the seconds are upper bounds for a quiet host; no limit was widened |

### Task 1: scratch gate and preflight of the W0 and TITLE head

Method (the 11-21 form): `git clone --no-checkout` of the main repository into
`build/12-review/title/src`, `git checkout --detach 9ea70f769ca90f69206eeb1cf5db1660bfde9272`, and a copy
(not a symlink) of `build/tools/install` into the clone's `build/tools/install`. `git archive` of the head
in the clone and in the main repository have the same SHA-256
(`4d2d594c516c12d6f6cda3af905ca0991b4735b0281f5de23ef96b96a1fa2738`), so the gated tree is exactly the
committed head; `git status --short` of the clone is empty before and after the gate. The clone's own
`build/gate` holds the title fixtures, the install fixture (`install.setup`), the consumer builds and the
control scratch directories (its CMake home directory is the clone); the gate output was written outside the
clone tree (`build/12-review/title/gate.out`, ignored, not committed).

Command: `sh scripts/gate.sh` inside `build/12-review/title/src`, default environment (no `CI` variable).

Result: `[gate] PASS`, `[gate] total 236s`, exit 0. Steps in seconds: sh-n 1 (40 scripts), lint-selftest 8,
fixture-qualification 2, configure 0, build 22, boot-evidence-selftest 19, test-inventory-selftest 48,
test-inventory 1, ctest 124 (`100% tests passed out of 336`, `Total Test time (real) = 124.52 sec`),
core-lint 11. No name was excluded and no limit was widened. Phase 12 names: `ctest -N -L phase12 -FA .`
lists 7 (`harness.title-oracle`, `runner.title-two-run`, `controls.real.title-wrong-input`,
`controls.real.title-refused-frame`, `installed.consumer.title`, `installed.consumer.title-persist`,
`installed.consumer.title-refused`), equal to the `PHASE12_TESTS` block; the preset has 336 names, 7 more
than the 329 of the Phase 11 close. Their seconds in this gate run (parallel, loaded host): 3.44, 3.01, 0.51
(the three consumer names), 0.70, 0.44, 0.52 (the runner name and the two controls) and 0.10 (the oracle),
8.72 s summed.

Preflight: `sh scripts/preflight-phase12.sh --no-replay` in the clone, `[preflight] PASS`, total 51 s:
oracle-authority (blocked as required), ctest-counts 23 rows 0 mismatches, `phase12 label: 7 tests carry the
label phase12, matching the PHASE12_TESTS block`, abi-cheap 16 names 5 s, host-cheap 1 name
(`harness.title-oracle`), guards (check-api-init.py, check-consumer-includes.sh, check-stale-freeze-text.sh
passed; check-host-includes.sh not yet present, as expected before plan 12-07), host-load WARN at 9.3,
head-ci `origin/main` `dc247fa` completed success, timeout-audit 189 TIMEOUTs 0 too low (lowest ratio 2.9),
pii (1483 tree files, 37 commits, 0 findings), linux-compile 234 units with the act image cc 13.3.0 (42 s;
first C-relevant path `core/CMakeLists.txt`), post-gate-replay skipped by `--no-replay`. A first attempt
in the same clone exited 1 at the pii step (`RULE P-ZERO-SCANNED history`, 0 commits) and printed
`head-ci: gh unavailable`: a clone of the local repository has the source repository as `origin`, so
`origin/main` equals the head and the range `origin/main..HEAD` is empty. The clone's fetch URL was set to
the public repository (push URL disabled) and `origin/main` fetched (`dc247fa`); the rerun passed. This is a
review-procedure fact, not a defect of the preflight (finding T-05).

### Task 2: Wave 0 checklist (w1) to (w5) and TITLE checklist (a) to (k)

`BASE` is the Wave 0 base `60b9e3c34fc0` and `HEAD` the reviewed head `9ea70f769ca9` for every check, run inside
the clone and read from its logs, not from the plan summaries. Logs are under the ignored
`build/12-review/title/review/`; the reviewer's independent literal reader is a scratch file outside the
repository and is not committed.

| Check | Command or method | Result |
|-------|-------------------|--------|
| (w1) base and planning order (I6) | `git rev-list --reverse BASE..HEAD`, the parent of its first commit; `git cat-file -e BASE:<phase dir>/12-VALIDATION.md` and the same for `12-CONTEXT.md`, `12-RESEARCH.md`, `12-01-PLAN.md`, `12-18-PLAN.md`; `git merge-base --is-ancestor BASE HEAD` | 28 commits; the first is `6291262` (12-01 Task 1, "Phase 12 preflight tracer, regex count table and Wave 0 base") and its parent is the base; every planning artifact is in the base commit; the base is an ancestor of the head |
| (w2) preflight deltas | `diff scripts/preflight-phase11.sh scripts/preflight-phase12.sh`; `git log BASE..HEAD -- scripts/preflight-phase12.sh` | the differences are the 12-01 Task 1 deltas only (header text and usage name, `PF_WORK=build/preflight-phase12`, the default regex file with the `T64_PREFLIGHT_REGEXES` override kept, the label step on `PHASE12_TESTS` with its "no block yet" pass, `check-host-includes.sh` in the guards step with its "not yet present" line, the `host-cheap` step and its call in the step order) plus one later delta: `ctest -N -L phase12 -FA .` in the label step, added by 12-05 (commit `e19471f`, its recorded Rule 3 fix: the fixture closure of the consumer names made the label count 9 against the block's 7). No step of the Phase 11 copy changed behavior (finding T-04) |
| (w3) regex count rows | the preflight `ctest-counts` step in the clone (each row against `ctest -N -R` at the head); the comments above the zero rows | 23 rows, 0 mismatches; the five zero rows left (`^host\.pump\.`, `^repo\.host-`, `^repo\.api07-evaluation$`, `^repo\.p12-mutant-anchors$`, `^harness\.phase12-qualification$`) each name their owning plan (12-07 to 12-08, 12-07 to 12-09, 12-10, 12-08, 12-17) and the fixture closure the count will include; the TITLE rows were raised by their owners (`^harness\.title-oracle$` 1, `^runner\.title-two-run$` 1, `^controls\.real\.title-` 2, `^installed\.consumer\.title` 5 with `installed.consumer.build` and `install.setup`, `^installed\.consumer\.` 31) |
| (w4) PIN-IMPACT sections | `grep -n '^## '` and row counts of PIN-IMPACT.md | Wave bases, Registration map (18 `R-` rows, 2 `O-` rows), Bind pin-site map (35 `B-` rows, the measured counts and the multi-line aware proof commands 1 to 6), API-07 pin, Decision record (34 `DR-` rows, DR-01 to DR-34; DR-34 added by 12-04 under orchestrator decision B), Carry-in triage (11 rows), Host-found API defects (empty: "none at Wave 0"; no TITLE plan added a row, so plan 12-11 has no trigger yet), Owner questions ("none", two owner-review items), Gate-growth ledger (baseline, projection and the TITLE stub), Plan file-count exceptions (8 rows), and the later "Title expectations" section of 12-02 to 12-05 all present. The plan's second Task 2 verify body counts 33 DR rows; it exits 1 as written and 0 with 34 (finding T-06) |
| (w5) API-07 pin | the section text between "## API-07 pin" and "## Decision record" | one 40-hex SHA (`4177a43e67312e2db88ab21cba13aa7350063105`), "local, unpublished SHA", no filesystem path (a grep for home, temporary and drive paths and for `~/` finds none) |
| (a) oracle independence and order | `git log --diff-filter=A` of `title_oracle.py`, `title-spec.md`, `title.expected.json` and `title.S`; `git merge-base --is-ancestor`; `git log 8d6d330..HEAD` over the oracle files; an `ast` walk of `title_oracle.py` (`python3 -I`) | the three oracle files were added by `8090e92` (12-02 Task 1), an ancestor of `8d6d330`, the first commit that adds `title.S` (12-03 Task 1); no oracle file, input script or expectation changed after `8d6d330`. Imports: `__future__`, `argparse`, `ast`, `fractions`, `hashlib`, `io`, `json`, `math`, `os`, `re`, `sys`, all in `sys.stdlib_module_names`. `open()` sites: its own source (selftest, twice), the committed input scripts beside it (`input_path`, `HERE`), the `--input` script of `--print`, the `--check` file, and the two writers (`--write`, `--emit-inputs`); no string constant names `build/`, a `.z64`, an ELF, a payload, `core/`, `runner/` or the manifest (the one hit is the docstring saying it reads none). The content SHA-256 of spec, oracle and expectation equal the PIN-IMPACT "Title expectations" row |
| (b) scope cap | `grep -c . fixtures/title/title.S`; greps for `eret`, `syscall`, `0x80000180`, `tlbw`, `tlbp`, `tlbr`, `entryhi`, `entrylo`, `pagemask`, `wired`, `mtc0`, `mfc0`, SP register bases; a read of the whole source and its RDP list | 522 non-empty lines (557 total) of at most 600; 0 hits for every exception, TLB and RSP pattern; the one `mfc0`/`mtc0` pair clears Status.IE at entry; the RDP list is SetColorImage, SetScissor, SetOtherModes (fill, or one-cycle under `T64_TITLE_REFUSED`), two SetFillColor and FillRectangle pairs and SyncFull: no Z image, texture image, tile or texture rectangle command. The frame lock is `wait_vi`, a poll of MI_INTERRUPT bit 0x08 cleared by a VI_CURRENT write; the other loops poll SI, PI and DP status bits; no instruction-count delay. The title source and linker script SHA-256 equal the frozen revision `7b96296` (no change after it) |
| (c) manifest rows and rights | `fixtures/manifest.json` rows of the two stems; `git show --stat 54731f4`; `git ls-files` for `*.z64`, `*.n64`, `*.v64`, `*.payload.bin`, `*.elf`, `*.bin`, `build/*`; `sh scripts/rights-scan.sh` | both rows: `source_rights` authored-here, `binary_rights` cleared, `distribution` build-only, toolchain GNU Binutils 2.45, profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` (the composite rows' profile, so title and RUN-07 fixture share an identity), `source.path` `fixtures/title/title.S` at revision `7b962961fcb8e1c1600dc84dae8a67f0c88dce26` (whose tree holds `title.S`), `expected.kind` spec-derived with the revision `sha256:26ec0fb8...` of `title.expected.json`; `54731f4` changes only `fixtures/manifest.json`; no ROM, payload, ELF or build output is tracked; `rights-scan: tree=1483 history=5414 findings=0` |
| (d) runner name and controls, log by log | `ctest --test-dir build/gate --no-tests=error -R '^(runner\.title-two-run\|controls\.real\.title-)' -V` (3 names, all pass, 0.70, 0.47 and 0.45 s) | `runner.title-two-run`: `state_digest raw-run=57b683df... guest_tick=3693938`, three `compare video: checkpoint N equal` lines (49,152 bytes each; checkpoints 1 and 3 share `8227044b...`, the start and the reset position), the seeded `control video` line `checkpoint 2 mismatch: expected 548e2116..., actual 548e2016...`, audio 2 packets equal and its control (first difference at packet index 1), transcript 20 bytes equal and its control (offset 10), the pins line labeled `runner-recorded pin, not independent`, and the two DR-34 exclusion lines: `a run without raw capture ends harness-error/video-drain-error (stop host-error) at tick 1589376, the drain of the first 49152-byte video packet` and `raw capture with the save-out producer is refused as harness-error/usage at tick 0`. `controls.real.title-wrong-input`: `checkpoint 2 mismatch: expected 548e2016..., actual e063ea0a...` (not the blank digest) under the short expectation, and the same 3 frames, audio and transcript equal the short-wrong expectation, so it fails for its own reason. `controls.real.title-refused-frame`: `refused-frame limit holds: capture-error/host-error at tick 4000000, no transcript, no bundle (harness-error, capture-error)`, `0 transcript bytes`, the limit line naming the consumer case, and the six same-setup controls, each refused with its own text: completion (`oracle completes=0, run completed=1`), outcome (`the run passed (a refused frame must end non-pass)`), detail (`observed video-drain-error/host-error`), stop tick (`observed ... at tick 3999999`), transcript (`expected 0 bytes, captured 20`), bundle (`a raw-capture bundle was published`). Every control fails, or holds, for its own stated reason |
| (e) consumer cases | `ctest ... -R '^installed\.consumer\.title' -V` (5 names with `install.setup` and `installed.consumer.build`, all pass; title 1.85 s, title-persist 1.82 s, title-refused 0.47 s); `sh scripts/check-consumer-includes.sh`; `python3 -B scripts/check-consumer-literals.py --check`; the reviewer's own reader (`python3 -I`, no import of the checker) of `title.expected.json`, the pins file and the text of `t64c_title.h` split at the RECORDED PINS banner | `title: checks=99 failures=0`: short (tick 3693938, 3 frames, every checkpoint, audio, transcript and the 32,768-byte save export equal, no packet blank), the four runner-recorded pins equal (so the consumer's final digest equals the runner's raw-capture digest), long (13,078,313, 12 checkpoints, 4 audio packets, transcript, save) with the four consumer-recorded pins, and `shifted differs at checkpoint 1`. `title-persist: checks=60 failures=0`: the long save export imported into a fresh instance gives 3 frames, 17 transcript bytes (`L 3c 2c 00000002`), no audio and the unchanged save; the no-import control shows one fresh-boot frame, which is not the restored frame. `title-refused: checks=25 failures=0`: blank digests recomputed from zero bytes, 4 packets each printed `blank`, 0 audio, empty transcript, no completion, the run ends at the bound 5,000,000, save export equal to the blank save digest. `consumer-includes: files=19 findings=0`; `title.c` includes only C headers, `<tendies64/t64_machine.h>` and its own headers. Literals `findings=0`. Own reader `checks=81 failures=0`: every frame list, audio packet list and stream digest, transcript text and SHA-256 (recomputed) and save digest of short, long, shifted and persist equal the INDEPENDENT section; every audio packet equals the TS-8 tone table rebuilt from the spec text, and each stream digest equals that table repeated; each save digest equals the image rebuilt from its nonzero bytes; the blank digests equal SHA-256 of zero bytes; no accepted frame is blank; the four recorded values are absent from the INDEPENDENT section and present in the RECORDED one, and `title.expected.json` holds none of them |
| (f) pin labels | the pins file; the case output; a grep of the phase records, the title fixtures, the consumer and the runner test for each recorded value next to the word independent | `label` `runner-recorded pin, not independent` (top level, source "plan 12-04 runner.title-two-run, supervised run") and `consumer.label` `consumer-recorded pin, not independent` (source "plan 12-05 installed.consumer.title"); every case line reporting a pinned value carries its label (check (d) and (e)); no record calls a recorded value independent. The file holds the runner record at the top level and the consumer record nested, not two sibling sections as DR-07 words it (finding T-07) |
| (g) digest sweep | `python3 -B scripts/digest-sweep.py --base BASE --head HEAD` (Task 2 verify 1, after the base idiom) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, exit 0, 46 s: the core did not move in W0 and TITLE |
| (h) authority still blocked | `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked`; `--require-approved`; `git diff --name-only BASE..HEAD` over the checker, its record, the 13 corpus files and `docs/spec/phase09-cases.json` | exit 0 `authority record valid; blocked: no oracle operation is authorized`; exit 2; the diff is empty |
| (i) CI, gate, header and goldens unchanged | `git diff --name-only BASE..HEAD -- .github scripts/check-ci-yml.sh scripts/gate.sh core/include docs/abi` | empty. No `core` source changed either (only `core/CMakeLists.txt`, the title fixture target); the one production runner edit is the two rows of `runner/t64_real_fixture.c` |
| (j) PII over the range | `sh scripts/pii-scan.sh --range BASE..HEAD` | `pii-scan: tree_files=skipped commits=28 findings=0` |
| (k) COST on every phase12 name | `ctest --test-dir build/gate -L phase12 --show-only=json-v1` | 7 `phase12` names (9 listed with the fixture closure `install.setup` and `installed.consumer.build`, both `phase11`), every one with `COST` (0.09, 0.96, 0.62, 0.72, 2.72, 2.55, 0.69; sum 8.35) and a literal `TIMEOUT` (60 for the oracle and the consumer names, 120 for the runner name and the controls), each at least 3.5 times its largest native figure and twice its COST |

Task 2 verify (in the clone): (1) the base idiom and the digest sweep, exit 0 (check (g)); (2) as written
exits 1 at its last test (33 expected, 34 found), and with 34 exits 0 (w1 and w4 hold; T-06); (3)
`ctest --no-tests=error -R '^(harness\.title-oracle|runner\.title-two-run|controls\.real\.title-|installed\.consumer\.title)' --output-on-failure`
`100% tests passed out of 9`, `consumer-includes: files=19 findings=0`, `rights-scan: tree=1483 history=5414 findings=0`.

#### Decision-by-decision read

| Decision (CONTEXT, with its DR row) | Where implemented | Test names | Verdict |
|-------------------------------------|-------------------|------------|---------|
| Route A (DR-01, DR-33): an authored assembly title with the pinned Binutils 2.45 and the pinned open IPL3 through the fixture-builder pattern, no new toolchain, no committed ROM, rows authored-here / cleared / build-only; route B a non-goal | `fixtures/title/title.S`, `title.ld`, `scripts/build-title-fixture.sh`, the `t64-phase12-title-fixture` target, the two manifest rows (commit `54731f4`) | `harness.rdp-oracle-authority`, `gov.manifest.*`, `gov.rights.*` (gate) | implemented; checks (b), (c); no libdragon file, toolchain or row exists |
| Scope cap (DR-04): one screen, one 16x16 square by D-pad or stick, one tone burst on A, one SRAM slot, B resets, polled VI frame lock, at most about 600 lines, no Z, texture, RSP microcode, TLB or libultra | `title.S` (TS-3 to TS-13 blocks) | the title names via the consumer and runner | implemented; check (b) |
| Profile identity (DR-01): the composite profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` shared with the RUN-07 fixture | manifest rows, `runner/t64_real_fixture.c` rows (profile and its SHA-256 `9dfb90d8...` equal the composite rows) | `runner.title-two-run` (`profile_id` checked), the consumer cases | implemented |
| Builder copy (DR-01, research contradiction 2): a copy of `scripts/build-composite-fixture.sh` | `scripts/build-title-fixture.sh` | `sh scripts/build-title-fixture.sh --qualify` (12-03), the fixture target in the gate | implemented: a diff with the stems renamed shows only the provenance text, stems, comment, completion symbol, schema string, staging name, defsym and messages |
| Checkpoint digests (DR-05, DR-06): spec-derived oracle written first, checkpoint frame digests per scenario, not long raw captures | `fixtures/title/title_oracle.py`, `title.expected.json`, the four input scripts | `harness.title-oracle` | implemented; check (a) |
| Recorded pins (DR-07): runner-recorded and consumer-recorded pins with exact labels, never independent | `fixtures/title/title.runner-pins.json`, the RECORDED PINS section of `t64c_title.h` | `runner.title-two-run`, `installed.consumer.title`, `installed.consumer.literals` | implemented; checks (e), (f); file shape differs from the DR-07 wording (T-07) |
| Controls (DR-08, DR-34): a wrong-input control and a refused-packet control, a blank screen distinguishable | `runner/tests/test_title_two_run.c` cases `wrong-input`, `refused-frame`; `tests/installed-consumer/title.c` case `title-refused` and the no-import control of `title-persist` | `controls.real.title-wrong-input`, `controls.real.title-refused-frame`, `installed.consumer.title-refused`, `installed.consumer.title-persist` | implemented; checks (d), (e). Under DR-34 the runner shows only the refused run's limit; the blank-frame assertion is the consumer's |
| Runner scope (DR-34, orchestrator decision B): rows only, one raw-capture run with recorded exclusions, save equality and the cross-run digest in the consumer | `runner/t64_real_fixture.c` (two rows), `test_title_two_run.c` `check_exclusions` | `runner.title-two-run`, `installed.consumer.title` | implemented; both exclusions are executable and fail when a limit is lifted |
| Consumer cases and per-case ROM (DR-09) | `tools/abi/CMakeLists.txt` `T64_CONSUMER_ROM_<case>`, `cases.txt`, `main.c`, `t64c_cases.h`, `title.c`, `t64c_title.h`; the sanitize lane's case statement in `scripts/check-install.sh` | the three `installed.consumer.title*` names | implemented; the ROM mapping exists in two places (T-02) |
| G4 position (DR-26, DR-27): new names inside the gate with COST and TIMEOUT, no `ci.yml`, `check-ci-yml.sh` or `gate.sh` edit | registrations in `runner/CMakeLists.txt` and `tools/abi/CMakeLists.txt`; `PHASE12_TESTS` | the 7 names | implemented; checks (i), (k); hosted cost in Task 3 |

#### Findings

Severity scale as for Phase 11: blocking (stops the plan before the push), warning (a real gap that does not
block the push), info (a recorded observation or accepted limit). The reviewer fixes nothing.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| T-01 | info | `tools/abi/CMakeLists.txt` (`T64_CONSUMER_COST_title-persist 2.55`); `tests/installed-consumer/title.c` `t64c_case_title_persist` | `installed.consumer.title-persist` costs 2.55 s native (3.01 s in this loaded scratch gate), 1.65 s above the 0.9 s projection, because the real save export needs the whole long run of 13,078,313 ticks; an image built from the literals would cost about 0.7 s but would no longer be an export of a run. It is the named first candidate of the Phase 12 G4 reversal rule | accepted for now: the round trip is the evidence the case exists for; re-judged in Task 3 against the hosted numbers and at 12-12 and 12-18 (the rule moves it to a local lane first if the cold margin falls under 120 s) |
| T-02 | info | `tools/abi/CMakeLists.txt` `T64_CONSUMER_ROM_title`, `_title-persist`, `_title-refused`; `scripts/check-install.sh` `cmd_sanitize` case statement (lines 807-814) | The per-case ROM mapping exists twice: the CMake variables of the CTest names and a case statement of the local sanitize lane, which also ignores the lane's `--rom PATH` for the three title cases. A drift fails loudly (a title case given the composite ROM fails every checkpoint with its own text, and a missing ROM stops the lane with a message), the lane is local only, and the label is chosen by the name prefix `title` | accepted (no plan now): if a later plan adds a consumer case with its own ROM, it replaces both copies by a ROM column of `cases.txt` read by CMake and the lane (`scripts/check-installed-case-map.py` and the fresh-archive lane would follow) |
| T-03 | info | `tests/installed-consumer/title.c:49-50` | `title-refused` uses two hand-chosen constants, the bound 5,000,000 and a floor of 3 video packets (4 observed); the oracle's `refused` section gives the digest of each frame, not a count, so "every packet is blank" ranges over the observed packets with that floor | accepted: the floor keeps a run with no packets from passing, and each observed packet is compared with the blank digest |
| T-04 | info | `scripts/preflight-phase12.sh` label step (`-FA .`) | Wave 0 check (w2): beyond the 12-01 deltas the copy carries one later delta, the 12-05 Rule 3 fix that keeps the fixture closure out of the label count | accepted: recorded in the 12-05 SUMMARY; the `-R` rows keep counting the closure on purpose |
| T-05 | info | review procedure (`build/12-review/<wave>/src`) | In a clone of the local repository `origin` is the source repository, so the preflight's pii step scans `origin/main..HEAD` = 0 commits (`P-ZERO-SCANNED`) and `head-ci` reports `gh unavailable` | procedure note for the 12-12 and 12-18 reviews: set the clone's fetch URL to the public repository, disable its push URL and fetch `+main:refs/remotes/origin/main` before the preflight |
| T-06 | info | `12-06-PLAN.md` Task 2, second automated verify (`-eq 33`) | The verify body still counts 33 DR rows; DR-34 (decision B) amended the must-have and the action text of this plan to 34 rows but not that number. The body exits 1 as written and 0 with 34 | plan-text defect only, no later plan counts DR rows; recorded as a deviation of 12-06; the check is satisfied with 34 |
| T-07 | info | `fixtures/title/title.runner-pins.json`; PIN-IMPACT DR-07 | DR-07 describes two sections `runner` and `consumer`; the file holds the runner record at the top level (as the runner test and the 12-04 acceptance read it) and the consumer record as the nested object `consumer` (12-05 SUMMARY, choice 3). The labels and values are right; only the wording of DR-07 differs from the file | accepted: plan 12-08 reads the file's keys (its read_first) and must take the consumer values from `consumer`; no edit of DR-07 by the reviewer |
| T-08 | info | `runner/CMakeLists.txt` `runner.title-two-run` | The name says two runs, the case makes one raw-capture run plus the two recorded exclusion invocations (DR-34 item 1; DR-27 spelling kept) | accepted (DR-34; the reversal path names the moment the second run returns) |

Result before the push: zero blocking findings, zero warnings and eight info rows. The W0 records and the
TITLE wave are independently confirmed check by check and control by control, and the wave moved no digest
(check (g)).

### Task 3: push, hosted pair, TITLE ledger row and the Host base

Before the push, in the shared tree with a clean committed tree at `884153ac143eb04f167a27aa57bdd7faf67605b7`
(the reviewed head plus the two 12-REVIEW.md commits of Tasks 1 and 2):

- `sh scripts/gate.sh`: `[gate] PASS`, 336 of 336, ctest 102 s, total 173 s (load about 10; no limit widened).
- `sh scripts/preflight-phase12.sh` (full, with replay): `[preflight] PASS` in 68 s (oracle-authority, ctest-counts
  23 rows 0 mismatches, phase12 label 7 equal to the block, abi-cheap 16 names, host-cheap 1 name, guards,
  host-load WARN at 10.2, head-ci `dc247fa` completed success, timeout-audit 189 TIMEOUTs 0 too low with lowest
  ratio 2.9, pii, linux-compile 234 units with the act image cc 13.3.0 in 40 s, post-gate-replay 11 `ci.yml`
  steps in 19 s).
- `sh scripts/pii-scan.sh --tree` (1484 files) and `--range origin/main..HEAD` (39 commits): 0 findings;
  `sh scripts/rights-scan.sh --tree --range origin/main..HEAD`: `tree=1484 history=154 findings=0`.

Push: `git push origin 884153ac143eb04f167a27aa57bdd7faf67605b7:refs/heads/main`, a fast-forward from `dc247fa` (no
force, no tag, no other branch; `git tag --points-at` is empty). Hosted run 45887012740 (`ci.yml`, event push; Binutils
cache hit on both jobs with key `69d0bfd4`, so a warm sample; no rerun): conclusion success.

- gate-macos success, job 582 s (Gate 508 s, ctest 323 s, `100% tests passed out of 336`, test-inventory-selftest
  76 s, build 34 s, boot-evidence-selftest 33 s, core-lint 24 s; cmake 4.5.0, AppleClang 21.0.0).
- gate-linux success, job 263 s (Gate 222 s, ctest 149 s, `100% tests passed, 0 tests failed out of 336`; cmake
  3.31.6, GNU 13.3.0).
- The seven TITLE names, first hosted run, macOS then Linux: `installed.consumer.title` 8.30 and 5.98 s,
  `installed.consumer.title-persist` 7.30 and 5.44 s, `runner.title-two-run` 2.96 and 1.93 s,
  `installed.consumer.title-refused` 2.20 and 1.31 s, `controls.real.title-refused-frame` 1.94 and 1.44 s,
  `controls.real.title-wrong-input` 1.36 and 1.37 s, `harness.title-oracle` 0.28 and 0.18 s (24.34 s and 17.65 s).
  The title fixture build with the pinned Binutils, the runner rows, the consumer under the hosted compilers and the
  title literals pass on both operating systems.
- Delta against the Phase 11 warm pair (F-135, 539 s and ctest 279 s on 329 names): gate-macos +43 s, ctest +44 s,
  test-inventory-selftest +9 s. Cold projection (not a measurement; the cold run after the final push of 12-18 is
  orchestrator-only): 582 s plus the F-136 cold cost of 190 s = 772 s, margin 128 s; the measured cold baseline 729 s
  plus the hosted TITLE names and the inventory-selftest growth = 762 s, margin 138 s. Neither is above 900 s nor under
  the 120 s warning level, so the G4 reversal rule is not triggered (finding T-09).
- `docs/ci-reviews/LOG.md`: Runs rows for 45795103375 (the orchestrator's cold dispatch of F-136, which had no Runs
  row; recovered from the completed run without relaunching CI) and 45887012740, and finding F-137 (this pair).
  PIN-IMPACT "Gate-growth ledger": the TITLE row's hosted columns, final seconds and margin are filled; the Host base
  is recorded in "Wave bases" by its own commit after these records.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| T-09 | info | PIN-IMPACT "Gate-growth ledger" TITLE row; `docs/ci-reviews/LOG.md` F-137 | The projected cold gate-macos margin after the TITLE wave is 128 s, 8 s above the 120 s warning level of the reversal rule. The HOST wave adds about 8 s of native COST (DR-27 projection), about 25 to 35 s on hosted macOS at the measured multiplier of about 3 (24.34 s hosted for 8.35 s of COST here), so the 12-12 projection is likely to fall under the warning level. Of the seven TITLE names, `installed.consumer.title-persist` (7.30 s hosted macOS) is the named first candidate | no action now (not triggered); 12-12 measures the HOST wave and applies the reversal rule first if its projection is under 120 s of margin (a new numbered plan moves `installed.consumer.title-persist` and the selftests to a local lane) |

Result after the push: zero blocking findings, zero warnings and nine info rows; the TITLE wave is reviewed in a
scratch clone, pushed, and green on both hosted operating systems.

## HOST (12-07 to 12-11)

| Item | Value |
|------|-------|
| Host base | `0836e40f583f8b47486ee93b0de36889e5626f0b` (PIN-IMPACT "Wave bases", base idiom: exactly one 40-hex line, an existing commit, an ancestor of the head); the Wave 0 base `60b9e3c34fc0efc0ff64296cfa31415565b847b2` is read the same way |
| Reviewed head | `35a2736f1d82b4ca746eb947408cded3722385f9` (26 commits after the Host base: the 12-06 closeout, plans 12-07 to 12-10 and the one-line 12-11 record; 27 commits after `origin/main` `884153a`; 30 files changed outside `.planning/`, all under `host/`, `scripts/`, `tools/hostcheck/`, `docs/evidence/` and one sentence of `docs/spec/api.md`) |
| Host | Darwin arm64, Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2; 1-minute load 6.1 at the start of the scratch gate and 11 to 16 during it (an unrelated virtual machine used about 2.6 cores), so the seconds are upper bounds for a quiet host; no limit was widened |

### Task 1: scratch gate and full mutant run of the HOST head

Method (the 12-06 form, with the T-05 procedure note applied from the start): `git clone --no-checkout` of the
main repository into `build/12-review/host/src`, `git checkout --detach 35a2736f1d82b4ca746eb947408cded3722385f9`,
the clone's fetch URL set to the public repository with its push URL disabled and `origin/main` fetched (`884153a`),
and a copy (not a symlink) of `build/tools/install` into the clone's `build/tools/install`. `git archive` of the
head in the clone and in the main repository have the same SHA-256
(`413ae2defa59cf5b68ebc58089e6eb4d667a2bfbb8ded668def0b226b47e72b8`), so the gated tree is exactly the committed
head; `git status --short` of the clone is empty before and after the gate and after the mutant run. The gate
output was written outside the clone tree (`build/12-review/host/gate.out`, ignored, not committed).

Gate: `sh scripts/gate.sh` inside `build/12-review/host/src`, default environment. `[gate] PASS`, `[gate] total
193s`, exit 0. Steps in seconds: sh-n 0 (45 scripts), lint-selftest 5, fixture-qualification 1, configure 1,
build 6, boot-evidence-selftest 13, test-inventory-selftest 45, test-inventory 1, ctest 111 (`100% tests passed
out of 351`, `Total Test time (real) = 110.75 sec`), core-lint 9. No name was excluded. Phase 12 names:
`ctest -N -L phase12 -FA .` lists 22 (the 7 TITLE names and the 15 HOST-wave names), 24 with the fixture closure
(`install.setup`, `installed.consumer.build`); the preset has 351 names, 15 more than the 336 of the TITLE review.
The 15 HOST-wave names in this run (parallel, loaded host): `host.pump.title-two-speeds` 4.00, `repo.host-policy-selftest`
2.79, `repo.api07-evaluation` 2.64, `repo.host-include-guard-selftest` 2.27, `repo.host-isolation` 1.54,
`host.pump.stall` 1.40, `host.pump.build` 0.95, `host.pump.script` 0.48, `host.pump.drain-order` 0.45,
`repo.host-policy` 0.11, `host.pump.clock` 0.10, `repo.host-include-guard` 0.09, `repo.p12-mutant-anchors` 0.04,
`host.pump.input-map` 0.02, `host.pump.frames` 0.02 (16.90 s summed).

Mutant run: `python3 -B scripts/p12-mutants.py --run` inside the clone, after its gate (the moved prefix of the
clone's `build/gate` and the clone's title ROM, which equals its manifest row). It ran: the classifier denial of
plan 12-08 did not recur, so this is the first execution of the driver's worktree code (`git worktree add
--detach` of the clone's own repository under the clone's `build/12-mutants/wt`, the `Worktree.revert` path
with its clean-status proof, the RESULTS write and the clean-tree and prerequisite checks). Exit 0, 37 s wall:
`step host-build 0.9`, `step title-rom-build 0.5` (the worktree ROM equals its manifest row), `step baseline 7.1
(7 checks)`. After the run `git worktree list` of the clone shows only the clone and its status is empty.

| ID | Edit (file) | Expected | Result | Killing test | Seconds |
|----|-------------|----------|--------|--------------|---------|
| P12-P01 | tick divisor 16 written as 15 (`t64h_pump.h`) | KILLED | KILLED | `host.pump.clock` | 0.9 |
| P12-P02 | input hook once at the end of a step (`t64h_pump.c`) | KILLED | KILLED | `host.pump.title-two-speeds` | 2.4 |
| P12-P03 | ISViewer text drain skipped (`t64h_pump.c`) | KILLED | KILLED | `host.pump.drain-order` | 0.9 |
| P12-P04 | refused video packet dropped, not held (`t64h_pump.c`) | KILLED | KILLED | `host.pump.stall` | 1.8 |
| P12-P05 | frame rows read at the row length, not `stride_bytes` (`t64h_frames.c`) | KILLED | KILLED | `host.pump.frames` | 0.5 |
| P12-P06 | dead zone one unit too wide (`t64h_input.c`) | KILLED | KILLED | `host.pump.input-map` | 0.6 |
| P12-P07 | vertical axis by negation, not complement (`t64h_input.c`) | KILLED | KILLED | `host.pump.input-map` | 0.6 |
| P12-P08 | guest completion treated as resumable (`t64h_pump.c`) | KILLED | KILLED | `host.pump.stall` | 1.6 |
| P12-T01 | D-pad right and left bits swapped (`title.S`) | KILLED | KILLED | `host.pump.title-two-speeds` | 3.8 |
| P12-T02 | horizontal step 3 pixels (`title.S`) | KILLED | KILLED | `host.pump.title-two-speeds` | 3.8 |
| P12-T03 | one tone sample 0x2001 (`title.S`) | KILLED | KILLED | `host.pump.title-two-speeds` | 3.7 |
| P12-T04 | press count at slot offset 8 (`title.S`) | KILLED | KILLED | `host.pump.title-two-speeds` | 3.8 |
| P12-C-EQUIV | division by 16 written as a shift by 4 (`t64h_pump.c`; also `host.pump.title-two-speeds`) | SURVIVED | SURVIVED | - | 3.6 |
| P12-C-INVALID | `#error` after the pump's include (`t64h_pump.c`) | INVALID | INVALID | - | 0.5 |

12 of 12 mutants KILLED, each by its named test with a failed check line of `t64h_tests.c` or a comparator
MISMATCH line (the driver's kill rule; a crash or a timeout is never a kill), and both controls as expected:
no SURVIVED mutant and no unexpected control result, so Task 1 has no blocking finding. The table is the content
of the clone's `build/gate/12-p12-results.txt` (ignored).

Independent spot check (not the driver): a plain `git archive` copy of `scripts`, `host` and `fixtures` of the
head under `build/12-review/host/review/indep` with the P12-P04 and P12-P07 edits applied by `sed`, built with
`sh scripts/check-host.sh --host-build DIR build` against the clone's gate tree: `run stall` fails first at
`t64h_tests.c:780: status == T64H_STALL` and `:782: b.pump.counters.stall_retries is 5, expected 1` (a dropped
packet frees the ring, so the refusing sink never stalls), and `run input-map` fails at
`t64h_tests.c:1215: (unsigned)(pad.y + 128) is 1, expected 0` (host +32767 reaches -127, not -128). Both are the
cases' own assertions failing for the mutant's own reason.

### Task 2: HOST checklist (a) to (l)

`WAVE0_BASE` is `60b9e3c34fc0`, `HOST_BASE` is `0836e40f583f` and `HEAD` the reviewed head `35a2736f1d82` for every
check, run inside the clone and read from its logs and its source, not from the plan summaries. Logs are under the
ignored `build/12-review/host/review/`; the reviewer's own reader of the pump lines is a scratch file outside the
repository and is not committed.

| Check | Command or method | Result |
|-------|-------------------|--------|
| (a) installed API only | `sh scripts/check-host-includes.sh` and `--selftest`; `sh scripts/check-host-isolation.sh --build build/gate --with-selftest`; `grep -rn -E 'core/\|runner/\|tools/\|tests/installed\|add_subdirectory\|\.\./' host/`; `grep -n host CMakeLists.txt`; the clone's `build/gate/host-check/build.ninja` and `compile_commands.json` | `host-includes: files=16 findings=0`; selftest 16 failing seeds (HOST-INCLUDE 7, HOST-SDL-INCLUDE 2, HOST-SDL-IDENT 2, HOST-PUMP-TIME 1, HOST-CMAKE 4), 7 clean controls and the empty-scan error, `failures=0`; `host-isolation: files=494 archives=2 findings=0` after 14 seeded cases; the host grep has two hits, both comments (`host/CMakeLists.txt:2` names `tests/installed-consumer` as the model, `host/tests/t64h_sha256.h:4` names `tools/common` as what the hash is not), no path in code or CMake and no `add_subdirectory`; the root `CMakeLists.txt` holds no `host` (exit 1). The pump and test sources include only C99 headers, their own `t64h_*` headers and `<tendies64/t64_machine.h>`; `find_package(tendies64 0.1 CONFIG REQUIRED)` is the only package line outside the `T64H_SDL` branch. The host build of the clone names no `core/`, `runner/`, `tools/` or `tests/` path of the clone (0 and 0); its include directories are `host/pump` and `-isystem` of the moved prefix (a path with a space) only, and `check-host.sh build` configures with `CMAKE_PREFIX_PATH` removed from the environment |
| (b) clock rule | `grep -c -E 'clock_gettime\|gettimeofday\|nanosleep\|usleep\|pthread_\|fopen\|[^_a-zA-Z]time\(\|<time\.h>\|sleep\(\|SDL_'` over each file of `host/pump`; a read of `t64h_pump.c` against DR-11 and DR-35 | 0 in all eight files. The clock callback is read once in `t64h_pump_init` (the origin) and once per step (`t64h_pump_step` line 281), never after the terminal latch; a reading behind the last one returns `T64H_CLOCK_BACKWARD`, counts it and is not taken; the bound is `(now - origin) / T64H_TICK_NS` (`T64H_TICK_NS` 16, `t64h_ticks_from_ns` is the plain division), capped by `pump->tick + max_ticks` with overflow saturation and raised to the guest tick when behind it; the input hook may only lower it, never below the guest tick; an advance outside `[tick, bound]` is an `INTERNAL_DEFECT` answer of the pump. The pacing is a function of the clock values alone. The terminal set is `GUEST_COMPLETION`, `CPU_UNSUPPORTED` and `HALT`, one more than the header comment names (HR-01) |
| (c) pump equality | `ctest --test-dir build/gate --no-tests=error -R '^host\.pump\.title-two-speeds$' -V` (3 names with the fixture closure, all pass); the reviewer's own reader (`python3 -I`, no import of the comparator) of the four JSON lines against `title.expected.json` and `title.runner-pins.json` | comparator selftest ok, then `title.short.input.json: 3 and 240 Hz equal; 3 frames, 2 audio packets (stream), 20-byte transcript, save image, completion tick 3693938 and final digest equal the oracle and the runner pins` and `title.long.input.json: 3 and 240 Hz equal; 12 frames, 4 audio packets (stream), 20-byte transcript, save image, completion tick 13078313 and final digest equal the oracle and the consumer pins`. Own reader: `runs=4 checks=77 failures=0` (both scripts present at exactly 3 and 240 Hz; every frame list, audio packet list, stream, transcript and save digest equal the oracle scenario, no frame blank; completion tick, `GUEST_COMPLETION` and final digest equal the runner-recorded pin for short and the consumer-recorded pin for long, both labeled "not independent"; every non-information key equal across the two speeds). The two speeds are the two queueing extremes: at 3 Hz one step's bound spans the whole run, so the hook queues every event before the first advance (1 step, 1,033 and 3,331 advances, 1 and 5 mid-step drain stalls), and at 240 Hz the events are queued lazily over 15 and 51 steps (0 stalls); `events_deferred` is 0 in all four runs. Judgment: the claim of DR-21 holds as worded, byte-identical video, audio, transcript and save at two fake-clock speeds equal to the independent oracle, and final digest and tick equal to the headless pins; it is a measured fact of this title and its two scripts, not a proof for any guest (same-tick events are ordered by insertion sequence, HR-03) |
| (d) mapping table against DR-12 | a read of `host/pump/t64h_input.c` and `t64h_input.h` row by row; the pinned rows and the sweep of `input_map_axes` in `t64h_tests.c` | A 15 (`0x8000`), B 14 (`0x4000`), Z 13 (`0x2000`, left trigger `>= 16384`), Start 12 (`0x1000`), D-pad up 11, down 10, left 9, right 8 (`0x0800` to `0x0100`): the eight table rows equal DR-12 and the standard controller mask; X, Y, back, guide, both stick buttons, both shoulders, the right stick and the right trigger have no row; axis `v / 256` (C truncation toward zero), clamp to -128..127, `|scaled| < 8` is 0, vertical by `-1 - v` first (DR-36 item 2); `present == 0` or NULL is neutral; port 0 only (the default queue passes port 0); no keyboard source. The test's expected values come from a separate sign-and-magnitude formula and hand rows; the reviewer recomputed five of the 15 rows by hand (2047 vertical -8, -2048 vertical 0, -2049 vertical 8, -16384 vertical 63, 4095 vertical -16), all equal |
| (e) SDL out of the gate | `ctest --test-dir build/gate -N \| grep -c -i sdl`; `git diff --name-only WAVE0_BASE..HEAD -- .github scripts/check-ci-yml.sh scripts/gate.sh CMakeLists.txt`; the clone's host configure and build logs and cache | 0 names; the diff is empty (also empty for `core/include` and `docs/abi` since the Wave 0 base and for `core`, `runner`, `tools/abi` and `tests` since the Host base); the host logs hold 0 matches of sdl and the cache `T64H_SDL:BOOL=OFF`. The 30 files changed since the Host base outside `.planning/` are `host/` (16), 10 files under `scripts/`, `tools/hostcheck/CMakeLists.txt`, two evidence documents and `docs/spec/api.md` (the 8.3 tick-rate sentence) |
| (f) policy, audit order, lane | `sh scripts/check-host-policy.sh` and `--selftest`; `git log --diff-filter=A` of `host/sdl` and of the audit; `git merge-base --is-ancestor e38cd65 9940ebc`; greps of `host/sdl`; a path grep of PIN-IMPACT "Host steps" and of the two evidence documents; `sh scripts/check-host.sh sdl` once in the clone | `host-policy: files=16 findings=0`; selftest 30 failing seeds covering all 12 rules, 9 clean controls and the empty-scan error, `failures=0`; the audit `docs/evidence/sdl3-host-dependency-audit.md` is added by `e38cd65`, an ancestor of `9940ebc`, the first commit that adds `host/sdl` (zlib license recorded, no figure of speed); `T64H_SDL_TITLE` is exactly `"t64 minimal host (development)"` and is the only title argument of `SDL_CreateWindow`; 0 keyboard, scancode, keycode or text-input tokens and 0 renderer, GPU, OpenGL, Vulkan or Metal calls in `host/sdl`; underruns, bad frames and dropped audio are counted. No absolute path in "Host steps" or either evidence document (0 matches). Lane, verbatim: `[host-sdl] sdl 3.4.10`, `video-driver dummy`, `audio-driver dummy`, `mapping rows 30/30`, `title ok`, `blit ok`, `frames 8`, `audio ok`, `[host-sdl] PASS`, exit 0, so the SC2 virtual-joystick clause is closed on this host (a PASS, not a skip); the raw logs stay in the clone's ignored `build/host-sdl` and its `lane.out` holds no path |
| (g) API-07 | `sh scripts/check-api07-evaluation.sh` and `--selftest`; the pin line of `docs/evidence/phase12-libfuturo-fit.md` against the PIN-IMPACT row; `git grep -i -c libfuturo -- core runner host tools tests` | `api07-evaluation: files=508 findings=0`; selftest 32 lines ok (25 failing seeds, 6 controls, the empty-scan error), `failures=0`; `Pinned commit: 4177a43e67312e2db88ab21cba13aa7350063105` equals the "Pinned commit" row of "API-07 pin" (local, unpublished, said so in words); the Conclusion opens with "no adapter now"; the code-root grep prints nothing (exit 1): no adapter code, target or test |
| (h) 12-11 outcome | the PIN-IMPACT triage table (columns counted by `awk`), the trigger line, `12-11-SUMMARY.md`, `git log 884153a..HEAD -- docs/abi/FREEZE-CANDIDATE.md` | 15 rows (C-1 to C-6, H-01 to H-09), every "breaks a postcondition" cell "no", dispositions 13 accept and 2 document (C-1 done in 12-07, H-03 owed to 12-15), 0 amend; exactly one line `12-11 trigger: no`; the 12-11 SUMMARY is the one line "Not triggered: no host-found API defect with disposition amend; no code, no commit" with `commits: 0` and equal head before and after (its record commit `35a2736` touches only the SUMMARY and ROADMAP); no amendment-log row and no change of `docs/abi`. Not triggered, so no RED evidence or consumer re-run is owed. The reviewer read the H-03 site: `dispatch_front` of `core/t64_scheduler.c` pops the event and sets the guest tick before the handler runs, and the advance loop turns a whitelisted non-OK status into `RESOURCE_LIMIT`, so the instance stays resumable and "document" is the right disposition. The sentence is not yet in the 12-15 plan text (HR-06) |
| (i) digest sweep | `python3 -B scripts/digest-sweep.py --base WAVE0_BASE --head HEAD` (Task 2 verify 2, after the base idiom) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, exit 0, 47 s: the core did not move in the HOST wave, as expected with 12-11 not triggered |
| (j) authority still blocked | `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked`; `--require-approved` | exit 0 `authority record valid; blocked: no oracle operation is authorized`; exit 2 |
| (k) PII over the range | `sh scripts/pii-scan.sh --range HOST_BASE..HEAD` | `pii-scan: tree_files=skipped commits=26 findings=0` |
| (l) COST on every phase12 name | `ctest --test-dir build/gate -L phase12 --show-only=json-v1` | 22 `phase12` names (24 listed with the fixture closure, both `phase11`), every one with a COST and a literal TIMEOUT; the 15 HOST-wave names carry 11.51 s of COST (the 14 host names 10.17 s, `repo.api07-evaluation` 1.34 s) and TIMEOUTs of 30, 60 or 120 s, each at least 3.5 times its largest native figure seen here (4.00 s for `host.pump.title-two-speeds` against 60 s) and twice its COST |

Task 2 verify (in the clone): (1) `ctest --test-dir build/gate --no-tests=error -R '^(host\.pump\.|repo\.host-|repo\.api07-evaluation$|repo\.p12-mutant-anchors$)' --output-on-failure`
`100% tests passed out of 16` (15 names and `install.setup`, 11.09 s), `host-includes: files=16 findings=0`,
`host-policy: files=16 findings=0`, `api07-evaluation: files=508 findings=0`; (2) the base idiom and the digest sweep,
exit 0 (check (i)).

Linux, before the first hosted run (AUTONOMY section 2, local first): the selftests of `check-host-includes.sh`,
`check-host-policy.sh`, `check-host-isolation.sh` and `check-api07-evaluation.sh` and the two plain scans ran in the
act image already on disk (`ghcr.io/catthehacker/ubuntu:act-24.04`, no network, the clone mounted read-only; GNU Awk
5.2.1, git 2.56.1): all exit 0 with `failures=0` and `findings=0`. The host project itself was not built on Linux
here (it needs the Linux install fixture); its first Linux build and run is the hosted gate-linux job of Task 3 (HR-05).

#### Decision-by-decision read

| Decision (CONTEXT, with its DR row) | Where implemented | Test names | Verdict |
|-------------------------------------|-------------------|------------|---------|
| Pump and shell split (DR-10, DR-11, DR-35): a pure-C pump with no SDL unit-tested in the gate with a fake clock; an optional SDL3 shell, OFF by default, outside the gate; the core builds with no SDL and without the host target | `host/pump` (four modules), `host/tests/t64h_tests.c`, `host/sdl` under `if(T64H_SDL)` | the eight `host.pump.*` names, `repo.host-isolation` | implemented; checks (a), (b), (e) |
| SDL link-only (DR-16, DR-17, D4): `find_package(SDL3 CONFIG)` against a system install, never vendored or fetched, audit first | `host/CMakeLists.txt` line 44, `docs/evidence/sdl3-host-dependency-audit.md` | `repo.host-policy`, `repo.host-policy-selftest`, `repo.host-isolation` | implemented; check (f); no SDL source, `third_party` or fetch in the tree |
| "Plays" means correct (DR-21; no throughput, A/V sync or latency claim, underrun is silence and counted, pacing a pure function of the injected clock) | `t64h_pump.c` (clock callback only), `t64h_sdl.c` counters | `host.pump.clock`, `host.pump.title-two-speeds` | implemented; checks (b), (c); the speed figure of H-09 is information only and is in no evidence document |
| Evidence tiers (DR-21): scripted equality at two fake-clock speeds is the claim; the SDL smoke is host glue only; the physical checklist is owner-attested and non-blocking | PIN-IMPACT "Host steps" (tier line), `scripts/check-host-title.py` | `host.pump.title-two-speeds`; the local lane | implemented; the checklist `docs/evidence/phase12-host-manual.md` belongs to 12-18 |
| API-07 (DR-22): a written evaluation pinned to a commit, "no adapter now", no adapter code, no core change, no support claim | `docs/evidence/phase12-libfuturo-fit.md` | `repo.api07-evaluation` | implemented; check (g) |
| Amendment rule (DR-23): every host-found defect triaged; an amend row triggers 12-11 | PIN-IMPACT "Host-found API defects", `12-11-SUMMARY.md` | none (a record) | implemented; check (h): 0 amend, not triggered |
| Host layout (DR-10): `host/` top level, standalone project, registrations in `tools/hostcheck`, build tree `build/gate/host-check`, quoted paths with a space | `host/CMakeLists.txt`, `tools/hostcheck/CMakeLists.txt`, `scripts/check-host.sh` | `host.pump.build` | implemented; check (a) |
| Input mapping (DR-12, DR-36 item 2) | `host/pump/t64h_input.[ch]` | `host.pump.input-map` (and the smoke's 30 rows) | implemented; check (d); mutants P12-P06 and P12-P07 killed |
| SDL lane (DR-17, DR-31, DR-37 item 4): local only, never a CTest name or workflow step, a recorded skip when SDL3 is absent | `scripts/check-host.sh sdl` | none by design | implemented; checks (e), (f); PASS re-run here |
| Save persistence (DR-19): host-side save file omitted unless it fits one task | PIN-IMPACT "Host steps" DR-19 line | none | implemented as an omission with its measured reason; the headless and pump runs prove the save bytes (check (c)) |
| Throwaway measurement (DR-20): one figure, information only | 12-09 SUMMARY only | none | implemented; the audit holds no speed figure |
| LibFuturo re-pin (DR-22): re-read at plan time, published status stated | PIN-IMPACT "API-07 pin" (12-10 result paragraph) | `repo.api07-evaluation` (PIN-MATCH) | implemented; the pin did not move and is stated as local and unpublished |
| Keyboard off (CTX settled; DR-37 item 3, HP-KEYBOARD) | `host/sdl` (controller only) | `repo.host-policy`, its selftest | implemented; check (f) |

#### Findings

Same severity scale as the TITLE section. The reviewer fixes nothing.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| HR-01 | info | `host/pump/t64h_pump.c:335-339`; `host/pump/t64h_pump.h:29`; DR-35 item 3 | The pump latches `HALT` as terminal besides `GUEST_COMPLETION` and `CPU_UNSUPPORTED`; the header comment and DR-35 name only the two. api.md 8.1 says `HALT` is reserved and unreachable through the installed API, and the core keeps it sticky (`machine->halted`), so the extra member cannot change a run | accepted: conservative and unreachable; a wording candidate for any later plan that edits `t64h_pump.h` |
| HR-02 | info | `scripts/check-host-title.py` `judge` | The comparator judges the scripts that appear in the lines and requires two speeds for each, but does not require both known scripts to be present: a run set that lost the long script would pass it. `check-host.sh` always passes both scripts, the binary fails on any refused script, the CTest log shows both report lines and the reviewer's own reader checked that both are present | accepted: no current path drops a script; a one-line set check is a candidate for any later plan that edits the comparator (the 12-17 qualifier should read both report lines) |
| HR-03 | info | `host/pump/t64h_script.c` `t64h_script_feed` (QUEUE_FULL branch); `core/t64_scheduler.c` advance loop (`tick <= request->value`) | A QUEUE_FULL deferral (DR-36 item 1) lowers the bound to the event's own tick; the advance dispatches every queued event at that tick, so the deferred event is queued at the reached tick and runs after the same-tick events already queued (same-tick order is by insertion sequence), where a timely queue might have ordered it earlier. The path is pinned against a model queue only (`host.pump.script`); no authored scenario reaches it (`events_deferred` 0 in all four runs, at most 12 of 256 slots) | accepted: a host-side pump policy, not an API postcondition (QUEUE_FULL is documented, H-04); the pump equality claim is scoped to the title (check (c)) |
| HR-04 | warning | PIN-IMPACT "Gate-growth ledger" HOST row | Pre-push projection, not a measurement: the 15 HOST-wave names carry 11.51 s of native COST; at the hosted multiplier of 2.8 to 4.3 plus about 0.23 s of inventory selftest per name they add about 36 to 53 s to the TITLE projection of 772 s, so a projected cold gate-macos of about 808 to 825 s, margin about 75 to 92 s: under the 120 s warning level of the reversal rule and not above 900 s | the push goes ahead (no projection above 900 s); Task 3 replaces this with the hosted measurement and applies the reversal rule to it |
| HR-05 | info | hosted `gate-linux` | The first Linux build and run of `host/` (the pump, its tests and `check-host.sh` on gcc 13.3 and cmake 3.31) and the first hosted run of the host guards are at this push; the preflight `linux-compile` step covers the gate's compile units, not `host/` (12-09) | accepted: the host sources compiled with the act image gcc 13.3 under `-Werror` in plans 12-07 to 12-09 and the guard selftests pass in the act image here; a hosted mismatch is a finding for a new numbered plan |
| HR-06 | warning | `12-15-PLAN.md` (no H-03 text); `docs/spec/api.md:402`; PIN-IMPACT row H-03 | Row H-03 owes one sentence to `docs/spec/api.md` 8.1 (a masked handler status consumes its event and the next advance continues; the permanent stall is the ring-backpressure case), and the records name plan 12-15 as its carrier, but the 12-15 plan text does not mention H-03 or section 8.1 (`grep -c 'H-03\|masked handler'` prints 0), so an executor following that plan alone would leave the misleading line 402 ("A stall with no progress after a full drain is permanent") in place | carried: the orchestrator adds the H-03 sentence to the 12-15 action and acceptance text before 12-15 runs (`docs/spec/api.md` is already in its file list, B-28; api.md is not a golden, so no amendment); the 12-18 final audit checks the sentence. Not blocking: no code or bound text depends on it |

Result before the push: zero blocking findings, two warnings (HR-04, HR-06) and four info rows. The HOST wave is
independently confirmed check by check, the full mutant run kills all 12 mutants with both controls as expected, and
the wave moved no digest (check (i)).

### Task 3: push, hosted pair, HOST ledger row and the Bind base

Before the push, in the shared tree with a clean committed tree at `de21fcf7d5c30d8de59bde3d49143ab5dbb75faf`
(the reviewed head plus the two 12-REVIEW.md commits of Tasks 1 and 2):

- `sh scripts/gate.sh`: `[gate] PASS`, 351 of 351, ctest 90 s, total 148 s (load about 10; no limit widened).
- `sh scripts/preflight-phase12.sh` (full, with replay): `[preflight] PASS` in 65 s (oracle-authority, ctest-counts
  23 rows 0 mismatches, `phase12 label: 22 tests carry the label phase12, matching the PHASE12_TESTS block`,
  abi-cheap 16 names, host-cheap 7 names, guards including `check-host-includes.sh`, host-load WARN at 10.2,
  head-ci `884153a` completed success, timeout-audit 204 TIMEOUTs 0 too low with lowest ratio 2.9, pii, linux-compile
  234 units with the act image cc 13.3.0 in 37 s, post-gate-replay 11 `ci.yml` steps in 17 s).
- `sh scripts/pii-scan.sh --tree` (1516 files) and `--range origin/main..HEAD` (29 commits): 0 findings;
  `sh scripts/rights-scan.sh --tree --range origin/main..HEAD`: `tree=1516 history=105 findings=0`; no tag points at
  the head.

Push: `git push origin de21fcf7d5c30d8de59bde3d49143ab5dbb75faf:refs/heads/main`, a fast-forward from `884153a`
(29 commits; no force, no tag, no other branch). Hosted run 45904452436 (`ci.yml`, event push; Binutils cache hit on
both jobs with key `69d0bfd4`, so a warm sample; no rerun): conclusion success.

- gate-macos success, job 572 s (Gate 504 s, ctest 312 s, `100% tests passed out of 351`, test-inventory-selftest
  84 s, build 32 s, boot-evidence-selftest 32 s, core-lint 23 s, lint-selftest 13 s; cmake 4.5.0, AppleClang 21.0.0).
- gate-linux success, job 306 s (Gate 252 s, ctest 175 s, `100% tests passed, 0 tests failed out of 351`,
  test-inventory-selftest 22 s, build 26 s; cmake 3.31.6, GNU 13.3.0).
- The 15 HOST-wave names, first hosted run, all passed, macOS then Linux: `host.pump.title-two-speeds` 9.94 and
  10.61 s, `repo.host-policy-selftest` 7.46 and 1.16 s, `repo.api07-evaluation` 6.89 and 1.20 s,
  `repo.host-include-guard-selftest` 5.86 and 1.39 s, `host.pump.stall` 4.57 and 4.39 s, `repo.host-isolation` 3.78 and
  0.98 s, `host.pump.script` 2.83 and 1.33 s, `host.pump.build` 2.61 and 1.13 s, `host.pump.drain-order` 0.85 and
  1.36 s, `repo.host-policy` 0.25 and 0.08 s, `repo.host-include-guard` 0.21 and 0.06 s, `host.pump.clock` 0.19 and
  0.15 s, `repo.p12-mutant-anchors` 0.09 and 0.05 s, `host.pump.frames` 0.04 and 0.01 s, `host.pump.input-map` 0.03 and
  0.01 s (45.60 s and 23.91 s). This is the first Linux build and run of `host/` against the moved prefix (gcc 13.3,
  cmake 3.31), the first hosted run of the host guards and their selftests (awk and `git init` trees on Linux) and of
  the SDL-free host build on both operating systems: no mismatch, so HR-05 needs no plan. The seven TITLE names took
  18.82 s on macOS and 20.43 s on Linux.
- Delta against the TITLE pair (F-137, 336 names): gate-macos job -10 s and ctest -11 s for 15 more names whose
  hosted seconds sum 45.60 s, test-inventory-selftest +8 s: this runner was faster than the TITLE sample (warm spread
  on identical code up to 105 s, F-134), so the growth is read from the name seconds, not from the job delta.
  gate-linux job +43 s, ctest +26 s.
- Cold position (projections, never a cold figure; the cold run after the 12-18 push is orchestrator-only): (1) this
  warm job plus the F-136 cold cost of 190 s is 762 s, margin 138 s; (2) the measured cold baseline 729 s plus the
  hosted macOS seconds of the 22 Phase 12 names and the inventory-selftest growth since F-135 (+17 s) is 816 s,
  margin 84 s; (3) the TITLE warm sample plus the HOST names, the inventory growth since it and the cold cost is 826 s,
  margin 74 s. None is above 900 s, so the trigger of this plan is not reached and the plan goes on to the Bind base;
  the additive projections are under the 120 s warning level (HR-04 below).
- `docs/ci-reviews/LOG.md`: Runs row for 45904452436 and finding F-138. PIN-IMPACT "Gate-growth ledger": the HOST
  row's final seconds, hosted columns and margins are filled; the Bind base is recorded in "Wave bases" by its own
  commit after these records.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| HR-04 | warning | PIN-IMPACT "Gate-growth ledger" HOST row; `docs/ci-reviews/LOG.md` F-138 | Measured hosted cost of the HOST wave: 45.60 s on gate-macos for 15 names (a multiplier of about 4.0 on their 11.51 s of COST) plus +8 s of inventory selftest. The additive cold projections are 816 s (margin 84 s) and 826 s (margin 74 s), under the 120 s warning level and not above 900 s; with the names still to come (the two of 12-16 and the 12-17 qualifier, about 23 s hosted by its Phase 11 analogue) projection 2 reaches about 842 s, margin about 58 s. The warm job of this run (572 s) is faster than the TITLE sample, which shows the runner spread, not a smaller gate | not blocking (no projection above 900 s; the plan's stop condition is not met). The warning branch of the ledger's reversal rule applies: a new numbered plan appended after 12-18 (the next free number, 12-19) is recommended to move `installed.consumer.title-persist` and the two host selftests (`repo.host-policy-selftest`, `repo.host-include-guard-selftest`; about 18 to 21 s hosted macOS together) to a non-gating local lane and to repeat the measurement; `host.pump.title-two-speeds` stays because it is the SC2 evidence. Whether that plan runs before the bind (the rule's "before anything else") or after the orchestrator's final cold measurement is an orchestrator decision; the reviewer writes no plan |

Result after the push: zero blocking findings, two warnings (HR-04 with its hosted numbers, HR-06) and four info rows;
the HOST wave is reviewed in a scratch clone with the full mutant run, pushed, and green on both hosted operating
systems.

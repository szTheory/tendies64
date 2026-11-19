# Phase 9 fan-out F: cross-cutting QA, security, DevOps and plan slicing

Status: research recommendation for the Phase 9 discussion synthesis. It is not a
plan, a contract or an owner decision. Clusters A (SI/PIF/joybus), B (save media),
C (state and replay) and E (scheduler ordering) own their device facts; where this
document names a device behavior it is a slicing assumption for them to confirm.
No source was written, no ROM, save, firmware or asset was used, and no hardware or
oracle claim is made. Repo-relative paths only.

Evidence labels used below: **[F]** fact verified in a repo file or URL, **[C]** code
observation (read, not run), **[I]** inference, **[U]** unknown.

## 0. Grounding facts that drive the slicing

1. **[F] Phase 8 baseline.** Five strictly sequential plans (tracer, registers/cadence,
   output/determinism, capture, qualification). Execution ran from 2026-11-18 09:47 to
   closeout 2026-11-19 10:23 by commit timestamps (45 commits): plans 01-04 took about
   7 hours of commit span, Plan 05 (qualification) took the rest, with six native
   attempts and three hosted cycles (`08-05-SUMMARY.md`). Qualification, not
   implementation, was the long pole. The cost drivers were stale pinned inventories,
   workflow-scanner drift, GCC13 `-Werror`, verifier tails meeting real data late and
   bound-source churn (`08-05-SUMMARY.md` "Corrections found on the way"; `docs/ci-reviews/LOG.md`
   F-096..F-098).
2. **[F] Bound-source trap.** Any change to a file inside the bound source snapshot
   (qualifier, `ci.yml`, policy scanner) after native qualification forces a new native
   plus hosted cycle of about 15 minutes (`LOG.md` F-096, F-098; `08-05-SUMMARY.md`
   "only the last [hosted run] is the binding because later commits changed bound source").
3. **[F] Current hosted gate.** After the Binutils cache (F-100): about 129 s Linux,
   about 275 s macOS, with macOS Gate step about 220 s as critical path; the job limit
   is 10 minutes. The next measured candidates are macOS `test-inventory-selftest`
   (34 s), `boot-evidence-selftest` (19 s) and the duplicate `repo.rsp-no-hle` run (F-097).
   Phase 8 ended with 179 normal and 181 ASan CTests. Hosted headroom on macOS is about
   325 s. [I] New always-on hosted work should stay under roughly 60 s on macOS.
4. **[C] Input plumbing already exists but is inert.** `runner/t64_input.c` parses the
   closed `t64.input` v1 schedule (cap 128 events and 65,536 bytes, canonical JSON,
   digest); `runner/t64_real.c` queues each event with `t64_machine_queue_input`;
   `docs/spec/machine-contract.md` says the event is "not consumed by SI, PIF, or a
   controller model" and is a scheduler event (device 6, kind 1) included in the state
   identity until dispatched. Port is limited to 0. `runner/t64_verified_input.c`
   snapshots untrusted input into an unlinked private file and verifies size and SHA-256
   before use (a TOCTOU-resistant seam to reuse for new inputs). Raw capture already
   records `input_file_sha256`, canonical input digest and size in its manifest
   (`docs/spec/raw-capture.md`).
5. **[C] No SI, PIF RAM, EEPROM, SRAM, state serializer or state import exists.**
   SI and PIF windows are explicit unsupported windows (`docs/spec/machine-contract.md`
   lines 107-114). The digest (`core/t64_digest.c`, domain `t64.state.vN`) hashes fields
   but is not a serialization. The scheduler event carries a function pointer
   (`core/t64_internal.h`, `struct t64_event.handler`), so a pointer-free export needs a
   (device, kind) to handler registry. The profile descriptor already carries
   `synchronization_quantum` (`core/include/tendies64/t64_machine.h:151`), so the
   "quantum in profile identity" half of CORE-02 is largely present and needs only a
   preserved pin; the open half is the all-device ordering matrix.
6. **[F] Compatibility policy.** Preserve identities and bytes of existing profiles;
   version only concrete changes; no profile per device or version per fix
   (`docs/spec/cpu-compatibility.md`; `.planning/STATE.md` Decisions). Each profile has its
   own `state_serialization_version` (`core/t64_isviewer.c` shows versions 8..24 in use).
7. **[F] Policy stops.** Saves are rights-bearing; no ROM/save/firmware in public CI;
   hash proves identity not rights; no hardware claims; API/ABI freeze belongs to
   Phase 10 (`AGENTS.md`; `.planning/AUTONOMY.md` section 4; `ROADMAP.md` Phase 10).
8. **[F] External practice.** TASVideos' desync guidance: most movie desyncs come from
   inconsistent savestates, and the standard check is replaying the whole movie from
   power-on (https://tasvideos.org/DesyncHelp). libretro's netplay-safe savestate
   guidance: no pointers, fixed-endian integers, a flag for variable size, and zero-filled
   padding for determinism (https://libretropy.readthedocs.io/en/latest/api/libretro.api.savestate/;
   https://docs.libretro.com/development/retroarch/netplay/; the Virtual Jaguar PR
   zero-fills unused bytes in `retro_serialize()`). These are community/secondary
   sources used only to support the design rules in D-F5 and D-F6, not as N64 facts.

---

## 1. Decision points

### D-F1. Plan breakdown, wave order and whether scheduler completion goes first

**Options.**
- (a) Scheduler completion (CORE-02) first, then devices, then state, then replay.
- (b) Devices first, with a cheap scheduler ledger early and the full cross-device
  ordering matrix after all devices exist, then replay, then state export, then
  qualification.
- (c) Group by requirement (one plan per requirement, seven plans), ordering by
  roadmap listing.
- (d) Fewer, fatter plans (three plans: controller, save, state+replay).

**RECOMMENDED: (b), eight strictly sequential plans in seven waves** (list in section 2).

**Rationale.**
- Full CORE-02 closure cannot be proven first: the new events that most need ordering
  (SI DMA completion, joybus response, EEPROM busy-window completion) do not exist
  yet [I]. A first-position scheduler plan would be re-opened by every later device.
  The CORE-02 acceptance wording is an all-device coincident-event and
  partition/interleave matrix (`ROADMAP.md` Phase 9 criterion 4), which is naturally
  the last device-level check.
- But a cheap scheduler **ledger** belongs in Plan 1: a table of every existing event
  (device, kind, handler) and the equal-tick order rule, pinned by a seconds-long test
  and consumed later by state export for the handler registry (fact 5 above). This
  front-loads the one thing state export needs and costs little.
- Replay before state export (reverse of roadmap prose order): replay from power-on
  needs only SI consumption plus the canonical digest [I]; its recorded checkpoint
  digests then become an independent regression oracle for the state plan
  ("export at tick T, import, advance, digest equals the replay's digest at T+N").
  State export is the largest and riskiest surface and must come after every device's
  future-affecting state is final, so it follows all device plans. Putting replay
  after state would make replay re-depend on the state container's binding code,
  which is not needed for a power-on replay.
- Strictly sequential, not parallel: Phase 8 showed shared registrations
  (`scripts/check-test-inventory.sh`, `docs/spec/*-cases.json`, `CMakeLists.txt`,
  runner dispatch) make parallel plans collide, and `AUTONOMY.md` ("Coupled task
  commit boundaries") already prefers one verified slice per plan.

**Evidence.** `ROADMAP.md` Phase 8 plans lines (01-05 sequential waves); `docs/spec/machine-contract.md`
lines 107-114, 370-385; `core/t64_internal.h` `struct t64_event`; `AUTONOMY.md` section 2.

**Failure modes.** (1) Late device plan changes serialized fields after state export is
built, forcing a re-plan: mitigated by ordering state export last among implementation
plans and by a reset/digest/serialize audit table (the "Replacement reset snapshot"
table in `machine-contract.md` is the existing template) updated per plan. (2) Eight
plans exceed the Phase 8 coordination cost: mitigated by allowing the merge noted below.
(3) Ledger drifts from reality: mitigated by a test that enumerates registered handlers.

**Authored-evidence plan.** Ledger test (`unit.core.sched.ledger`) in Plan 1; cross-device
matrix (`unit.core.sched.cross-device`) in Plan 5.

**What would change it.** If cluster E finds that SI/PIF can only be made correct by a
scheduler restructuring (for example sub-tick ordering or an RSP quantum change), move
that restructuring to Plan 1 and keep the matrix last. If cluster C shows replay must
carry state checkpoints, swap Plans 6 and 7.

**Reversibility.** High. Plans are documents until executed; Plans 6 and 7 are
swappable before Plan 6 starts. Merge option: fold Plan 2 into Plan 1 if SI timing
turns out trivial (decide at Plan 1 close, as Phase 8 folded decisions).

### D-F2. Profile identity and state-version strategy

**Options.** (a) New composite profile once per phase, grown through the phase and
pinned at the end; (b) a new profile/state version per plan; (c) reuse the AI-v1
profile with optional devices.

**RECOMMENDED: (a)**: one new composite profile (working suffix only, for example
`...-ai-v1` successor; final id chosen by cluster A/C) created in Plan 1, with the
descriptor and state version bumped once; its fields grow across Plans 1-4 while it
is unpublished and unqualified, and its pins are frozen in Plan 8. Old profile
identities, digests and refusal behavior remain byte-identical and are checked by an
"old pins" control exactly as `unit.core.ai.profile-pins` did.

**Why.** The compatibility policy forbids a version per fix. [F] Per-plan bumps would
churn pinned hashes in `scripts/qualify-*.py`, claims and control inventories every plan.
Option (c) violates "never guessed from the title" for save devices and would
retroactively change AI-v1 behavior.

**Evidence.** `docs/spec/cpu-compatibility.md` (identity preservation); `.planning/STATE.md`
Decisions; `core/t64_isviewer.c` version ladder; `docs/claims/phase08.md` profile id.

**Failure modes.** The in-progress profile gets a hash pinned in a test, then a later plan
adds a field and silently invalidates it: mitigate by having tests reference a symbolic
current-profile accessor and pinning the literal hash only in Plan 8 (old profiles pinned
literally from the start). Save-device selection must be a descriptor field so a profile
with EEPROM-4K, EEPROM-16K, SRAM and none are distinguishable and each is exercised
(cluster B decides whether these are separate profiles or one profile plus a field).

**Authored-evidence plan.** Reuse the Phase 8 descriptor-mutation and "every policy
field mutated through hash, serialization and known-ID construction" control
(`AUTONOMY.md` "New-descriptor identity check").

**Would change it.** Cluster B concluding save-device choice must be a separate profile id
per device (then count the profiles, but still one version bump).

**Reversibility.** Medium: a profile bump after qualification is a new phase's cost, so decide at Plan 1.

### D-F3. Authored diagnostic fixtures: which ROMs and how many

Constraint: hosted fixture build is cheap now that Binutils is cached (about 1-2 s,
F-100); the cost is per-fixture build, boot admission and guest run time, plus inventory
and manifest registration. Phase 8 used exactly one ROM (`fixtures/audio`, built by
`scripts/build-audio-fixture.sh`) and extended it with cases.

**Options.** (a) One monolithic ROM with all tests; (b) one small ROM per behavior
(about 10+); (c) a small family of focused ROMs, each exercising a plan's seam.

**RECOMMENDED: (c), five new authored ROMs, all built with the pinned assembler,
all asserting through the guest-output verdict path plus an independent literal**:

| ID | Plan | Purpose |
| --- | --- | --- |
| `pad-poll` | 1-2 | Reads SI/PIF joybus status for a scripted button/axis schedule and writes observed words to guest output at known ticks; also issues an unknown joybus command and an unknown channel for the refusal postconditions |
| `eeprom-rw` | 3 | EEPROM 4K and 16K variants share one source with an assembler constant (two builds): reads, writes, block boundary, back-to-back write during busy window, unsupported size command |
| `sram-rw` | 3-4 | SRAM read/write through PI domain 2 with the explicit-profile selection; refused when the profile does not select it |
| `save-roundtrip` | 4 | Writes a literal pattern; the host exports the save, re-imports into a fresh instance, and a guest read proves the pattern (host-side expected bytes independent of the guest) |
| `replay-mix` | 6-7 | Combined controller poll, EEPROM write and AI DMA over a guest span long enough to produce several checkpoint digests (a few million ticks should suffice; [U] measure the real cost before fixing the length) |

Rules for each: a cleared fixture manifest row (source/revision, recipe, toolchain,
digest, rights, expected-result provenance) per `AUTONOMY.md` section 2; built under
`build/` only; no ROM committed; expected values derived independently of the code
under test (anchor raw joybus packets as literal bytes before decoding, per the
"Raw-command conformance preflight" rule); **a build-only identity and rights gate runs
before any guest use**; and a same-setup wrong-value control that reaches its comparator
(the Phase 8 `reached-wrong-sample` pattern) per ROM.

**Why not (a)/(b).** A monolith couples failures and hides which seam broke; ten ROMs
multiply manifest, inventory and CI surface (each registration is a stale-inventory risk,
F-98/Phase 8). Five ROMs, built from three assembler sources, keeps hosted build to
seconds.

**Evidence.** `scripts/build-audio-fixture.sh`; `docs/spec/audio-cases.json` (75 cases / 22 CTests
for one ROM, a measure of registration cost); `AUTONOMY.md` fixture-rights rule.

**Failure modes.** Fixture author is also oracle: mitigated by independent literals and
the rights-clean provenance record, never claimed beyond tier `unit-tested` or
`self-checking-fixture` (D-F4). Guest output verdict channel being too narrow for
button data: use the existing capture/output drain, not a new channel. Unknown: joybus
packets exact bytes are a cluster A fact.

**Authored-evidence plan.** Each ROM listed in the fixture manifest with its own
case-inventory rows; builds covered by the existing `fixture-qualification` step.

**Would change it.** If a ROM needs data that only a commercial game produces (it should
not): stop under the rights rule.

**Reversibility.** High; a ROM can be split or merged without identity impact outside its manifest row.

### D-F4. Evidence tiers and claim wording per requirement

Use only tiers from `docs/claims/TEMPLATE.md`. The reserved words "accurate",
"compatible", "supports" are refused outside `hardware-derived` claims, so claim text
must avoid them. No `emulator-differential`, `hardware-derived`, or commercial claim.

**RECOMMENDED claims (one claim block per requirement group, in `docs/claims/phase09.md`):**

| Req | Tier | Fixture set (working) | Claim sentence core | Not claimed (must be listed) |
| --- | --- | --- | --- | --- |
| SAVE-01 | `self-checking-fixture` if the ROM reports its own verdict, else `unit-tested` | `pad-poll` | An authored ROM reads the scheduled button and axis values at the scheduled guest ticks through SI/PIF joybus for the standard controller on port 0 | other ports, other peripherals, physical controllers, joybus timing against hardware, rumble, Controller Pak, commercial software |
| SAVE-02, SAVE-04 | `self-checking-fixture` | `eeprom-rw`, `sram-rw` | Explicitly selected EEPROM 4/16 Kbit and SRAM behave per the authored block, busy-window and refusal contracts; unselected or unknown device requests are refused with defined postconditions | FlashRAM, real cartridge save-chip timing, title-based save detection, battery or wear behavior |
| SAVE-03 | `unit-tested` | `save-roundtrip` | Exported save bytes round-trip with the defined byte order; a refused import leaves the save digest unchanged | third-party save formats, emulator-specific container formats, interchange with other emulators |
| CORE-02 | `unit-tested` | scheduler ledger plus cross-device matrix | Device events are ordered by one guest-time scheduler under the authored coincident, partition and interleave cases | hardware event ordering, cycle accuracy, RSP/RDP timing against hardware |
| CORE-04 | `unit-tested` | state round-trip controls | Exported state re-imports and reproduces the canonical digest and continuation digests for the authored machines | cross-build or cross-version state compatibility, state from other emulators |
| CORE-05 | `regression-pinned` for replays of recorded runs, plus `unit-tested` for the format and refusals | `replay-mix` | A recorded input replay bound to profile, build and content hash reproduces the recorded digests | replay portability across builds, other profiles, TAS-style frame-exact claims on hardware |

Tier choice reasoning: `regression-pinned` is honest for CORE-05 because the "oracle" is an
earlier run of this same project (the template says exactly this); it must not be
upgraded. A shared fixture author oracle for SAVE-01/02 is `self-checking-fixture` per the
template ("the fixture author is the oracle") only if the guest itself reports pass/fail
via the verdict grammar; otherwise use `unit-tested`. Decide per fixture at plan time.

**Evidence.** `docs/claims/TEMPLATE.md` tiers and reserved words; `docs/claims/phase08.md`
(phase-wide claim used `unit-tested` for one authored case and a long `not_claimed`);
`scripts/check-claims.sh` enforces the `statement` byte-for-byte render.

**Failure modes.** Reuse of Phase 8's template with `oracle_kind` misnamed; a claim block
that does not render byte-for-byte fails `gov.claims.repo` only at the full gate (seconds-long
local check exists: `sh scripts/check-claims.sh --files docs/claims/phase09.md` [U: confirm flag use]).
Overclaim risk: SAVE-01 "supports the standard controller" is a reserved word.

**Authored-evidence plan.** Draft the claim block skeleton in Plan 1 with `pass/total`
counts left to Plan 8 so the checker runs early on the skeleton; fill counts at qualification.

**Would change it.** Owner decision to permit hardware-derived vectors: out of scope.

**Reversibility.** High (docs only), but claim `manifest_digest` and `build_id` must be refreshed at qualification.

### D-F5. Security review of new untrusted-input surfaces

New surfaces: (1) input script (existing `t64.input`, extended if needed), (2) save-file
import, (3) state-file import, (4) replay-file parse, (5) runner CLI paths for all of
these, (6) export destinations. Uniform rules, **RECOMMENDED as a Phase 9 threat-model
checklist (STRIDE rows numbered like Phase 8's T-08-xx, ASVS L1 with L2 depth on
publication and import)**:

1. **Parse before allocate; cap before allocate.** Total byte cap checked against the
   file's reported size and the snapshot's actual size (reuse `t64_verified_input_prepare`
   so size and SHA-256 are verified on an unlinked private copy, [C]
   `runner/t64_verified_input.h`). Each count, length and offset field is validated against
   remaining bytes and a per-field maximum before any allocation or copy; use checked
   arithmetic (`a > MAX - b`) not wraparound. Save media sizes are closed sets (EEPROM 512 B and 2 KiB; SRAM a fixed size: cluster B confirm [U]); any other length is refused. State blob
   size is bounded by the profile (RDRAM size plus fixed device state) and the header's
   declared size is checked against that exact expectation, not trusted.
2. **Validate everything, then commit.** Import is transactional: decode into a scratch
   candidate (or validate in place against a shadow), check every field's range and
   cross-field invariants (event queue ordering, token/sequence limits, pointer-free), then swap.
   Refusal leaves the machine bit-identical; assert with the existing refusal-snapshot
   pattern ("Refusal assertions compare every present machine observation", `machine-contract.md`).
3. **No overlap with live storage.** Reuse the Phase 8 range-overlap guard
   (`t64_internal_make_range`; `core/t64_ai.c` `t64_audio_copy`) for export/copy spans: caller buffer vs machine memory, image, RDRAM, and result vs data.
4. **Identity binding, not filename trust.** State and replay headers bind profile id/hash,
   build id, content hash, state/format version; mismatches are refused (a replay or state
   for another profile/build/content must not run). A hash proves identity, not rights.
5. **No-clobber exports and safe publication.** Reuse the Phase 8 transaction
   (`runner/t64_raw_capture.c`: parent-FD walk, `renameatx_np RENAME_EXCL` / `renameat2 RENAME_NOREPLACE`, 0700
   staging, mode 0600 files, identity-checked cleanup, unsupported platforms fail closed)
   for save export, state export and replay record. Do not write a second publisher.
   Import side: open read-only with `O_NOFOLLOW`-class handling, `fstat` regular-file check
   on the descriptor, copy to private snapshot; never re-open by path after validation (TOCTOU).
   Symlink and special-file (FIFO, device) inputs refused.
6. **Resource and time bounds.** Event count caps (inherit 128 or justify a new cap),
   replay length cap (guest ticks) so a replay cannot spin for unbounded host time;
   deadlines remain at the supervisor.
7. **Determinism leakage.** Exported blobs contain no host paths, times, hostnames or
   pointers; padding zero-filled (also the libretro guidance, fact 8); reserved fields must
   be zero on import or refused.
8. **Fuzz and mutation.** For each parser: (a) a bounded structure-aware mutation test
   (truncate at every byte offset of a small valid artifact; flip each header byte;
   inflate each length/count to boundary values: 0, 1, max, max+1, 2^32, 2^64-1) run in
   the gate (should be seconds); (b) a longer corpus run under ASan/UBSan in the ASan preset
   only. Existing precedent: `unit.json-fuzz`, `unit.result-seeds`, seeded tests under
   `tests/seeded`. Each mutation must reach its **specific** refusal (assert the reason),
   not just a nonzero exit (`AUTONOMY.md` "refusal matrices ... every intended case reaches its specific refusal").

**Evidence.** `runner/t64_input.h` (caps and statuses); `runner/t64_verified_input.h`;
`docs/spec/raw-capture.md` publication section; `.planning/phases/08-audio-interface-and-raw-capture/08-SECURITY.md`
(28 threats closed, ASVS L1, the low finding about a `..` component in a gate path check);
`AUTONOMY.md` mutation rules.

**Failure modes.** Import allocation sized by attacker header; integer overflow in
`offset + length`; restore that partially mutates before failing; export that
follows a swapped directory; a replay that references a different build but runs anyway;
fuzz that "passes" because every mutant fails at the magic check (shallow coverage):
mitigate with mutants that keep the magic and checksum valid (fix up integrity fields after mutation) so deeper checks run.

**Authored-evidence plan.** Security control rows per plan; independent security audit
(`gsd-secure-phase`) after Plan 7, before Plan 8's qualification, so a finding does not
invalidate qualified evidence.

**Would change it.** Choosing a compressed or variable-length state format (decompression bomb surface): not recommended; keep raw fixed layout.

**Reversibility.** High before code; rules are cheaper to apply than to retrofit.

### D-F6. Rights and privacy posture for saves, state blobs and replays

**RECOMMENDED.**
- Saves, state blobs and replay files are **private by default**: written only under an
  ignored `build/` or an owner-selected local path; never uploaded by CI; never committed.
  A state blob embeds RDRAM contents derived from a ROM (rights-bearing, even for an authored
  ROM it is part of the unpublished fixture build); a save from a commercial game may carry the
  game's data. [I] Even for authored fixtures keep blobs local; publish only hashes and a
  bounded projection (counts, digests, sizes), as Phase 8 did for raw capture
  (`docs/spec/raw-capture.md` "do not commit or upload ... without rights review";
  CI uploads only `manifest.json` and bounded JSON, `ci.yml` upload step).
- Extend the existing scans to the new surface: add the new docs and evidence projections
  to the tree `rights-scan.sh`/`pii-scan.sh`; add the new artifact extensions to the
  "tracked raw artifact" git grep that the Phase 8 security audit used; extend the CI
  upload allowlist test in `scripts/check-ci-yml.sh` so a state/save/replay path in an
  upload step is refused (mutation control for the upload list, as Phase 8 did).
- Exported artifacts must not carry host paths or usernames (header fields are fixed identity
  fields only). Run the cheap tree PII scan before any evidence commit
  (`AUTONOMY.md` Plan09-evidence lesson).
- Replay files bind to content hash, so they identify a ROM without containing it: record
  only digests; document that a hash is not rights.

**Evidence.** `AGENTS.md` ("saves ... rights-bearing"); `AUTONOMY.md` section 4 stop 1;
`.github/workflows/ci.yml` upload paths (JSON only); `08-SECURITY.md` (git grep for raw extensions).

**Failure modes.** A debugging helper dumps a save into the evidence tree; a CI step globbing
`build/evidence/**`. Mitigation: explicit file lists in upload steps (already the pattern) and a scanner
rule.

**Authored-evidence plan.** Controls that inject a `.sav`/state/replay path into the upload list and
into a tracked file and confirm both scanners refuse.

**Would change it.** Owner documenting rights for a specific save corpus: then a per-item exception, not a rule change.

**Reversibility.** High.

### D-F7. CI/CD efficiency and the bound-source cycle trap

**Facts.** The gate is about 129 s Linux / 275 s macOS (F-100). Phase 8's cost came
from qualifier/CI/scanner changes after native qualification and late verifier tails.

**RECOMMENDED rules (to be written into the Phase 9 plans' acceptance):**

1. **Freeze-order for the bound set.** Land all qualifier, collector, `ci.yml`,
   `check-ci-yml.sh`, `check-test-inventory.sh` and `scripts/qualify-*.py` edits in
   **Plan 1 and Plan 2** (a "harness skeleton" with a Phase 9 inventory, workflow lane and
   qualification stub that fails closed on zero cases), then treat them as frozen: later
   plans add rows to data files only (case JSON, inventory list) and never edit
   scanner/CI logic unless a concrete failure demands it, in which case that is a
   declared qualification-reset event. Plan 8 touches no logic file. [I] This is the single
   largest lever; Phase 8 did the opposite (logic landed in Plan 5 task 1).
2. **Cheap consistency checks first (seconds each), in this order, before any long suite:**
   `sh scripts/check-ci-yml.sh`; `sh scripts/check-test-inventory.sh --preset gate`
   (+ `--selftest`); `sh scripts/check-claims.sh`; rights/PII tree scans; fixture
   build-only identity check; the new case-inventory JSON-versus-binary `--list-cases`
   check; `ast.parse` for Python edits (not `py_compile`); the `gcc:13` read-only,
   network-disabled `-Werror` check of changed C translation units
   (`AUTONOMY.md`: Plan05-04 Clang-clean macro failed GCC13; Phase 8 hit it again).
   Wire these into one local command (an existing alias if one exists, else documentation
   only; do not add a new wrapper unless the owner's plan allows).
3. **`act` pre-flight for Linux** (`act -l`, `-n`, `--container-architecture linux/amd64`
   on Apple Silicon, reuse an image already on disk). `act` is never hosted evidence and cannot run macOS.
4. **Replay verifier tails against real retained artifacts early.** In Plan 2 (not Plan 8),
   run the new collect/verify tail against the first real hosted artifacts of Plan 1 or 2
   (a push is cheap there) and keep a real two-platform pair on disk to build comparison
   controls (F-098; `AUTONOMY.md` "build cross-platform comparison controls from real
   two-platform data"). Platform-invariant comparison only (guest outputs, digests that
   do not embed build identity); build-identity hashes differ per platform by design.
5. **Hosted cost budget.** New always-on hosted additions in total: target at most about
   +40 s on macOS and +25 s on Linux (inference from fixture build seconds, ASan-excluded).
   Keep out of hosted jobs: ASan/UBSan full suites, GCC13 route, fuzz corpora longer than
   the bounded mutation test, any measurement benchmark, the long `replay-mix` soak. These
   stay local/native or in a scheduled lane (`AUTONOMY.md`: "Long timing/stress suites ...
   belong in scheduled or release lanes"). State export of a 4 MiB RDRAM image costs about
   milliseconds in memory; [U] confirm when hashing and mutation loops multiply it (a
   byte-flip loop over a 4 MiB blob is not acceptable in the gate: mutate header/device
   regions and sampled RDRAM offsets, with exhaustive offsets only for small artifacts).
6. **Do not add CI jobs.** Keep two jobs, ten-minute limits, `ctest` jobs 4. Take the
   existing low-risk candidates when they touch the same files and *before* the freeze
   point: dedupe `repo.rsp-no-hle` (F-097) and look at macOS `test-inventory-selftest`
   (F-100). A docs-only gate lane (F-099) stays declined unless the owner wants it; if taken,
   land it in Plan 1 with the freeze, not later.
7. **Retention discipline.** Copy owning `LastTest.log` before the next CTest/discovery;
   unique absolute JUnit per manual batch; commit sanitized, portable records only
   (`AUTONOMY.md` section 1-2).
8. **Hosted proof and bounded retries.** One hosted cycle per freeze; do not rerun a passing
   pair for bookkeeping; commit pushes pinned to the verified SHA.

**Evidence.** `docs/ci-reviews/LOG.md` F-096..F-100; `.github/workflows/ci.yml` (two jobs, 10 min,
Binutils cache); `scripts/gate.sh` steps; `AUTONOMY.md` section 2 CI/CD paragraph.

**Failure modes.** A required qualifier fix after freeze (declared reset costs about one
native plus one hosted cycle, 15 minutes by F-098); the cache key miss when
`scripts/build-boot-fixtures.sh` changes (rebuilds Binutils, +85 s Linux / +179 s macOS):
do not touch that script in Phase 9 (fixture builds must not edit it; new ROMs get their own
build script or a new function in a separate file, as `build-audio-fixture.sh` did).

**Authored-evidence plan.** Mutation controls on the CI policy scanner for any new lane or
step (command, permissions, uploads, action pin, bounds) per `AUTONOMY.md`.

**Would change it.** A measured hosted time over the budget: move more to native-only evidence.

**Reversibility.** High.

### D-F8. Runner/CLI surface (provisional; Phase 10 owns the freeze)

**RECOMMENDED minimal provisional flags**, all behind the existing real-mode runner and
the supervisor passthrough used for `--raw-capture-dir` (`docs/spec/raw-capture.md`), all
rejected before guest start on any duplicate, malformed or inconsistent combination, and
documented as **provisional, not an ABI promise** with an explicit version string in each file format:

- Input: keep the existing `t64.input` schedule path; extend its schema only if needed (for
  example more ports are out of scope).
- Save: `--save-device <none|eeprom4k|eeprom16k|sram>` (explicit, default none; never
  derived from header), `--save-import <path>` and `--save-export <path>` (no-clobber).
- State: `--state-export-at <tick> <path>`, `--state-import <path>`.
- Replay: `--replay-record <path>` and `--replay-play <path>`.

At the C library level, keep new entry points in the existing facade style
(`core/include/tendies64/t64_machine.h`: caller-owned buffers, required-size reporting,
atomic refusal, serialized with advance), documented as pre-freeze. [I] Prefer size-query
plus copy-out for export (the Phase 8 `audio info/copy` and `output_drain` seam) so the
core never opens files; the runner owns all file access (AGENTS: durable storage separate from the machine).

**Why.** Phase 10 requires an installed consumer and freezes C-ABI "lifetime, profile selection,
transactional admission, timed input, bounded advance, output rings ... reset, unload"
(`ROADMAP.md` Phase 10); making Phase 9 shapes freezable-looking now is the risk.

**Failure modes.** CLI sprawl and combinations: reject conflicting flags; interaction matrix
test (`--replay-play` with `--state-import`, with input script, etc.): refuse all combos not
explicitly defined. Supervisor `RLIMIT_FSIZE` of about 68 KiB (documented Phase 8 limit) will
kill a 4 MiB state export with `SIGXFSZ`: **this is a known blocker for state export from the
real runner** and the limit mechanism must be raised for export directories in Plan 1/2 (frozen
harness), not discovered in Plan 7. [C] `docs/spec/raw-capture.md` "Known limit".

**Authored-evidence plan.** A tiny real-runner control that writes a state-size file under the
supervisor in Plan 2 (cheap actual startup before the long work).

**Would change it.** Cluster C proposing in-core file handling: reject.

**Reversibility.** High (provisional).

### D-F9. Compatibility policy interplay, deferred items

- **Preserve** all prior profiles and their descriptor/state versions and digests; add the
  new composite profile only (D-F2). Old-profile controls assert SI/PIF/save windows still
  refuse (`docs/spec/machine-contract.md` lines 107-114 become profile-conditional).
- **Digest vs serialization.** The canonical digest remains authoritative; a state export is a
  separate format with its own version and a header carrying the digest. Do not redefine the
  digest domain for old profiles.
- **FlashRAM, Controller Pak, Rumble**: deferred to v2 (EXP-04, EXP-05; `ROADMAP.md` v2).
  Required handling now: refusal with defined postconditions (SAVE-04), a stable
  "unsupported device" code distinguishable from "unselected device", and tests that a request
  for FlashRAM/Pak (joybus accessory command, PI domain 2 flash command sequence) is refused
  without state change. Do not implement stubs that appear to work.
- **Explicit selection** is the design invariant: no heuristic from title or header
  (REQUIREMENTS SAVE-02). Add a control where the same ROM bytes under different explicit
  profiles behave differently only through the selection.

**Failure modes.** A refusal path that silently succeeds (reading all 0xFF) is a false support
claim: tests assert the specific status and unchanged digest.

**Reversibility.** High.

### D-F10. Verification slicing: cheap-first order inside each plan

Use the Phase 8 pattern: formal RED (tdd=true tasks, first behavioral assertion, not a
setup failure), cohesive implementation, then ladders. Per plan, in this order:
(1) fixture build-only identity and rights check (seconds); (2) schema/inventory/claims/CI scanner
checks; (3) `gcc:13` read-only `-Werror` on changed C units; (4) actual-consumer accepted
operation through the real runner with the authored fixture (one happy path, cleared input);
(5) focused CTest selection with the existing four-job preset; (6) ASan selection with the same
suite; (7) wave-level old-profile regressions; (8) full gate only in Plan 8, except one gate run
at the end of Plan 2 (to prove the frozen harness) and one at the end of Plan 5.

**Why.** Each late finding of Phase 8 (stale inventory, scanner drift, missing include path)
had a seconds-long local check (`AUTONOMY.md` section 2 CI/CD paragraph).

---

## 2. Recommended plan list, waves and acceptance

Sequential waves; one executor/plan at a time; summary and state by orchestrator.
Effort is in **elapsed active hours** calibrated on Phase 8 (plans 01-04 each 1 to 3 h
of commit span; qualification about 3 h of active work plus idle). [I] The estimate
is low-confidence: Phase 8 had one device; Phase 9 has about four new state-bearing devices
plus a serializer.

| Plan | Wave | Requirement(s) | Vertical slice | Owning tests (working names) | Estimate |
| --- | --- | --- | --- | --- | --- |
| 09-01 | 1 | SAVE-01 (tracer), CORE-02 (ledger) | New composite profile and descriptor/state bump; SI registers, PIF RAM window and one standard controller poll end to end: `t64.input` event consumed by PIF, authored ROM reads button/axis words through fetched CPU loads/stores and guest output; scheduler ledger of all existing events; **harness skeleton frozen here** (inventory lane, `qualify-phase09` stub failing on zero cases, `ci.yml`/scanner admission, supervisor file-size limit for export dirs); claim skeleton | `unit.core.si.tracer`, `runner.pad-poll`, `controls.real.pad-wrong-value`, `unit.core.sched.ledger`, `unit.core.si.profile` | 4-6 h |
| 09-02 | 2 | SAVE-01, SAVE-04 (joybus part), CORE-02 (SI part) | SI/PIF register and DMA matrix, joybus command set for the standard controller (info, status, reset), unknown command / unknown channel refusal postconditions, MI SI interrupt, SI DMA timing as scheduler events, equal-tick SI vs AI/VI/PI order; **replay verifier tail against real artifacts from Plan 1 hosted run**; one early real-runner file-export control | `unit.core.si.registers`, `unit.core.si.joybus`, `unit.core.si.timing`, `unit.core.si.refusals`, `harness.phase09-collect-real` | 4-6 h |
| 09-03 | 3 | SAVE-02, SAVE-04 | Explicit save-device selection; EEPROM 4/16 Kbit with block and busy-window rules over joybus; SRAM on PI domain 2; refusal of unselected/unknown/deferred (FlashRAM) requests with unchanged digest; save state in descriptor/digest/reset audit | `unit.core.save.eeprom`, `unit.core.save.sram`, `unit.core.save.selection`, `runner.eeprom-rw`, `runner.sram-rw`, `controls.real.save-wrong-byte` | 5-8 h |
| 09-04 | 4 | SAVE-03, SAVE-04 | Save import/export seam and runner persistence: closed sizes, endianness handling, transactional import (refused import leaves digest unchanged), no-clobber export reusing the Phase 8 publisher, snapshot input; bounded mutation test; rights/upload scanner controls | `unit.core.save.import-export`, `runner.save-roundtrip`, `controls.real.save-import-refused`, `unit.save.mutation`, `harness.upload-allowlist-save` | 3-5 h |
| 09-05 | 5 | CORE-02 | Cross-device ordering matrix: coincident events among CPU, RSP, RDP, VI, AI, PI, SI, MI and save-busy events; whole/split advance; partition and interleaved-instance isolation; pinned quantum in profile identity and a quantum-change identity control; finalize event-kind registry (handler by id) | `unit.core.sched.cross-device`, `unit.core.sched.partition`, `unit.core.profile.quantum-identity` | 3-5 h |
| 09-06 | 6 | CORE-05 | Replay record/play: container binding profile, build and content hash, input stream (reusing `t64.input`), checkpoint digests; play-time refusal on any binding mismatch; deterministic across split advances; fuzz of the replay parser; claim `regression-pinned` | `unit.replay.format`, `unit.replay.mutation`, `runner.replay-mix`, `controls.real.replay-mismatch` | 4-6 h |
| 09-07 | 7 | CORE-04 | Versioned state export/import covering every device in the profile (CPU, RSP, RDP, VI, AI, PI, SI, MI, scheduler queue, save media); pointer-free event registry; atomic import; round trip digest; continuation equality against Plan 6 replay digests; importer mutation and caps | `unit.core.state.roundtrip`, `unit.core.state.continuation`, `unit.core.state.mutation`, `unit.core.state.refusal`, `runner.state-export` | 6-10 h |
| 09-08 | 7 | all | Independent review, security audit, then one cohesive qualification: native normal, ASan/UBSan, GCC13, both hosted jobs; evidence doc, claims, no logic edits | `harness.phase09-qualification` (frozen in 09-01), full existing inventory | 3-5 h active plus waits |

(Plans 7 and 8 share wave numbering only because 8 depends on 7; the list is sequential.)
**Total estimate: about 32 to 51 active hours; plan for 4 to 6 working days including
two to three qualification cycles.** [I]

Riskiest plans, in order: **09-07 state** (every device, pointer-free events, importer attack
surface, size limit), **09-03 EEPROM timing and block rules** (hardware facts partly unknown,
cluster B), **09-01** (profile bump plus harness freeze; mistakes here propagate to all), **09-08**
(qualification cycles).

Per-plan acceptance template (each plan must show, with retained original command and result):
1. Fixture build-only identity/rights check passes; wrong-value control reaches its comparator.
2. Focused CTest group passes in normal and ASan presets; discovery uses `--no-tests=error`;
   case list equals the JSON inventory exactly.
3. Old-profile pins and refusal controls still pass (no identity drift).
4. For parsers: mutation test where every mutant reaches its specific refusal and refusals leave
   the instance/save digest bit-identical.
5. Scanners clean: check-ci-yml, test-inventory, claims, rights, PII, `gcc:13` on changed units.
6. Summary lists commands, results, limits; no hardware claim.

Specific acceptance for the final plan: success criteria 1-4 of the Phase 9 roadmap entry are
each mapped to a named test row and a claim block; both hosted jobs pass on the exact
qualified SHA; verifier replays against retained hosted artifacts succeeded before the push.

---

## 3. Risk register (top 8)

| # | Risk | Likelihood / impact | Mitigation | Early-warning signal |
| --- | --- | --- | --- | --- |
| R1 | State export misses a future-affecting field, so the digest round trip passes but continuation diverges | M / H | Continuation-equality test (export at T, import, advance N, compare to uninterrupted run and to replay digests), a per-device reset/digest/serialize audit table updated in every plan, mutation of each field | Digest equal but a later digest differs; any new device field not in the audit table |
| R2 | Bound-source churn: qualifier/CI/scanner edit after native qualification | H / M (15 min per cycle each) | Freeze harness logic in Plans 1-2; data-only changes after; declared reset event if unavoidable | A diff touching `scripts/qualify-*.py`, `ci.yml` or `check-*.sh` after Plan 2 |
| R3 | Verifier tail meets real hosted data late (repeat of F-098) | M / M | Replay collect/verify on real artifacts in Plan 2; real two-platform pair controls | First hosted run after Plan 8 start reveals a verifier defect |
| R4 | Hardware-fact uncertainty (joybus response timing, EEPROM busy window, block rules) leads to wrong or over-claimed behavior | H / M | Explicit software-profile policy, named as such; authored witnesses; tier `unit-tested` or `self-checking-fixture`; no hardware claim; clusters A/B record unknowns | A spec sentence without a primary source; a claim using reserved words |
| R5 | Import/parse attack surface (state, save, replay): allocation or overflow before validation, partial mutation, shallow fuzz | M / H | D-F5 rules, specific-refusal mutation tests, checksum-fixed mutants, ASan corpus run, independent security audit before qualification | A mutation test whose mutants all fail at the magic check; any refusal without a digest-unchanged assertion |
| R6 | Hosted time growth past the 10-minute job limit (macOS about 275 s now, plus fixtures, state hashing, inventory selftest growth) | M / M | Hosted budget (+40 s macOS), long/ASan/fuzz native-only, bounded mutation loops, take F-097/F-100 savings first | macOS job over about 400 s on any push |
| R7 | Supervisor `RLIMIT_FSIZE` (about 68 KiB) kills state/save export from the real runner | H (known) / M | Raise limit for export directories in the frozen harness (Plan 1/2), with a real-runner control; keep fail-closed | `SIGXFSZ` in the first export control |
| R8 | Rights/privacy leak: save, state or replay artifact tracked or uploaded; workstation paths in evidence | L / H (stop condition) | Private-by-default, explicit upload lists, scanner rules and mutation controls, tree PII scan before evidence commits, repo-relative paths | Scanner finding or a `.sav`/state path in an upload list |

Secondary risks noted but not top-8: GCC13 `-Werror` sites (use the gcc:13 read-only check per
changed unit); stale pinned inventory (seconds-long inventory check before each commit); profile hash
pinned before the descriptor is final (symbolic accessor, D-F2); scope creep into FlashRAM/Pak/ABI
(refuse and defer, D-F9/D-F8); plan-count coordination cost (merge Plan 2 into 1 if trivial).

---

## 4. Unknowns that need the owner (or another cluster)

Most are routine and can be resolved by the synthesized default under the delegated autonomy;
these are the ones worth batching:

1. **[U] Save file format at the runner boundary.** Raw bytes only (recommended, no
   interchange claim) vs. a named interchange format. Recommend raw bytes plus a documented
   byte-order rule; owner input only if third-party save interchange becomes a goal.
2. **[U] Whether the owner wants a docs-only CI lane (F-099).** Declined so far; a decision is
   cheap if taken at Plan 1 and expensive afterward.
3. **[U] Replay and state formats: binary vs. JSON header plus binary payload.** Recommend a fixed
   binary layout with a JSON-free parser for state (small attack surface) and reuse of the
   existing `t64.input` JSON for the input stream inside replays; cluster C confirms.
4. **[U] Supervisor file-size limit policy** (raise for export dirs vs. runner writes to an inherited
   FD): a security tradeoff (a larger `RLIMIT_FSIZE` reduces a containment guard). Default: raise
   narrowly for export directories only.
5. **[U] SRAM size and EEPROM block/busy specifics** (cluster B) and **joybus timing** (cluster A):
   any unknown becomes an explicitly named software-profile policy, not a hardware statement.
6. **[U] Hosted measurement** of fixture count and state-export cost: measure on the first hosted
   push (Plan 2) before committing to the budget in D-F7.
7. **No owner stop is expected** if the work stays authored and local. The standing stops
   (ROMs/saves/firmware rights, physical hardware, PII, publication) are unchanged; the one
   place a stop could trigger is any proposal to use a commercial save, ROM or title to
   validate EEPROM/SRAM behavior: do not.

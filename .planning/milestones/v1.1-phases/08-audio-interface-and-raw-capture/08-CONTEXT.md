# Phase8: Audio Interface and Raw Capture - Context

**Gathered:** 2026-11-18
**Status:** Ready for planning; owner approved sequencing from qualified authored Phase7 foundation

<domain>
## Phase Boundary

Deliver AUD-01/02/03 and RUN-06: deterministic AI DMA/queue/interrupt/cadence,
exact authored PCM evidence and actual headless raw A/V capture. Phase7's
oracle acceptance remains incomplete and stopped. No player, audio resampling,
RSP microcode expansion, ABI freeze, Phase9 save/replay export or release.
</domain>

<decisions>
## Implementation Decisions

### Real consumer and timing
- **D-01:** Lead with a build-only rights-cleared authored diagnostic making
  fetched CPU writes to ordinary AI registers. Transport exact literal signed
  stereo samples across a DMA boundary into the actual raw observer and real
  runner. Derive expected samples independently; a same-setup wrong-sample
  control reaches its comparator. Run cheap accepted setup/identity/rights and
  representative consumer checks before expensive suites. Core-only output,
  non-silence, hashes alone or a fixed observation record do not prove RUN-06.
- **D-02:** Sample progress comes only from canonical guest time. Use exact
  bounded rational cadence in the existing62.5MHz timeline; NTSC audio clock
  is48,681,812Hz with divisor DACRATE+1. Batching may materialize output but
  cannot hide partial-DMA semantic sample boundaries or defer all RAM reads
  until completion. Resolve primary register masks, enable/reset, source reads,
  latching and same-tick order in phase research; explicitly name deterministic
  software-profile rules where hardware evidence does not resolve a choice.
- **D-03:** Two-entry DMA queue and ai_full1→0 MI AI interrupt are locked
  primary-derived requirements, not an IRQ per submission/start. Verify the
  one-buffer and two-to-one queue cases. The documented boundary failure
  ((address+length)&0x3fff)==0x2000 is a specific condition; do not generalize
  it into an8KiB carry model. Unresolved hardware status effects stay unknown
  or qualified software policy. No emulator implementation or physical oracle.

### Ownership and observation
- **D-04:** Use a separate bounded lossless audio packet queue and info/copy
  seam analogous to VI. Decide complete packet granularity before coding.
  Queue-full leaves the same producing event pending: no sample phase,
  source/DMA cursor or AI/MI effect commits until capacity permits resume.
  No drop/overwrite/reorder; host drain advances no guest time. Short/invalid
  destinations refuse atomically with required-size reporting as applicable.
  Verify partial DMA, source-mutation witness for selected read policy,
  split/whole advance, drain cadence, interleaved machines and same-tick ties.
- **D-05:** Keep all future-affecting AI state in the machine and canonical
  state/digest; caller memory, file state and host observer cursors stay out.
  Apply the approved compatibility policy through a changed composite profile
  and descriptor/state versions where required. Preserve all old identities,
  bytes and refused AI behavior. No profile-per-device or public ABI promise.

### Actual capture and evidence
- **D-06:** The runner owns raw files: numbered actual RGBA frames and explicit
  signed PCM bytes plus a JSON manifest. Specify byte order/channels/packing,
  geometry/stride, sample index/rational rate/tick bounds, sequence and exact
  profile/build/content/input identities. Capture both real observer paths,
  not fixed-size test observations or reconstructed expected bytes. Use raw
  packet bundles without codec/container dependencies; PAM/WAVE can wait for
  an actual interoperability need. A/V metadata relates separate streams.
- **D-07:** Finalize a requested capture all-or-nothing. Existing/unsafe targets
  stay untouched; incomplete files cannot appear as a successful final bundle.
  Define staging/finalization and bounded disk/output/error effects first.
  Missing/zero/truncated data, output-full/resume and I/O/capacity failures
  are explicit, never success over a partial bundle. Output errors remain
  host-side and do not invent guest progress or mutate canonical core policy.
- **D-08:** Keep C17/CMake/CTest and current dependencies/jobs/limits. No new
  codec, serializer, cache, package install or production thread. Preserve
  owning LastTest before CTest/discovery and after success/failure; manual
  batches get unique absolute JUnit paths. Qualify cohesive source changes,
  review before final combined suites, and reuse unchanged exact evidence.
- **D-09:** Existing two hosted jobs, ten-minute limits and CTestjobs4 remain.
  Generated ROMs and raw capture files remain local/build-only. Publish only
  a bounded cleared projection after rights/privacy checks, retaining original
  command/outcome/producer/sample evidence privately. Authored software tests
  are not silicon, general compatibility, oracle permission or VID-04 evidence.
  D2/D4/D5/D6/D7, private windows and physical/paid/destructive stops persist.

### Agent discretion

Choose concrete internal representations, queue sizing, packet cap, provisional
schemas and profile version numbers under these contracts and primary evidence.
Research unresolved fields once; begin the smallest verifying implementation
promptly. Reopen a choice only for a concrete contradiction or failing witness.
</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- Video info/copy-out and packet metadata in core/include/tendies64/t64_machine.h.
- Per-instance scheduler/MI/profile/digest, bounded VI producing-event admission
  and atomic copy-out in core/t64_scheduler.c, t64_vi.c and t64_digest.c.
- Existing fixture builder/manifest, real runner/supervisor and retained
  raw-evidence helper provide a consumer and strict evidence patterns.

### Integration Points
- AI bus currently has a warm placeholder; implement real behavior under the
  new composite profile while preserving prior refusals.
- runner/t64_real.c currently exports a fixture-specific4x3 observation. RUN-06
  needs general packet consumers/files, not expansion of that fixed record.
</code_context>

<specifics>
## Specific Ideas

Owner: “gameplan this and follow ur recs”. Three independent decision clusters
and an adversarial check are synthesized in08-DISCUSSION-LOG.md. Source latching
must have an authored in-flight RAM mutation witness after its policy is chosen.
</specifics>

<deferred>
## Deferred Ideas

Merged A/V envelope, PAM/WAVE/codec formats, host playback/resampling, DSP/RSP
microcode breadth, Phase9 replay/state export, installed ABI consumer, public
raw capture distribution, physical testing and release remain outside Phase8.
</deferred>

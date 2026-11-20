# Phase08 discussion proposal

Draft for adversarial check; owner delegated routine reversible choices and
approved Phase8 sequencing. Requirements AUD-01/02/03 and RUN-06 are unchanged.
Phase7/VID-04 and oracle/private/physical/paid/release stops remain in force.

1. Lead with a cleared authored PCM diagnostic through fetched CPU AI register
   writes, ordinary bus/scheduler/MI and actual raw-audio observer into the real
   runner. Compare literal independently derived signed stereo samples across
   a DMA boundary plus metadata; wrong-sample control must reach comparator.
   Cheap actual accepted setup precedes full/sanitizer suites.
2. Sample progress is guest-timed, with exact rational NTSC cadence from
   48,681,812/(DACRATE+1) against the existing62.5MHz guest timeline. Batching
   may optimize materialization but cannot delay semantic sample boundaries or
   read source RAM only at DMA completion. Source latching, DMA disable/reset,
   DACRATE writes while active, ambiguous encodings and equal-tick ordering
   need sourced facts or named deterministic software-profile policy.
3. Two-entry queue and MI AI interrupt at ai_full1→0 are locked. Primary AI
   header/manual sources define address-before-length and a specific failure
   predicate ((address+length)&0x3fff)==0x2000, not a generic8KiB carry rule.
   Detailed masks/status/latching/boundary effects remain phase research.
4. Add separate bounded lossless raw-audio packets and info/copy-out analogous
   to VI. Reserve complete producing capacity before its canonical boundary;
   output-full leaves a resumable pending event with no sample loss/reorder.
   Copy refuses atomically on invalid/short spans, host drain advances no guest
   time. Include every future-affecting AI field in canonical digest/state;
   exclude caller memory and host observer positions. Set packet granularity
   before implementation and prove partial DMA, drain/partition/instance and
   same-tick behavior. Avoid a merged A/V queue absent a real consumer need.
5. Runner-owned raw bundle: numbered RGBA packets and explicit signed PCM bytes
   plus JSON metadata with geometry, stride, channel/byte order, sample index,
   rational rate, tick bounds, profile/build/content/input identities and end
   status. No codec/container dependency. Stage and finalize only complete
   captures; distinguish output-full resumption, missing/zero/truncated data,
   disk/capacity failure and refused existing/unsafe destinations. Define
   incomplete-capture visibility and final publication transaction first.
6. Apply existing compatibility policy: preserve old profiles/bytes/refusals;
   change the composite profile and descriptor/state versions only where new
   AI semantics require it. No ABI freeze, host player, resampling, RSP audio
   microcode support, save/replay export, new dependency or production thread.
7. Keep existing two CI jobs, ten-minute limits and CTestjobs4. Build-only
   authored fixture rights/identity gate precedes guest use. Generated ROMs and
   raw capture files remain local; any public evidence is a bounded reviewed
   projection scanned for rights and privacy. Retain exact original command,
   producer/sample bytes and owning LastTest/JUnit before later discovery.

Decision-focused sources:
- AI timing advisor: Nintendo RCP header and AI buffer reference:
  https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm
  https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/n64man/os/osAiSetNextBuffer.htm
  DACRATE latching and full-queue raw-write effects remain unknown.
- Ownership advisor: core public machine header video info/copy seam;
  core/t64_scheduler.c producing-event backpressure; core/t64_vi.c copy-out;
  core/t64_digest.c canonical output. Shared envelope would couple A/V stalls.
- Capture advisor: runner/t64_real.c current fixed4x3 observation vs real packet
  API; raw bundle keeps timing/identity without PAM/WAVE sidecars/writers.

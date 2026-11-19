# Phase08 bounded authored claims

This claim covers one accepted authored guest audio case: a fetched guest program
queues a literal 16-bit stereo PCM stream through the AI DMA and two-deep queue,
the machine's guest-time cadence turns it into raw sample buffers, and the actual
headless runner captures the video frames and audio buffers as files with
metadata. The expected sample values are an independently authored literal, not
the output of the code under test. It is a unit-tested result, not a hardware or
emulator-differential claim. The scope, exact bindings and limitations are in
[the evidence](../evidence/phase08-audio.md); the register, cadence and capture
contracts are in the [audio interface](../spec/audio-interface.md) and
[raw capture](../spec/raw-capture.md) specifications.

The hosted Linux and macOS jobs both executed the same capture and no-replace
finalization from the tested source and agree on the PCM literal, packet counts
and byte total. Their build identities differ by platform and are recorded in
the evidence. Raw capture files stay private; only a rights- and privacy-cleared
projection is published. A passing authored sample stream is not evidence about
audio hardware, other rates or lengths, or any commercial software. No VID-04,
hardware, release or broader compatibility result is claimed.

```t64-claim
build_id: 0.0.0+t64-build-v1-7ef62833681d7ac17886bd48d969207078f6debdd3a1c30f5cafce202fdb828d
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1
tier: unit-tested
fixture_set: audio-pcm-literal
manifest_digest: sha256:f7c1953d651c77b5121ce8e454f6c0953ed3495a911bcbb883dfd8ff09917931
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-pcm-literal-contract
oracle_revision: pcm-literal-sha256:9baa9bdf9d93a5b3795913af026fa403f0ff9d16cbd243e7e27af9e779fcd2ed
not_claimed: other builds, profiles, fixtures or command combinations; general ROM behavior; audio hardware timing, DAC output, analog quality or latency; host audio presentation or sound devices; other sample rates, DMA lengths or queue patterns beyond the authored case; oracle agreement; external diagnostics; commercial software
statement: Tendies64 0.0.0+t64-build-v1-7ef62833681d7ac17886bd48d969207078f6debdd3a1c30f5cafce202fdb828d on profile ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1: unit-tested for audio-pcm-literal @ sha256:f7c1953d651c77b5121ce8e454f6c0953ed3495a911bcbb883dfd8ff09917931 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-pcm-literal-contract pcm-literal-sha256:9baa9bdf9d93a5b3795913af026fa403f0ff9d16cbd243e7e27af9e779fcd2ed. Not claimed: other builds, profiles, fixtures or command combinations; general ROM behavior; audio hardware timing, DAC output, analog quality or latency; host audio presentation or sound devices; other sample rates, DMA lengths or queue patterns beyond the authored case; oracle agreement; external diagnostics; commercial software.
```

# Phase08 authored audio and raw-capture qualification

The reviewed source `e15624cbf435a4542d7d2dc1902f92f0ccf2de6e` passed the
complete normal gate, the full ASan/UBSan inventory and the pinned GCC 13 check
natively, and both existing hosted jobs passed on the same commit. The
exact-binding verifier (`python3 -B scripts/qualify-audio.py --verify-evidence`)
then accepted the native record, both hosted artifact archives and the combined
platform pair. The bounded claim is [phase08](../claims/phase08.md).

Evidence tier: **unit-tested**. The expected samples are an authored literal
(`pcm_literal_sha256` below); the machine under test did not produce its own
expectation. This is not hardware-derived, not emulator-differential, and uses
no commercial software, ROM, firmware or oracle.

## Native record

| Check | Observed result |
| --- | --- |
| Complete normal gate (`scripts/gate.sh`) | 179 CTests, inventory and core lint passed |
| Full ASan/UBSan CTest | 181 cases passed |
| Pinned GCC 13 route | all 39 affected C units compiled with `-Werror` |
| Audio qualification inventory | 75 cases, 22 CTests, snapshot `2f68e71e9f68f2869dad61b5054f19c9057c5ea6c7a82d06120672d5717874a8` |
| Source inputs bound | 216 files, manifest `8fbb0e192adbef2cd40b0ab06f5150c590bad2d6c276ca76a6f48f217fe14157` |
| Native record SHA-256 | `4f6337c6fd73e6601f3ad3fc45dccebf4ef3515aee957b156f2ea2d04b132006` |

Each suite retained three production `t64-supervisor` capture records (actual
accepted A/V bundle, literal PCM files, capture drain and resume), and the
verifier excluded the two supervisor-test drain/resume records by name. The
original command, exit status, LastTest log, JUnit and the executable and
library bytes are retained privately and bound by hash. Earlier failed attempts
on the way to this commit are retained too and were not replayed.

## Hosted record

Run [44542244958](https://github.com/szTheory/tendies64/actions/runs/44542244958),
attempt 1, workflow `ci`, source `e15624cbf435a4542d7d2dc1902f92f0ccf2de6e`.

| Job | Result | Duration | Artifact ID | Archive bytes | Archive digest | Audio manifest SHA-256 |
| --- | --- | ---: | --- | ---: | --- | --- |
| `gate-linux` | success | 219 s | 14537449911 | 8873 | `sha256:6a2d846e2c33cca81976518ca00d86453fdd607c442ac415c6d698218a2a9632` | `5726551d6f71fc0d9a788043a43eeb3329dd1a4805360d8e62efcbe44372cd91` |
| `gate-macos` | success | 429 s | 14538264657 | 8865 | `sha256:5ba2821bdec8585257e0cff2e35aea9da7bf536a08afb468b9bbd2d9e7a4b6cc` | `1f5aee32e6a7d52e86647f73543054e83ae05e6699af444b4a475d06b4f0d244` |

Both archives were downloaded as original ZIPs by artifact ID and checked
against the API digest, the exact run, attempt, job and commit, and the strict
member list before any public projection was read. Linux uses `renameat2` with
`RENAME_NOREPLACE` and macOS uses `renameatx_np` with `RENAME_EXCL`; each hosted
job ran the actual accepted capture and no-replace finalization, not a report of
one. The Linux and macOS records agree on the PCM literal, packet counts and
byte total. Their core build identities differ because the toolchains differ:

| Job | Core build ID |
| --- | --- |
| `gate-linux` | `t64-build-v1-63c65514e3946b7f439c6a829d11ed377ce50dbe90bf383f59303c7c8138aa76` |
| `gate-macos` | `t64-build-v1-7a1c0fa966c2105e3a7c3bb43fc79990137d1503d8e1c41ac966a50f638476cb` |

The native normal build used for the claim is
`t64-build-v1-7ef62833681d7ac17886bd48d969207078f6debdd3a1c30f5cafce202fdb828d`.

## Authored case and independently expected facts

| Fact | Value |
| --- | --- |
| Profile | `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1` |
| Fixture (`audio-pcm-literal`) SHA-256 | `cd9556444480b8b02b043207013e7ce5bf5697a1c040058bcde1348e55e80b99` (4,728 bytes) |
| Input script SHA-256 | `6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d` (54 bytes) |
| Expected PCM literal SHA-256 | `9baa9bdf9d93a5b3795913af026fa403f0ff9d16cbd243e7e27af9e779fcd2ed` |
| Captured audio | 2 packets, 16-bit stereo; literal compared byte for byte |
| Captured video | 29 packets |
| Bundle | 16,581 bytes total, manifest last |
| Guest stop | authored completion at guest tick 551,001 against bound 600,000 |

Cadence follows DACRATE and guest time only: the specification schedules each
sample boundary with checked integer arithmetic and never reads host wall time
([audio interface](../spec/audio-interface.md)). The `unit.core.ai.cadence` and
`unit.core.ai.determinism` tests and the `controls.real.audio-drain-cadence`
control exercise that. The `controls.real.audio-wrong-sample` control rejects a
wrong sample, and "non-silent" is never accepted. No run here compared different
host throttle settings against each other.

## Review and repair history

An independent source review of the Phase08 source (`08-REVIEW.md`) found
CR-01 and WR-01, and a recheck found W1, W2, I3 and I6. All were fixed and
rechecked before this qualification (`08-REVIEW-FIX.md`, `08-RECHECK-FIX.md`
in the phase directory). Several verifier defects surfaced only when the native
and hosted tails first ran against real artifacts, and each was fixed with a
control reproduced from the real format: native LastTest retention after test
discovery, multiple retained capture records per suite, ASan original
containment, the Phase 07 collector's repeated record-path line, and a
cross-platform comparison that wrongly required build-identity hashes to match.
An earlier green hosted run on `1fdd663` (run 44538801603) was not used for
binding because the collector defects were found after it; the final binding is
the run above, on the commit whose native record it matches.

## Limitations and preserved stops

- One authored case: sample rate, DMA lengths and queue patterns beyond it are
  covered only by the unit matrices in `docs/spec/audio-cases.json`, and no
  hardware or other-emulator sample stream was compared.
- No audio DAC, analog output, latency or host sound device is modeled or
  claimed; raw output stays separate from host presentation.
- Descriptor-less prohibitions in the audio and raw-capture contracts are
  flagged judgment review, not auto-verified tests, and the edge report's
  AUD-02/AUD-03 categories stay labeled unclassified in
  `.planning/phases/08-audio-interface-and-raw-capture/08-VALIDATION.md`; their
  explicit contracts are carried by plans 01 and 03.
- Raw capture files remain private. Only the cleared hosted projections were
  published as workflow artifacts, after the sanitized-output rights and PII
  scans passed.
- Phase07 VID-04 and Plan 13 remain blocked on oracle authority and the
  parallel-rdp rights audit. No oracle was executed, copied or used, no private
  diagnostic was replayed, no ROM or firmware was used, and no release, name,
  hardware or paid-service decision was made.

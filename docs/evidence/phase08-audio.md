# Phase08 authored audio and raw-capture qualification

The reviewed source `3a41734c8cc09c0e5cbcf8d32e77a4f53f756ffd` passed the
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
| Source inputs bound | 216 files, manifest `f9263789193f4cb4c15e214b8d3b18e5a2b068c7c74ade5a44226d630f7bc0e6` |
| Native record SHA-256 | `fcec400ba3582d76f3b08574fd878fe972242d2cc2e919d44703d702db7433c2` |

Each suite retained three production `t64-supervisor` capture records (actual
accepted A/V bundle, literal PCM files, capture drain and resume), and the
verifier excluded the two supervisor-test drain/resume records by name. The
original command, exit status, LastTest log, JUnit and the executable and
library bytes are retained privately and bound by hash. Earlier failed attempts
on the way to this commit are retained too and were not replayed.

## Hosted record

Run [44549490908](https://github.com/szTheory/tendies64/actions/runs/44549490908),
attempt 1, workflow `ci`, source `3a41734c8cc09c0e5cbcf8d32e77a4f53f756ffd`.

| Job | Result | Duration | Artifact ID | Archive bytes | Archive digest | Audio manifest SHA-256 |
| --- | --- | ---: | --- | ---: | --- | --- |
| `gate-linux` | success | 188 s | 14542207032 | 8873 | `sha256:b17981e3a8dbf1f593dc3611d7b63f9626649e82f5bad135d35cb3e4e5f37aa3` | `606a4e0b877c9fad5247aea5eb104129f33de4c3a4d109be155dac126c4f446e` |
| `gate-macos` | success | 507 s | 14542367221 | 8864 | `sha256:45175af3d69bc378334f3002e20e34563638fe0d7becc8299437a49bd0db9c41` | `a4b40c75759ac3e81b9c2ca6be8510fb8dc74500e27b7fae69abcee90b085713` |

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
control exercise that. The `controls.real.audio-wrong-sample` control proves that wrong
sample bytes reach the strict byte comparator, which refuses them; "non-silent"
is never accepted. No run here compared different
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
Earlier green hosted runs on `1fdd663` (run 44538801603) and `e15624c` (run
44542244958) were not used for the final binding. After the second, the
independent goal verification found that the specified rule "completing a lone
descriptor raises no MI AI interrupt" was implemented but not pinned: the
falling-edge case never acknowledged the first interrupt, so
interrupt-on-every-completion and an extra interrupt on the final completion
both survived every Phase08 suite. The case now acknowledges the interrupt and
asserts no re-raise on the 1-to-0 completion, and both mutants fail it. The
same change refuses `..` components in the gate's private JUnit hook, found by
the independent security audit. Because these edits are inside the bound source
snapshot, the final binding is the native record and hosted run above, on the
commit that contains them.

## Independent review results

The independent security audit (ASVS L1, block on high) closed all 28 declared
threats with no open item
(`.planning/phases/08-audio-interface-and-raw-capture/08-SECURITY.md`). The
independent goal verification confirmed the three success criteria and four
requirements within the authored scope
(`.planning/phases/08-audio-interface-and-raw-capture/08-VERIFICATION.md`),
reproduced the production capture, and killed seven behavior mutants; its one
unpinned behavior is the fix above. Its three judgment-tier prohibitions
(authored PCM presented as silicon accuracy, generated ROMs or captures
published, oracle/physical/paid/install/release stops bypassed) were reviewed by
the parent and none is violated: this document and the claim state the authored
and unit-tested limits, no ROM or raw capture is published, and every stop is
preserved below.

## Limitations and preserved stops

- One authored case: sample rate, DMA lengths and queue patterns beyond it are
  covered only by the unit matrices in `docs/spec/audio-cases.json`, and no
  hardware or other-emulator sample stream was compared.
- The real runner runs under a supervisor `RLIMIT_FSIZE` of about 68 KiB per
  file, so a single capture file, or the manifest (roughly 390 frames), larger than
  that ends the run with `SIGXFSZ` (fails closed, nothing published). The authored 12x7 case totals 16,581 bytes;
  larger geometries need that limit raised for the capture directory before they
  can be captured. See [raw capture](../spec/raw-capture.md).
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

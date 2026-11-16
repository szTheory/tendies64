# Phase 06 native model evidence

The native tests exercise the selected deterministic software policies. The
CPU timing table uses named T1 classes on the two profiles below; its issue and
stall mapping is an emulator policy informed by the cited VR4300 manual table.
The RDRAM protocol tests cover the authored software state transitions. Neither
set is a physical timing measurement or a hardware-accuracy result.

| Evidence class | Bound scope | Result and limit |
| --- | --- | --- |
| Native model | `unit.core.cpu-time`, `unit.core.cpu-cp0`, `unit.core.profile`, `unit.core.rdram-protocol` | Passed in the local Phase 06 native gate. Exercises fixed T1 scheduling and modeled protocol transitions only. |
| Actual guest | Tier A and Tier B cold hello, with the exact build and archived artifacts in [the receipt](phase06-final.json) | Both named profiles emit the same authored 30-byte transcript and reach their separately authenticated terminal boundary. This is one cleared guest fixture per profile. |
| Hosted authored checks | Existing hosted gates use the cleared authored hello path | They do not run or publish private BASE diagnostics. No hosted result is added to these two local profile claims. |
| Hardware observations | Physical N64 CPU, RDRAM and IPL2 behavior | Unknown; no hardware capture is claimed. |
| BASE diagnostic lanes | Required, informational and stress records | Original records and C classification stay local. A complete source classification is not a claim that every member passed. |

The [claims](../claims/phase06.md) cover only the two named Tier A/B T1
profiles, the exact archived build, the rights-cleared hello entries in the
fixture manifest, and their literal authored transcript oracle. The public
receipt binds those claims to the archived runner, test executables and
cartridge. Its source annotation was dirty when captured; it is retained as
provenance and is not substituted for the exact executable bytes.

No claim is made for other ROMs or profiles, Expansion Pak, RDP, audio,
peripherals, T2 pipeline timing, physical behavior, private diagnostic results,
external suite execution, or commercial software. Release channel and version
selection remain separate decisions.

The local receipt validator reads original cold-hello and cold-startup command
and outcome records selected by the saved CTest blocks, checks their log
digests, and compares actual transcript, retirement and terminal observations.
It does not infer results from this note. Saved-artifact verification is
separate from the live pre-use identity check, so rebuilding does not relabel
a historical executable. The initial validator's report-based comparisons and
incomplete inventory mutation were corrected before final review.

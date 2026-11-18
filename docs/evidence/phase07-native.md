# Phase07 authored native qualification

The reviewed source `2c8e9ef487792970b588b14ea253f4a706d0a861` passed the
complete normal gate, full ASan/UBSan inventory and pinned GCC13 check. The
later harness repair and exact hosted checkpointe6f5466 also pass, including
actual cross-host raw artifact verification. The
[native record](../../.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-11-NATIVE-EVIDENCE.md)
binds the exact source inputs, commands, compiler settings, build identities,
original outcomes, LastTest/JUnit and retained executable/library bytes.

| Check | Observed result | Time |
| --- | --- | ---: |
| Complete normal gate | 157 CTests, inventory and core lint passed | 77.894 s |
| Full ASan/UBSan CTest | 159 cases, no failure/error/skip, including sanitizer controls | 238.655 s |
| Existing GCC13 route | All 30 changed C files plus two test variants compiled | 23.579 s |

The native source manifest covers 505 source/build/test/fixture/spec inputs.
The final 93 normal and 95 instrumented artifacts were retained; the parent
independently rechecked all 188 original/copy pairs, lengths, SHA-256 and Git
blob IDs. Original revision annotations remain attached to their actual bytes.
No diagnostic was replayed to refresh metadata.

The complete source-derived RDP/VI denominator covers fill/copy, triangle
attributes and texture formats, combiner/blender/coverage/depth, transport and
FullSync/DP interrupt, VI timing/filters, raw output and policy identity. The
[case inventory](../spec/rdp-vi-cases.json) and its actual dispatch checks are
membership and assertion-reach checks; they are not hardware measurements.

The actual fetched guest fill path exported 24 RDRAM bytes at address `0x3000`
and a 48-byte 4×3 RGBA8 packet with stride16 through the bus and video-copy API.
The known authored color expectations are independent comparators. Public JSON
contains a canonical hex projection of those actual bytes, never a copied
expected frame or cartridge/boot bytes. Private originals remain retained.

At the initial native checkpoint, hosted raw collection was still pending. Local
mock transport verifies layout and refusal behavior only. No oracle agreement,
silicon behavior, broad ROM compatibility or VID-04 completion follows.

The first checkpoint's hosted gates passed, but its new emission entry stopped
before raw production. The [setup repair](../../.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-11-HOSTED-EMITTER-REPAIR.md)
adds a reached real-entry regression to cheap preflight. That failed run is
retained separately from the subsequent successful real raw pair.

The harness-only repair passed focused independent review, a fresh complete
normal gate in60.269s and its owning sanitizer-preset harness in2.03s. C/build/
fixture/spec/workflow inputs are unchanged; the original full instrumented and
GCC evidence remains bound to its actual earlier artifacts. Updated normal
binaries are retained separately. A single-job combined upload passed the
actual privacy and rights entrypoints before the hosted retry.

The repaired [hosted run44228987686](https://github.com/szTheory/tendies64/actions/runs/44228987686)
passed both jobs for exact checkpointe6f5466. Actual bounded ZIP downloads matched
their API digests and passed strict member checks; decoded RDRAM/VI bytes,
profile/policy/timing and observed DP/MI/VI fields agree across hosts. Both
downloaded upload trees pass rights/PII verification, and selected boot semantics
compare. The [public receipt](phase07-hosted.json) keeps distinct host build and
executable identities. The first failed run remains preserved and VID-04 is
still incomplete.

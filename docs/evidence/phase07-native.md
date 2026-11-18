# Phase07 authored native qualification

The reviewed source `2c8e9ef487792970b588b14ea253f4a706d0a861` passed the
complete normal gate, full ASan/UBSan inventory and pinned GCC13 check. The
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

Live hosted raw-artifact collection is pending this checkpoint's push. Local
mock transport verifies layout and refusal behavior only. No oracle agreement,
silicon behavior, broad ROM compatibility or VID-04 completion follows.

The first checkpoint's hosted gates passed, but its new emission entry stopped
before raw production. The [setup repair](../../.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-11-HOSTED-EMITTER-REPAIR.md)
adds a reached real-entry regression to cheap preflight. A fresh successful
hosted raw pair remains required; this failure does not replace that evidence.

# Authored preset and cold-protocol boot evidence

The exact cleared hello cartridge completes under both named T1 profiles and
emits the same literal `Tendies64 hello via open IPL3\n` bytes. The bounded
current-core startup and comparison checks pass. This is local authored
software-contract evidence, not hardware qualification or general ROM support.

| Profile | Authenticated terminal tick | Retired instructions | Output |
| --- | ---: | ---: | --- |
| `ntsc-cart-4m-openipl3-rdram-a-t1-v1` | 530448 | 530437 | Exact 30 bytes |
| `ntsc-cart-4m-openipl3-rdram-b-t1-v1` | 538871 | 538840 | Exact 30 bytes |

The core takes architectural BREAK exception 9 at canonical
`0xffffffff80000478`, with instruction `0x0000000d`, no branch-delay flag, and
actual preceding retirement `0x80000474 / 0xa50e0016`. The runner authenticates
that boundary and reports `qualified-terminal`; it does not turn the CPU's
BREAK into a new debug-stop operation or count it as a retirement. Transcript
text alone cannot qualify completion. Tier B additionally has observed ordered
RI delay/configuration, device-ID/broadcast, MI repeat/upper-mode, AUTO
calibration and absent-device capacity operations, with matching canonical
protocol state. Tier A follows the separate prepared path.

## Exact input and tested artifact

The two added [fixture contracts](../../fixtures/manifest.json) reuse the
existing cleared bytes and recipe; no ROM variant or padding was introduced.

- Cartridge SHA-256: `fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c`.
- Embedded IPL3 payload SHA-256: `f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5`.
- Tested build ID: `t64-build-v1-fac0bdd94d9ce7835f5682efb199df9a1e0519703f7870f4fbb1d0f4898f8794`.
- Tested runner SHA-256: `310b04818985852f1e58e88dbfea5a77253c10aa6785ec9f70e7140f621117ed`.
- A profile SHA-256: `d59b5e9500acb56621f1995e2c816f73597dce9109b46a4008b12269e42c02a2`.
- B profile SHA-256: `4d807406f1e8ab897a8328cf9f205e8d8faf77dbf8892cb999156265403023fa`.

The build ID binds compiler/version, target and build policy through the
canonical build-identity generator. The recorded source annotation is
`292405fadaae9299991063717d3966799c520d26`; runner/test/registration changes were
still uncommitted at capture. The retained exact executable, rather than that
annotation alone, identifies this experiment. Final coherent-source and hosted
qualification remain later Phase 6 work.

## Checks and original records

All-target normal and ASan/UBSan builds pass. Fast checks first validate exact
registered names, inventory, real profile/device access, calibration read
visibility and the production control report. The production controls reject
wrong profile/bytes, wrong terminal/opcode/delay, partial/inexact/lost output,
and missing, manual-only or out-of-order protocol families.

`integration.cold-determinism` executes both complete boot paths independently.
Within each profile, one-shot advances, partition cuts through actual reached
MDU intervals, and separately interleaved instances produce identical canonical
state and exact output. It verifies refused altered-IPL3 admission leaves the
whole state unchanged and that readmission during boot restores fresh state and
reproduces completion. The existing warm determinism check also passes normally
and under sanitizers. A and B state digests are intentionally not compared.
Exact inventories retain inherited names and add both cold hello and cold
determinism registrations.

Original local logs, hash manifests, and executables captured before use are
retained under ignored `build/gate/06-05-final-owning-checks/` and
`build/gate/06-05-final-authored-comparisons/`; actual comparison is cold-hello
attempt07. Earlier failures and exact attempted artifacts remain separately
retained. The original Plan09 startup archive is unchanged. A misleading
secondary exception and an incorrect static LUI/address inference were
withdrawn before choosing fixes; neither justified changing the fixture or PI
bounds.

The [RDRAM contract](../spec/rdram-boot.md) defines functional feedback v1 as a
source-derived software rule. It does not model analog current, noise, physical
module arrangement or measured timing. No external diagnostic result, hardware
comparison, public external input or broader compatibility claim is implied.

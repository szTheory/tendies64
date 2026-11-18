# Phase07 bounded authored claims

This claim covers the one accepted fetched guest fill-to-raw-frame case and its
independent authored comparator. It is a unit-tested result, not a hardware or
emulator-differential claim. The complete broader native matrix and exact build
bindings are in [the native evidence](../evidence/phase07-native.md).

The later versioned triangle, texture, combiner, blender, depth and VI filter
profiles have their own authored unit matrices in that source snapshot. Their
software arithmetic, timing, noise and neighborhood choices are named in the
[RDP](../spec/rdp.md) and [VI](../spec/vi.md) contracts. The hosted raw export
is the earlier v2 fill profile; one exported frame is not evidence for every
later renderer mode. Raw output remains distinct from host presentation.

Hosted raw collection is pending. No VID-04, hardware, commercial ROM, release
or broader compatibility result is claimed.

```t64-claim
build_id: 0.0.0+t64-build-v1-4f10a216af1f0564c70eeb58590dbb08fe534486f86afb7c64e06ff602daf5b9
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2
tier: unit-tested
fixture_set: rdp-vi-fill-diagnostic
manifest_digest: sha256:5e810cd53be877d643da36aa20184e55904177a63ac8d7c165b9af2cbb4a1f1e
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-fill-byte-contract
oracle_revision: source-content-sha256:1d49ee49594481e3803dd055522f5b22a97c6628a64ff131bdb6042e182b160b
not_claimed: other builds, profiles, fixtures or command combinations; general ROM behavior; hardware timing or accuracy; oracle agreement; external diagnostics; commercial software
statement: Tendies64 0.0.0+t64-build-v1-4f10a216af1f0564c70eeb58590dbb08fe534486f86afb7c64e06ff602daf5b9 on profile ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2: unit-tested for rdp-vi-fill-diagnostic @ sha256:5e810cd53be877d643da36aa20184e55904177a63ac8d7c165b9af2cbb4a1f1e (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-fill-byte-contract source-content-sha256:1d49ee49594481e3803dd055522f5b22a97c6628a64ff131bdb6042e182b160b. Not claimed: other builds, profiles, fixtures or command combinations; general ROM behavior; hardware timing or accuracy; oracle agreement; external diagnostics; commercial software.
```

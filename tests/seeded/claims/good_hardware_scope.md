# Seed: accurate is allowed in a hardware-derived block with a scope; passes

```t64-claim
build_id: 0.1.0
profile_id: example-profile
tier: hardware-derived
fixture_set: example-set
manifest_digest: sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
pass: 5
total: 7
guest_fail: 1
boot_fail: 0
timeout: 0
missing: 0
harness_error: 1
skipped: 0
oracle_kind: hardware-capture
oracle_revision: example-capture-0001
not_claimed: accurate behaviour outside the scope, any other profile
scope: example-cpu-subset
capture_provenance: example capture log 0001 (illustrative, not a real capture)
statement: Tendies64 0.1.0 on profile example-profile: matches hardware-derived expectations within example-cpu-subset for example-set @ sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef (5/7; missing 0, skipped 0, timeout 0, harness-error 1, guest-fail 1, boot-fail 0). Oracle: hardware-capture example-capture-0001. Not claimed: accurate behaviour outside the scope, any other profile.
```

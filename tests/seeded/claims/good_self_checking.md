# Seed: valid self-checking-fixture claim; passes

```t64-claim
build_id: 0.1.0
profile_id: example-profile
tier: self-checking-fixture
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
oracle_kind: guest-self-check
oracle_revision: example-1
not_claimed: any other profile, any commercial software
statement: Tendies64 0.1.0 on profile example-profile: passes self-checking fixtures for example-set @ sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef (5/7; missing 0, skipped 0, timeout 0, harness-error 1, guest-fail 1, boot-fail 0). Oracle: guest-self-check example-1. Not claimed: any other profile, any commercial software.
```

---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T15:36:00Z
scope: CR-01 and IN-01 from 07-07-REVIEW-INITIAL.md
status: recheck_complete
---

# Plan 07-07 Profile Finding Recheck

This recheck covers only the profile identity blocker and profile overview note from the initial review. WR-01 and WR-02 remain outside this recheck and are still open in the initial report.

## CR-01 — FIXED

The descriptor-v9 repair is complete. `profile_fields_are_valid()` now requires v9's shared RDP/VI policies, triangle policy, and combiner policy (`core/t64_machine.c:1194-1212`). `write_profile()` includes v9 in the metadata and policy serialization branches (`core/t64_machine.c:1354-1380`). The known blender profile is admitted only for descriptor v9 and compared against the canonical blender profile hash (`core/t64_machine.c:1040-1075`); `t64_machine_create()` therefore rejects a known-ID profile whose serialized contract was changed.

The added regression iterates the registered RDP profiles v3-v6 under review and mutates each policy included by that descriptor plus the three SHA-256 metadata fields and seed-reset version. It checks that each mutation changes both hash and serialized bytes and is rejected by `t64_machine_create`; it also checks blank and unterminated policy text rejection (`core/tests/test_profile.c:772-872`). Hard-coded hash and serialized-length goldens for v3, v4, and v5 preserve their prior bytes. The serializer diff adds only descriptor v9 to the existing version gates, leaving older-version serialization branches unchanged. The v6 VI policy now names both RGBA5551 and RGBA8888 output (`core/t64_machine.c:757-765`).

The retained normal and ASAN `unit.core.profile` logs each report 793 checks with zero failures (`build/gate/07-07-review-profile-retained-artifacts/build/gate/07-07-review-profile-final-LastTest.log`, `build/gate/07-07-review-profile-retained-artifacts/build/asan/07-07-review-profile-final-LastTest.log`). The retained pre-repair reproduction log records the original accepted mutations. I did not run tests in this recheck. The retained GCC command record shows strict C17 compilation was invoked, while its paired output log is empty, so this report does not independently infer its exit status.

## IN-01 — WITHDRAWN

The initial report's profile-count finding was incorrect. `docs/spec/rdp.md:3-12` now accurately lists five registered composite profiles, descriptor/model/state versions v2 through v6. There is no registered v1 composite RDP profile to add, and the overview contains all five. No further change is warranted.

## Verification limits

This recheck reviewed the current diff and retained evidence only. It did not rerun builds or tests, and it makes no hardware-accuracy or VID-04 claim. The two test-oracle warnings in the initial report are unaffected by these profile changes and remain for their separate repair/review.

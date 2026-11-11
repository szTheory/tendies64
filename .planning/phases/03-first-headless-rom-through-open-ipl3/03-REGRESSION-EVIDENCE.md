# Phase 1 and 2 regression gate during Phase 3 close

The successful source 7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6 hosted run
43103669084 attempt 1 ran the canonical gate on both Linux/GCC and macOS/Clang.
Its exact 91-name inventory includes all 61 Phase 1 and 12 Phase 2 names plus
18 Phase 3 additions. The inventory checker refuses missing, duplicate, disabled
or unexpected registrations; its per-name removal controls remain in the gate.
Both complete gates passed, covering the prior harness, rights/PII/manifest,
core-lint, JSON, admission, scheduler, bus, DMA, digest and model tests.

This satisfies the prior-phase regression gate on the current source. Only
planning/evidence metadata changed afterward. AUTONOMY forbids repeating broad
passing checks solely for bookkeeping, so no duplicate full run was launched.
The exact hosted pair and test/sanitizer coverage limits are recorded in
docs/evidence/phase3-ci.md and 03-13-SUMMARY.md.

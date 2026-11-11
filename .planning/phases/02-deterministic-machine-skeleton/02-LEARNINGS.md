# Phase 2 learnings

## Decisions

- Keep C17 with bounded parser admission, scoped sanitizer evidence and explicit
  caller pointer preconditions. The evidence does not prove memory safety.
- Keep synthetic model observations separate from real guest results and support
  claims. Current DMA deadlines and counters are named model policies.
- Track all-device scheduling as partial until the later device integration;
  Phase 9 owns completion of CORE-02.

## Lessons

- Build every executable used by a selected test. Target-only builds can leave
  subprocess producers stale even when their consumer is fresh. The two new
  consumer targets now declare their producer/checker dependencies explicitly.
- A claimed sanitizer result must identify rebuilt inputs. An initial component
  selection was stale; the final full rebuild and 14-test selection supersede it.
- Exercise the second hosted compiler early in a buildable slice. GCC exposed
  warning errors accepted by local Clang. Three focused fixes preserved strict
  warnings. Consider a keep-going compiler diagnostic pass in future planning
  to collect independent errors before a hosted retry.
- Keep workflow handoffs bounded and evidence records concise. Source review
  found real issues, while repeated report/fingerprint handoffs added delay.
  Routine parent metadata corrections should stay inline with the owning CLI.

## Patterns

- Canonical fieldwise digest; reservation before mutation; completion-time DMA
  source reads; per-instance state and a copy-only observer.
- Production-path deterministic fuzzing with exact case inventory and independent
  refusal snapshots, plus a deliberately failing sanitizer child.
- Phase-owned immutable CI snapshots prevent global append-only logs from
  invalidating unrelated historical verification.

## Surprises

- macOS queue wait dominated the final hosted run; caching would not address it.
- Installed verifier rules contradicted their autonomous judgment soft gate.
  The reconciliation keeps all flags visible and preserves explicit owner stops.

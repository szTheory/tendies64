# Plan 16 source-contract correction

The inventory contract is version 3. The earlier version 2 draft treated
`Weird` registrations as a separate, nonrequired lane. That was a mistaken
translation of the pinned test runner's default profile: `src/tests/mod.rs`
routes `BasicFunctionality`, `RarelyUsed`, and `Weird` through `BASE`, while
`Cycle` is dispatched separately. Inventory v3 therefore keeps all three base
levels in `required-base` and leaves `Timing`, `Cycle`, `COP0Hazard`,
`PoorlyUnderstoodQuirk`, and `StressTest` outside the required denominator.

This correction preserves the approved default CPU scope and does not enable
the separate cycle lane or claim timing accuracy. The protocol rejects stale
v2 inventories so a previously smaller denominator cannot be silently
reinterpreted. Source membership, protected inputs, and any diagnostic-derived
records remain private; this note contains no private inventory or outcomes.

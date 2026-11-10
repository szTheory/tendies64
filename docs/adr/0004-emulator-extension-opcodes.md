---
status: accepted
date: 2026-11-09
decision-makers: szTheory
consulted: none
informed: none
---

# Hardware profiles implement no nemu64-test emulator-extension opcodes

**Status:** accepted

## Context and Problem Statement

Some test ROMs written for the nemu64 emulator signal results through emulator-only instruction encodings in
the COP0 coprocessor-operation space (function codes 0x20, 0x25 and 0x2c). Real hardware does not define
them. If a hardware profile implemented them, it would pass tests by a mechanism the console does not have.
How should a hardware profile treat these opcodes, and how is a run completed without them?

## Decision Drivers

- A hardware profile must model hardware; an emulator-only convenience is not hardware behavior.
- Test completion must be observable by a mechanism the real console also has.
- Runs must stay deterministic and bounded in guest time.
- Facts and inferences must stay labelled (AGENTS.md).

## Considered Options

- Implement the extension opcodes in a dedicated emux profile.
- Implement them in every profile.
- Implement none in hardware profiles, and complete runs through the ISViewer output.

## Decision Outcome

Chosen option: "Implement none in hardware profiles, and complete runs through the ISViewer output".

- Hardware profiles implement no nemu64-test emux semantics: COP0 CO function codes 0x20, 0x25 and 0x2c
  carry no special meaning.
- There is no emux profile in v1.
- Completion is the ISViewer `Done!` line plus the guest-time bound.
- Unhandled-exception lines that the guest produces as a result are tolerated by the runner and counted in
  `unhandled_exception_lines`. They do not by themselves fail a run.
- The behavior of these encodings on a hardware profile is the reserved-instruction behavior. That is an
  inference, not yet verified against the VR4300 manual. Phase 3 validates it on day one.

### Consequences

- Good: a pass cannot come from an emulator-only mechanism.
- Good: the same completion rule applies to every profile.
- Bad: fixtures that rely on emux semantics cannot pass until an emux profile exists, and they are reported
  as out of scope.
- Bad: tolerating exception lines needs a count so the tolerance stays visible.

### Confirmation

`docs/spec/result-protocol.md` defines `counts.unhandled_exception_lines`, and the control matrix replays the
completion rule. Phase 3 checks the reserved-instruction inference against the VR4300 manual before relying on it.

## Reversal evidence

This decision is wrong if the VR4300 manual contradicts the reserved-instruction inference for these encodings,
or if the owner decides to add an emux profile. In the first case the hardware behavior is updated and
re-measured. In the second case a new record defines the profile and its evidence tier, and hardware profiles
stay unchanged.

## Pros and Cons of the Options

### Implement the extension opcodes in a dedicated emux profile

- Good: lets emulator-targeted ROMs run to completion.
- Bad: adds a profile that does not model hardware, and its results need a separate, weaker tier.

### Implement them in every profile

- Good: simplest runner.
- Bad: contaminates hardware profiles with non-hardware behavior.

### Implement none in hardware profiles

- Good: hardware profiles stay hardware profiles.
- Bad: some fixtures are out of scope until the owner decides otherwise.

## More Information

- Related: ADR-0005 (outcomes), ADR-0003 (timing tiers).
- Unknown: the exact reserved-instruction result for these encodings on the VR4300. Any placeholder
  implementation is unverified until Phase 3 checks it.

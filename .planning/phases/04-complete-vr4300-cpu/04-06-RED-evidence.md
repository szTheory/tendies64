# Plan 04-06 portable RED evidence

## Task 1: guest TLB refill and retry

The intended assertion was executed before the translation implementation:

```sh
ctest --preset gate --no-tests=error -R '^unit\.core\.cpu-path$' --output-on-failure
```

The new `mapped load miss is repaired by a guest TLBWI handler and retried`
case reached its architectural-state assertion and failed. It expected the
32-bit refill vector `0xffffffff80000000`; the observed vector was the common
exception vector `0xffffffff80000180`, with ExcCode 2, BadVAddr `0x4000`,
EntryHi 0, destination GPR8 still `0xfeedface`, and zero retired instructions.
The failed access had not changed its destination, backing bytes, sampled
data-cache line, or LL/SC reservation. This was a behavior RED, not a fixture,
addressing, build, or assertion-control failure.

The architectural expectations use NEC, *VR4300/VR4305/VR4310 64-Bit
Microprocessor User's Manual*, U10504EJ7V0UM00, pp. 143, 155–158, 180–189;
the pinned public copy is <https://hack64.net/docs/VR43XX.pdf>, SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`.

This record describes software behavior and the cited manual. It makes no
hardware-accuracy or human-approval claim.

## Task 2: XKPHYS physical-width refusal

The registered MMU assertion was executed before the correction:

```sh
ctest --preset gate --no-tests=error -R '^unit\.core\.cpu-mmu$' --output-on-failure
```

The test reached its intended result-field assertion for XKPHYS address
`0x8000000100000000`. The address correctly returned an address fault and no
translation, but the result reported physical domain `BUS` instead of
`XKPHYS`. The input address identifies XKPHYS and must retain that domain even
when its unsupported physical width is refused. This was a translation-result
behavior RED; the test target compiled and was discovered by CTest.

The address-width rule follows NEC U10504EJ7V0UM00, pp. 139–140: XKPHYS
physical address bits 58:32 must be zero. This evidence records the emulator's
software result and does not claim hardware execution.

## CACHE translation scope

The warm-v1 `unit.core.cp0-cache` fixture retains its legacy direct-only
supported subset. Its unqualified operation and KUSEG hit operation stop as
model-unsupported; that fixture is not hardware evidence. The VR4300 profile
has a separate assertion that a supported `Hit_Writeback_Invalidate_D`
operand at KUSEG `0x100` raises a TLB refill before cache or reservation
effects. NEC U10504EJ7V0UM00, p. 402, specifies virtual-to-physical translation
for CACHE operands and says TLB miss/invalid exceptions may occur. Cache line
state and writeback semantics remain outside this plan.

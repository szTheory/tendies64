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

# Compatibility v2 behavioral baseline

This is authored native evidence before the additional policies, not an
external diagnostic report. Raw captures remain ignored under `build/`.

## Source and setup

The strict and published compat-v1 baseline is `61c9c17`. Root added separate
v2 admission, state11 serialization and the CPU data-range predicate before
the operation tests. V2 profile/boot/reference checks pass; these do not
implement COP2, odd-register aliases, the CP0 policies or data zero reads.

The first v2 profile test observed unknown-profile status2 versus expected0.
The corrected profile and boot tests then pass, including retained v1 vectors.
The state11 independent reference equals production and its prepared vector
is fixed. COP2 latch sensitivity applies only to state11.

Compile errors in new tests (reservation-width type, a mismatched TLB helper
signature, and misplaced diagnostic declarations) were fixed before accepting
behavioral evidence. One initially launched profile test used the old binary
while its build was still running; it is not RED evidence. The subsequent
completed-build run is the recorded unknown-profile baseline.

## Actual operation observations

`ctest --preset gate` with the owning 12-test selection fails in the new
operation controls while profile, boot, MMU and unchanged cache controls pass.
The narrower tracer rerun confirms these fetched operations:

| Authored operation | Full fetched PC | Opcode | Observed boundary |
|---|---|---|---|
| First MTC2 | `0xffffffff80001000` | `0x48882800` | Model-unsupported, Cause0, CU2-enabled Status `0x74000000`. |
| First odd FR0 ADD.D | `0xffffffff80001000` | `0x463ee840` | Guest FPE, Cause `0x3c`, Status `0x30000002`. |
| Uncached KSEG CACHE writeback | `0xffffffffa4000040` | `0xbd01000c` | Model stop, zero retirement, unchanged Cause/Status and selected dirty line. |
| Unbacked data LL | `0x0000000000000000` | `0xc0880000` | Fetch maps to PA `0x1000`; data model stop, Cause0, no reservation or retirement. |
| Cached 16 MB-page final LW | `0x0000000000000000` | `0x8c880000` | Fetch maps to PA `0x1000`; data model stop, Cause0, destination unchanged. |

No fetched-PC/opcode precondition failed in the accepted tracer run. The first
unsupported MTC2 prevents its dependent transfer sequence from executing;
those later instructions are not independent original RED observations. The
same limit applies to dependent conversions after the first odd ADD.D.
Independent exception and old-profile controls still execute.

Direct CP0 staging records the PageMask write refusal and the missing six-bit
Random values. Its TLB readback checks depend on admitting the initial write;
they are not separate original write/readback defects. Direct cache controls
record rejected TagLo01 and an unfilled D-cache line, not a fetch failure.
Zero-data store/fetch refusal controls are required to stay green after reads
are implemented; earlier setup rejection alone cannot prove those later paths.

Commands and raw results: `build/compat-v2-coordinated-red*.log` and
`build/compat-v2-tracer-red*.log`. The source policies are implemented only
after these observations. No synthetic false assertion, setup failure or
later reconstructed result is presented as original behavioral RED.

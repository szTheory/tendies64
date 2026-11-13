# VR4300 COP1 behavior contract

Tendies64 models the VR4300 COP1 instruction boundary in the canonical CPU.
FPRs, FCR0, FCR31, the condition bit, and each operation's exception state are
owned by one machine. Arithmetic uses the project's pinned Berkeley SoftFloat
Release 3e binary32/binary64 integer routines through an explicit per-operation
rounding and flag context. COP1 translates those raw IEEE results into VR4300
effects; the generic arithmetic library does not decide FCR31 or exceptions.

This contract describes the implemented software model. It does not claim
measured silicon behavior, broad game compatibility, or qualification from
agreement with generic IEEE results.

## Primary source and test provenance

The primary reference is NEC, *VR4300/VR4305/VR4310 User's Manual*, document
U10504EJ7V0UMJ1, seventh edition, August 2000. The public copy is
[VR43XX.pdf](https://hack64.net/docs/VR43XX.pdf), SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`.
Relevant material includes FR/FPR pairing and FCR fields on printed pages
208–216; rounding, flush and exception tables on pages 213–215 and 236–244;
and the E-only Cause rule in Appendix B.1.5, printed page 631.

The native tests use in-tree bit-pattern and rational expectations derived from
the cited format and exception rules. Examples include exact halfway sums
`1 + 2^-24` and `1 + 2^-53`, exact powers of two, signed conversion limits,
and the directed flush table. They do not use SoftFloat under test or another
emulator as an expected-value oracle. No external ROM or diagnostic corpus is
used by these tests.

## FPR layout and control registers

FCR0 reads as `0x00000b00` in the current profile model. This is a fixed model
identity value, not a claim that every VR4300 revision reports the same
revision field. FCR31 is per-machine state. In FR=1, each FPR is an independent
64-bit lane. In FR=0, a double uses an even/odd pair, with the even register
holding the low word and the following odd register holding the high word;
odd or unavailable double registers are unimplemented.

| FCR31 field | Bits | Mask | Model behavior |
|---|---:|---:|---|
| C condition | 23 | `0x00800000` | Compare result; written by CTC1 |
| FS flush | 24 | `0x01000000` | Enables the documented result flush when permitted |
| Cause E/V/Z/O/U/I | 17:12 | `0x0003f000` | Replaced by the last applicable operation's causes |
| Enable V/Z/O/U/I | 11:7 | `0x00000f80` | Enabled IEEE causes trap; E has no enable |
| Flag V/Z/O/U/I | 6:2 | `0x0000007c` | Sticky flags for masked IEEE exceptions |
| RM | 1:0 | `0x00000003` | RN, RZ, RP, or RM for conversions/arithmetic |

| RM | Rounding direction |
|---:|---|
| 0 | Nearest, ties to even |
| 1 | Toward zero |
| 2 | Toward positive infinity |
| 3 | Toward negative infinity |

Each conversion or computational instruction except MOV replaces Cause with
the current operation's causes, including zero when no exception occurred.
Masked IEEE causes set their corresponding sticky Flags. Enabled causes trap
without setting their Flag. Flags remain set until CTC1 clears them. MOV is a
bit transfer and leaves Cause and Flags unchanged. Comparisons update C only
when they complete without an enabled exception; a trapped compare preserves C.

E is unmaskable. An E operation replaces the Cause field with only E, preserves
existing sticky Flags, enters CP0 floating-point exception code 15, and
suppresses destination and compare-condition writes. E has no Enable or Flag
bit. Enabled IEEE exceptions likewise suppress the destination (and C for a
compare) while recording the current IEEE Cause.

## Instruction and format coverage

The native instruction tests exercise these defined COP1 arithmetic rows in
both binary32 (`.S`) and binary64 (`.D`) formats:

| Instruction family | Encodings exercised |
|---|---|
| Arithmetic | ADD, SUB, MUL, DIV, SQRT, ABS, MOV, NEG |
| Compare | All 16 `C.cond.S` and `C.cond.D` predicates (`0x30`–`0x3f`) |
| Format conversion | CVT.S.D, CVT.D.S |
| Integer conversion | CVT.S.W/L, CVT.D.W/L, CVT.W/L.S/D |
| Explicit integer rounding | ROUND, TRUNC, CEIL, FLOOR.W/L.S/D |

Every computational format is exercised with all four applicable FCR31 RM
values. ROUND uses nearest-even, TRUNC uses toward-zero, CEIL uses toward
positive infinity, and FLOOR uses toward negative infinity regardless of the
FCR31 RM selector. Comparison results follow less/equal/unordered predicate
bits, treat positive and negative zero as equal, and check quiet versus
signaling behavior for every predicate encoding. Compare conditions are also
consumed by BC1.

CVT.W/L.S/D interprets the source as a signed binary floating-point value and
rounds according to FCR31 RM. W results write the destination's low 32-bit
word; L results write 64 bits. W/L source conversions interpret the low word
or full doubleword as a signed two's-complement integer. The tests cover exact
and inexact values, signed minima and maxima, just-in-range values, and values
that overflow the destination integer range.

## Exception and FS policy

SoftFloat uses tininess detection after rounding. COP1 then applies the
VR4300-specific source and result rules below before committing the destination.

| Condition | Cause/Flag behavior | Result and destination |
|---|---|---|
| Masked inexact, divide-by-zero, overflow, or invalid | Set current Cause; set sticky Flag for each masked cause | Commit rounded IEEE result |
| Enabled IEEE cause | Set current Cause; do not set the enabled Flag | Enter FPE; preserve destination and, for compare, C |
| Denormalized arithmetic/conversion source | E only, except compare | Enter FPE; preserve destination |
| QNaN arithmetic/conversion source | E only, except compare | Enter FPE; preserve destination |
| SNaN arithmetic source | Invalid V | Canonical quiet result if masked; otherwise enter FPE and preserve destination |
| Invalid operation such as `0/0` or negative square root | Invalid V | Canonical quiet result if masked; otherwise enter FPE |
| FP-to-W/L source is infinity, NaN, or out of signed range | E only | Enter FPE; preserve destination |
| Result is denormalized and FS is clear | E only | Enter FPE; preserve destination |
| Tiny result with FS set and U/I both disabled | Set U and I Causes and Flags | Flush by sign and RM table below |
| Tiny result with FS set and U or I enabled | E only | Enter FPE; preserve destination |

The FS flush values follow the manual's Tables 7-2 and 8-2:

| Tiny result sign | RN | RZ | RP | RM |
|---|---|---|---|---|
| Positive | `+0` | `+0` | `+minimum normal` | `+0` |
| Negative | `-0` | `-0` | `-0` | `-minimum normal` |

The flush is applied to the arithmetic result after rounding. A denormalized
input remains E even when FS is set; FS controls the documented result-flush
path. Compare accepts denormalized operands and does not raise underflow.
Overflow sets both O and I Causes. Cause bits describe only the last
conversion/computational operation, while Flags accumulate masked causes.

## Deterministic model choices and limits

The manual identifies NaN classes using the opposite fraction-MSB polarity
from the generic SoftFloat wrapper: fraction MSB set is signaling and clear is
quiet. COP1 classifies operands using the VR4300 rule before raw arithmetic.
When a masked invalid operation needs a NaN result, the model writes positive
canonical quiet payload `0x7f800001` for S and `0x7ff0000000000001` for D.
Those deterministic payloads make machine state reproducible; they are an
implementation policy, not a measured payload chosen by VR4300 silicon.

Reserved operation/format combinations, including CVT.S.S and FP-to-integer
operations encoded with W/L as the source format, take E. No cycle-accurate
FPU latency or revision-specific quirks are claimed by this contract. The
current FCR0 revision identity, canonical NaN payloads, and undefined/reserved
format handling remain explicit model policy pending independent hardware
evidence. Native tests establish the listed software behavior only; they do
not establish broad compatibility or hardware accuracy.


## Compat-v2 FR0 alias policy

The model5/state11 software profile aliases odd FR0 source indices to the even
pair for its covered double-format arithmetic and conversion paths. Conversion
source aliasing includes the selected scalar CVT.S.W and CVT.W.S forms. A
destination is normalized only for a 64-bit pair result; a 32-bit conversion
result retains its requested destination, even when that index is odd. Reads
and writes use the existing FPR state; ordinary scalar transfers and
strict/compat-v1 behavior are unchanged.
This is software policy for manual-undefined operands, not physical evidence.
Authored ADD.D, CVT.D.L and CVT.D.S controls check exact output bits, fetched
PC/opcode and retained old-profile exceptions. Replay resets both full and
low PC and invalidates the rewritten instruction's I-cache line before each
attempt; a different fetched instruction cannot establish arithmetic results.

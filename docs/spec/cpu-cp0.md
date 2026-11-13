# VR4300 CP0 transfer contract

This document describes the selected Tendies64 VR4300 profile's CP0 transfer
behavior. It records implementation policy and is not hardware qualification.
The legacy warm-v1 CP0 handler and its identity are unchanged.

## Evidence

The primary reference is NEC, *VR4300/VR4305/VR4310 64-Bit Microprocessor
User's Manual*, U10504EJ7V0UMJ1, 7th edition, August 2000,
SHA-256 `b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`
([public PDF](https://hack64.net/docs/VR43XX.pdf)). Relevant sections are CP0
registers §5.4, pp.146–155; exception registers §6.3, pp.163–179; and MFC0,
MTC0, DMFC0, and DMTC0, pp.419–420, 469, and 474.

Manual statements below are identified as hardware facts. Versioned behavior
choices are identified as model policy. A manual-defined or model-supported
instruction is not evidence that a physical NUS-CPU-03 has been qualified.

## Transfer and fault rules

| Transfer | Behavior |
|---|---|
| MFC0 | Reads the low 32 bits of the selected register and sign-extends bit 31 to the 64-bit GPR. This is a manual-defined rule (p.469). |
| MTC0 | Writes the low 32-bit GPR word through that register's field contract. Read-only fields remain hardware-owned. |
| DMFC0 | Returns the complete selected 64-bit register when the register is 64-bit. On a 32-bit CP0 register, the version-4 compatibility profile returns its masked low word zero-extended to 64 bits; the version-3 native profile refuses the operation. The manual defines the instruction in 64-bit mode and 32-bit Kernel mode; it is Reserved Instruction in 32-bit User or Supervisor mode (p.419). |
| DMTC0 | Writes a full 64-bit source through that register's field contract. On a 32-bit CP0 register, the version-4 compatibility profile applies the source low word through the MTC0 field contract; the version-3 native profile refuses the operation. The manual defines the instruction in 64-bit mode and 32-bit Kernel mode; it is Reserved Instruction in 32-bit User or Supervisor mode (p.420). |

CP0 is usable in Kernel mode regardless of Status.CU0. User and Supervisor
access requires CU0. A disabled CU0 access raises Coprocessor Unusable before
the transfer. A nonzero reserved selector or an access to a CP0 register
reserved for future use raises Reserved Instruction. DMFC0/DMTC0 applied to a
32-bit CP0 register are undefined by the manual (pp.419–420). The native
version-3 profile reports model-unsupported before changing CP0 or the
destination GPR. The version-4 compatibility profile makes this undefined case
deterministic: DMFC0 exposes the register's existing field-masked low word with
zero extension, while DMTC0 sends the source low word through the same field
mask, validation, and staged side effects used by MTC0. These are software
compatibility choices, not statements about physical VR4300 behavior. The
documented privilege and reserved-selector checks remain in effect for both
profiles.

The MFC0 sign-extension and DMFC0 full-width behaviors are hardware facts.
For 64-bit CP0 registers, MTC0 updates the low word and preserves the existing
upper word; this is the selected profile's word-write policy. DMTC0 applies the
register's defined 64-bit mask. Transfer effects are prepared first and commit
only after encoding, register, and field validation succeeds.

## Register fields

Masks are hexadecimal. “Preserve” means the field comes from hardware-owned
state and is not replaced by an MTC0/DMTC0 source. The low five Random/Wired
bits select TLB entries; bit 5 is visible but ignored by TLB selection.

| Reg. | Register | Read fields | Software write fields and side effects |
|---:|---|---|---|
| 0 | Index | P[31] and Index[5:0] | Write Index[5:0] (`0x0000003f`); preserve P; zero RFU. |
| 1 | Random | Random[5:0] | Only bit 5 is software-writable; preserve hardware-stepped bits[4:0]. On instruction retirement, decrement the low five-bit selection within Wired..31, wrapping to 31. |
| 2–3 | EntryLo0/1 | PFN[25:6], C[5:3], D, V, G (`0x03ffffff`) | Write defined bits with `0x03ffffff`; zero upper RFU. |
| 4 | Context | PTEBase[63:23], hardware BadVPN2[22:4] | MTC0 writes PTEBase[31:23], preserving upper PTEBase and BadVPN2. DMTC0 writes PTEBase[63:23], preserving BadVPN2. Bits[3:0] read zero. |
| 5 | PageMask | Mask[24:13] (`0x01ffe000`) | Accept exactly `0x00000000`, `0x00006000`, `0x0001e000`, `0x0007e000`, `0x001fe000`, `0x007fe000`, and `0x01ffe000`. A different Mask encoding is manual-undefined (p.149) and model-unsupported atomically. |
| 6 | Wired | Wired[5:0] | Write low six bits (`0x0000003f`); set Random to 31 as a staged side effect. The TLB selection uses Wired[4:0]. |
| 7 | Reserved | — | Reserved register access raises Reserved Instruction. |
| 8 | BadVAddr | Full 64-bit virtual address | Hardware-owned; writes are accepted as no-ops. Address exceptions update it. |
| 9 | Count | Low 32 bits | Write low 32 bits while preserving the half-clock phase. The selected model advances Count once per two guest ticks, including idle and non-retiring CPU attempts; this is not a physical clock-rate claim. |
| 10 | EntryHi | Region[63:62], VPN2[39:13], ASID[7:0] | MTC0 mask `0xffffe0ff`; preserve upper region/VPN2 fields. DMTC0 mask `0xc00000ffffffe0ff`. Zero bits[61:40] and [12:8]. |
| 11 | Compare | Low 32 bits | Write low 32 bits and clear Cause.IP7 at the transfer commit. A newly reached Count equality sets IP7 before CPU interrupt sampling; a Compare commit suppresses relatching the unchanged current equality. |
| 12 | Status | Defined fields, including hardware TS[21] | Writable mask `0xf757ffff`; preserve TS[21] (`0x00200000`), force RFU bits 23 and 19 and selected RP[27] to zero. |
| 13 | Cause | BD, CE, IP[7:0], ExcCode | Only software IP[1:0] (`0x00000300`) is writable. Preserve all other defined hardware fields, including IP[7:2], ExcCode, CE, and BD; zero RFU. Compare owns the IP7 clear effect. |
| 14 | EPC | Full 64-bit return address | MTC0 updates low 32 bits; DMTC0 updates all 64 bits. Exception entry writes it only when EXL was clear. |
| 15 | PRId | Selected VR4300 identifier | Read-only. The manual specifies implementation ID `0x0b` (p.151); revision `0` is the prepared profile's selected value, not a silicon measurement. |
| 16 | Config | Prepared fixed fields, EP, BE, CU, K0 | Writable fields are EP[27:24], BE[15], CU[3], and K0[2:0] (`0x0f00800f`). The prepared seed is `0x7006e463`; its EP/BE initialization window is closed. A post-window EP or BE change, or unsupported EP encoding, is model-unsupported with no mutation. CU/K0 changes are applied while fixed fields are preserved. |
| 17 | LLAddr | PA[31:4] in bits[27:0] | Diagnostic read/write mask `0x0fffffff`; does not set, clear, or retarget the separate LL/SC link record (manual p.154). |
| 18 | WatchLo | PAddr[31:3], R, W | Write mask `0xfffffffb`; bit 2 is RFU and reads zero. |
| 19 | WatchHi | Compatibility physical-address bits[3:0] | Write/read mask `0x0000000f`; higher bits read zero (manual p.175). |
| 20 | XContext | PTEBase[63:33], hardware region[32:31]/BadVPN2[30:4] | MTC0 cannot reach PTEBase and leaves it unchanged. DMTC0 writes PTEBase (`0xfffffffe00000000`) and preserves hardware fields. Bits[3:0] read zero. |
| 21–25 | Reserved | — | Reserved register access raises Reserved Instruction. |
| 26 | PErr | Diagnostic[7:0] | Write mask `0x000000ff` (manual p.178). |
| 27 | CacheErr | Zero | Read-only and unused by VR4300; reads zero and writes are no-ops (manual p.178). |
| 28 | TagLo | PTag[27:8], PState[7:6] | Write/read mask `0x0fffffc0`; zero RFU (manual pp.154–155). |
| 29 | TagHi | Zero | The VR4300 implements no TagHi bits; reads zero and writes zero. |
| 30 | ErrorEPC | Full 64-bit return address | MTC0 updates low 32 bits; DMTC0 updates all 64 bits. |
| 31 | Reserved | — | Reserved register access raises Reserved Instruction. |

The register numbers and field definitions come from the manual. The masks
above follow those fields. Invalid PageMask rejection, post-preparation Config
rejection, the native-profile refusal and compatibility-profile behavior for
doubleword access to a 32-bit CP0 register, no-op writes to read-only registers,
and the 32-bit MTC0 preservation policy for the upper word of a 64-bit CP0
register are explicit model choices.

## Status and profile policy

The manual defines Status.CU usability, KSU, IE/IM, exception/error levels,
address-mode bits, the read-only TS bit, and RFU fields (§6.3.5, pp.165–170).
For this profile the 133 MHz-family RP bit is selected fixed-zero, yielding the
software mask `0xf757ffff`. That preserves each defined writable bit while
keeping TS read-only and RFU bits 23/19 at zero. The mask is tested one bit at a
time, with a separate set and clear attempt for each bit.

The manual states that Config.EP/BE can change only in initialization directly
after cold reset and before a store, and that operation is not guaranteed
outside that window (§5.4.6, pp.151–153). Tendies64 admits a prepared entry
after that window. Its `cp0_config_init_window` state starts closed and is
included in the version-9 CPU digest. EP/BE changes after that point are refused
atomically; CU/K0 remain software writable.

Random stepping is a selected instruction-boundary policy: after each retired
instruction, step the low five-bit value over Wired..31; an instruction that
uses Random for TLBWR consumes `t64_cp0_random_tlb_index()` before that step.
The VR4300 profile implements TLBP, TLBR, TLBWI, and TLBWR through the shared
MMU contract. TLBWI uses Index's low five bits; TLBWR consumes the pre-retirement
Random selection. Probe and lookup include invalid entries in tag matching;
duplicate matches set the read-only TS state and select shutdown until reset.
TLB faults commit BadVAddr, EntryHi, Context, and XContext through the precise
exception path. XContext region and BadVPN2 hardware fields update in both
32-bit and extended modes. These are native model contracts; cache-line
behavior and hardware qualification are separate.

## Count, Compare, and interrupt boundary

The manual defines Count as a free-running timer that increments at half the
PClock rate whether or not instructions execute, and rolls over from all ones
to zero (§6.3.3, p.164). It defines Compare equality as setting Cause.IP7 and a
Compare write as clearing the timer interrupt (§6.3.4, p.165). These are
hardware-document facts; the selected emulator tick rate is model policy.

For the prepared profile, one guest tick is one selected model RSP-cycle unit
and Count increments once per two guest ticks. The CPU:RSP 1:1 choice and
two-tick mapping are deterministic model policy, not a VR4300 or NUS clock
measurement and not a T1 instruction-cost table. Clock synchronization runs at
every dispatched guest event and direct time-bound endpoint, so idle jumps and
non-retiring exceptions advance Count without per-tick host loops.

At each CPU boundary the model synchronizes Count, latches any newly reached
equality, merges the MI line into Cause.IP2, then samples the Status IE/IM and
EXL/ERL gates. An accepted interrupt enters the normal exception path before
fetch or instruction effects. CP0 effects are committed only after a selected
instruction executes. Count writes keep the half-clock phase and assert IP7
when the written value equals Compare; Compare writes clear IP7 and do not
re-latch the unchanged equality. Software IP[1:0], MI IP2, and timer IP7 retain
their separate owners. Equal-tick device events keep scheduler FIFO order.

## Tests

`unit.core.cpu-cp0` directly exercises independent expected field values,
per-bit Status set/clear behavior, all seven PageMask encodings, strict and
compatibility register-width rules, compatibility DMFC0 zero extension,
DMTC0 field masking and refusal atomicity, Config refusal atomicity,
Cause/Compare preservation, Random/Wired effects, and reserved selectors.
`unit.core.cpu-path` executes guest MTC0/MFC0
Status operations and verifies that CU0 removal faults the next transfer
without changing its destination. Its timer-handler path verifies equality,
interrupt entry, Compare clearing, ERET, and execution of the interrupted
instruction after a non-retiring interrupt attempt.

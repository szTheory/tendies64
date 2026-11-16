# Observed-gap decisions for compatibility v2

These are agent-selected reversible implementation decisions within the owner's
approved versioned software target. They are not additional owner decisions,
risk acceptance or physical-console observations. Decision research was split
across cache, CPU/COP1 and CP0/MMU/data policy clusters and checked against
current source, the pinned primary manual and private software expectations.
External source and derived results stay private.

| Area | Selected v2 policy | Counterargument and boundary |
|---|---|---|
| Cache aliases | Allow already defined CACHE operations after valid translation/privilege checks even for uncached attributes; preserve VA index and translated hit-tag semantics. | Existing strict refusal avoids undefined cases. This acceptance is a software expectation, not a general hardware rule. |
| Index Store Tag | D PState01 becomes invalid/clean; I01 becomes invalid and I11 clean/valid. | Undefined encodings need not have these physical results. Strict and compat-v1 keep refusals. |
| COP2 transfers | One per-machine 64-bit latch; rd aliases, MFC2 sign-extends its low word, DMFC2 reads all bits, MTC2/DMTC2 write the full GPR. CFC2/CTC2 are inert. | The generic manual describes banked CPR and word writes, conflicting with this narrow software expectation. No memory, branch or operation-space COP2 capability is inferred. |
| COP2/3 faults | CU2 checks precede validity; enabled double control forms raise RI with CE2. COP3 and the selected reserved primary forms raise RI/CE0. | Only these forms are selected; no physical COP2/3 implementation is established. |
| FR0 odd operands | Alias covered FR0 arithmetic/conversion sources, including selected scalar conversion sources, to the even pair. Normalize destinations only for 64-bit pair results; retain odd 32-bit destinations. | Manual-undefined behavior. Do not alter ordinary scalar transfers or claim every odd-register operation behaves this way. |
| Random/Wired | When Wired[5] is set, visible Random counts down modulo64. TLBWR still uses the low5 bits. Otherwise preserve the existing cycle. | The manual's normal upper bound is31; the out-of-range Wired policy is software-only. |
| PageMask | Writes store the masked field. TLB entries retain the raw masked field; TLBR projects each pair using its upper bit. Raw undefined masks remain unsupported for translation. | The manual defines seven canonical page sizes. Undefined mask translation stays unsupported; register roundtrip does not claim arbitrary mappings. |
| Unpopulated data | V2 returns zero for translated scalar/linked data reads and D-cache fills wholly within unpopulated RDRAM below the register aperture or unbacked Domain1C. | System-bus data semantics are not established by the CPU manual. No global bus-decoder, fetch, mapped-MMIO or other media-domain behavior changes. |

The zero-data predicate excludes the modeled absent-RDRAM probe, all region
boundaries and excessive/overflowed ranges. Stores to these ranges retain the
model-unsupported stop, preserving registers, reservations and resident cache
lines before retirement. Alignment, privilege, Watch, TLB and high-address
fault checks remain ahead of the data fallback. It must not turn unsupported
mapped device operations into successful reads.

## Version and evidence ownership

Use the separate `ntsc-cart-4m-openipl3-vr4300-compat-v2`, model5/state11 and
semantic3. Keep published compat-v1 model4/state10 and strict model3/state9
unchanged. State11 retains the earlier CPU fields and branch origin, then adds
the COP2 latch under its own domain. Root owns profile/header/digest and runner
selection; workers own disjoint policy modules and owning controls.

The first shared build caught test-authoring type/helper errors; those compile
failures are not behavioral RED. The corrected run passes admission/boot and
old-profile controls while the new operation controls fail. Tracers record
fetched opcode, full PC, outcome and exception state before that evidence is
accepted. Dependent operations after an unsupported first instruction are not
independent original REDs.

After implementation, run affected actual cases first using unchanged
qualified inputs, exact identities and existing limits. Such runs remain
diagnostic-only. The final full source-derived set and shared C aggregator
alone establish Plan16 completion; no targeted denominator or fabricated fail
can replace an unreached case. Long sanitizer/fuzz work remains separate from
wall-capped actual guest runs.

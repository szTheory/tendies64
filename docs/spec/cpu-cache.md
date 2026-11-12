# VR4300 instruction cache model

## Scope and primary facts

This document describes the functional instruction-cache model for
`ntsc-cart-4m-openipl3-vr4300-v1`. The CPU executes from an I-cache on
cacheable fetches, so guest-visible stale code, invalidation, and line
replacement are represented. The model has no cache miss timing, bus
arbitration, or pipeline-stall simulation.

The primary reference is NEC, *VR4300/VR4305/VR4310 64-Bit Microprocessor
User's Manual*, seventh edition, August 2000, U10504EJ7V0UMJ1, SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`
([public PDF](https://hack64.net/docs/VR43XX.pdf)). Sections 11.2.1–11.2.3
describe the 16 KiB, direct-mapped, virtually indexed I-cache, its 32-byte
lines, physical tags, and VA[13:5] index (pp. 276–281). Section 11.3.3
describes sequential line refill (p. 282). The CACHE operation table includes
`Hit_Invalidate` for I and D lines (pp. 404–405). These are primary hardware
facts; they do not qualify the Tendies64 model on a physical console.

## Implemented behavior

| Area | Model behavior |
| --- | --- |
| Geometry | 512 direct-mapped lines × 32 bytes = 16 KiB. |
| Index | `(virtual_address >> 5) & 511`, corresponding to VA[13:5]. Virtual synonyms with different index bits select separate entries. |
| Physical identity | A line hit requires the selected virtual index and matching translated physical line. The internal `physical_tag` field stores the physical line address aligned down to 32 bytes (`PA & ~31`); this is an equivalent lookup key that includes redundant page-offset bits already implied by the virtual index. The manual's hardware PTag is PA[31:12]. No current I-cache operation exposes this internal field as TagLo. The v9 digest includes the chosen internal value. |
| Fill and replacement | A miss reads all 32 bytes from one accepted backing range into temporary storage. The model checks every touched byte before changing the selected line. Only a complete fill replaces its valid bit, data, and physical-line key. A refused fill preserves the old line and does not publish the attempted instruction. |
| Accepted backing | RDRAM, SP DMEM, SP IMEM, and bytes within the admitted cartridge image. Other physical/device ranges and cartridge offsets outside the admitted image are refused. |
| Cache attributes | Translation carries KSEG0 Config.K0 or TLB C. Attribute value 2 bypasses the I-cache; all other 3-bit values are treated as cacheable by this profile. This is the selected functional model policy and does not claim coherence or timing for every hardware cache algorithm encoding. KSEG1 is uncached. |
| Reset | The 512 lines start invalid with zero tags and data. |
| Invalidation | `CACHE Hit_Invalidate I` translates the target, requires a cacheable attribute, and invalidates only when the selected index contains the matching physical line. It does not alter D-cache line state or an LL/SC link. Other cache-operation details are outside this I-cache slice. |
| External writes | CPU data stores, including stores through uncached aliases, and PI/SP DMA update backing storage without snooping valid I-cache lines. Executed code remains stale until the guest invalidates that line or an index replacement installs another line. |
| Identity | State v9 already hashes every I-cache line's key, valid bit, and data, along with cache-operation diagnostics. Tests verify field sensitivity and independent state across machines; no serializer change is needed for this slice. |

The aligned physical-line key is an internal representation choice. For valid
translations, page offsets are preserved and a 32-byte line cannot cross the
4 KiB minimum page boundary. Therefore the virtual index and aligned physical
line key distinguish the same fetch identity as the manual PTag plus index,
including virtual synonyms. This field is not a claim that the hardware stores
PA[11:5] in its tag RAM.

## Evidence and limits

Native tests execute code before and after uncached writes and targeted
invalidation; exercise indices 0 and 511, byte offset 28, same-index tag
replacement, virtual synonyms, cacheable and uncached TLB attributes, all
accepted backing domains, failed full-line fills, PI/SP DMA no-snoop behavior,
and v9 digest sensitivity. The warm-v1 profile keeps its separate CACHE path
and existing byte-for-byte fixtures.

The non-snooping behavior for CPU backing writes and PI/SP DMA is a selected
functional model policy. The manual geometry and operation facts above do not
establish that policy's physical-console behavior. Revisit this policy if an
authorized hardware measurement or an independent permitted hardware vector
shows different stale-code or invalidation behavior. Revisit the internal key
encoding only if an implemented architectural tag operation or an independent
state contract requires the raw PA[31:12] representation; such a change must
also review v9 digest identity.

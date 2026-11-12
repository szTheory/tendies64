# VR4300 functional cache model

## Scope and primary facts

This document describes the functional I-cache and write-back D-cache model
for `ntsc-cart-4m-openipl3-vr4300-v1`. Cacheable guest fetches, cached stores,
line replacement, primary CACHE operations, and the selected non-snooping
visibility policy are represented. The model has no cache miss timing, bus
arbitration, or pipeline-stall simulation.

The primary reference is NEC, *VR4300/VR4305/VR4310 64-Bit Microprocessor
User's Manual*, seventh edition, August 2000, U10504EJ7V0UMJ1, SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`
([public PDF](https://hack64.net/docs/VR43XX.pdf)). Sections 11.2.1–11.2.3
describe the 16 KiB, direct-mapped, virtually indexed I-cache, its 32-byte
lines, physical tags, and VA[13:5] index (pp. 276–281). Sections 11.2.4–11.2.6
describe the 8 KiB D-cache, its 16-byte lines and VA[12:4] index (pp. 282–285).
Section 11.3 describes write-back replacement (pp. 279–282). The primary CACHE
operation table and instruction behavior are in §16, pp. 402–405. These
hardware facts do not qualify Tendies64 on a physical console.

## Implemented behavior

| Area | Model behavior |
| --- | --- |
| I-cache geometry | 512 direct-mapped lines × 32 bytes = 16 KiB; index `(VA >> 5) & 511` (VA[13:5]). |
| D-cache geometry | 512 direct-mapped lines × 16 bytes = 8 KiB; index `(VA >> 4) & 511` (VA[12:4]). |
| Physical identity | A hit requires the selected virtual index and matching translated physical line. Internal `physical_tag` stores the aligned line key (`PA & ~31` for I, `PA & ~15` for D). This is equivalent to hardware PTag plus index because translation preserves page offsets. Hardware PTag is PA[31:12]. |
| Fill and replacement | I misses stage 32 bytes; D load/store misses stage 16 bytes. Each full line is validated within one backing domain before replacement or dirty writeback. A refusal preserves the old line, dirty bytes, and backing. A D store miss fills before applying selected bytes and setting dirty. |
| Accepted backing | RDRAM, SP DMEM, SP IMEM, and admitted cartridge bytes can be read for fills. Writeback accepts mutable RDRAM and SP DMEM/IMEM only. Read-only cartridge and unsupported device ranges refuse cache writeback before any backing byte or line state changes. |
| Cache attributes | Translation carries KSEG0 Config.K0 or TLB C. Value 2 bypasses each cache; other 3-bit values are treated as cacheable by this profile. KSEG1 is uncached. This selected functional policy does not claim timing or coherence for every hardware algorithm encoding. |
| Reset | Both 512-line arrays start invalid with zero tags/data; D lines start clean. |
| Primary CACHE rows | Implemented rows match the VR4300 table: op0 I Index_Invalidate; op1 D Index_Write_Back_Invalidate; op1 I/D Index_Load_Tag; op2 I/D Index_Store_Tag; op3 D Create_Dirty_Exclusive; op4 I/D Hit_Invalidate; op5 I Fill and D Hit_Write_Back_Invalidate; op6 I/D Hit_Write_Back. Other region/opcode combinations stop before cache or link mutation. |
| Writeback details | D Index_Write_Back_Invalidate writes every non-Invalid line, including clean lines, from its stored tag, then invalidates it. D hit writeback writes only a dirty matching line and leaves it valid/clean. I Hit_Write_Back writes a matching valid line unconditionally. Writeback uses the line's stored physical tag, not the operation's translated target. |
| TagLo/TagHi | TagLo bits[27:8] carry hardware PTag PA[31:12], bits[7:6] PState, and reserved bits return zero. TagHi reads zero. Index operations translate the virtual address as specified in §16; their index uses VA bits. Internal aligned line keys are converted to/from hardware PTag, reconstructing PA bits below bit 12 from the index. I valid state is PState `10`; D clean/dirty are `10`/`11`; `00` is invalid. Undefined PState encodings refuse atomically. |
| Invalidation and links | I invalidation never changes D lines or the LL link. D hit invalidation clears the link only when it overlaps the selected 16-byte physical line. D Index_Write_Back_Invalidate uses the stored physical line interval. A failed writeback changes neither line nor link. |
| CPU stores and external writes | A cacheable CPU store changes D-line bytes and marks dirty; backing remains unchanged until writeback/replacement. KSEG1/uncached stores and PI/SP DMA update backing without snooping valid D lines. Cached aliases therefore remain stale until maintenance/replacement. Actual committed CPU/device bytes invalidate intersecting LL intervals; cache writeback is a distinct writer and does not snoop or clear unrelated links. |
| Identity | State v9 already hashes every I/D line key, validity, bytes, D dirty bits, link state, and cache-operation diagnostics. Tests verify sensitivity and machine isolation; no serializer change is needed. |

The aligned physical-line keys are internal representation choices. For valid
translations, page offsets are preserved and neither cache line can cross the
4 KiB minimum page boundary. The virtual index plus aligned physical key
therefore distinguishes the same identity as hardware PTag plus index,
including virtual synonyms. Hardware TagLo operations explicitly convert
between this key and PTag; the internal fields do not claim that hardware
stores PA[11:5] or PA[11:4] in tag RAM.

## Evidence and limits

Native tests execute code before and after uncached writes and targeted
invalidation; exercise both cache geometries at indices 0 and 511, I offset
28, same-index replacement, virtual synonyms, cacheable and uncached TLB
attributes, accepted backing domains, failed fills and writebacks, every
defined primary CACHE row, undefined states, cached user-RE byte/half/word/
doubleword stores, PI and both SP DMA-direction no-snoop behavior, and v9
digest sensitivity. Guest regressions check precise DBE for unmapped dirty
writeback from CACHE and ordinary cached load/store eviction. The warm-v1
profile keeps its exact separate CACHE and memory behavior.

The non-snooping behavior for CPU backing writes and PI/SP DMA is a selected
functional model policy. The manual geometry and operation facts above do not
establish that policy's physical-console behavior. Revisit this policy if an
authorized hardware measurement or an independent permitted hardware vector
shows different stale-code or invalidation behavior. Revisit the internal key
encoding only if an implemented architectural tag operation or an independent
state contract requires the raw PA[31:12] representation; such a change must
also review v9 digest identity.

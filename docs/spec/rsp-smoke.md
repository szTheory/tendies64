# Authored CPU–SP–RSP smoke fixture

This fixture is a small end-to-end software check for the
`ntsc-cart-4m-openipl3-rsp-functional-v1` profile (machine model 6, serialized
state 12, SP model `rsp-functional-issue-dma-divider-integer-v1`). Its purpose
is to prove that an authored CPU program can load and execute an authored RSP
program through the production CPU, bus, SP DMA, IMEM, DMEM, and RSP paths.
It does not establish compatibility beyond this profile or equivalence to
physical Nintendo 64 hardware.

## Guest flow

The CPU program starts at `0x80000400`. It copies the linked RSP words from the
cartridge window into RDRAM and uses SP DMA to place them at IMEM `0x1000`. It
then DMA-copies a 32-byte authored input block to DMEM `0x100`, clears SP HALT,
and waits for RSP BREAK. The RSP starts at PC `0x000`; the BREAK instruction is
at PC `0x09c`.

The fetched RSP stream includes:

| RSP PC | Raw word(s) | Purpose |
| --- | --- | --- |
| `0x010–0x014` | `0x10000001`, `0x24030009` | Taken BEQ and retired delay instruction. |
| `0x018–0x01c` | `0x24040007`, `0x4a0208d0` | Independent scalar add with a vector VADD issue pair. |
| `0x040–0x048` | `0x8c0a0100`, `0x01405821`, `0xac0b0024` | DMEM load, immediate dependent scalar use, and result store. |
| `0x050–0x064` | `0xc8c42000`, `0x4b031133`, `0xe8c42000`, `0xe8c42000` | Nonzero partial LQV, single-lane VMOV, aligned SQV, and a seven-byte partial SQV. |
| `0x068–0x080` | `0x4a020980`, `0x4a02095d`, `0x4a22099d`, `0x4a4209dd` | VMULF and VSAR reads of the high, middle, and low accumulator slices. |
| `0x08c–0x098` | `0x4a090232`, `0x4a090231` | VRCPH/VRCPL high/low sequence with zero input. |
| `0x09c` | `0x0000000d` | BREAK. |

The load-use pair is a fetched RSP LW immediately followed by an ADDU using the
loaded register. The runtime model schedules that dependency before the value
is stored to DMEM. The runner also requires nonzero RSP stall slots, the taken
branch and delay retirements, the independent SU/VU pair, and the BREAK
retirement.

## Independent expected values

The CPU input block begins at DMEM `0x100` with bytes
`11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff 00`. LQV uses effective address
`0x107`; it consumes the nine bytes up to the next 16-byte boundary. The
authored RSP setup writes `v1[0]=0x7fff`, `v2[0]=1`, `v1[1]=0xffff` (signed
`-1`), and `v2[1]=1`.

The expected arithmetic and memory checks follow directly:

- VADD lane 0 computes `0x7fff + 1`; signed saturation leaves `v3[0]=0x7fff`.
- VADDC lane 1 computes unsigned `0xffff + 1`; its result is zero and carry
  bit 1 is set. CFC2 stores VCO as `0x00000002` at DMEM `0x20`.
- VMOV copies `v3[0]` to only `v4[2]`. That replaces the loaded bytes `cc dd`
  with `7f ff` while preserving the other lanes.
- The aligned SQV at `0x80` and partial SQV at `0x99` yield this 32-byte
  CPU-checked DMEM window:

  ```text
  8899aabb7fffeeff 0000000000000000
  0000000000000000 008899aabb7fffee
  ```

- The load-use store at DMEM `0x24` and the input word at `0x100` both equal
  `0x11223344`.
- VMULF replaces the accumulator with `(2 × 32767 × 1) + 0x8000 =
  0x000000017ffe`. VSAR outputs `0x0000`, `0x0001`, and `0x7ffe` at DMEM
  `0xa0`, `0xb0`, and `0xc0` respectively.
- VRCPH first returns the prior high result, zero. VRCPL of zero then yields
  `0xffff` in the selected lane under this profile's named integer divider
  policy; the CPU checks DMEM `0xd0=0x0000` and `0xe0=0xffff`.

After BREAK, the CPU checks those DMEM values, SP status bits HALT/BROKE,
`SP_PC=0x09c`, and the read-to-set semaphore (first read zero, second read
one). Only then does it emit the exact 19-byte transcript
`T64-RSP-SMOKE:PASS\n`. The consumer additionally authenticates the ROM digest,
checks fetched CPU and RSP retirements, and requires the terminal CPU branch
`0x1000ffff` at `0x800006c0` followed by its exact NOP delay instruction at
`0x800006c4`.

## Encodings and selected policies

The RSP words are literal encodings. The small `RSP_MTC2`, `RSP_LQV`, and
`RSP_SQV` assembly macros name register, element, subopcode, and immediate
fields; the builder independently decodes representative linked words and
checks literal anchors. For Q-memory operations, the macro argument is a
**byte** offset and must be 16-byte aligned. The signed seven-bit encoded
immediate is `byte_offset / 16` (arithmetic shift right four), so byte offset
`0xa0` encodes immediate `0x0a`. The helper rejects unaligned and out-of-range
byte offsets.

The expected vector and accumulator values use the guide's operation
definitions and integer arithmetic. Two ambiguities are explicitly profile
policies, not hardware facts: VADDC follows the operation-specific Appendix
flag behavior where it conflicts with the overview, and the divider's zero
input/high-low behavior follows the integer policy named by this profile. The
VMULF and VSAR output slices are documented in the RSP instruction ledger.
Those choices are software-profile behavior; this guest is not a silicon
measurement.

Primary reference: *Nintendo Ultra64 RSP Programmer's Guide*, Version 1.1,
Silicon Graphics Computer Systems, Inc.,
[public PDF](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf),
printed pages 34–43, 47–56, 58, 60–79, 84–97, 237, 271–272, 284,
301–313, and 315. SHA-256 of the referenced guide
is `6ec646ba9bd836af9aa30ae90099af55dbaf291fbd8b39ff8be5664ada3bd668`.
The ledger in [rsp-instructions.md](rsp-instructions.md) records the specific
operation-level source pages and profile policies.

## Negative controls

The builder composes separate ROMs with separate receipts and manifest
identities. Each test starts a fresh machine and requires the baseline first.

| Variant | Change | Required result |
| --- | --- | --- |
| 1 | VMOV reads `v5[0]=0x8000` instead of `v3[0]=0x7fff`. | CPU follows its fail path; the checked output byte is `0x80` while expected is `0x7f`. |
| 2 | Fetch reserved RSP function word `0x4a0208f8` at PC `0x01c`. | Refusal at that exact fetched word. |
| 3 | Replace the BREAK word at PC `0x09c` with NOP. | `missing-break` on the fetched NOP. |
| 4 | Change the terminal CPU branch delay word to `0x24170001`. | Completion-delay mismatch after the terminal branch. |
| 5 | Start the 32-byte input DMA at DMEM `0x0ff8`, wrapping in the selected DMEM bank. | CPU-visible expected-byte check fails; it cannot qualify as a pass. |

The runner rejects setup errors and incomplete trace evidence as harness errors
or `incomplete`; an output transcript alone cannot qualify success or a guest
failure.

## Reviewed consumer boundary

The consumer admits only the six authored image identities above, verifies the
owned image and pinned boot payload before emulation, and reports its actual
core build identity. The builder checks tool binary hashes before any version
command, binds the reused packer to its audited source and saved binary receipt,
and verifies installed bytes against its checked candidate. Tests use independent
expected classifications and exact fields; a failed positive baseline stops
negative execution. Each subprocess retains child10s/outer12s; the serial matrix
has a separate bounded120s aggregate timeout. The generated fixtures use the
manifest's build-only channel for local and hosted checks, with no binary commit
or artifact upload. Hardware and external-diagnostic evidence remain separate.

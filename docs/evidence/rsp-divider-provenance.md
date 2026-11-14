# RSP divider source provenance

**Audit date:** 2026-11-14

**Status:** Pre-copy source, notice, and selected-file history audit passed.

**Evidence type:** Permitted software algorithm input. This is not a hardware
measurement, a physical divider-ROM dump, or proof of silicon edge behavior.

## Selected source and scope

The audited repository is [ares](https://github.com/ares-emulator/ares) at
commit [0b928930af4cc59ed99e942996f0d3c815379388](https://github.com/ares-emulator/ares/commit/0b928930af4cc59ed99e942996f0d3c815379388).
The exact saved source bytes were checked against the prior acquisition receipt:

| Upstream path | Selected functions or expressions | SHA-256 | Bytes |
| --- | --- | --- | ---: |
| [ares/n64/rsp/rsp.cpp](https://github.com/ares-emulator/ares/blob/0b928930af4cc59ed99e942996f0d3c815379388/ares/n64/rsp/rsp.cpp#L152-L165) | Integer reciprocal and inverse-square-root word generators, lines 152–165 | d475d3f891710750123fbde27f4064913b3d9f5e7f8a0d1ca8fc10854f489390 | 4,146 |
| [ares/n64/rsp/interpreter-vpu.cpp](https://github.com/ares-emulator/ares/blob/0b928930af4cc59ed99e942996f0d3c815379388/ares/n64/rsp/interpreter-vpu.cpp#L1334-L1363) | VRCP/VRCPH normalization, result assembly, and input/result latch sequence | 563e7d8a65fe2e4fc2312ee9e439ba0507ac92608f65af08bb4fa0bcc2ba82cf | 44,930 |
| Same interpreter file, [VRSQ/VRSQH](https://github.com/ares-emulator/ares/blob/0b928930af4cc59ed99e942996f0d3c815379388/ares/n64/rsp/interpreter-vpu.cpp#L1385-L1415) | Indispensable inverse-square-root normalization, exponent, and latch equivalent | Same whole-file digest above | — |

The second divider range is needed because the requested implementation covers
both reciprocal and reciprocal-square-root instruction families. The intervening
VRND implementation and the rest of the VU file were not selected.

## License and history result

The pinned [root LICENSE](https://github.com/ares-emulator/ares/blob/0b928930af4cc59ed99e942996f0d3c815379388/LICENSE#L1-L15)
contains the ISC grant and notice for ares. Its exact saved excerpt is
SHA-256 808eca8b48abaea476248d13d35c951f85137b210277c9c78f26919720d39593
(821 bytes). The selected source paths are part of the ares repository tree,
not its separately licensed thirdparty tree. No license or bundled-component
contradiction was found for these selected files.

The pinned commit is dated 2026-11-09 and changes PlayStation disc files only;
it does not change either selected RSP source file. The path-bounded history
captured through that revision contains 44 commits for rsp.cpp and 22 for
interpreter-vpu.cpp. The latest path commits are
6fcaa213fbe1aab501423a61903c3299f7658805 (2026-06-02) and
a4ac042f27eb3c7ee5165dd25ff57a12e7140841 (2026-04-15), respectively. The
reviewed subjects and path history show ordinary ares N64 RSP maintenance; no
moved-in third-party source or conflicting license signal was found. The full
bounded history receipt is checked by the local audit command.

The copied file will carry SPDX ISC and copyright ares team, Near et al.
The complete verbatim ISC text will be stored in LICENSES/ISC.txt. The copy
record will name both upstream paths, the pinned revision, this retrieval date,
the exact modifications, szTheory as maintenance owner, active upstream
status, and this allowed-source exposure.

## Prospective adaptation envelope

This audit authorizes only these source-derived software equations:

- Generate each 9-bit reciprocal word with integer division and the source's
  increment/shift rule.
- Generate each inverse-square-root word with a bounded integer search and
  squared-integer inequality.
- Adapt the selected normalization, sign/magnitude, exponent assembly, and
  high/low latch formulas from VRCP/VRCPH and VRSQ/VRSQH into small standalone
  C17 functions.

The adaptation will use explicit unsigned bit carriers, no host floating point,
no process-global mutable table, no literal table dump, no new dependency, and
no ares execution. Any separate edge outcomes chosen for zero, negative values,
minimum signed values, or latch sequencing are named software policy. Their
software tests cannot qualify physical ROM contents or exact hardware behavior.
Independent rational/integer inequalities, guide-supported anchors, fetched
instruction sequences, and per-instance latch tests provide the implementation
evidence; agreement with ares is not an oracle.

No helper has been copied at this pre-copy stage. The existing provenance
checker requires a real path and matching header before it can register a
per-copy entry, so provenance/copies.json and LICENSES/ISC.txt will be added
with the actual adapted file. The scoped audit and local receipt verification
are already complete; no checker rule is weakened.

## Verification controls

scripts/audit-rsp-divider.py --check reads the actual saved candidate source,
notice, acquisition metadata, and path-history receipt, and checks their
accepted hashes, exact scope markers, pinned revision, and history closure.
It does not contact the network. --selftest and
scripts/tests/test_rsp_divider_audit.py use only authored synthetic fixtures
under the ignored build tree and reject changed/missing source, foreign license,
altered notice, and history outside the selected path/revision.

No source has yet been copied or executed. After adaptation, the production
tests will independently prove table bounds and fetched opcode/latch effects.

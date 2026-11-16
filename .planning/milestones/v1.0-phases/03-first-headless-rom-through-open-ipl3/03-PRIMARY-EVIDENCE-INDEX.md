# Phase 3 Primary Evidence Index

**Retrieved:** 2026-11-10 (America/New_York)

The sources below are archival mirrors of Nintendo- or NEC-authored documents. SHA-256 records the exact bytes retrieved on this date; it does not independently prove mirror provenance or grant redistribution rights. Manual bytes remain outside the repository. This index contains source links and hashes only.

| Document / exact locator | Retrieved bytes SHA-256 | Identity and claim coverage |
|---|---|---|
| [NEC VR4300/VR4305/VR4310 User's Manual](https://hack64.net/docs/VR43XX.pdf) | `b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75` | PDF metadata and cover identify U10504EJ7V0UMJ1, 7th edition, August 2000, 655 pages. §§5.2.4/5.4.9: 32-bit kernel address translation; §§11.3–11.5: cache operations and state. |
| [Nintendo 64 Programming Manual PDF, NU6-06-0030-001G](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf) | `1cf05657ec35b807a6222e59e63913096bc21a047199e274b5667ecedd6ca89a` | 534-page manual copy. Hardware architecture, RSP/RDP, VI, SI, and OS sections; its §6.10 boot procedure appears on printed pages 87–88. The online manual identifies the material as draft. |
| [Nintendo Programming Manual, Chapter 3: Hardware Architecture](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro03/index.htm) | `27f4b8c201af03b50022dacb12cfcc5fd5de7bf14cafa244120039a774cd6f10` | Nintendo-authored architecture overview; CPU/RCP/RSP/RDP and concurrency context. |
| [Nintendo Programming Manual, Chapter 11: RSP Graphics index](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro11/contents.html) | `f740c843fe67adac1035047373555cfae1541cc41bff43bb8e27c0ac83b01bb7` | RSP graphics programming background; not evidence that Phase 3 needs RSP instruction execution. |
| [Nintendo Programming Manual, Chapter 12: RDP Programming](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro12/index.htm) | `08b2cfc83d7103686d0386943b830564c57f865eae461197dcd862e2fac21b93` | RDP programming background; outside Phase 3 execution scope. |
| [Nintendo Programming Manual, §30.1: Video Interface](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm) | `25c00e74b6d8d58f6f14788a3d8a4c555efcbce90a5b25d11b413db65da1c1a2` | VI background; outside the hello boot path. |
| [Nintendo Programming Manual, §26.1: SI device introduction](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro26/26-01.htm) | `42bfa0cb875b0a14adabf03044254c24e9ff29915ab352b2855d7ecc33fb313a` | SI device background; the Phase 3 input trace does not imply guest-visible controller emulation. |
| [Nintendo Programming Manual v5.2, §26.7: Transfer Pak](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro26/26-07.htm) | `03c41c798ca261b663f1a524cd883f3799a91325def2a456b62571329f99f2b0` | Transfer Pak background; outside the named cartridge-only profile. |
| [Nintendo 64 64DD Programming Manual, Chapter 1](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/64ddman/dd01/index.htm) | `75cf7a845e31ef58035928440ff1ee247d5fb3a92470f1edc317bbd11b8cba00` | Separate-system profile background; outside Phase 3. |
| [Nintendo 64 Introductory Manual, §1.2.3 RCP overview](https://ultra64.ca/files/documentation/online-manuals/man/kantan/step1/2-3.html) | `e414b9b7250bfde38d21118bb6fbb47ac574e52346f6fb5907fc20db7a07c50f` | CPU/RCP architecture context; does not specify a custom IPL3 register handoff. |
| [Nintendo Programming Manual v5.2, §6.10: Boot Procedure](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro06/06-10.htm) | `07de17c36db262657a0f3a3eb1e8c12449a3e440eef911ff3517618e24c75040` | Boot-chain overview. It describes aggregate boot-code responsibilities; it does not divide them among IPL1/2/3 or enumerate the CPU register state at a custom IPL3 entry. |

## Locator corrections

Three bootstrap-pack URLs returned HTTP 404 on retrieval: the v5.1 RSP `contents.html` locator, the v5.1 SI `contents26.4.html` locator, and the concurrency page under the v5.1 path. The table records the retrieved current mirror pages used for those topics. The exact failed URL text remains in `.planning/bootstrap/EMULATOR-DNA.md`; no claim depends on a missing page.

## Use and rights boundary

Use the citations and hashes for planning and source traceability. Do not add the manual PDFs, downloaded HTML, extracted manual text, or screenshots to the repository or CI artifacts. Hashes establish identity of retrieved bytes only; they are not rights clearance.

# Deterministic RDRAM boot seed and first Tier B observation

## Scope

This note records the software seed and the first bounded Tier B startup
observation for the rights-cleared, authored `hello-openipl3` fixture. The
fixture is 4,280 bytes with SHA-256
`fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c`; its
cleared 4,032-byte IPL3 payload has SHA-256
`f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5`. This
is a deterministic emulator startup experiment, not a physical cold-reset
measurement or a general ROM support claim.

## Seed ownership

The canonical machine owns the HLE prepared state. Admission clears the
machine, RDRAM, SP DMEM, and SP IMEM, then copies the pinned IPL3 payload from
ROM offset `0x40` to DMEM offset `0x40` and starts at `0xA4000040`. The
deterministic common seed retains the existing selected S3–S7 values and
VR4300 state documented in [CPU profile](cpu-profile.md). The two Phase 6
profiles bind the device selection explicitly:

| Profile | Installed topology | `MI_VERSION` | `RI_SELECT` | Timing policy |
| --- | --- | ---: | ---: | --- |
| `ntsc-cart-4m-openipl3-rdram-a-t1-v1` | 4 MiB, no expansion | 0 | 5 | `t1-instruction-cost-v1` |
| `ntsc-cart-4m-openipl3-rdram-b-t1-v1` | 4 MiB, no expansion | 0 | 0 | `t1-instruction-cost-v1` |

These values are profile-selected software inputs. In particular, zero
`RI_SELECT` is the seed for the source-described cold path, not a claimed
observation of a console's IPL2 handoff or register reset values. The existing
warm-v1 identity and behavior remain frozen.

## First Tier B result

One production `integration.cold-startup` invocation used the cleared fixture
and Tier B profile. The first observed RI operation was:

| Field | Observation |
| --- | --- |
| Operation | Read |
| Physical address | `0x0470000c` (`RI_SELECT`) |
| Width | 4 bytes |
| Value | `0x00000000` |
| CPU PC / opcode | `0xA4000094` / `0x8c42000c` (`lw v0,0xc(v0)`) |
| Guest tick | 53 |
| Bounded run stop | `CPU_UNSUPPORTED` at tick 75 |
| Retained output | 0 bytes; empty-prefix SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| Dropped trace records | 0 |

The run therefore reached the selector read and recorded the seeded zero. It
did not establish successful RDRAM initialization, an authored hello pass, or
the exact terminal unsupported instruction. The traced selector read itself
was serviced successfully; the saved terminal record has no stop PC/opcode or
bus refusal status/address, so it cannot distinguish an unsupported CPU
encoding from a later unsupported bus/register operation. The stop reason is
an observed model boundary, not guest completion. The exact invocation and
unmodified runner result are retained in the ignored local execution
checkpoint beneath `build/gate/06-cold-startup/`.

## Tier B native protocol model

This section specifies the bounded model used by `unit.core.rdram-protocol`.
It is a versioned software policy for the named Tier B profile, not a second
hardware observation or a claim that the retained guest run completed RDRAM
initialization.

### Evidence and model boundary

The Nintendo SDK `rcp.h` map names the RDRAM register window at `0x03f00000`,
the RDRAM broadcast window at `0x03f80000`, and the RI/MI register blocks.
The pinned open IPL3 routine defines RDRAM registers 0 through 9, writes the
broadcast delay/ID/mode/ref-row sequence, encodes device IDs, and probes IDs
in increments of two. Its `wait(0x100)` is 256 loop iterations. These are
facts about that named software algorithm and register map; they do not prove
post-IPL2 register values, a physical module layout, an analog current, or
hardware timing. The implementation references are [Nintendo SDK `rcp.h`](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm),
[pinned `boot/rdram.c`](https://github.com/n64brew/libdragon/blob/98154dafd5ac50e74ee09a5de1035d24a53f54f0/boot/rdram.c),
and [pinned `boot/minidragon.h`](https://github.com/n64brew/libdragon/blob/98154dafd5ac50e74ee09a5de1035d24a53f54f0/boot/minidragon.h).

The v1 model maps each source `wait(0x100)` to an absolute deadline 256 guest
ticks after the command. This is a named deterministic clock policy; it does
not equate source loop iterations with hardware time or CPU retirement. The
deadline is independent of CPU instruction count, survives partitioned
`t64_machine_advance` calls, and refuses an addition that would overflow guest
time.

For this 4 MiB profile, the model names two logical software slots, IDs 0 and
2, and treats each as a 2 MiB logical range because the pinned discovery loop
advances IDs by two and describes its supported device geometry as 2 MiB. The
model does not assert that a console contains two physical chips or any
particular package arrangement. The absent ID 4 probe completes a modeled
capacity scan and reports the profile's installed 4 MiB backing; installed
capacity alone is not used to claim physical topology.

The mode command with auto-current bit `0x80` records that the software sent
its auto-current command. The model does not simulate the source's analog
search or choose a numeric calibration current. AUTO readback uses the
functional feedback rule below. After that command, the modeled type-register
word is `0x000019b4` (row bits 9, bank bits 1, column bits 11, ninth bit 1,
other decoded fields zero), solely to make the source's 2 MiB geometry branch
deterministic. Manufacturer value zero is an explicit unknown sentinel, not a
vendor identity. These values are model assumptions, not hardware
measurements. The test's `0xdeadbeef` delay and `0x12345678` refresh/RAS words
are authored opaque payloads used to check storage and byte order; they are
not copied calibration or timing constants.

### Functional calibration feedback v1

The permitted routine's zero-origin sweep has deterministic software feedback:
manual logical CCI zero hides read output, while nonzero manual codes and AUTO
expose stored data. Writes and backing bytes remain intact. AUTO returns the
requested logical CCI in the six source-defined current fields instead of
echoing the complemented programming bits. This is a selected software
handshake, not an analog threshold, measured current or hardware-accuracy
claim. The guest's calibration/capacity loops remain intact. No PC, ROM,
transcript or completion check selects this behavior.

One pure visibility owner serves byte/word/wide bus reads, instruction/data
cache hits and misses, and both SP DMA read paths. Cache storage keeps its
ordinary contents; filtering at consumption avoids storing synthetic zeros in
cache lines. A later manual-zero command hides output again. Controlling mode
and state fields already belong to state v13; RI reset clears them. There is
no read-side counter, host input or hidden state. Independent native sequences
cover both logical slots, preserved backing, AUTO readback, cache hits, DMA,
later mode changes and reset. Earlier raw-echo AUTO assertions describe the
incomplete model and are replaced by literal logical-CCI expectations.

### Accepted words and refusals

All accesses below are aligned 32-bit words. Other widths, unlisted offsets,
out-of-order commands, and unmodeled devices are refused without changing
device state or MI interrupt pending/mask/line fields. RDRAM-register values
use the byte-swapped wire representation used by the pinned source; the table
labels the source's logical value where it helps explain a transition.

| Address / register | Accepted operation | Readback and effect | Refusal boundary |
| --- | --- | --- | --- |
| `0x04700004` RI_CONFIG | `0x40` while seeded RI_SELECT is zero | Reads `0x40`; starts the `tick + 0x100` model deadline | Other values/order; repeated identical write is idempotent |
| `0x04700008` RI_CURRENT_LOAD | `0` at or after the RI_CONFIG deadline | Reads `0`; records that the model load command was applied, without a numeric analog result | Before deadline, nonzero value, or absent config |
| `0x0470000c` RI_SELECT | `0x14` after current-load application | Reads `0x14`; enables the next modeled RI stage | Other values or earlier order |
| `0x04700000` RI_MODE | `0` after RI_SELECT, then `0x0e` after the first `tick + 0x100` deadline | Reads the last mode command; reset initializes the two logical slots and clears repeat/capacity progress; standard mode starts a second deadline | Wrong value/order or early standard-mode write; reset deliberately restarts the device sequence |
| `0x04300000` MI_MODE | `0x10f` after standard mode's deadline; `0x80` clears repeat | Reads `0x10f` while repeat length 16 is enabled, otherwise `0`; repeated identical commands are idempotent | Other repeat encodings or before RI is ready; MI pending/mask state remains separately owned |
| `0x03f80000 + reg*4` broadcast | `reg 2` opaque delay word after repeat; then `reg 1` logical ID 511, `reg 3` logical mode `0x44`, and `reg 5` logical row zero, in that order | Applies the modeled register word to both logical slots | Broadcast reads, wrong values/order, unsupported registers, or missing repeat mode |
| `0x03f00000 + id*0x400 + reg*4` device register | ID register 1 remaps 511→0 then 511→2, then valid packed IDs without collision; mode register 3 accepts source enable word `0x46` and later mode commands retaining its device-enable bits; delay 2 and RAS interval 6 are opaque stored words | ID/delay/RAS and manual-mode reads echo in wire byte order; AUTO current fields use functional feedback v1. After the auto-current command, type register 0 returns wire word `0xb4190000`; manufacturer register 9 returns unknown sentinel zero | Wrong ID/order, missing device, unmodeled register, mode without device-enable, or RAS before the auto-current command |
| Device ID 4 / mode register 3 | The next source probe's ID assignment and enable write are accepted as an absent logical slot after IDs 0 and 2 complete the modeled auto-current command | Mode read returns zero and marks the scan complete; logical capacity becomes the profile's 4 MiB backing | The model does not expose an extra slot or alias above the installed extent |
| `0x04700010` RI_REFRESH | Any aligned word, including after the source's zero-weighted calibration branch and before capacity enumeration | Reads back the same word; no refresh timing is simulated | Unsupported access widths |
| `0x04700014` RI_LATENCY and all other unlisted registers | None | No modeled effect | `T64_STATUS_BUS_UNSUPPORTED` |

Native tests execute independent register sequences, check before/after state
on reached refusals, and exercise idle, variable-cost, and partitioned guest-
time advances. The RDRAM bus window, CPU data/fetch/store ownership, PI/SP DMA
range checks, and state-v13 digest sensitivity have separate native coverage.
The compat-v2 CPU-only absent-memory zero-read windows and their exclusions
remain unchanged; this table does not expand them.

## Limits

The result applies only to this build, profile, cleared fixture, and bounded
machine run. It does not establish the next RDRAM operation, the full source
branch, device topology, calibration values, Expansion Pak support, physical
boot behavior, or hardware accuracy. Do not infer a hello success from the
selector read or the CPU stop. Further modeled behavior requires its own
source-reached operation and exact address, width, effect, state, and refusal
controls while preserving the historical profile contracts.

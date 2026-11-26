# C API contract of the installed library

Status: 0.x freeze candidate. The Phase 11 freeze record
(`docs/abi/FREEZE-CANDIDATE.md`) binds it, and only defect-driven amendments with
evidence may change it before the final bind at the end of Phase 12 (section 14). This document is not a release
statement: no tag, package channel or version promise is made, and installing
the package is a local act.

## 1. Scope and evidence tier

This document is the written contract of the public C API declared in
`core/include/tendies64/t64_machine.h` (installed as `tendies64/t64_machine.h`)
and implemented by the static archive `libt64_core.a`. It describes the code as
it is at the freeze candidate. The statements about status order and out
parameters were read from the code of each entry; section 16 lists the CTest
names that pin them, and section 15 lists the known limits.

The candidate is a software-interface claim only, authored scope. It states
what each call does to the instance and to the caller's memory, which status it
returns and in what order it checks. It states no emulation compatibility, no
accuracy and no hardware claim: the guest results the API reports are
deterministic software expectations of the authored profiles
(`docs/profile-limits.md`), and nothing here says that a profile matches a
console, a game or another emulator.

Sources, in order of authority:

1. The header: declarations, macros and the comments next to them.
2. This document: the rules and the per-function table of section 12.
3. `docs/abi/status-classes.txt`: the one class table of every status and stop
   reason. It is the single source; this document points to it and does not
   copy its rows.
4. `docs/abi/layout.c`: the generated layout pins (sizes, offsets, constants).
5. `docs/spec/machine-contract.md` and the device specifications it links:
   admission, scheduler, device and reset behavior.

When this document and the code disagree, the code and the CTest names win and
the disagreement is a defect to repair by a numbered plan.

## 2. ABI identity and the installed package

### 2.1 Version constants

`T64_ABI_VERSION_MAJOR` is 0 and `T64_ABI_VERSION_MINOR` is 1. `T64_ABI_VERSION`
is `(major << 16) | minor` (0x00000001), and `t64_abi_version()` returns that
value for the library built from the same sources, so a host can assert that
the header it compiled against and the library it linked agree.
`t64_build_identity()` returns a string of the form `t64-build-v1-` followed by
64 hexadecimal digits, a fingerprint of the project version, the compiler and
build settings and the hashes of the production sources, headers and build
inputs; it holds no timestamp and no local path. `t64_source_revision()` is the
source revision recorded at build (the text `unrecorded` when none was
available); it is informational and enters no identity and no digest. A host
checks the package against the library with `t64_build_identity()` equal to the
`tendies64_BUILD_ID` of the package configuration.

### 2.2 The create-time version rule

The create-info carries the `abi_version` the caller compiled against
(`T64_MACHINE_CREATE_INFO_INIT` fills it with `T64_ABI_VERSION`).
`t64_machine_create` accepts a caller whose major equals the library major and,
while the major is 0, whose minor equals the library minor. After a major bump
the coded rule accepts a caller minor at most the library minor; no major bump
has happened, so that branch is not exercised. Any other value is refused with
`T64_STATUS_ABI_MISMATCH`, `*machine_out` is NULL and nothing was allocated.

Before the final bind the minor increments on every change of a bound golden
(section 14). While the major is 0 a consumer is therefore rebuilt against the
header of the library it links.

### 2.3 The package

The package is named `tendies64` and is installed in the install component
`Development` as six files: `lib/libt64_core.a`,
`include/tendies64/t64_machine.h` and
`lib/cmake/tendies64/tendies64{Config,ConfigVersion,Targets,Targets-<config>}.cmake`
(the library directory follows `GNUInstallDirs`). The package version is the
ABI candidate number 0.1.0 with `COMPATIBILITY SameMinorVersion`:
`find_package(tendies64 0.1 CONFIG REQUIRED)` accepts it, while a request for
0.2 or for 0.1.5 is refused (`installed.controls.version-mismatch-minor` and
`installed.controls.version-mismatch-patch`). `project(VERSION)` stays 0.0.0 and
is not a release version; "version mismatch" in this document always means an
ABI constant. The imported target is `tendies64::core`. The configuration
records `tendies64_BUILD_ID` and `tendies64_BUILD_TYPE` for the one built
configuration, and the package is relocatable (a moved prefix, including a path
with spaces, is exercised by `installed.controls.relocated-spaces`).

### 2.4 The installed header set

Exactly one header is installed: `tendies64/t64_machine.h`. It is
self-contained: it compiles alone as C99 (pedantic, warnings as errors) and as
C++17, carries `extern "C"` guards, and includes `stddef.h`, `stdint.h` and, in
C only, `stdbool.h`. `t64_facade.h` is the header of the in-tree synthetic
guest runner; it is neither installed nor part of this contract.

### 2.5 The `t64i_` prefix rule

The 35 public functions are marked `T64_API` (default symbol visibility under
GCC and Clang) and are listed in `docs/abi/public-symbols.txt`. The library is
a static archive, so `T64_API` hides nothing else. Every other global symbol
the archive defines carries the prefix `t64i_` (a generated rename header
covers the internal functions, the vendored floating-point library and the
folded SHA-256 sources). A host global such as `f32_add` or `t64_sha256_init`
therefore cannot collide with an archive symbol; the symbol-collision control
(`installed.controls.symbol-collision`) links such a consumer and its results
do not change. The prefix is a naming rule for the archive's globals, not a
promise about any compiler's visibility handling.

### 2.6 What the library asks of the host

The host provides the allocator callbacks and nothing else. The archive imports
no file, clock, entropy, thread or environment symbol. Its archive-level
undefined set is the memory functions (`memcmp`, `memcpy`, `memset`, and
`bzero` on macOS), the stack-protector pair of the platform and, on ELF,
the linker-defined `_GLOBAL_OFFSET_TABLE_`; the per-platform rows are in
`docs/abi/installed-undefined.txt`, and a seeded object that imports a clock,
file, entropy or thread symbol makes the check fail (`api04.members`,
`api04.disasm`, `api04.seeded`). The core keeps no writable file-scope data and
no constructor (the determinism lint rules `L2-W-SECTION` and `L2-CTOR`), so
distinct instances share no state (section 11).

## 3. The struct_size rule

Every in/out struct except `t64_trace_record` starts with a `uint32_t
struct_size` that the caller fills with the size of the struct it was compiled
against. The `T64_*_INIT` macros fill it (`T64_HOST_ALLOCATOR_INIT`,
`T64_MACHINE_CREATE_INFO_INIT`, `T64_STOP_REQUEST_INIT`, `T64_STOP_RESULT_INIT`
and `T64_STOP_RESULT_INIT_WITH`, `T64_CPU_STOP_DETAIL_INIT`,
`T64_OUTPUT_DRAIN_RESULT_INIT`, `T64_OUTPUT_STATUS_INIT`,
`T64_VIDEO_PACKET_INFO_INIT`, `T64_AUDIO_PACKET_INFO_INIT`,
`T64_STATE_IMPORT_REPORT_INIT`, `T64_SAVE_INFO_INIT`); they are brace
initializers valid in C99 and C++17. The rules:

- The callee never writes the `struct_size` member of a caller's struct.
- The callee reads or writes at most `min(struct_size, known size)` bytes, so a
  struct built against a later, larger layout keeps its trailing bytes.
- A `struct_size` below the v1 size is refused with
  `T64_STATUS_STRUCT_TOO_SMALL`, before the admission and state checks (the
  `NOT_READY` family and the rest), with every output, the struct and the
  instance untouched. The v1 sizes are pinned in `docs/abi/layout.c`.
- The struct span is judged before it is read: a span that wraps or overlaps
  the instance's own storage is `T64_STATUS_INVALID_ARGUMENT`, and a hostile
  pointer is never read for its `struct_size`.
- A caller whose `struct_size` is larger than the v1 size gets exactly the v1
  members.

The entries that take a struct are `t64_machine_create` (the create-info, the
allocator and the profile descriptor), `t64_machine_advance` (request and
result), `t64_machine_state_import` (the report when it is not NULL: a NULL
report carries no size and is accepted), `t64_profile_hash` and
`t64_profile_serialize` (the descriptor), `t64_machine_cpu_stop_detail`,
`t64_machine_output_drain`, `t64_machine_output_status`, `t64_video_info`,
`t64_audio_info` and `t64_machine_save_info`.

Exceptions, each stated in the header:

- `t64_trace_record` is an array element without `struct_size`:
  `t64_machine_trace_drain` takes the caller's record size as a parameter
  (`record_size`, below the v1 record size is `STRUCT_TOO_SMALL`) and uses it
  as the stride.
- `t64_host_storage` is filled by the core, which is its caller: the core sets
  `struct_size` before every allocate callback and the callback writes only
  `data`, `capacity` and `alignment`.
- `t64_profile_descriptor.struct_size` is outside `t64_profile_hash` and
  `t64_profile_serialize` (both read named members only), so a descriptor hash
  does not depend on it. `t64_machine_profile` returns the instance's own copy
  at the v1 descriptor size; a consumer built against a later, larger
  descriptor must check `struct_size` before it reads a member beyond the v1
  members.

Struct members are fixed-width integers, `char` arrays and pointers: no
`size_t`, no `bool` and no enumeration type is a member. Function parameters
and the allocator callback keep `size_t` and `bool`. No `sType` or `pNext`
chain exists. The layout pins of `docs/abi/layout.c` hold for targets with
64-bit pointers (macOS arm64 and Linux x86_64 are the exercised targets); six
structs contain implicit alignment padding (section 15).

## 4. Status classes

Every `T64_STATUS_*` and every `T64_STOP_REASON_*` constant has exactly one row
in `docs/abi/status-classes.txt`, which is the single source of the classes:

- `ok`: success.
- `refused`: the call is rejected; the caller (or the host's allocator) broke a
  stated precondition, or the instance is in a state that does not admit the
  call. The instance and every public observation are bit-identical afterwards.
  The one carve-out is `t64_machine_bus_write` (section 10.1). A call may
  refuse after it asked the allocator for a block and gave it back
  (admission and create do); the instance then holds nothing it did not hold
  before.
- `guest-stop`: not an error. It is a stop reason of `t64_machine_advance`; the
  guest state is valid. The row says whether it is `resumable` or `terminal`.
- `defect`: the library found one of its own invariants broken. The instance is
  poisoned (section 6).
- `reserved`: a value kept for numbering and never returned through the
  installed API: `T64_STATUS_UNSUPPORTED_ROM_ORDER` (6) and
  `T64_STOP_REASON_HALT` (4).

A caller fault never poisons an instance. There are 39 status values (0 to 38)
and 8 stop reasons (1 to 8); no value is renumbered or reused. The checker
`repo.abi-status-classes` fails when a constant of the header has no row, a row
has no constant, a value differs from the header, or a class or resume field is
outside the closed sets; the installed consumer's own copy of the table is
compared with it.

A refused call reports where the data it was asked for stands: statuses such as
`BUFFER_TOO_SMALL` write the required length (or, for output drain, the result
record) as stated per function in section 12, and a state import writes its
report. No other out parameter of a refused call changes.

## 5. Entry order

Two entries have no guard: `t64_machine_destroy` and
`t64_machine_is_poisoned`. Every other entry that takes a machine runs the entry
guard first, in this order:

1. a NULL machine is `T64_STATUS_INVALID_ARGUMENT`;
2. a poisoned instance is `T64_STATUS_POISONED`;
3. a call made while the core is inside a host allocator callback of the same
   instance is `T64_STATUS_REENTRANT`.

A guard refusal returns before the entry reads or writes anything else, so
every output buffer and out-struct stays exactly as the caller left it. After
the guard each entry checks, in this order unless section 12 says otherwise:
argument pointers and spans (NULL, wrapping spans, overlap with the instance's
own storage, overlap between the caller's outputs:
`T64_STATUS_INVALID_ARGUMENT`), then `struct_size`
(`T64_STATUS_STRUCT_TOO_SMALL`), then admission and state (`T64_STATUS_NOT_READY`
and the entry's own refusals). The exceptions are the save export and import
entries, which judge admission before the pointer row, and `t64_machine_create`
and `t64_machine_admit_rom`, whose own orders are in section 7.

The entries that take no machine (`t64_machine_create`, `t64_profile_lookup`,
`t64_profile_hash`, `t64_profile_serialize` and the three version queries) have
no guard and no instance to refuse on.

## 6. Poison and reentrancy

A call that finds an internal invariant of the core broken returns
`T64_STATUS_INTERNAL_DEFECT` and poisons the instance. The defect sites are
listed in the `T64_STATUS_INTERNAL_DEFECT` row of the class table (the CP0
count phase, the stored profile digest at admission, a failing reset after the
image swap, the digest stream and state export inconsistency sites, a handler
status outside the whitelist during advance, and the output observer
inconsistencies that state import rejects, so no caller input reaches them).

On a poisoned instance every entry that takes a machine except
`t64_machine_destroy` and `t64_machine_is_poisoned` returns
`T64_STATUS_POISONED` and leaves every output buffer and out-struct as the
caller left it. `t64_machine_destroy` still releases every host allocation. The
flag is sticky: reset, unload and admission do not clear it, and the only exits
are destroy and a new create. A defect is a fault of the library; a host should
log `t64_build_identity()` with it and stop using the instance.

`t64_machine_is_poisoned` writes 1 or 0 to `*poisoned_out`; a NULL machine, a
NULL output or an output span that overlaps the instance is
`T64_STATUS_INVALID_ARGUMENT` with the output untouched.

A call made on an instance from inside one of its own host allocator callbacks
(admission, unload and destruction call the host) is refused with
`T64_STATUS_REENTRANT` before it reads or writes anything. `t64_machine_destroy`
is the one call that is not refused: it is unguarded so that it can always
release the instance, and it must not itself be called from inside one of that
instance's allocator callbacks (undefined behavior of the caller).

## 7. Create, admission, reset, unload and destroy

### 7.1 Create

Check order of `t64_machine_create`, each refusal with no allocation and
`*machine_out` NULL when `machine_out` is not NULL: a NULL `machine_out` or a
NULL `info` is `INVALID_ARGUMENT`; `info->struct_size` below the v1 size is
`STRUCT_TOO_SMALL`; an `abi_version` that fails the rule of section 2.2 is
`ABI_MISMATCH`; a NULL allocator or profile is `INVALID_ARGUMENT`; an allocator
or profile `struct_size` below its v1 size is `STRUCT_TOO_SMALL`; then the
allocator and profile checks (a NULL callback, a descriptor with an invalid text
or numeric member, an event queue capacity above the compiled limit, a pinned
profile id carrying other bytes than the registered descriptor, a non-zero save
kind or peripheral flag that the descriptor version has no device for, all
`INVALID_ARGUMENT`). The allocator then provides the instance block and, for a
profile with a video device and a profile with an audio device, one packet-ring
block each. A host allocator that refuses is `ALLOCATION_FAILED`; one that
returns a block that breaks the allocator contract (a wrong capacity or
alignment, or a range that overlaps the instance, the caller's create arguments
or another block) is `BAD_STORAGE`; blocks already obtained are released first.
The allocator contract itself is in `docs/spec/machine-contract.md` "Host
allocator contract".

A created instance has no admitted image, guest tick 0 and tracing off. A
profile comes from `t64_profile_lookup`; the descriptor handed to create is
copied into the instance (the instance's copy is what `t64_machine_profile`
returns).

### 7.2 Admission and replacement

`t64_machine_admit_rom` takes one caller span. The host selects and reads the
file; the core never opens a path. The first admission installs the image;
admission on an admitted instance replaces it.

Admission checks, in order: the entry guard; the stored profile digest against
the profile bytes (a mismatch is `INTERNAL_DEFECT`); then the input. For the
skeleton profile (`all-known-orders-to-big-endian-v1`) the length must be 4096
to 67108864 bytes and divisible by four, and the first four bytes must be one of
the three raw dump signatures (canonical big endian `80 37 12 40`, byte swapped
`37 80 40 12`, 32-bit word reversed `40 12 37 80`); all three are stored in
big-endian order. Every pinned Open IPL3 profile (every other shipped profile;
its `rom_order_policy` starts with `exact-source-or-flat-payload-z64`) narrows
admission: after the length and input interval checks it accepts only the exact
pinned 4096-byte source asset or an authored big-endian z64 cartridge (first
bytes `80 37 12 40`) that passes the payload, header and size checks recorded
in `docs/spec/boot-limits.md`, and refuses every other input, byte-swapped and
word-reversed dumps included, with `T64_STATUS_INVALID_ROM`. The profile-
specific rule lives in the profile's `rom_order_policy`, which is part of the
hashed descriptor. A NULL span, a length outside the profile's interval, a span
that wraps or overlaps the instance or its admitted image and an input that
fails the profile's checks are all `INVALID_ROM`; the host allocator can answer
`ALLOCATION_FAILED` or `BAD_STORAGE` for the candidate block.

A successful admission replaces the previous content as one commit: image
storage, size, the 64-byte header, content digest and metadata, guest tick 0,
cleared RDRAM and device state, counters back to zero, empty rings and an empty
drain cursor, a cleared trace buffer (with one admission record when tracing is
enabled) and blank save content. The profile and the allocator are kept. The
previous image block is released only after the new candidate is complete. A
refused admission leaves the previous image, its digest, header and size, the
profile, the guest state and the outstanding ownership unchanged. A failure
after the image swap (the reset of the guest state) is a defect: the instance
is poisoned and every block is released exactly once, so destroy stays exact.

### 7.3 Reset

`t64_machine_reset` returns the admitted guest to its first-admit state. The
postcondition is that of replacing the image with the same bytes: guest tick,
instruction and frame counters, RDRAM, SP memories, CPU and RSP state, the
scheduler queue, every device and the save device are back at the values right
after the first admission, the video and audio rings and the ISViewer drain
cursor are empty, the trace records and drop count are cleared (trace enablement
is kept and one admission record is emitted when it is enabled), and the machine
digest equals the digest taken right after the first admission. The image, the
profile and the allocator are kept; reset calls neither allocator callback.
Without an admitted image the answer is `NOT_READY` with the instance unchanged.
A failure after state was touched is a defect.

### 7.4 Unload

`t64_machine_unload` releases the image block through the host allocator (one
release callback, run with the in-call flag raised) and clears the image
members. The guest state is cleared as for a replacement, no boot entry is
prepared, and the instance answers every entry that needs an image with
`NOT_READY` and untouched outputs, exactly as an instance that never admitted
one (the table is in section 12; `docs/spec/machine-contract.md` "Replacement
reset snapshot" lists the state rows). Admission after an unload equals a first
admission: the machine digest is the one a fresh instance reports, and destroy
after an unload leaves no outstanding host allocation. Unload with no image is
`NOT_READY`.

### 7.5 Destroy

`t64_machine_destroy` releases each block of the instance exactly once through
the allocator's release callback (the image block when admitted, the ring
blocks, the instance block). `t64_machine_destroy(NULL)` does nothing. It works
on a poisoned instance. The instance pointer is invalid afterwards. The host
serializes destroy against every other call on that instance (section 11).

## 8. Advance, stop reasons and guest time

`t64_machine_advance` runs the guest until a stop condition the caller names and
reports why it stopped. The request has a kind and a value:

- `T64_STOP_GUEST_TICK`: the value is an absolute guest tick. A value behind the
  current guest tick is `T64_STATUS_TIME_REVERSED`. The stop reason is
  `T64_STOP_REASON_BOUND` and `result_out->tick` is the bound.
- `T64_STOP_INSTRUCTION_COUNT`: the value is a limit on the instruction counter,
  which CPU retirement advances. A limit behind the counter is
  `T64_STATUS_LIMIT_REVERSED`. The stop reason is
  `T64_STOP_REASON_INSTRUCTION_COUNT`.
- `T64_STOP_FRAME_EVENT_COUNT`: see 8.2.
- Any other kind is `T64_STATUS_INVALID_ARGUMENT`.

Check order: the entry guard; the request and result spans (NULL, wrapping or
overlapping the instance), judged before either `struct_size` is read; the two
`struct_size` values (`STRUCT_TOO_SMALL`); `NOT_READY` without an admitted
image; then the request. The result is built in a local and copied out only on
`T64_STATUS_OK`, after `struct_size`, so a refused call leaves the result struct
untouched. `result_out->tick` is the guest tick at which the call stopped.

### 8.1 Stop reasons

`T64_STATUS_OK` with a stop reason is the normal answer. The reasons and their
classes are in the class table; in words:

- `BOUND`, `INSTRUCTION_COUNT` and `FRAME_EVENT_COUNT` are resumable: the caller
  asked for the stop and the next call continues.
- `QUIESCENT`: a counter bound is not reached and the event queue is empty;
  resumable by queued input only.
- `RESOURCE_LIMIT`: resumable after drain or yield. The causes are the
  per-advance event budget of the profile, an event that cannot dispatch for
  VI or AI output backpressure (a full packet ring) or counter overflow, and a
  masked handler status from a closed whitelist (any other handler status
  poisons the instance and advance answers `INTERNAL_DEFECT`). The host drains
  the rings (or simply calls advance again after the budget stop) and
  continues. A stall with no progress after a full drain is permanent.
- `CPU_UNSUPPORTED`: terminal. The CPU met an instruction it does not
  implement. It neither poisons the instance nor claims that the guest chose to
  stop: it is an emulator limitation, and `t64_machine_cpu_stop_detail`
  reports the retired instruction count, the program counter and the opcode.
- `GUEST_COMPLETION`: terminal. A software debug-stop convention, not a guest
  pass and not a CPU exception.

After a terminal stop every later advance whose bound is not behind the guest
tick or the counter (`TIME_REVERSED` and `LIMIT_REVERSED` are still answered
first) answers `OK` with the same terminal reason, even when a counter limit
equals the counter, until the instance is reset or replaced (an unloaded
instance answers `NOT_READY`). `HALT` is reserved and unreachable through the
installed API.

### 8.2 The frame-event counter

The instruction counter is advanced by CPU retirement. The frame-event counter
is advanced only by events that no public entry can schedule, so it keeps its
value from create or reset (zero) or from a state import; it does not count
video frames. Through this API a `T64_STOP_FRAME_EVENT_COUNT` target above that
counter never ends an advance with `T64_STOP_REASON_FRAME_EVENT_COUNT` (another
stop reason ends it), and a target equal to the counter returns at once with
that reason unless a halt or CPU stop is already present. A host that wants a
frame-paced loop bounds advance by guest ticks and observes the video ring
(section 9).

### 8.3 Guest time and the host clock

Host wall time is never an input. No entry reads a clock. Guest time advances
only through `t64_machine_advance`, by the bound the host chooses and by guest
execution; the host's frame pacing, sleeps and drain schedule are its own and
do not reach the guest. The machine digest after a given call sequence does not
depend on when or how often the host drains the rings
(`installed.consumer.hostloop-cadence`, `installed.consumer.hostloop-delay`).
Each advance dispatches at most the profile's event budget, so a call does a
bounded amount of work.

The guest tick of the profiles that model the VI and the AI runs at 62,500,000
ticks per second, which is one tick per 16 ns of host time, a host policy value
that the core never reads (no entry takes or reports it and a profile names the
rate only inside its VI and AI policy strings), so a host that paces guest time
against its own monotonic clock bounds `t64_machine_advance` at the elapsed
nanoseconds divided by 16.

## 9. Output: rings, text capture, trace and pacing

### 9.1 Video and audio rings

Raw video and raw audio are bounded, lossless, two-packet queues, and the core
never hands out a pointer into its memory: the host copies a packet out.
`t64_video_info` and `t64_audio_info` describe the oldest packet. `t64_video_copy`
and `t64_audio_copy` take the oldest packet: a NULL buffer with zero capacity
(or a capacity below the packet length) is the size query, answered
`BUFFER_TOO_SMALL` with the required length and the queue and the output bytes
untouched; a successful full copy dequeues exactly that packet and frees
capacity without advancing guest time or changing the digest. An empty queue,
a profile without that device and an instance without an image answer
`NOT_READY`.

`t64_machine_advance` never drops a video or audio packet. When a field or
sample boundary has no free slot, the scheduled event keeps its place, advance
stops with `RESOURCE_LIMIT`, and a later advance resumes once the host has copied
a packet out. A host that never drains stalls at that stop; the digest does not
depend on when or how often a host that does drain does so
(`installed.consumer.hostloop-cadence`).

### 9.2 Pacing: `t64_machine_output_status`

`t64_machine_output_status` is the read-only pacing call. Per ring it reports
the queued packet count, the free capacity (queued plus free is the ring
capacity, two packets) and the byte size of the next packet (0 for an empty
ring), and it reports the ISViewer bytes not yet drained. `reserved0` is
written as 0. The call changes nothing: not the digest, a ring, a cursor or a
counter. Without an admitted image the answer is `OK` with zero counts, full
free capacity and no pending text; a profile without a video or audio device
reports that ring empty with the full capacity free. The v1 size is 48 bytes
(`T64_OUTPUT_STATUS_V1_SIZE`).

### 9.3 ISViewer text capture

`t64_machine_output_drain` copies the not-yet-observed suffix of the guest's
retained ISViewer capture. The capture keeps a prefix of 4096 bytes: bytes past
that are counted in `total_attempted_bytes` and reported as `lost_output`, so
text output is not lossless beyond the retained prefix, and the result says so
(`lost_output`, `total_attempted_bytes_exact`, the SHA-256 of the retained
prefix). The guest's lifetime capture is never consumed: draining moves an
observer cursor only. A short buffer reports `BUFFER_TOO_SMALL` with the
required length and the observations, with `copied_bytes` zero and no byte or
cursor changed; an empty drain succeeds with zero bytes. Draining is available
after admission, including after a terminal CPU stop. Reset and replacement
clear capture and cursor.

### 9.4 Trace

Trace is observer-only and never changes the canonical digest. Tracing is off
until `t64_machine_trace_set_enabled(machine, true)`. The ring holds 256
records; a record that finds the ring full is dropped and counted in a
saturating 64-bit drop count, so trace, unlike the video and audio rings, is
lossy by design and says so (`dropped_out`, `drop_count_saturated_out`).
`t64_machine_trace_drain` copies up to `capacity` whole records in FIFO order;
record `i` is written at byte offset `i * record_size`, the v1 members fill its
first bytes and the bytes after them are left untouched.

## 10. The debug bus, input, save media and state

### 10.1 Debug bus

`t64_machine_bus_read` and `t64_machine_bus_write` give the host debug access
to physical byte addresses between advance calls. The width must be 1, 2 or 4
(otherwise `INVALID_ARGUMENT`); halfwords and words are big-endian and require
natural alignment; device registers can restrict width further. The calls
execute no guest code and translate no virtual address, and a rejected read
leaves `value_out` unchanged. A debug write is the host's view of a guest
store, so an accepted write changes the instance as that store would.

The `value_out` span of `t64_machine_bus_read` is the caller's responsibility:
a NULL `value_out` is `INVALID_ARGUMENT`, but a `value_out` that points into
storage the instance owns (the machine storage, the admitted image or a packet
storage) is not refused, unlike the out pointers of the other entries, and the
caller must not pass one. `t64_machine_bus_write` takes its value by value and
has no pointer to judge.

`t64_machine_bus_write` is carved out of the rule that a refused call changes
nothing. A device that refuses a store records the refusal itself, so a write
that answers `T64_STATUS_BUS_DEVICE_REFUSED` (a refused-class status) may change
exactly these records and no other byte of the instance or of any public
observation:

- PI: the latched transfer length register that the refused request addressed
  and, for a request made while a transfer is active, the sticky PI error flag;
- SI: the sticky SI DMA error flag (a request made while a transfer is active,
  or with an RDRAM address that is not 8-byte aligned);
- one refusal record in the trace buffer, when tracing is enabled.

A write that the bus rejects for its address, width, alignment or range
(`BUS_UNMAPPED`, `BUS_UNSUPPORTED`, `BUS_ALIGNMENT`, `BUS_RANGE`,
`INVALID_ARGUMENT`) changes nothing at all, and a device refusal that has no
record to keep (the read-only cartridge window) changes nothing either. Neither
kind poisons the instance.

### 10.2 Input

`t64_machine_queue_input` submits a deterministic port-0 input event: buttons
are bits 0 to 15 and the signed axes are logical 8-bit values in [-128, 127].
Events are scheduled and traced in tick and sequence order; the composite
profile family latches the value so a poll sampled at tick E sees the last input
stamped strictly before E, and other profiles record a timed observation only.
Submission is accepted for an admitted, nonterminal machine and a tick at or
after the current guest tick. Refusals change neither the queue nor the
sequence.

### 10.3 Save media

The save device is selected by the profile only: no header, title, game code,
checksum or file name is an input. `t64_machine_save_info` reports the device of
an admitted instance when the requested kind equals the profile's kind.
`t64_machine_save_import` and `t64_machine_save_export` move exactly the kind's
size (512, 2048 or 32,768 bytes, no padding, no container, no autodetect) in the
named byte order (`T64_SAVE_FORMAT_RAW`, `SWAP16` or `WORDREV32`); the core never
opens a file or flushes a save on its own. The check order of the two is the
exact table R0 to R9 in the header comment. A successful import stores the
normalized bytes, clears the EEPROM deadline, leaves the scheduler queue and the
guest tick unchanged and increments `write_generation` (an observer value
outside the digest and the state container). Replacement restores blank save
content and resets the generation. `docs/spec/save-media.md` has the device
rules.

### 10.4 State export and import

State blobs are build-identity-bound and outside the ABI promise. A blob is
accepted only by the build identity and profile that wrote it, and no
cross-build, cross-version or cross-implementation portability is promised. The
container version word, the format strings (`t64.state-container.v1` and `.v2`)
and the section layout are state-format constants of one build family, not
public API (`docs/spec/state-container.md` "Scope and evidence"). The header
constants `T64_STATE_*` are the closed vocabulary of the import report.

`t64_machine_state_export` is size-then-copy-out: a NULL buffer or a capacity
below the required length returns `BUFFER_TOO_SMALL`, sets `*length_out` to the
required length and writes no byte of `bytes_out`. An exact or larger buffer
receives the container. The machine is never modified.
`t64_machine_state_import` accepts a container produced by the same build for
the same version, profile and content. It checks in a fixed order (arguments
and disjointness from live storage, header, whole-blob SHA-256 trailer,
identity binding with a distinct status per mismatch, section table, per-field
range rules, structural and per-device invariants, the canonical digest
recomputed from the blob), then commits in one infallible step. A refusal leaves
the machine bit-identical, makes no host call and releases nothing. The report
(`t64_state_import_report`) carries the status, the first failing reason, the
section and rule where they apply and a position; it never carries blob bytes.
The hashes inside the container are integrity checks, not authentication, and no
statement is made that an imported state is reachable. With
`T64_STATE_IMPORT_DROP_OBSERVERS` both rings are emptied and the drain cursor is
set to the retained capture length; the canonical digest is the same either way.

## 11. Threading

Distinct instances are independent: the library has no writable file-scope data
and no thread-local state (section 2.6), so two threads may each use their own
instance without any synchronization. The functions that take no machine read
only immutable data and may run concurrently with anything.

One instance needs external serialization. The host must not make two calls on
one instance at the same time, whichever functions they are. A call that takes
a `const` machine (a digest or an observation) is still a call on the instance
and may not overlap another one: a defect found by a `const` entry writes the
poison flag. `t64_machine_destroy` is serialized against every other call on
the instance as well. The in-call flag does not make an instance thread-safe; it
turns a call made from inside an allocator callback of the same instance into
`T64_STATUS_REENTRANT`. A caller's buffers handed to one call must not be
modified by another thread during that call. The allocator callbacks run on the
calling thread; the allocator's own thread safety is the host's.

The statement is exercised by a two-thread consumer run, one instance per
thread, under the thread sanitizer in a local lane (`sh scripts/check-install.sh
tsan`); that lane is not part of the gate.

## 12. Postconditions of every public function

Status names are written without the `T64_STATUS_` prefix. "Guard" means the
entry guard of section 5: a NULL machine answers `INVALID_ARGUMENT`, a poisoned
instance `POISONED`, a call from inside an allocator callback `REENTRANT`, each
before anything is read and with every output as the caller left it. In the
table, "NULL machine" and "poisoned" repeat that statement per entry, and
"image" says whether an admitted image is required (`NOT_READY` without one,
outputs untouched). "Refusals" are listed in check order after the guard, and a
refusal changes nothing except what the row names.

| Function | Accepted postcondition | Refusals (check order) | NULL machine | Poisoned | Image |
| --- | --- | --- | --- | --- | --- |
| `t64_build_identity` | Returns the build identity string, valid for program life; a pure function of the build inputs | none | n/a (no machine) | n/a | no |
| `t64_source_revision` | Returns the source revision string recorded at build; informational | none | n/a | n/a | no |
| `t64_abi_version` | Returns `T64_ABI_VERSION` of the library | none | n/a | n/a | no |
| `t64_profile_lookup` | Writes the registered immutable descriptor to `*profile_out`; it stays valid for program life and is never freed or edited by the caller | NULL `profile_out`: `INVALID_ARGUMENT`, nothing written; NULL or empty `profile_id`: `INVALID_ARGUMENT`, NULL written to `*profile_out`; unknown id: `UNKNOWN_PROFILE`, NULL written | n/a | n/a | no |
| `t64_profile_hash` | Writes the 32-byte SHA-256 of the canonical descriptor serialization (named members only, `struct_size` excluded) | NULL `digest_out` or NULL profile: `INVALID_ARGUMENT`; descriptor `struct_size` below v1: `STRUCT_TOO_SMALL`; an invalid text or numeric member: `INVALID_ARGUMENT`. `digest_out` untouched on every refusal | n/a | n/a | no |
| `t64_profile_serialize` | Writes the canonical serialization (at most 2048 bytes) and its length; size-then-copy-out | NULL `length_out` or profile: `INVALID_ARGUMENT`; `struct_size` below v1: `STRUCT_TOO_SMALL`; invalid member, spans that wrap or overlap each other or the profile, or a serialization above 2048 bytes: `INVALID_ARGUMENT`; NULL `bytes_out` or capacity below the length: `BUFFER_TOO_SMALL` with `*length_out` set and the bytes intact | n/a | n/a | no |
| `t64_machine_create` | Allocates and initializes an instance with no admitted image, guest tick 0 and tracing off; writes it to `*machine_out`; the instance copies the descriptor | order of section 7.1: NULL `machine_out` or `info`: `INVALID_ARGUMENT`; `STRUCT_TOO_SMALL`; `ABI_MISMATCH`; NULL allocator or profile; sizes; allocator and profile checks: `INVALID_ARGUMENT`; `ALLOCATION_FAILED`; `BAD_STORAGE`. `*machine_out` is NULL on every refusal after the NULL check; no allocation survives a refusal | n/a | n/a | no |
| `t64_machine_destroy` | Releases every host block of the instance once; the pointer is invalid afterwards | none | NULL does nothing | works and releases every allocation | no |
| `t64_machine_is_poisoned` | Writes 1 for a poisoned instance and 0 otherwise to `*poisoned_out` | NULL machine, NULL output or an output span that overlaps the instance: `INVALID_ARGUMENT`, output untouched | `INVALID_ARGUMENT` | works (no guard) | no |
| `t64_machine_admit_rom` | Installs or replaces the image as one commit and resets the guest to its first-admit state (section 7.2); previous image released after the new one is installed | NULL span, length outside the profile interval or not divisible by four, wrapping span, overlap with the instance or its image, or an input the profile refuses (order, payload, header): `INVALID_ROM`; `ALLOCATION_FAILED`; `BAD_STORAGE`; stored profile digest mismatch or failing reset after the swap: `INTERNAL_DEFECT` (poisons). A refusal leaves the previous image and the whole guest state | `INVALID_ARGUMENT` | `POISONED` | no (it creates it) |
| `t64_machine_reset` | Returns the guest to the state right after the first admission of the same image; no allocator callback (section 7.3) | `NOT_READY`; failure after state was touched: `INTERNAL_DEFECT` (poisons) | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_unload` | Releases the image block, clears the guest state and the image members; the instance answers like one that never admitted an image (section 7.4) | `NOT_READY` | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_advance` | Runs the guest to the requested bound or another stop and writes the stop reason and tick (section 8) | request and result spans (NULL or overlapping the instance): `INVALID_ARGUMENT`; either `struct_size` below v1: `STRUCT_TOO_SMALL`; `NOT_READY`; unknown kind: `INVALID_ARGUMENT`; `TIME_REVERSED` (tick bound behind the guest tick); `LIMIT_REVERSED` (counter limit behind the counter); handler status outside the whitelist: `INTERNAL_DEFECT` (poisons). The result struct is untouched on every refusal | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_content_digest` | Writes the 32-byte SHA-256 of the normalized (big-endian) image | NULL or overlapping `digest_out`: `INVALID_ARGUMENT`; `NOT_READY` | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_rom_size` | Writes the admitted image size in bytes | NULL or overlapping `size_out`: `INVALID_ARGUMENT`; `NOT_READY` | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_rom_header` | Copies the 64 normalized header bytes and writes their length | NULL `length_out`, NULL buffer with a non-zero capacity, spans that overlap each other or the instance: `INVALID_ARGUMENT`; `NOT_READY`; NULL buffer or capacity below 64: `BUFFER_TOO_SMALL` with `*length_out` 64 and the bytes intact | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_profile` | Writes a borrowed pointer to the instance's own descriptor copy (v1 size), valid until destroy | NULL `profile_out`: `INVALID_ARGUMENT` | `INVALID_ARGUMENT` | `POISONED` | no |
| `t64_machine_digest` | Writes the 32-byte canonical machine digest of the current state | NULL or overlapping `digest_out`: `INVALID_ARGUMENT`; `NOT_READY`; an internal inconsistency in the digest stream: `INTERNAL_DEFECT` (poisons) | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_state_export` | Copies the integrity-checked state container; the machine is not modified (section 10.4) | NULL `length_out`, wrapping spans, overlap with each other or the instance: `INVALID_ARGUMENT`; `NOT_READY`; a profile outside the composite family: `INVALID_ARGUMENT`; container above the size ceiling: `RESOURCE_EXHAUSTED`; NULL buffer or capacity below the length: `BUFFER_TOO_SMALL` with `*length_out` set and no byte written; a core inconsistency: `INTERNAL_DEFECT` (poisons). Every refusal except the size answer leaves `*length_out` and the buffer intact | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_state_import` | Replaces the canonical state with the blob's after every check passed and fills the report (status `OK`, reason `NONE`); no allocation and no host call | NULL or overlapping report span; NULL blob, length 0 or above the ceiling, unknown flag bits, blob overlapping live storage: `INVALID_ARGUMENT`; report `struct_size` below v1: `STRUCT_TOO_SMALL` (report untouched, blob unread); no image: `NOT_READY`; then the `STATE_*` family by reason (format, integrity, version, profile, content, build, range, invariant). The machine is bit-identical on every refusal and the report holds the first failing reason | `INVALID_ARGUMENT` (report untouched) | `POISONED` | yes |
| `t64_machine_cpu_stop_detail` | Writes the retired instruction count, program counter and opcode of a terminal CPU stop | NULL or overlapping `detail_out`: `INVALID_ARGUMENT`; `STRUCT_TOO_SMALL`; no image or no terminal CPU stop: `NOT_READY` | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_output_drain` | Copies every not-yet-observed ISViewer byte, advances the cursor and writes the result record (section 9.3); an empty drain copies zero | NULL or overlapping result span: `INVALID_ARGUMENT`; `STRUCT_TOO_SMALL`; NULL data with a non-zero capacity or a data span that wraps, overlaps the instance or the result: `INVALID_ARGUMENT`; no image or boot not prepared: `NOT_READY`; capacity below the pending bytes: `BUFFER_TOO_SMALL` with the result written (required bytes and observations, `copied_bytes` 0) and no data byte or cursor changed; a cursor or capture length out of range: `INTERNAL_DEFECT` (poisons; state import rejects both) | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_output_status` | Writes ring occupancy, free capacity, next packet sizes and pending text bytes; changes nothing (section 9.2) | NULL `status_out`: `INVALID_ARGUMENT`; `STRUCT_TOO_SMALL` (struct untouched); a span that overlaps the instance: `INVALID_ARGUMENT`; a ring head, count, cursor or capture length out of range: `INTERNAL_DEFECT` (poisons) | `INVALID_ARGUMENT` | `POISONED` | no (zero counts without one) |
| `t64_video_info` | Writes the description of the oldest queued video packet; the queue is unchanged | NULL or overlapping `info_out`: `INVALID_ARGUMENT`; `STRUCT_TOO_SMALL`; no image, no video device or an empty queue: `NOT_READY` | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_video_copy` | Copies the oldest packet whole, writes its length and dequeues it | NULL `length_out`, NULL buffer with a non-zero capacity, spans that wrap or overlap each other or the instance: `INVALID_ARGUMENT`; no image, no video device or an empty queue: `NOT_READY`; NULL buffer or capacity below the packet: `BUFFER_TOO_SMALL` with `*length_out` set, queue and bytes untouched | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_save_info` | Writes kind, size, busy flag and `write_generation` of the profile's save device | NULL or overlapping `info_out`: `INVALID_ARGUMENT`; `STRUCT_TOO_SMALL`; `NOT_READY`; then R3 to R6 of the header: no device or a kind other than the profile's: `SAVE_DEVICE_MISMATCH`; kind 0 or above 7: `SAVE_UNKNOWN_DEVICE`; kinds 4 to 7: `SAVE_UNSUPPORTED_DEVICE`. Output untouched | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_save_export` | Writes exactly the kind's size in the named byte order and the length; export has no busy check | `NOT_READY` (before the pointer row); NULL `length_out`, NULL buffer with a non-zero capacity, spans that wrap or overlap: `INVALID_ARGUMENT`; R3 to R6 as above; format not RAW, SWAP16 or WORDREV32: `SAVE_BAD_FORMAT`; capacity below the size: `BUFFER_TOO_SMALL` with `*length_out` set and the buffer untouched | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_save_import` | Stores the normalized bytes, clears the EEPROM deadline, adds one to `write_generation`; the queue and the guest tick are unchanged | `NOT_READY` (before the pointer row); NULL bytes or a span that wraps or overlaps the instance: `INVALID_ARGUMENT`; R3 to R6 as above; `SAVE_BAD_FORMAT`; length other than the kind's size: `SAVE_SIZE_MISMATCH`; an EEPROM write window open or an SRAM DMA active: `SAVE_BUSY`. Nothing changes on a refusal | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_audio_info` | Writes the description of the oldest queued audio packet; the queue is unchanged | NULL or overlapping `info_out`: `INVALID_ARGUMENT`; `STRUCT_TOO_SMALL`; no image, no audio device or an empty queue: `NOT_READY` | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_audio_copy` | Copies the oldest packet whole, writes its length and dequeues it | as `t64_video_copy`, with the audio device and queue | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_queue_input` | Schedules one port-0 input event at the given guest tick (section 10.2) | port other than 0 or an axis outside [-128, 127]: `INVALID_ARGUMENT`; no image, boot not prepared, terminal CPU stop: `NOT_READY`; tick behind the guest tick: `TIME_REVERSED`; `QUEUE_FULL`; `SEQUENCE_EXHAUSTED`. Queue and sequence unchanged | `INVALID_ARGUMENT` | `POISONED` | yes |
| `t64_machine_bus_read` | Writes the big-endian value at the physical address to `*value_out` (section 10.1) | NULL `value_out`: `INVALID_ARGUMENT`; width not 1, 2 or 4: `INVALID_ARGUMENT`; a range that leaves the 32-bit space: `BUS_RANGE`; misalignment: `BUS_ALIGNMENT`; `BUS_UNMAPPED`; `BUS_UNSUPPORTED` (including the cartridge window without an image); a device refusal. `value_out` is unchanged on every refusal. The `value_out` span is not checked against the instance | `INVALID_ARGUMENT` | `POISONED` | no (the cartridge window needs one) |
| `t64_machine_bus_write` | Applies the store as the host's view of a guest store | as `t64_machine_bus_read` for the address; a read-only or refusing device: `BUS_DEVICE_REFUSED`, which may change only the records of section 10.1; every other refusal changes nothing | `INVALID_ARGUMENT` | `POISONED` | no |
| `t64_machine_trace_set_enabled` | Sets trace enablement; no other effect | none | `INVALID_ARGUMENT` | `POISONED` | no |
| `t64_machine_trace_drain` | Copies up to `capacity` whole records FIFO at stride `record_size`, writes the count, the drop count and the saturation flag (section 9.4); zero capacity copies nothing | NULL count, drop or saturation output, or NULL records with a non-zero capacity: `INVALID_ARGUMENT`; `record_size` below 24: `STRUCT_TOO_SMALL`; a capacity whose byte size overflows, spans that wrap or overlap each other or the instance: `INVALID_ARGUMENT`. Outputs and queue untouched | `INVALID_ARGUMENT` | `POISONED` | no (zero records) |

The table has 35 rows, one per public function. The behaviors of an entry on an
instance that never admitted an image or that was unloaded are the same; the
record of every entry's answer is the `unit.core.api.not-ready-table` case.

## 13. The additive-extension rule

This section is the frozen-record voice for what may change without breaking a
host built against this candidate. A change outside it is a recorded amendment
with its cost (section 14), not a silent edit.

The following are additive. Each is made in a commit that also updates the bound
goldens, adds the class row where a value is new, and raises the ABI minor
(section 2.2):

- New trailing struct fields, guarded by `struct_size`. A new field is appended
  after the last member, never inserted. A caller built against the older layout
  passes its smaller `struct_size`, and the callee then reads and writes exactly
  the members it knows (section 3); a new input field must have a default that
  means the earlier behavior when it is absent. The v1 size stays the minimum a
  caller may pass.
- New function entries, which also change the public symbol list.
- New statuses and stop reasons, appended after the last value (statuses after
  38, stop reasons after 8) and never reused or renumbered. Each is classified
  in `docs/abi/status-classes.txt` in the same commit. A host written against
  this version should treat a status it does not know as a failure of the call
  and a stop reason it does not know as a stop it cannot resume past.
- New stop kinds, save device kinds and formats, trace devices and kinds,
  peripheral flag bits and state-import reasons and rules, appended after the
  last value of their family. The state-import reasons and rules
  (`T64_STATE_REASON_*`, `T64_STATE_RULE_*`) are the closed error subcodes of
  the import report; a new subcode never repurposes an old value.
- Additive profiles: a new profile id registered with `t64_profile_lookup` with
  its own descriptor, so a profile id names one hashed descriptor and an
  existing id keeps its descriptor, its descriptor hash and its digests. A
  descriptor that gains a field in a later descriptor version keeps the older
  versions' hashes (a legacy descriptor hash ignores the later fields).
- Additive devices: a device is added to the machine only under new profile ids
  (a new descriptor version and state serialization version), so no existing
  profile changes its observable behavior or its digests. The repository's
  digest sweep (`scripts/digest-sweep.py`) is the check that a change moved no
  existing digest.

The following are not additive and are breaks: removing or renaming a function,
changing a signature or a macro value, removing, reordering or resizing a
struct member, renumbering or reusing a status, stop reason or other constant,
changing a documented postcondition for an input the call already accepts or
refuses, and changing the class of an existing status. Narrowing the inputs a
call accepts (a new refusal for an input that was accepted) is among them even
when the new refusal is a documented status.

State blobs are outside the promise (section 10.4): a state serialization
version may change with a build without any compatibility path. The profile
registry and the state formats grow only through the additive profiles above.

## 14. Candidate, final bind and amendments

Phase 11 binds a freeze candidate; Phase 12 ends with the final bind. The
candidate is bound by four generated or checked goldens and one record that
recomputes them:

| Golden | Path |
| --- | --- |
| installed header hash (SHA-256 of every installed header with LF line endings) | `docs/abi/header.sha256` |
| public symbol list | `docs/abi/public-symbols.txt` |
| struct-layout table | `docs/abi/layout.c` |
| status-class table | `docs/abi/status-classes.txt` |

The freeze record, `docs/abi/FREEZE-CANDIDATE.md`, binds the four: its
machine-read block holds each golden's own SHA-256, the ABI and package versions,
the counts and the padding holes, and `repo.abi-freeze-record` recomputes all of
them from the installed tree and fails on any difference (the seeded incompatible
changes of `repo.abi-freeze-selftest` show it failing on each). The goldens are
regenerated, never edited by hand, where a generator exists: `python3 -B
scripts/check-abi.py layout --write docs/abi/layout.c` and `python3 -B
scripts/check-abi.py symbols --write docs/abi/public-symbols.txt --lib
<archive>`; the class table is edited by hand and checked by
`repo.abi-status-classes`; `header.sha256` and the block are written by
`python3 -B scripts/check-abi.py freeze --write` (the record gives the full
re-bind procedure).

From the record until the final bind, a change to a golden is an amendment and
only a defect-driven one: it needs evidence of the defect, a re-run of the
installed-consumer suite against the amended installed tree and a minor
increment, recorded in the record's amendment log. The Phase 12 final bind is a
separate act that either confirms the candidate or lists the amendments; this
document does not promise a 1.0 version, a tag or a release.

The candidate evidence is the installed-consumer suite on both hosted operating
systems (section 16). It is a software-interface claim only: nothing in the
suite or the record says that a profile is compatible with, or as accurate as,
a console or an emulator.

## 15. Limits of this candidate

These are known and accepted for the candidate; each is a candidate amendment or
a Phase 12 final-bind question, not a hidden behavior.

- The `value_out` span of `t64_machine_bus_read` is not judged against the
  instance (section 10.1). Judging it narrows the accepted inputs (section 13)
  and needs an edit of a sealed RSP path, so it is an open amendment.
- Six public structs contain implicit alignment padding on the exercised
  64-bit targets (`t64_host_storage`, `t64_host_allocator`,
  `t64_cpu_stop_detail`, `t64_output_drain_result`, `t64_audio_packet_info`,
  `t64_state_import_report`); `t64_output_status` has an explicit `reserved0`.
  Copy-out builds each record in a zeroed local, so no stack byte leaks. The
  offsets are fixed by each platform ABI and `docs/abi/layout.c` pins them for
  targets with 64-bit pointers only; explicit reserved members are a final-bind
  question.
- The frame-event counter and `T64_STOP_FRAME_EVENT_COUNT` are defined with no
  public way to advance the counter (section 8.2).
- `T64_STATUS_UNSUPPORTED_ROM_ORDER` and `T64_STOP_REASON_HALT` are reserved
  and never returned through the installed API.
- The gate samples the machine digest over every second burst of the
  call-sequence generator; the local shipped-library lane verifies every burst
  (`installed.consumer.sequence`, `sh scripts/check-install.sh sanitize`).
- A trace ring that is full drops and counts (section 9.4), and the ISViewer
  capture keeps a 4096-byte prefix (section 9.3): neither is lossless.
- State import and export are defined for the composite profile family only.

## 16. Where the statements are checked

| Statement | CTest names |
| --- | --- |
| classes, the single table, the header compiles alone | `repo.abi-header`, `repo.abi-status-classes`, `repo.abi-selftest`, `install.abi-header` |
| layout and constants | `repo.abi-layout`, `install.abi-layout` |
| the bound freeze-candidate record and its seeded incompatible changes | `repo.abi-freeze-record`, `repo.abi-freeze-selftest` |
| public symbols, the `t64i_` prefix, API-04 | `install.abi-symbols`, `repo.abi-prefix`, `api04.members`, `api04.disasm`, `api04.headers`, `api04.seeded`, `installed.controls.symbol-collision` |
| ABI version, handshake, `struct_size` | `unit.core.api.abi-version`, `unit.core.api.handshake`, `unit.core.api.struct-size`, `installed.consumer.ops-handshake`, `installed.consumer.ops-struct-size` |
| poison, reentrancy, NULL machine, refusals never poison, defect sites | `unit.core.api.poison`, `unit.core.api.reentrant`, `unit.core.api.null-machine`, `unit.core.api.refusals-never-poison`, `unit.core.api.defect-sites`, `installed.consumer.ops-poison-query` |
| reset, unload, the answer of every entry without an image | `unit.core.api.reset`, `unit.core.api.unload`, `unit.core.api.not-ready-table`, `installed.consumer.ops-reset`, `installed.consumer.ops-unload` |
| `t64_machine_bus_write` carve-out | `unit.core.api.bus-carve-out`, `installed.consumer.ops-bus` |
| output status, rings, text, trace | `unit.core.api.output-status`, `installed.consumer.ops-rings`, `installed.consumer.ops-text`, `installed.consumer.ops-trace` |
| the host loop, drain cadence, two instances | `installed.consumer.hostloop`, `installed.consumer.hostloop-cadence`, `installed.consumer.hostloop-delay`, `installed.consumer.two-instance` |
| refused calls leave observations bit-identical, a guest-stop resumes, a defect poisons | `installed.consumer.sequence` (gate sample) and the local sanitizer lanes |
| the composite fixture through the installed package, agreeing with the runner and the independent literals | `installed.consumer.composite`, `installed.consumer.literals`, `installed.consumer.three-way` |
| install, relocation and the negative controls | `install.setup`, `installed.consumer.build`, `installed.consumer.smoke`, `installed.controls.*` |
| one accepted case and one refusal per operation | `installed.consumer.ops-lifetime`, `installed.consumer.ops-profile`, `installed.consumer.ops-admission`, `installed.consumer.ops-advance`, `installed.consumer.ops-input`, `installed.consumer.ops-save`, `installed.consumer.ops-state`, `installed.consumer.ops-queries` |

Local lanes that are not part of the gate: `sh scripts/check-install.sh
sanitize shipped`, `sanitize instrumented` and `tsan`. They run the call-sequence
generator over the shipped library under the address and undefined-behavior
sanitizers and the two-thread run under the thread sanitizer.

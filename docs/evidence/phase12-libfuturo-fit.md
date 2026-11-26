# LibFuturo fit evaluation (API-07)

Written 2026-11-26 for Phase 12, plan 12-10, requirement API-07 and decision DR-22. Maintenance
owner: Tendies64 maintainers.

Evidence tier: a written design comparison of two contracts, read from committed text. There is
no adapter, no test, no measurement and no compatibility, accuracy or hardware claim in this
document. It is not a statement about what any other project promises beyond the words quoted
from its committed files.

## The pin

Pinned commit: 4177a43e67312e2db88ab21cba13aa7350063105

- Commit date and subject: 2026-11-23T06:27:54-05:00, "docs: finalize browser-host v2.4 state".
- Re-taken at plan time (2026-11-26): the commit is the head of the checkout it was read from, it
  has 1,169 commits of history and no commit follows it.
- Published status: local and unpublished. The checkout has no configured remote, so no public URL
  is known to hold this SHA. The public repository of the same name had a different history of two
  commits when the planning lens looked at it on 2026-11-23; it was not asked again for this
  document, because a request would carry the project's identifiers to an outside service. This
  evaluation therefore pins a local SHA and says so: a reader without that checkout can verify
  none of what follows against the pin.
- Relation to the earlier read: the Phase 11 paper read took the same SHA, so the pin did not move
  between the two reads.

## Method

Committed objects only. Four files were read by name at the pinned commit, through `git show` of
that commit and never from a working tree: the public header (347 lines), the shared contract
document (430 lines), the project README and the LICENSE (the Apache License, Version 2.0). The
list of committed file names was also looked at. The checkout has uncommitted changes; they were
not read. Nothing was written to that checkout, nothing was fetched, and no filesystem path of it
is recorded here. Quotations are limited to short identifiers; the comparison is in our own
words.

The Tendies64 side is the contract as it stands on the date above: the installed header
(`t64_machine.h`), `docs/spec/api.md` and the class table `docs/abi/status-classes.txt`. At this
date the ABI is the 0.1 freeze candidate and the final bind of Phase 12 is not yet made; a row
that cites the ABI is dated by this sentence.

## Reading the table

The fit column judges how an adapter between the two contracts would fare on that axis, using
three words only:

- compatible: the shapes map one to one and the adapter's work is bookkeeping;
- convertible with loss: the adapter can answer every call, but a fact of one side has no place on
  the other (it is dropped, rounded or guessed);
- missing: one side has no entry or statement for what the other one relies on.

## Axis-by-axis comparison

| Axis | LibFuturo at the pin | Tendies64 as bound (ABI 0.1 candidate) | Fit |
|------|----------------------|----------------------------------------|-----|
| Instance and allocation | `lf_create(profile_id, options, &out)` with `allocate(ctx, bytes)` and `deallocate` callbacks; `lf_destroy(&owned)` consumes and clears the handle; the host owns the handle and serializes every call (LC-LIFE-01) | `t64_machine_create` takes a create-info (leading `struct_size`, `abi_version`, allocator, profile); the allocator callback is told size and alignment and fills a storage record that the core checks (a wrong capacity, alignment or overlap is `BAD_STORAGE`); `t64_machine_destroy` by value; one instance needs external serialization, distinct instances are independent | compatible: the adapter wraps its allocator to honor the alignment argument that the other side does not pass and keeps the raw pointer for the release call |
| Content admission | `lf_load` with a borrowed span and an optional configuration id; transactional (a rejected load keeps the previous session, an unloaded instance stays unloaded); no unload and no reset call; limits come from the profile (`max_content_bytes`) | `t64_machine_admit_rom` with a borrowed span, transactional in the same sense; plus reset and unload; admission rules belong to the profile (the pinned Open IPL3 profiles accept the exact boot asset or an authored big-endian z64 cartridge); no configuration selector | convertible with loss: a configuration id has no counterpart, and the machine kind has no profile entry on the other side (see "What an adapter would need") |
| Input | `lf_set_input` stores one `uint32_t` button word as pending state; the profile fixes the mask and the only sampling point is the start of an advance (`LF_INPUT_AT_ADVANCE_START`); no stick axes in the call | `t64_machine_queue_input(tick, port, buttons, x, y)`: events stamped with an absolute guest tick, 16 button bits and signed 8-bit axes, kept in a 256-entry queue shared with scheduler events; a tick behind the tick reached is refused | convertible with loss: tick stamps are dropped, and a stick can only be thresholded into button bits |
| Time | `work_units` of a profile-owned unit (for a real core a frame), with overshoot, an atomic quantum and `actual_units`; stop reasons are budget reached, rejected and core halted; cadence is an exact rate or 0/0; a separate cumulative core-tick field has its own unit enum | `t64_machine_advance` runs to an absolute guest tick (62,500,000 ticks per second, one tick per 16 ns), or to an instruction count; eight stop reasons including `RESOURCE_LIMIT`, `QUIESCENT` and two terminal ones; no entry reads a host clock; a video field is a number of ticks, not an entry of the API, and the frame-event stop cannot be reached through the API | convertible with loss: a frame unit is found by bounding in ticks and watching the video ring, the cadence is 0/0, and the unit enum has no value for this tick |
| Video | one destination span, one validity flag and one geometry per run request and result; formats `LF_VIDEO_GRAY8`, `LF_VIDEO_RGB24` and `LF_VIDEO_INDEX8`; the library must not keep the pointer (LC-OWN-01) | raw packets of 4 bytes per pixel in memory order R, G, B, A up to 320 by 240, in a lossless ring of two packets per device; the host copies a packet out (size query first); an advance never drops a packet and stops with `RESOURCE_LIMIT` when the ring is full; the packet carries its tick span and VI facts | convertible with loss: the alpha byte is dropped for RGB24, and an advance that finishes two fields has two packets where the request names one destination span |
| Audio | interleaved signed 16-bit samples in host byte order into a caller span; one rate per profile or run as a numerator and denominator; complete channel groups only | stereo signed 16-bit big-endian packets from a lossless ring of two, each with its own rational rate (`rate_numerator` over `rate_denominator`, derived from the guest's DAC rate register) | convertible with loss: a byte swap on a little-endian host is exact, but a rate that changes between packets inside one run has no place in a per-run rate |
| Media and saves | `lf_copy_media` copies the loaded persistent span out with an identity string, an extent and a `guest_generation` token; `lf_import_media` is allowed only after load and before the first positive advance and is not a guest write; the host owns files and acknowledgments (LC-HOST-01) | `t64_machine_save_info`, `save_export` and `save_import` move exactly the kind's size (512, 2,048 or 32,768 bytes) in a named byte order, at any time on an admitted instance unless a device is busy (`SAVE_BUSY`); `write_generation` counts host writes only, not guest writes; the core never opens a file | compatible: both are copy-in and copy-out with the host owning durability; the generation tokens mean different things and are not shared |
| Snapshots | opaque caller-owned spans (`lf_get_snapshot_size`, `lf_capture_snapshot`, `lf_restore_snapshot`); ordinary validity is the same live instance, profile, build and load session; fresh-process restore only for named exact fixtures on one local platform; restore also carries the adapter's own continuation (pending input, frame history); no stable format | `t64_machine_state_export` and `state_import`: a container bound to the build identity and profile, size-then-copy-out, a report with a first failing reason, defined for the composite profile family only, outside the ABI promise | compatible in kind, different in scope: both are opaque, build-bound and non-portable; neither side offers a shared format |
| Errors and postconditions | six statuses with no classes; the contract tells callers to rely on each operation's documented postcondition and never to infer rollback from a status name; no outcome for an unusable instance | 39 statuses and 8 stop reasons in one class table (1 ok, 35 refused, 7 guest-stop, 2 defect, 2 reserved); a refused call changes nothing (one documented carve-out for a debug bus write); a defect poisons the instance and only destroy and the poison query succeed; a per-function postcondition table | convertible with loss: refused statuses fold into the nearest of six values, the stop reasons `GUEST_COMPLETION` and `CPU_UNSUPPORTED` fold into core halted, and the poisoned state has no outcome on the other side |
| ABI promise | the README and the contract call the API experimental, promise no stable binary or plugin ABI and tell consumers to rebuild against the matching header; appended fields already changed a struct size and an enum between revisions; `size_t` for every leading size | a candidate bound by four goldens and a record, `uint32_t` leading sizes, an additive-only rule with a minor-number rule; the final bind of this phase turns it into the bound identity of the installed package, without any release or tag | missing: the other side offers nothing to rely on at the binary level, so neither side could rely on the other; an adapter would be source level and rebuilt with the matching header |

## What an adapter would need

These are the contract deltas an adapter would have to find on the LibFuturo side. They are
recorded as feedback for that project, not as requests: each is a change of the LibFuturo contract
(or of a LibFuturo profile for this machine), never a Tendies64 core change, and none is a defect
of a Tendies64 postcondition.

| Delta | Why an adapter needs it | Side of the change |
|-------|-------------------------|--------------------|
| A tick-stamped input call, or an explicit cadence | one button word sampled at the start of an advance cannot say when a button changed or carry stick axes; with an explicit cadence of whole advances the adapter could stamp each sample at a known tick | LibFuturo |
| An RGBA video format, or a stated rule that alpha is dropped; and a statement for more than one image per run | the packets are 4 bytes per pixel with alpha and a run can finish two fields | LibFuturo |
| A byte-order statement and a per-packet or per-run rational rate for audio | the packets are big-endian with their own rate; the contract states host byte order and one rate | LibFuturo |
| An admission entry: a registered profile for this machine kind (identifier, input mask, bounds, content limit, formats) and a time-unit value for a 62,500,000 ticks per second guest tick | the committed contract names a closed set of profiles and none describes this machine, so there is no profile to select and no unit to report | LibFuturo |
| A defect class: an outcome for an instance that is no longer usable and may only be destroyed | the other side has six statuses and no such state, so a poisoned instance would have to be reported as an ordinary failure | LibFuturo |

## Conclusion

no adapter now.

The reasons, each from the table above:

- The LibFuturo contract is experimental by its own words and promises no stable binary or plugin
  ABI, so a Tendies64 adapter would track a moving source contract; our own bound ABI is an
  identity of the installed package, not a release, and neither side can promise the other
  anything at the binary level.
- Input timing is lossy by design: a single word sampled at the start of an advance cannot express
  tick-stamped events or stick axes, which the Tendies64 API and its title guest use.
- Admission is missing: the committed LibFuturo contract names no profile for this machine, so
  there is nothing for an adapter to register against until that project adds one; adding it is
  that project's decision.
- The other differences (the alpha byte, byte order and a changing audio rate, the defect class)
  are convertible with loss inside an adapter and do not decide the question alone.

Any adapter would be optional, would live outside the core and must not shape the core API: no
Tendies64 core API change follows from this evaluation, the deltas above are LibFuturo-side, and a
host-found change of the core API is admitted only as a defect of a postcondition through the
amendment rule. No adapter code, build target or test exists in this repository. No LibFuturo
support exists, and no claim of it is made.

A re-evaluation would be useful when the other contract is published and versioned (a tag or
release that a third party can fetch, with a stated stability promise) or when it gains a profile
for this machine kind; it would take a new pin at that time.

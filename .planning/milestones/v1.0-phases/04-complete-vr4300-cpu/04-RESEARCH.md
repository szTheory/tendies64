# Phase 4 implementation research synthesis

**Status:** ready for planning, 2026-11-11. This consolidates the three
decision-focused researchers and one adversarial check; it is not execution
evidence. `04-CONTEXT.md` owns the corrected decisions. Reuse these findings
rather than starting another general research pass.

## Standard Stack

Retain C17, CMake/Ninja, CTest, portable integer helpers, explicit per-machine
state and existing canonical JSON/SHA/descriptor/scheduler seams. The candidate
copied arithmetic is the minimal generic Berkeley SoftFloat 3e closure, with
per-file BSD-3-Clause provenance and explicit-context adaptation before use.
The pinned external Rust test toolchain remains an isolated local build tool,
subject to its own complete pre-use legitimacy audit. It is not a core dependency.

## Architecture Patterns

Use one canonical interpreter with profile-selected semantic policies. New
architectural guest exceptions, TLB, caches, timer and COP1 state live behind
internal seams and the new descriptor/model/state identity. Keep warm-v1 bytes
and behavior qualified. Model faults are not guest exceptions; guest exceptions
must schedule handler execution even without retirement. Stage effects until
higher-priority faults are excluded. Translate to address plus attributes before
bus/cache access. Serialize every future-affecting phase, reservation, TLB,
register, delay-slot and cache field; observers remain outside canonical state.

Candidate implementation paths (planner chooses exact scope and ownership):

| Area | Existing integration | Candidate owned additions |
|---|---|---|
| Profile/seed/state | `core/t64_machine.c`, `core/t64_internal.h`, `core/t64_digest.c`, public profile header | CPU profile spec and independent v9 vectors |
| Exception/CP0/time | `core/t64_cpu.c`, `core/t64_scheduler.c` | `core/t64_exception.c`, `core/t64_cp0.c`, focused CPU tests |
| Translation/memory/cache | `core/t64_cpu.c`, `core/t64_bus.c`, PI/SP writers | `core/t64_mmu.c`, `core/t64_cache.c`, load/store helpers and focused tests |
| Arithmetic/COP1 | canonical CPU dispatch/build identity/lint | `core/t64_cop1.c`, `core/t64_softfloat.*`, audited minimal helper subtree |
| Diagnostic host adapter | `runner/t64_verified_input.c`, result/JSON helpers | separate local executable, exact inventory/protocol, private build/run script and authored adapter controls |

New paths are intentional creations, not existing APIs. Do not write source into
an installed runtime mirror. New helper subtrees must enter build identity and
production source/object lint explicitly; a top-level source glob is insufficient.

## Don't Hand-Roll

Reuse the verified snapshot/FD boundary, scheduler ownership, overflow/range
helpers and canonical JSON machinery. Prefer the audited mature arithmetic
closure over new IEEE arithmetic only when its full dependency/context
adaptation is qualified. Do not write an archive parser, a second machine or a
host-FP shortcut. No new package is installed simply because upstream README
uses it; qualification precedes installation.

## Common Pitfalls

The four adversarial corrections are binding: Status mask/RP policy, nested
EPC+BD capture, full Count/Compare/interrupt order with non-retiring exception
rescheduling, and a separately prepared executable CPU profile rather than a
renamed zero seed. The p.181 vector conflict and LL/SC/DMA/undefined-operation
choices remain explicit model policy. Native expected values must be separately
derived; running the same arithmetic function to create goldens is circular.
SoftFloat globals/TLS, omitted helper identity/lint coverage, cache alias mistakes
and handler progress tied to retirement can each defeat deterministic isolation.

The external suite is not a CPU-only ROM as shipped. Its source constructor
counts are not runtime denominators, and its aggregate result lacks per-CPU
statuses. A qualified CPU overlay must preserve complete required membership
and distinguish test failures from missing/blocked execution. Assets, boot bytes,
toolchain locks and crate-source correspondence are separate qualification tasks.
Derived ROM/output/results stay private under D2; public native vectors and
authored adapter controls remain independent. Do not weaken SC4 to native-only.

## Validation Architecture

Use the existing gate/asan infrastructure, no Wave 0 setup-only plans. Each
vertical slice adds and registers its behavior and independent positive/edge/
negative controls immediately. Preserve the existing exact 91 gate/93 sanitizer
names and all historical warm/synthetic controls; plan the exact new names and
final totals rather than assuming counts. Require `--no-tests=error` and an
explicit failure direction for every command. Build all invoked targets before
running tests. Focused checks after relevant changes, full gates at coherent
boundaries/final source, instrumented changed paths and retained fault/fuzz
controls. No broad repetition for metadata.

Coverage must include integer widths/overflow/branches/delay faults, all CP0
fields/interrupts/Count phases, 32/64-bit TLB and permission cases, unaligned
merge boundaries and LLSC, I/D alias/writeback/invalidation, all COP1 formats/
rounding/traps, instance isolation and canonical state. Authored protocol and
inventory controls verify the local adapter publicly without external ROM data.
The actual pinned local CPU suite has its own private exact inventory/status
record and mandatory run; absent/reduced/unreached members cannot pass it.

## Sources and Confidence

- `04-DECISION-CPU.md` — hash-pinned NEC primary sections, field rules, policies
  and uncertainty; corrected by `04-ADVERSARIAL.md` and final context.
- `04-DECISION-FPU.md` — official SoftFloat release/commit/license verification
  and NEC COP1 sections; exact closure audit is still required before copy.
- `04-DECISION-TESTS.md` — pinned official test source/package metadata and
  inspected interfaces; runtime counts, composition, install and boot remain
  pre-use qualification tasks, not claimed results.
- `04-ADVERSARIAL.md` — concrete primary-page and existing-code contradictions
  reconciled in `04-CONTEXT.md`. No source, package, ROM or hardware was run.

Confidence is high in the selected architecture and located manual statements,
conditional in the unqualified helper/tool/asset closures, and explicitly unknown
for the listed silicon/undefined-behavior policies. No hardware qualification is
inferred from document agreement or emulator output.

## Package Legitimacy Audit

No npm/pip/Cargo binary-package installation is selected by this plan. The
private recipe uses the Rust compiler to emit ELF, existing qualified
objcopy/linker tools and the project-owned flat ROM packer. It does not run
the upstream Cargo runner or install the candidate packaging tool.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---|---|---|---|---|---|---|
| nust64 (candidate 0.4.1) | crates.io | First published 2022-06-16 per registry check | 60 weekly in check | https://github.com/rust-n64/nust64 | [SUS] low-downloads | NOT SELECTED; no installation/invocation. Project packer provides the required capability. |

The actual GSD legitimacy query was run with the runtime's supported environment
proxy enabled. Its registry signals confirmed existence, source repository,
first publication date and no deprecation; its only remaining reason was
low-downloads. The earlier all-unknown response was a transport failure and was
not used as evidence of missing metadata. This heuristic is not proof that
the tool is malicious, and it is not permission to install it. The official
upstream links/package identity are recorded in 04-DECISION-TESTS.md.

Packages removed for SLOP: none. SUS candidate: nust64, excluded from the selected
recipe. If later proposed again, its exact source/lock/behavior audit and the
required blocking package-verification checkpoint precede installation.

Other pre-use qualification remains incomplete: the exact Rust nightly host
channel/component/compiler manifests and checksums, plus every actual Cargo
library dependency in the pinned test lock, source correspondence, licenses,
resolution and build hooks. These are not asserted registry-OK or approved.
Plan16.1 must enumerate the actual graph and run per-package legitimacy/source
checks before acquisition/build; ASSUMED/SUS inputs stop for the protocol's
required verification, with no inferred approval. Nothing is installed by this
research or by an unverified package name. The SoftFloat copy is a separately
audited fixed source closure under the existing BSD-3-Clause copy policy, not
a registry installation.

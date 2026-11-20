# Phase 7: RDP Reference Renderer and VI Scanout - Pattern Map

**Mapped:** 2026-11-16  
**Files classified:** 19 candidate paths from CONTEXT/RESEARCH  
**Analog coverage:** role matches exist for all paths; no existing RDP rasterizer or VI scanout implementation exists.

## File Classification

| New/Modified File | Role | Data Flow | Closest tracked analog | Match |
|---|---|---|---|---|
| core/t64_rdp.c (new) | device | event-driven, transform | core/t64_pi.c; core/t64_scheduler.c | role-match |
| core/t64_vi.c (new) | device | transform, event-driven | core/t64_observe.c | partial |
| core/t64_internal.h | model/state | shared state | same file | exact |
| core/t64_bus.c | bus/dispatch | request-response | same file; PI branch | exact |
| core/t64_scheduler.c | scheduler | event-driven | same file | exact |
| core/t64_machine.c | lifecycle/profile | request-response | same file | exact |
| core/t64_digest.c | state serialization | transform | same file | exact |
| core/t64_observe.c; core/include/tendies64/t64_machine.h | observer/API | copy-out | same files, output-drain API | exact |
| core/tests/test_rdp.c; core/tests/test_vi.c (new) | tests | transform/refusal | core/tests/test_pi_dma.c; core/tests/test_bus.c | role-match |
| core/CMakeLists.txt | build/test config | build registration | same file | exact |
| runner/t64_real.c | runner | guest execution, file I/O | same file | exact |
| runner/t64_real_fixture.c | fixture catalog | input validation | same file | exact |
| runner/tests/test_real_controls.c | integration control | process/result validation | same file | role-match |
| runner/CMakeLists.txt | build/test config | target/test registration | same file | exact |
| scripts/check-test-inventory.sh | test inventory | batch/check | same file | exact |
| scripts/check-rsp-no-hle.py; scripts/tests/test_rsp_no_hle.py | source/build guard | batch/check | same checker and wrapper | exact |

The RDP/VI filenames and the two core test names are research recommendations. Keep DP/VI behavior profile-gated through ordinary bus dispatch. Start with the authored CPU-written fill → RDRAM → VI raw packet → real headless runner path; do not treat it as broader renderer coverage.

## Pattern Assignments

### RDP admission and event work — core/t64_rdp.c

Use core/t64_pi.c:48-110 for validate → reserve → commit → mutate. The admission path calls t64_schedule_reserve_after and t64_schedule_commit before setting active transfer state (lines 89-109). Reuse the central scheduler contract, not a private queue or synchronous END-register renderer. core/t64_scheduler.c:88-127 validates time/capacity/sequence before producing a reservation; :149-189 commits in tick/sequence order.

    status = t64_schedule_reserve_after(machine, delay, T64_DEVICE_PI, ...,
                                        t64_pi_complete, &reservation);
    if (status != T64_STATUS_OK) return status;
    status = t64_schedule_commit(machine, &reservation);
    if (status != T64_STATUS_OK) return status;
    pi->active_valid = true;

### Bus, state, completion and identity — core device integration

- core/t64_bus.c:145-153 currently refuses DP and VI windows; PI read/write dispatch at :412-422 and :580-588 is the concrete normal-bus branch shape. Preserve width checks, status propagation and bus trace on successful writes.
- core/t64_internal.h:581-619 keeps device state and scheduler events per machine. Add future-affecting RDP/VI values here; output_drain_cursor at :602-603 is explicitly host-observer state.
- core/t64_mi.c:28-60 owns pending/clear and IRQ recomputation. RDP FullSync completion should call this owner; do not add a parallel DP IRQ latch.
- core/t64_machine.c profile writer at :910-950 and core/include/tendies64/t64_machine.h:129-160 show explicit policy/version serialization. Name new timing/undefined-output policies and version actual changed identities per D-08.
- core/t64_digest.c:301-309 hashes queued events; :375-390 hashes retained guest output but excludes the observer cursor. Include every future-affecting DP/VI phase, register and pending event; keep host cursor/file buffers out.

### VI output and real consumer — core/t64_vi.c, runner files

core/t64_observe.c:17-83 is the copy-out pattern: validate disjoint spans, report required size, refuse undersized buffers without cursor movement, then copy and advance only the observer cursor. The existing public output-drain contract is in core/include/tendies64/t64_machine.h:247-261. runner/t64_real.c:339-363 drains core bytes into host-owned storage; :611-625 creates/adopts the machine and :674-680 advances guest time then drains trace/output. Keep VI packet metadata and frame bytes separate from RDRAM expectations and host scaling.

For the wrong-pixel control, follow runner/tests/test_real_controls.c:492-639: launch the same supervised fixture, parse/check the result, and assert guest progress plus the intended comparator result. Register controls in runner/CMakeLists.txt:310-338. runner/t64_real_fixture.c:5-43 is the pinned fixture identity catalog; use only authored, rights-cleared fixture inputs.

### Tests, inventory and build/source guards

- core/tests/test_pi_dma.c:651-690 captures digest before/after queue-full refusal and checks that the triggering latch stays unchanged. Use this refusal shape alongside core/tests/test_bus.c; RDP/VI literal-pixel expectations need new independent cases.
- core/CMakeLists.txt:8-13 glob-adds top-level t64_*.c sources to the build identity, but unit tests are explicitly registered (PI example at :450-457). runner/CMakeLists.txt:258-273 separates production and test runner targets.
- scripts/check-test-inventory.sh:141-153 currently ends its phase list at Phase 6; add Phase 7 test names to the explicit inventory. scripts/check-rsp-no-hle.py:18-45 and :311-327 also maintain an explicit canonical core-source set; include new t64_*.c files there. Its tracked CTest wrapper is scripts/tests/test_rsp_no_hle.py, registered as repo.rsp-no-hle in runner/CMakeLists.txt:137-142.

## Shared Patterns

- Reserve complete bounded scheduler/output capacity before guest-state mutation; specify refused-operation digest/tick/register effects.
- Keep MI as DP interrupt owner; keep machine profile/version and semantic digest explicit.
- Keep guest-produced raw bytes in core, with copy cursors and file writes in the runner.
- For CTest, use build/gate and `ctest --test-dir build/gate --no-tests=error -R '^unit\.core\.(rdp|vi|dp)' --output-on-failure` after registering the exact names; retain the owning build's LastTest.log before discovery. Python harness tests use python3 -B and PYTHONDONTWRITEBYTECODE=1.
- Acceptance order from RESEARCH: first accepted real-runner fill path plus a reached one-literal wrong-pixel control; then source/inventory/refusal checks; only later longer suites. VID-04 remains blocked by existing rights gates.

## No Analog Found

No existing code implements RDP command decoding/raster effects or VI register-driven raw scanout. Use the source-owned facts and scoped recommendations in 07-RESEARCH.md for those semantics; existing analogs establish only project boundaries and test/build conventions.

**Metadata:** Analog search stayed within tracked core device/state/bus/scheduler, core tests, runner fixture/control and CTest/source-check paths named above. All analog paths are git-tracked; no runtime mirror paths are referenced. No builds, tests, guest assets, oracle outputs, CI jobs or hardware analysis were run for this mapping.

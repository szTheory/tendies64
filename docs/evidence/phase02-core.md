# Phase 02 core implementation evidence

This record covers local native-model, inventory and sanitizer checks for the
deterministic machine skeleton. These results do not establish guest execution,
boot behavior, cycle accuracy or hardware compatibility.

## Tested source and environment

| Item | Measured value |
|------|----------------|
| Tested source revision | `3fcfc019260f304b0989115f0ae1f632d4ab7ccd` |
| Named profile | `ntsc-cart-4m-skeleton` |
| Profile SHA-256 | `b7d1600ad960b4e04fdc3ec372675edc47b26b15be8e9cd84cbab4fd75835647` |
| Gate build identity | `t64-build-v1-270d7045b235cdf3d5f0b2e17501b507dbb5889c1d0b24928bed2265111b42dd` |
| ASan build identity | `t64-build-v1-1e566e21d1c4786ac9ab9893ca59daf1ce97cda82831416fd27bc4e56d86b529` |
| Host/compiler | Darwin arm64; Apple clang 21.0.0 (`clang-2100.1.1.101`) |
| CMake | 4.4.3 |

Both builds embed source revision `3fcfc019260f304b0989115f0ae1f632d4ab7ccd`.
The build fingerprints are separate because the gate and ASan configurations
use different compiler flags.

## Measured checks

| Check | Result |
|-------|--------|
| Named-profile producer, strict model checker, unchanged legacy result/identity/seal/suite/claims refusal, and positive legacy control | Focused regression: 3/3 passed; current code passed again in final 73/73 gate; canonical v1 goldens unchanged |
| Cross-device determinism | `unit.core.determinism`: 1/1 passed, 1,933 assertions, 0.68s |
| Exact inventory | Gate 73/73; ASan 75/75; 148 single-registration removals rejected; duplicate, unexpected, disabled, empty and enumeration-error controls rejected |
| Final local gate | `./scripts/gate.sh`: PASS; CTest 73/73; 51s total |
| Gate inventory-control cost | 13s for the 148 removal controls and five malformed-inventory controls; live gate inventory comparison rounded to 0s |
| ASan/UBSan component, campaign and negative control | 14/14 passed; 75.84s wall |
| Production admission fuzz campaign | Seed `0x7a64c0de`; 250,000 mutations plus 148 fixed cases; 75.83s; passed |
| Sanitizer negative control | Separate faulting child was detected; parent CTest passed |
| Core lint | 15 files; zero violations |

The named probe admitted a runtime-authored 4 KiB image under the named
profile. Its independently expected PI copy was 16 bytes visible at tick 9,
the SP copy was 8 bytes visible at tick 22, and MI pending reported the PI bit
(`0x10`). The canonical `t64.model-result` v1 record reported 94 checked and 0
failed assertions, content digest
`658c40a86a711053f3a36d29993181811cce283a623a6e7b1fee4003649cfe3b`, and state
digest `63afb49d9669dea05112953b01c369def27d242787825823d8a310815cf7dfb6`.
The producer, strict checker and existing consumer/claims controls passed
together in the final local gate.

The determinism test compares exact PI (24-byte) and SP (8-byte) output bytes
with independent expectations across isolated, interleaved, one-shot and
partitioned schedules. It checks stop bounds using labeled synthetic counter
events, PI cancellation, SP pending promotion/source changes, refusal of ROM
replacement while PI and SP work is pending, and no host allocate/release calls
during every `advance`. Trace-on, trace-off and overflow runs preserve the same
state digest and expected output. The overflow case observes a 256-record ring
and exactly 49 dropped records.

## Commands and environment limits

The final local command was `./scripts/gate.sh`. Its measured steps were:
shell syntax 0s, lint selftest 15s, configure 0s, build 0s, inventory selftest
13s, exact gate inventory 0s, CTest 22s and core lint 1s (51s total, rounded by
the gate to whole seconds).

The sanitizer command was:

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset asan --no-tests=error \
  -R '^(unit\.core\..*|unit\.model-result|fuzz\.rom-admission|sanitizer\.negative-control)$' \
  --output-on-failure
```

LeakSanitizer `detect_leaks=1` is unsupported in this host environment. Address
and undefined-behavior sanitizers remained enabled. The instrumented build
reported nonfatal large-stack-frame warnings in `t64_machine_digest`,
`write_profile`, `t64_sp_complete` and `decode_descriptor`; the normal gate
build remained warning-clean under its configured error policy. There was no
unexpected sanitizer finding; the negative control detected its deliberate
fault. The full fixed campaign stayed below its 120-second limit. No reduced
campaign, new dependency, protected asset or hosted job was used.

## Evidence limits and pending handoff

The profile continues to label PI/SP timing and arbitration as model choices.
The test evidence exercises native device code only. CPU execution, real PIF or
IPL boot, VI/AI output and hardware timing remain unsupported, deferred or
unknown as listed in [profile limits](../profile-limits.md).

Independent code review, ASVS L1 security review at the configured high
threshold, goal-backward verification, hosted Linux/macOS CI results, push and
phase completion remain with the parent orchestrator. None is claimed complete
by this local execution record.

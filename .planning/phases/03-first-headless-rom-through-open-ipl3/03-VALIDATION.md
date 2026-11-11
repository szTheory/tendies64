---
phase: "03"
slug: first-headless-rom-through-open-ipl3
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-10"
---

# Phase 3 — Final Validation Contract

Post-implementation audit preserves all six roadmap criteria, eight requirements and fifteen decisions. All thirteen plans now have summaries. The required automated behavior checks and actual exact-source hosted pair passed; details and coverage limits are in docs/evidence/phase3-ci.md. No human validation is asserted. Tests were registered with their owning features; no setup-only Wave 0 was needed.

## Infrastructure, sampling and limits

Existing C17/CTest, CMake gate/asan presets, canonical JSON/SHA helpers, exact inventory and source/object core lint remain. Commands run from the checkout root headlessly/in the background. Configure/build all invoked producer/checker/fixture targets before the owning named CTest, with explicit consumer dependencies. CTest always uses --no-tests=error and exact names. Every PLAN automated command has its immediate fails_when sibling; tests reject intended violations and require valid positive counterparts.

The first actual-entry tracer extends existing unit.core.tracer. Qualification determines numeric header/payload/copy/entry layout from the actual pinned source bytes, with separate whole upstream asset SHA256/MD5, embedded executable payload SHA256 and authored normalized whole-ROM SHA256. Header transformations are explicit; full upstream asset identity is not blindly compared to a transformed ROM prefix. Both private fields and required public-header/scheduler expansion are owned by 03-01/1–2. Authored tools/hello are 03-02; gate/registration and early existing GCC lane are 03-03.

Full gate runs at coherent inventory boundaries and before final verification; no unnecessary repetition after irrelevant changes. Shared build/inventory/profile/digest state serializes all thirteen waves. Exact existing 73 gate/75 ASan names are retained; all 18 additions yield final 91/93. Remove each expected name individually in inventory selftests, plus duplicate/disabled/unexpected/empty cases.

Full all-target ASan/UBSan build precedes selected tests. Current host requires ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1; address/undefined remain active. Existing bounded admission fuzz and intentional sanitizer fault child remain. A fault child succeeding cleanly is failure of the sanitizer control.

## Exact test/task/wave/requirement/threat/command map

All named rows below are implemented and passed in the successful exact-source hosted common gates (run 43103669084 attempt 1, source 7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6). Each row has behavior and failure-direction controls in its owning test. The required collector/comparator passed on the actual pair; same-host mock controls are supplementary. Requirement coverage is complete; no missing automated test requires a Nyquist test-generation agent. Phase acceptance remains the verifier's separate decision.

| CTest identity | Plan/task; wave equals plan number | Requirement | Threat ref | Exact command |
|---|---|---|---|---|
| `unit.core.boot-entry` | 03-03/1 | BOOT-01,BOOT-02 | T-03-01,T-03-02,T-03-05 | `ctest --preset gate --no-tests=error -R '^unit\.core\.boot-entry$' --output-on-failure` |
| `unit.core.cpu-path` | 03-04/1 | BOOT-01,PROF-05 | T-03-07,T-03-08 | `ctest --preset gate --no-tests=error -R '^unit\.core\.cpu-path$' --output-on-failure` |
| `unit.core.cp0-cache` | 03-04/2 | BOOT-01,PROF-05 | T-03-07,T-03-08 | `ctest --preset gate --no-tests=error -R '^unit\.core\.cp0-cache$' --output-on-failure` |
| `unit.core.warm-device-policy` | 03-05/1-2 | BOOT-01,BOOT-03 | T-03-09,T-03-10 | `ctest --preset gate --no-tests=error -R '^unit\.core\.warm-device-policy$' --output-on-failure` |
| `unit.core.isviewer` | 03-06-07/1 | RUN-05,BOOT-04 | T-03-11,T-03-12,T-03-13 | `ctest --preset gate --no-tests=error -R '^unit\.core\.isviewer$' --output-on-failure` |
| `runner.result-v2` | 03-10;13/1;2 | RUN-01,RUN-03 | T-03-19,T-03-20,T-03-25 | `ctest --preset gate --no-tests=error -R '^runner\.result-v2$' --output-on-failure` |
| `runner.fixture-descriptor` | 03-09/1-2 | RUN-01,BOOT-02 | T-03-17,T-03-18 | `ctest --preset gate --no-tests=error -R '^runner\.fixture-descriptor$' --output-on-failure` |
| `integration.open-ipl3-hello` | 03-11/1-2 | BOOT-01,BOOT-04,RUN-01,RUN-05 | T-03-21,T-03-22 | `ctest --preset gate --no-tests=error -R '^integration\.open-ipl3-hello$' --output-on-failure` |
| `integration.open-ipl3-determinism` | 03-12/2 | PROF-05,RUN-01 | T-03-24 | `ctest --preset gate --no-tests=error -R '^integration\.open-ipl3-determinism$' --output-on-failure` |
| `controls.real.empty-halt` | 03-12/1 | RUN-03 | T-03-23 | `ctest --preset gate --no-tests=error -R '^controls\.real\.empty-halt$' --output-on-failure` |
| `controls.real.empty-bound` | 03-12/1 | RUN-03 | T-03-23 | `ctest --preset gate --no-tests=error -R '^controls\.real\.empty-bound$' --output-on-failure` |
| `controls.real.wrong-transcript` | 03-12/1 | BOOT-04,RUN-03 | T-03-23 | `ctest --preset gate --no-tests=error -R '^controls\.real\.wrong-transcript$' --output-on-failure` |
| `controls.real.unsupported-opcode` | 03-12/1 | BOOT-01,RUN-03 | T-03-07,T-03-23 | `ctest --preset gate --no-tests=error -R '^controls\.real\.unsupported-opcode$' --output-on-failure` |
| `controls.real.output-overflow` | 03-12/1 | RUN-05 | T-03-12,T-03-23 | `ctest --preset gate --no-tests=error -R '^controls\.real\.output-overflow$' --output-on-failure` |
| `controls.real.source-replaced` | 03-09;11/1-2;2 | RUN-01 | T-03-17,T-03-18,T-03-22 | `ctest --preset gate --no-tests=error -R '^controls\.real\.source-replaced$' --output-on-failure` |
| `controls.real.identity-mismatch` | 03-12/1 | BOOT-02,BOOT-04,RUN-01 | T-03-01,T-03-03,T-03-19,T-03-23 | `ctest --preset gate --no-tests=error -R '^controls\.real\.identity-mismatch$' --output-on-failure` |
| `controls.real.input-partition` | 03-08/1 | RUN-01,PROF-05 | T-03-15,T-03-16,T-03-24 | `ctest --preset gate --no-tests=error -R '^controls\.real\.input-partition$' --output-on-failure` |

unit.core.tracer remains its existing identity, now with mandatory actual pinned entry/retirement/unsupported cases in 03-01. isviewer registers in 03-06 then expands drain/input submission and inventory in 03-07. runner.result-v2 adds hosted-wrapper cases in 03-13/2 without changing the guest v2 or legacy v1 schema. source-replaced begins with descriptor helpers in 03-09 and MUST repeat actual CPU-backed admission in 03-11/2; helper-only proof cannot close SC6. Input trace proves scheduling only, not SI/PIF consumption.

## Success criteria and requirements

| Roadmap criterion | Exact owners | Required proof |
|---|---|---|
| SC1 | 03-01/1,03-03/1,03-13/3 | Primary source/hash/section index checked first; cite hardware claims; no manual bytes published. |
| SC2 | 03-02/1,03-06–08,03-10–11 | Actual IPL3/DMA/hello/completion, exact text, fixed reset/profile/input/bound, trace and real result. |
| SC3 | 03-01–03,03-12/1,03-13 | Separate whole asset/payload/ROM digests, rights/license/revision/owner and protected-byte scans over repo/staged output. |
| SC4 | 03-03/1,03-10/1,03-12/1 | Real empty halt AND bound are boot-fail; PIF/CIC/cold/timeout limits explicit. |
| SC5 | 03-12/2,03-13/1–2 | Required actual exact-SHA Linux/GCC and macOS/Clang publication, collection and comparison. |
| SC6 | 03-09/1–2,03-11/1–2 | Verified read-only unlinked inherited fixture/input descriptors, barrier replacement and digest of actual admitted owned bytes. |

| Requirement | Concrete owners |
|---|---|
| BOOT-01 | 03-01/1–2,03-04/1–2,03-05/1–2,03-11/1–2 |
| BOOT-02 | 03-01/1,03-02/1,03-09/1,03-12/1,03-13/1–3 |
| BOOT-03 | 03-03/1,03-05/1–2,03-10/1,03-12/1,03-13/3 |
| BOOT-04 | 03-02/1,03-06/1,03-11/1–2,03-12/1 |
| RUN-01 | 03-07–11,03-13/1–2 |
| RUN-03 | 03-10/1,03-11/1–2,03-12/1 |
| RUN-05 | 03-06–07,03-11/1–2,03-12/1 |
| PROF-05 | 03-01/2,03-04–05,03-07–08,03-12/2,03-13/1–2 |

## Inventory progression

| Completed owner | Registered gate / ASan names | Exact protection |
|---|---|---|
| 03-01–02 | 73 /75 | Existing tracer extended, no new identity. |
| 03-03 | 74 /76 | boot-entry; common gate provisions verified fixtures. |
| 03-04 | 76 /78 | cpu-path, cp0-cache. |
| 03-05 | 77 /79 | warm-device-policy. |
| 03-06 | 78 /80 registered | isviewer owning CTest runs immediately; next 03-07 owns matching inventory addition; full gate waits until that addition. |
| 03-07 | 78 /80 | isviewer inventory coherent; copy/drain/input-submit behavior added. |
| 03-08 | 79 /81 | input-partition. |
| 03-09 | 81 /83 | fixture-descriptor, source-replaced. |
| 03-10 | 82 /84 | result-v2. |
| 03-11 | 83 /85 | open-ipl3-hello and actual replacement repetition. |
| 03-12–13 | 91 /93 | Six real controls and open-ipl3-determinism; each single missing name rejected. |

## Mandatory hosted handoff and comparator

Exactly the existing two hosted jobs run the common gate against the tested implementation SHA. Each MUST emit one closed canonical t64.boot-evidence v1 wrapper with commit_sha/run_id/run_attempt/job_name and actual strict nested real-v2 pass, using the exact authorized fixture/payload/profile/input/bound/transcript/state. Rights/PII scans precede scoped upload. Artifact names include SHA/run/attempt/job and only permitted JSON is uploaded; raw/private logs, manuals, tools and unqualified binaries are excluded.

03-13/2 MUST run the authorized post-run collector with selectors derived from the actual implementation commit/run/attempt, not from owner input. It validates repository/workflow/head SHA, completed successful run and exactly one completed successful named job per lane, then requires exactly one unexpired matching artifact per job. It retrieves those exact records into one fresh private workspace, bounds/validates them, rejects extra files/symlinks/wrong or duplicate identities and atomically places the pair at build/evidence/gate-linux.json and gate-macos.json. Only then does the mandatory comparator validate expected selectors and equality of source asset/payload/fixture/profile/input/bound/log/state plus exact transcript; truthful compiler build IDs may differ. Local same-host samples cannot satisfy this proof.

The required collector command is `sh scripts/collect-boot-evidence.sh --collect --repo szTheory/tendies64 --sha "$T64_TESTED_COMMIT" --run "$T64_CI_RUN_ID" --attempt "$T64_CI_ATTEMPT" --out build/evidence`. It fails for unset/wrong selectors, missing/duplicate/wrong-SHA or unsuccessful job/artifact, unsafe content or failed transfer. The required comparator is `sh scripts/compare-boot-evidence.sh --sha "$T64_TESTED_COMMIT" --run "$T64_CI_RUN_ID" --attempt "$T64_CI_ATTEMPT" build/evidence/gate-linux.json build/evidence/gate-macos.json`; it fails without the exact actual hosted pair or on any required identity/state/transcript difference. Selftests have positive controls and all relevant bad-input controls. Final phase verification cannot proceed without this collection/comparison evidence.

## Critical negative controls and final checks

Real empty halt/bound, wrong complete text, incomplete prefix/bound, unsupported opcode before completion, output overflow, each asset/payload/tool/fixture/profile/input/seed/entry mismatch, skipped-IPL3 milestones, raw invalid/missing/truncated/unknown report, capture error, crash and wall kill never pass. Host/report errors take precedence. Exact source/payload transformations and recorded policy identity are independently checked. Current source tests run under both sanitizers after full build, with the active fault child and retained admission campaign. ./scripts/gate.sh checks exact inventory, semantics, manifest/rights/PII/claims and core lint.

No full CPU/RSP/cold calibration/controller capability is added by revision. No physical/UI/owner-file verification is planned. Concrete rights/authority/evidence gaps remain blocking; routine builds/CI/reviews/fixes stay authorized and background/headless.

## Prominent retained flags

Fourteen surfaced edge predicates remain in current must_haves and 03-PROBE-LEDGER.md; the five original unclassified rows remain explicitly flagged, not auto-green. Three descriptor-less judgment prohibitions in 03-13 concern inflated warm/hardware claims, trace-as-controller claims and identity-as-rights/source-regeneration claims. They are non-authoritative with human review recommended under AUTONOMY; no human validation/risk acceptance is invented. See 03-SOURCE-AUDIT.md for full four-source/decision coverage and dependency ownership; 03-PLAN-CHECKS.md records document checks only.

## Validation sign-off

- [x] Every planned executable command has an immediate failure direction.
- [x] All tests/requirements/threats have exact planned ownership.
- [x] Required exact-SHA two-job evidence handoff/comparator is concrete.
- [x] Feature checks and exact hosted pair passed, with full sanitizer and later affected-path coverage limits recorded.
- [x] Actual runtime/source/rights evidence and resolved post-implementation review are recorded.

Automated execution evidence is recorded; no routine owner acknowledgment is required. Full ASan/UBSan passed before later test-setup and shell changes, followed by affected-path instrumented checks and current hosted gates. Three non-authoritative judgment flags remain under AUTONOMY; no human sign-off is inferred.

## Validation Audit 2026-11-11

| Metric | Count |
|---|---|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |

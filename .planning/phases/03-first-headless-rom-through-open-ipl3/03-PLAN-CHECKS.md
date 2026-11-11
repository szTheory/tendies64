# Phase 3 revision-2 planning verification

Document checks only. No Phase 3 source implementation, build, runtime test or human validation is claimed.

## Review findings addressed

| Original finding | Required property | Concrete correction |
|---|---|---|
| Blocker: original03-01 scope | Bounded plan while retaining first actual-entry tracer | Split into current03-01–03; first plan has7 declared files, real tracer plus explicit public-header/scheduler ownership. Fixture and gate tasks remain dependent, with no scaffold detour. |
| Blocker: original03-08 handoff | Both actual hosted records reach one required comparator | Current03-13/1 requires two exact-SHA canonical JSON uploads; /2 requires authorized exact-run job/artifact validation, retrieval into one workspace and strict comparison before final verification. Missing/duplicate/wrong-SHA inputs fail. |
| Warning: original03-04 scope | Bounded output/input plans | Current03-06,07,08 each own five files: guest capture, copy/submission seam and canonical schedule parser respectively. |
| Warning: original03-06 scope | Checker before producer and supervisor after runner | Current03-10 owns five checker-contract files; current03-11 owns eight runner/supervisor files with explicit dependency. |

These are planner corrections to all four required properties; independent failed-item review remains next. No reviewer approval is invented.

## Current document checks

| Check | Observed result |
|---|---|
| GSD frontmatter and plan structure | All thirteen plans valid, complete closing output tags; no structure errors. |
| Scope/ownership |21 tasks; maximum9 files per plan and5 per task. Exact task-owned file union equals every frontmatter list. Every modified path is explicit in read_first, including earlier-created files. |
| First actual-entry ownership | core/t64_internal.h private state, public machine header and scheduler.c expansion all belong to current03-01; real tracer is still the first task and uses the existing generic callback path before named stop expansion. |
| Dependency/shared-state audit | Thirteen sequential waves with the correct preceding dependency, no same-wave overlap; shared build/inventory/profile/digest state serialized. |
| GSD verify-failure-directions3 |73 automated commands; zero blockers/warnings, no read error. Each command has its immediate fails_when. |
| GSD decision coverage |All15 locked decisions covered. All8 assigned requirement IDs are in frontmatter and all6 roadmap criteria have exact owners. |
| GSD verify-command-paths3 |73 commands, zero blockers/warnings; recognized path detector reports not_applicable. Direct source/read/ownership audit supplements it. |
| Threat uniqueness |26 distinct numbered T-03 entries; only reserved T-03-SC repeats. Every plan includes ASVS1 high/critical-blocking mitigations with specific proof. |
| Probes |All14 edges preserved;9 explicit planned predicates and5 originally unclassified flags retained. All80 recall candidates remain dispositioned into3 descriptor-less judgment prohibitions. |
| API detector |Nonempty revised plan+Phase3 scope returned detected=false; exit1 is its normal negative verdict, not skipped. COVERAGE.md distinguishes CI administration from runtime integrations. |
| ORM/assumption scope |No ORM schema paths. Existing assumption-delta negative verdict remains applicable to the unchanged phase goal; v1/v2 dispatch and separate model identity remain explicit. |
| Portable/privacy/whitespace scan |No literal private home path in current plans; portable @$HOME execution references retained. git diff --check passed. No protected/manual/raw/tool/binary material was added. |

The one-way v2 warning now belongs to03-10: D-12 already explicitly settled adding real v2 while preserving v1, so the installed settled-decision rule requires no repeated checkpoint.

## Current threat-to-proof ownership

| Threat refs | Owning proof |
|---|---|
| T-03-01,02 |03-01 actual tracer, asset/payload qualification, public/private/profile/scheduler stop and state checks. |
| T-03-03,04 |03-02 tool/packer reproducibility, separate asset/payload/authored-ROM identities and rights manifest. |
| T-03-05,06 |03-03 boot-entry, exact inventory/gate provisioning, hash-only primary index and boot-limit claim checks. |
| T-03-07,08 |03-04 cpu-path/cp0-cache, real unsupported/bound cases and instrumented CPU run. |
| T-03-09,10 |03-05 warm-device-policy/sp-dma/pi-dma and adjacent guest transfer bounds. |
| T-03-11,12 |03-06 ISViewer lanes/counts/capacity and actual overflow control. |
| T-03-13,14 |03-07 bounded alias-safe copy, canonical produced-byte identity and queue-input refusal/state behavior. |
| T-03-15,16 |03-08 canonical parse/tick/count boundaries and actual input-partition/trace proof. |
| T-03-17,18 |03-09 descriptor faults, synchronized replacement and cleanup; actual CPU repeat in03-11. |
| T-03-19,20 |03-10 strict v2/raw consistency/outcome controls, unchanged v1/model contracts. |
| T-03-21,22 |03-11 actual IPL3/DMA/hello/descriptor admission and report/capture/process controls. |
| T-03-23,24 |03-12 real failure controls, future-state/partition/interleave/trace and exact90/92 inventories. |
| T-03-25,26 |03-13 required exact-SHA successful-job JSON delivery/collector/comparator, current sanitizers and sanitized rights/PII evidence. |
| T-03-SC |Qualified source/byte pins and bounded rights-audited build tools; no npm/pip/cargo installation. |

Three judgment flags and five original unclassified flags remain non-authoritative/prominent; no human validation or risk acceptance is recorded. Concrete rights/authority/evidence stops still apply. Dirty AGENTS.md/config.json, untracked state.json and the original reviewer report are outside this revision commit.

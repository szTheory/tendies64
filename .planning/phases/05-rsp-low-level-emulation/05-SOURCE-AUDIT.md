# Phase05 source coverage audit

Planning coverage only, 2026-11-13. No implementation, ROM, external tool or
diagnostic execution occurred during planning. Ten plans,24 tasks, waves1–10.
Shared canonical core/header/profile/digest and CMake/inventory trees require
serialization; there is no setup-only wave. The first task is the real tracer.
Every task has read_first, action, automated/fails_when, acceptance and done.

| SOURCE | ID | Feature or constraint | Plan/task | Status | Notes |
|---|---|---|---|---|---|
| GOAL | Phase05 | Real IMEM/DMEM scalar/vector microcode, DMA/status, portable reference and local RSP groups |01–10|COVERED|No HLE; no reduced opcode or diagnostic denominator |
| GOAL | SC1 | ROM/native fetched instructions and SP_STATUS/semaphore/halt/BREAK/PC |01.1,02.1–2,05–07,09|COVERED|Independently expected state and authored ROM |
| GOAL | SC2 | Permanent reference, per-opcode fuzz, optimized differential if present; separate scheduled long stress vmulf→vrsq32 |06–08|COVERED|No optimized path is introduced, so no differential success is claimed; authored stress separate, external disabled until D2 |
| GOAL | SC3 | DMA alignment, bank wrap, stride and queue |02.1–2,09.2|COVERED|Concrete setup12/8-byte beat functional model; hardware timing limit explicit |
| GOAL | SC4 | Every RSP group/member passes or gets individual reason; automated no HLE |03–04,08.2,10.1|COVERED|Exact source-derived private seal; refusal/incomplete never pass |
| REQ | RSP-01 | SU/VU(COP2), IMEM/DMEM, status/semaphore/halt/BREAK/PC |01–02,05–07,09–10|COVERED|Full defined encoding ledger and documented reserved/refused slots |
| REQ | RSP-02 | Portable permanent reference and per-opcode verification |01,05–08,10|COVERED|Independent integer/rational/vectors/invariants; future optimization requires full-state comparison |
| REQ | RSP-03 | DMA alignment/wrap/queue |02,09–10|COVERED|Count/stride, queue capacity, HALT-independent progress, notifications |
| REQ | RSP-04 | Local RSP groups individually classified |03–04,10|COVERED|Component identity and exact sealed membership distinct from frozen CPU contracts |
| REQ | RSP-05 | No canonical HLE |01–02,08.2,09–10|COVERED|Fetched path, source/object route check and bypass mutation controls |
| CONTEXT | D-01 | Distinct immutable capability profile, all state, old IDs/bytes/refusals unchanged |01.1–2,02,05,10.3|COVERED|Model6/state12 domain and exact predicates; build coverage distinct from profile semantics |
| CONTEXT | D-02 | Portable bytes/lanes, masked48, complete flags/divider; no speculative optimization/deps; independent per-op fuzz |01,05–08|COVERED|One permanent reference; no false differential label |
| CONTEXT | D-03 | Named deterministic functional issue, guide hazards, explicit timing/arbitration/corruption limits |02.1–2,05–07|COVERED|Exact guide latency transcription before affected use; policy values and refusal effects explicit |
| CONTEXT | D-04 | Primary entry-gate ledger; VMOV/clamp/divider evidence/provenance and unknowns |02,05–07;05-PROBE-LEDGER.md|COVERED|Permitted software algorithm correction honored; physical-edge unknown remains |
| CONTEXT | D-05 | Early actual qualified component build/composition/startup/refusal, before full ISA |03–04 before05–07|COVERED|No CPU receipt rights transfer; source/tool/lock/assets before build, actual composition before run |
| CONTEXT | D-06 | Shared canonical primitives and distinct exact RSP contract; CPU frozen |03.1,04.1–3,10.1|COVERED|Required component=rsp, exact schema1 inventory/batch/run, no Python completeness duplication |
| CONTEXT | D-07 | Private/local external artifacts, exact caps/stops, headless tests, native/ASan/fuzz/hosted pair/scheduled lane |All;08.1,10|COVERED|Guest10M/step16/child10s/outer12s/capture4096; admission120 and old inventories retained |
| CONTEXT | D-08 | Thin authored fetched tracer, early SP/DMA/integration, meaningful RED and coherent review |01–04,09–10|COVERED|Actual fetched opcode/PC/attempt asserted; no helper-only tracer or fabricated RED |
| RESEARCH | Architecture | RSP native core; SP scheduler/bus; host-only qualification/shared reporting |01–04|COVERED|Responsibility-map tiers honored |
| RESEARCH | State/profile | Explicit new capabilities, fields/events, all historical descriptor/digest encodings |01–02|COVERED|Profile development versus qualification boundary; no numeric ordering or per-opcode profile churn |
| RESEARCH | Issue/hazard/DMA | Adjacent SU/VU pairing, readiness/stall and branch policy, setup/beat/queue/refusal |02|COVERED|Primary table transcribed and controls before affected execution; no full speculative pipeline |
| RESEARCH | Instruction matrix | Scalar/control/CP0, vector transfers/moves, arithmetic/flags/accum/clip and reciprocal |05–07|COVERED|Full slot disposition, not a reduced successful subset |
| RESEARCH | Divider correction | Exact selected ares ISC audit; integer helper; complete notices/provenance; independent math |07.1–3|COVERED|Initial blocked verdict superseded for software implementation only; original physical limit preserved |
| RESEARCH | First use | Component qualification and actual early tool/build/ROM start or refusal |03–04|COVERED|Unchanged accepted inputs reused, exact new RSP closure required |
| RESEARCH | Canonical reporting | Separate sealed RSP lifecycle on shared parser/encoding/aggregation |04,10.1|COVERED|CPU schemas, membership and completion rules frozen |
| RESEARCH | Test infrastructure | Preserve111/113 names; real additions; all-target sanitizer; exact controls/fuzz |01–10;05-VALIDATION.md|COVERED|Measured current CTest inventories111/113 during planning;16 planned additions→127/129 |
| RESEARCH | Security | Input/lane/DMA validation, transactional refusal, private evidence, no HLE |Every threat model;10.2–3|COVERED|ASVS1; high-or-higher blocks; evidence threats explicitly reviewed |
| RESEARCH | Unknowns | Exact arbitration, active-PC corruption, async halt/step and physical divider edges |02,07;05-PROBE-LEDGER.md|COVERED|Specific refusal or named software policy, never a successful silicon check |

## Capability contributions and discovery

All active planner-targeted inline contributions in saved plan-pre-hooks were
read. Security applies (ASVS1, high blocking threshold) and every plan includes
unique threat rows plus reserved T-05-SC. API coverage does not apply: RSP vector
registers are not AI vectors and this phase has no external service integration.
ORM schema push does not apply to diagnostic JSON records. Frontend gate is
false. The assumption-delta query returned detected:false; nevertheless01
records explicit generalized capability identity to prevent a CPU-only anchor
from leaking. Existing C17/CMake/parser/scheduler patterns and supplied primary
research are sufficient discovery; no new dependency/install or broad research.
Graph capability is disabled; project-local skills directories were absent;
planner skills-only query returned no configured injection. Estimate calibration
factor1, sample_count0, confidence low is used verbatim.

## Artifact ownership and dependency graph

| Plan | Needs | Creates | Checkpoint | Wave |
|---|---|---|---|---|
|01|Existing CPU/SP/scheduler/test infrastructure|Fetched tracer, model6/state12, rsp module/profile contract|No|1|
|02|01 real fetched route|SP status/PC/semaphore, issue/hazard and beat DMA/control edge tests|No|2|
|03|02 new SP route and existing private audit receipts|RSP source seal/pre-use closure, actual build/composition evidence|Concrete rights/closure stop if encountered|3|
|04|03 component build/receipt|Shared primitives, RSP schema/adapter, actual early startup/refusal|Concrete rights/authority stop if encountered|4|
|05|04 observed route/refusal|Complete scalar/CP0/moves/vector transfers and encoding ledger|No|5|
|06|05 fetch/element/transfer routes|Arithmetic/multiply/clip flags and full function-slot ledger|No|6|
|07|06 portable VU/latches and saved ISC candidates|Audited adapted integer divider and independent tests|Concrete provenance contradiction blocks copy|7|
|08|07 complete defined instructions|Per-opcode fuzz, authored scheduled stress, no-HLE controls|No|8|
|09|08 canonical full low-level path|Authored ROM/source/manifest/composition and actual CPU/SP/RSP evidence|No|9|
|10|All saved source/evidence|Exact private partition, configured reviews/fixes, final native/ASan/hosted/artifact checks|Unresolved high finding or explicit stop remains blocking|10|

Each PLAN's Artifacts section owns every planned source, script flag and report;
all files_modified belong to its tasks. Generated/private outputs are owned by
the producing command in ignored build storage. No unplanned standalone spec,
AI/UI contract or ORM migration is introduced. Parent owns routine summaries,
ROADMAP/STATE, commits and hosted tracking; planner has not mutated them.

## Remaining evidence flags

All required sources are planned; none is silently deferred. The five raw
unclassified edge-probe rows remain unresolved, explicitly mapped in
05-PROBE-LEDGER.md. This is not automatic green coverage. Guide latency values
must be transcribed/controlled before affected behavior; undocumented physical
arbitration/corruption/divider edges remain declared limitations. No diagnostic
rights/publication stop is removed. No execution evidence or human acceptance
is claimed by this audit.

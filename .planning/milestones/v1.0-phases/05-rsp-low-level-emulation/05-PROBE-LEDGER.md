# Phase05 edge and primary entry-gate ledger

Planning only, 2026-11-13. The saved generic edge probe returned five
unclassified/unresolved rows. This ledger adds explicit domain predicates and
owners; it does not rewrite those rows to green or claim execution.

Primary guide: Silicon Graphics Computer Systems, *Nintendo Ultra64 RSP
Programmer's Guide*, cover Version1.1/internal Revision1.0, public PDF:
https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf
SHA256 `6ec646ba9bd836af9aa30ae90099af55dbaf291fbd8b39ff8be5664ada3bd668`.
Period programming guidance is distinguished from measurement and model policy.

## Raw unclassified probes and explicit owners

| Raw row | Raw status | Domain predicate that must be checked | Owning task/test | Planned disposition |
|---|---|---|---|---|
|RSP-01|unclassified/unresolved|Fetched instruction really retires, full PC/opcode is intended, element15/register31/DMEM0xfff wrap is bounded; flags/accumulator/latches and SP controls are complete; old profile refuses|01.1 tracer;02.2 control;05–07 instruction tests;09 ROM|Explicit behavioral proof pending; no generic backstop pass|
|RSP-02|unclassified/unresolved|Every supported opcode has nonzero deterministic seed cases and independent full-state expectation/invariant; absent optimized path is explicitly reported|08.1 unit.core.rsp-fuzz;06–07 independent vectors|Reference/invariant evidence pending; optimized differential not applicable without second path|
|RSP-03|unclassified/unresolved|Lengths0/7/8/0xfff, count1/256, stride0/0xfff, both bank ends/directions, active+pending/full, busy/HALT and exact refusal before mutation|02.1–2 unit.core.sp-dma/unit.core.rsp-control;09 ROM|Explicit byte/event/notification matrix pending|
|RSP-04|unclassified/unresolved|Exact sealed source membership including aliases/repeated names/zero values partitions every selected registration/value; no stale/wrong-component/forged completion or missing reason|03 source closure;04 protocol/run controls;10.1 final partition|Local-only authenticated execution/classification pending; failures/refusals never pass|
|RSP-05|unclassified/unresolved|All canonical RSP writes require fetched decode; inserted task-ID/signature/CRC bypass is detected; unsupported fetched instruction cannot retire silently|08.2 repo.rsp-no-hle and mutations;01/09 refusal controls|Source/object/behavior proof pending; absence of a word is insufficient|

## Entry-gate supported/refused/model contracts

| Named area | Evidence | Concrete contract to implement/test | Owner | Evidence status |
|---|---|---|---|---|
|DMA copy shape/timing|Guide pp96–97:8-byte alignment, length/count/stride,2 requests,8 bytes/clock peak,6–12 setup|Mask low3 address bits; length=((raw&0xfff)\|7)+1; rows=((raw>>12)&0xff)+1; skip=(raw>>20)&0xfff. Bank bit12 fixed, low12 wraps. Selected software setup12 opportunities then one8-byte visible beat/opportunity; every DRAM row/event/queue admission validated first|02.1–2|Guide shape versus exact selected software timing separated|
|Dual issue/order|Guide pp39,41–44|Opportunity ratio1:1 CPU guest tick as software policy; pair only original adjacent oneSU/oneVU when exact documented readiness relations permit; no reordering to fill slot; preflight both instructions before effects|02.1–2|Exact guide latency/hazard table transcription required before affected implementation; no guessed pipeline values|
|Stall/hazard rules|Guide pp41–44|Publish numeric ready-slot delays and branch/load-use dependency table from pinned guide, with source/destination relation and discriminating tests. Each tested hazard asserts fetched words, issue slot, stall reason and ready slot. A missing source-backed relation is an explicit limitation, never a guessed hardware result|02.1–2|Transcription pending, bounded obligation; no silicon claim|
|DMEM/IMEM contention|Guide pp96–97/134–135 single-ported banks, no exact arbiter result|Preflight complete issue opportunity and DMA beat. Same-bank simultaneous instruction/CPU/DMA conflict unsupported; refuse before any register, memory, queue, PC, event or interrupt effect. Different-bank case proceeds. Never choose an undocumented winner|02.1–2|Unknown physical arbitration preserved; modeled refusal test pending|
|Halt corruption/active IMEM overwrite|Guide p96 says overwrite at active PC undefined|Refuse running active-PC IMEM overwrite and running-PC write transactionally; no corruption bytes invented. Halted IMEM loading/start and BREAK remain accepted|02.1–2|Undefined result is not hardware checked|
|Asynchronous halt/single-step|Guide pp85–88 does not settle exact in-flight effects|Halted configuration accepted where documented; asynchronous halt/single-step request with in-flight work refused before effects and with specific reason. BREAK HALT/BROKE/optional MI effect supported; DMA progresses independently under HALT|02.1–2|Explicit supported boundary; no guessed asynchronous pipeline behavior|
|SP_STATUS/semaphore/SP_PC|Guide pp85–88,95,170|Status clear/set commands, signal bits and interrupt enable; read-to-set semaphore0→1 and explicit clear; PC masked0xffc; guide-backed command conflict precedence tested. CP0 SP0..7 supported; DP8..15 specifically refused since RDP absent|02,05.1|Documented facts versus unsupported DP isolated|
|VMOV|Guide pp74–79|Copy vt selected element into only vs-selected destination lane; preserve others, including aliases; test all element/lane mappings|05.2|Guide-supported independent byte expectations pending|
|Multiply/clamp|Guide pp36–38,60–64|Explicit unsigned masked48 accumulators; signed/unsigned product/scaling/rounding and opcode-specific clamp; check each high/mid/low slice, sign limits and both sides of saturation|06.2|Guide/independent exact-integer evidence pending|
|Reciprocal/rsqrt software algorithm|Bounded correction in05-RESEARCH.md; ares ISC pinned0b928930af4cc59ed99e942996f0d3c815379388|Exact selected-file/history/license audit before small adapted integer helper; generate lookup by integer equations/search, normalize/assemble edge+latch policy. Independent rational/square inequalities cover every index, guide-supported cases and per-instance high/low sequences|07.1–3|Permitted implementation route, not physical oracle|
|Reciprocal physical ROM/special edges|Guide lacks table contents/full physical edge result|Expose named software algorithm policy and unknown silicon evidence; no literal hardware ROM dump, host float substitute or hardware-accuracy claim|07,10|Original physical-edge limit retained|

Before the final evidence seal, check-rsp-evidence must fail on a missing
entry-gate row, an undocumented claimed hardware result, missing per-opcode
test, missing primary transcription/control, or reasonless refusal. Runtime
behavior checks remain the evidence; a complete Markdown table alone cannot
resolve these probes. Unresolved judgment flags follow AUTONOMY's soft gate
only after real automated requirements pass; concrete rights/authority/high
security/required evidence stops remain in force. No human acceptance is
inferred.

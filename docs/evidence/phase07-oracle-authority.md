# Phase07 Oracle Authority Request

**Status:** blocked; no oracle acquisition, build, or execution is authorized.
**Purpose:** prepare a bounded proposal for a future local-only comparison. This record is not differential evidence, a compatibility claim, or hardware truth.

The Phase07 authored RDP/VI behavior and raw video export were qualified before this request. VID-04 remains incomplete until competent authority and all relevant source, asset, and build-input rights are resolved. Silence, an existing project license, and comparison code do not grant authority.

The machine-readable request and per-path audit are in [phase07-oracle-authority.json](phase07-oracle-authority.json). The request digest pins every requested restriction. Both trusted decision pins in the checker are intentionally empty. A JSON field that says `approved` is not a competent decision.

## Proposed local scope

The manifest contains **216 suite-scoped native test-membership rows across 11 suites**. These identify Tendies64 suites, CTest names, binaries, and native case labels. They are not 216 prepared black-box replays. Rows include refusals and negative controls, and repeated case labels under different suites are distinct membership rows. The inventory has no per-case classification or oracle-input/output mapping.

The differential preparation record therefore marks case classification and mapping `blocked`, and the exported comparison corpus and replay adapter `missing`. The two build-only fixtures (`rdp-fill` and `rdp-fill-payload`) do not turn all native unit, refusal, and control rows into black-box inputs. A rights-cleared exported corpus, exact per-case identities/mappings, and a replay adapter must be supplied and reviewed before live execution can be approved. This task does not build an exporter or adapter.

If a later competent decision clears the full scope and those inputs are ready, the proposal is to acquire and build only outside the Tendies64 worktree under `${TMPDIR}/tendies64-oracle/phase07/<oracle-id>/<revision>/<run-id>/`. Admission resolves the current TMPDIR-backed path for both exact oracle/revision pairs and refuses if either canonical checkout/build location is inside the worktree, including through existing symlinks. Validation does not create the directory or fetch anything. A future acquisition process must repeat that check for its actual resolved run root immediately before use.

All resource limits below are **proposals only**. They do not grant execution, renew a budget, create a run window, or authorize any current process:

| Resource | Proposed cap |
| --- | ---: |
| Checkout plus build storage | 4 GiB |
| Total retained output | 1 GiB |
| Output per case | 8 MiB |
| Case count | 216 maximum; actual exported-corpus count remains unknown |
| Acquisition wall time | 900 seconds |
| Build wall time | 1,800 seconds |
| Individual case wall time | 60 seconds |
| Aggregate comparison wall time | 3,600 seconds |

On timeout, a future runner must send SIGTERM to the complete isolated process group, wait five seconds, then send SIGKILL to the group and verify it exited. It must retain the timeout record and refuse without retry. Overflow also refuses before truncating or dropping a case. Proposed output is private-only and excludes CI/repository publication.

| Oracle candidate | Exact metadata identity | Proposed recipe and current status |
| --- | --- | --- |
| [Angrylion-Plus](https://github.com/ata4/angrylion-rdp-plus) | Repository `ata4/angrylion-rdp-plus`, remote `https://github.com/ata4/angrylion-rdp-plus.git`; `v1.6` resolves directly to commit `3744ec766ed8d2c15fd5cbb4ac3c241b852b6e69`. | The pinned README metadata describes `mkdir build`, `cmake ..`, and `make`, with CMake and freeglut/OpenGL development files as prerequisites. This is proposed only; no install, build, or execution is authorized. The revision contains `MAME License.txt` (blob `622420419a88127f87c8ad8eadd96ce5c9293312`, SHA-256 `455a30b92aba9a8e17702e6458dae6ca78b7e90b83800182fba9c4285ec2d385`). Its notice has non-commercial and source-condition terms. This observation is not a legal conclusion. Per-file source and generated build inputs remain unaudited. |
| [parallel-rdp-standalone](https://github.com/Themaister/parallel-rdp-standalone) | GitHub API full name `Themaister/parallel-rdp-standalone`, remote `https://github.com/Themaister/parallel-rdp-standalone.git`; observed `master` pinned as commit `388d70f5835b352d841d9d9e5a08c5de01470f41`. Its `COMMIT` metadata points to the separate upstream `parallel-rdp` library revision `1cecd042b2619bc505c12bfdc713808386f2b54d`. | This is a standalone source bundle, not an identified executable or test harness. Its complete metadata tree has 198 entries and no gitlink; root files are `COMMIT`, `LICENSE`, `README.md`, and `config.mk`, with no root CMakeLists or Makefile. `config.mk` is an integration source list, not a self-contained recipe. The README's generic parent-project route recursively fetches Angrylion-Plus and Granite and is excluded. There is no proposed build command until an exact wrapper and its complete input closure are established. |

The repository API reports MIT for the standalone bundle's root license. That root classification does not clear individual source files, generated shaders, tests/data, or transitive build inputs. Shader material includes `slangmosh.hpp` and `slangmosh.json`; generator and source provenance remain unresolved. The audit therefore blocks `parallel-rdp/**`, generated shader paths, Vulkan/util/volk/header inputs, and the compiler/include closure. Upstream tests, conformance data, fuzz vectors, and expected outputs are excluded. No implementation or header source was downloaded or inspected for this request.

Revision identity checks used patch-free Git database commit/tree metadata endpoints, repository metadata, and `git ls-remote` against the exact repository URLs above. No patch-bearing commit endpoint, implementation source, header, patch, or diff was fetched, stored, or exposed.

## Rights and authority boundary

The exact rights-audit rows and evidence status are in the JSON record. Its `complete` field is false. The Angrylion-Plus notice needs an actual owner/legal decision, and its implementation/generated build-input closure has not been audited. The standalone candidate has only root-license metadata; its per-file, generated, test/data, and transitive build inputs remain blocked. The project root's MIT license is not blanket clearance for any of those inputs.

No code or asset acquisition, oracle build/run, copy, upstream test/data use, package installation, private guest/boot use, physical-console test, paid operation, or publication is authorized. Any later local decision must come from competent owner/legal authority and bind both exact revisions, the complete operation list, full request digest, exact exported corpus/mapping/replay adapter, complete cleared rights audit, and a bounded expiry. The proposed storage, output, time, and cleanup limits are part of the request digest. An agent-created record or synthetic self-test decision cannot supply the real decision pin.

Publication is separate, not requested, and remains `not-authorized`; any oracle-derived bytes or expectations stay private under this request. A local-use decision cannot imply a publication grant. The publication decision also binds the full request digest and requires its own trusted source pin.

## Checker contract

Run `python3 -B scripts/check-rdp-oracle-authority.py --selftest` for authored validator controls and `--expect-blocked` to validate this exact prepared state. `--require-approved` is the live pre-use gate and must refuse while the trusted local authority pin is unset.

The validator binds local and publication decisions to the canonical digest of every request field, not just the corpus and rights rows. It rejects missing or duplicate JSON fields, changed request/audit pins, incomplete corpus identities, partial oracle or operation scope, revision mismatch, expired grants, unresolved rights rows, unready build recipes, unprepared comparison inputs, in-worktree private roots, workstation paths anywhere in nested text, and synthetic/test-domain authority. It resolves and validates the proposed storage root without creating it. The synthetic approved control supplies a complete authored-only request, audit, decision, and temporary test pin through `_require_local_approved`; production pins remain unset and the CLI cannot accept that test context. Publication is checked separately and cannot inherit a local grant.

Passing these controls proves only that the prepared record is internally bound and the current stop is enforced. It does not authorize oracle use, produce differential evidence, or complete VID-04.

## Fixture manifest binding (identity-only rebind, 2026-11-24)

Phase 10 (GATE-02) changed how the request binds `fixtures/manifest.json`. The request no longer pins the whole file. It pins each of the 39 manifest rows present at the rebind by its own canonical hash (`corpus.fixture_manifest.bound_rows`), looked up by row id, never by position. A missing, duplicated or changed bound row, or an empty bound set, refuses. Rows appended later are outside this request and no longer force a rebind. The file is no longer an entry of the bound authored inputs.

The request digest was rebound once, identity-only, under the owner approval recorded in `.planning/STATE.md` (2026-11-23, per-row binding plus a standing identity-only rebind): old `a4bb7774b72321261e37a44bbb518161b9b564e4beb076dfebe06881aab67733` (reproduced from the committed record before editing), new `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab`. Only the manifest identity fields, the derived request digest and `PINNED_REQUEST_SHA256` changed. Both trusted decision pins remain unset, `local_decision` stays blocked and publication stays not authorized. This rebind grants no oracle, rights or publication authority.

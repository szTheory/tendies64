# Phase07 Oracle Authority Request

**Status:** blocked; no oracle acquisition, build, or execution is authorized.  
**Purpose:** prepare a bounded local comparison request for the authored RDP/VI corpus. This record is not differential evidence, a compatibility claim, or hardware truth.

The machine behavior and raw video export were qualified in the authored Phase07 work before this request. VID-04 remains incomplete until the exact comparison scope and all relevant source, asset, and build-input rights are resolved. Silence, an existing project license, and the existence of comparison code do not grant authority.

The machine-readable identity and per-path audit are in [phase07-oracle-authority.json](phase07-oracle-authority.json). The checker pins the canonical request and audit digests, the corpus file SHA-256 values, and the distinct Git blob ID values. The trusted local-decision and publication-decision pins in the checker are intentionally empty. A field that says `approved` in JSON is not a competent decision.

## Proposed local scope

The request proposes out-of-tree acquisition, a bounded build using only already available tools, and black-box execution on the 216 cases in `docs/spec/rdp-vi-cases.json` (11 suites). The corpus identity also includes the fixture manifest and authored RDP/VI producer and test sources listed in the JSON. Only the two authored build-only fixtures `rdp-fill` and `rdp-fill-payload` are in scope. Their manifest rights remain distinct from rights in either oracle.

If a future competent decision clears the full scope, proposed storage is private, mode `0700`, under `build/private/phase07-oracle/<oracle-id>/<revision>/<run-id>/`. Checkout plus build is capped at 4 GiB; all retained output at 1 GiB; each case at 8 MiB; and execution at 216 cases. Overflow refuses the run before truncating or omitting a case. The request retains exact inputs, raw comparison bytes, per-case metadata, command/process records, and bounded logs. It does not authorize CI use or repository output.

| Oracle candidate | Exact metadata identity | Proposed recipe and current status |
| --- | --- | --- |
| [Angrylion-Plus](https://github.com/ata4/angrylion-rdp-plus) | Repository `ata4/angrylion-rdp-plus`, remote `https://github.com/ata4/angrylion-rdp-plus.git`; `v1.6` resolves directly to commit `3744ec766ed8d2c15fd5cbb4ac3c241b852b6e69`. | The pinned README metadata describes `mkdir build`, `cmake ..`, and `make`, with CMake and freeglut/OpenGL development files as prerequisites. This is proposed only; no install, build, or execution is authorized. The selected revision contains `MAME License.txt` (blob `622420419a88127f87c8ad8eadd96ce5c9293312`, SHA-256 `455a30b92aba9a8e17702e6458dae6ca78b7e90b83800182fba9c4285ec2d385`). Its notice has non-commercial and source-condition terms. This observation does not decide whether local use is permitted. Per-file source and generated build inputs remain unaudited. |
| [parallel-rdp-standalone](https://github.com/Themaister/parallel-rdp-standalone) | GitHub API full name `Themaister/parallel-rdp-standalone`, remote `https://github.com/Themaister/parallel-rdp-standalone.git`; observed `master` pinned as commit `388d70f5835b352d841d9d9e5a08c5de01470f41`. Its `COMMIT` metadata points to the separate upstream `parallel-rdp` library revision `1cecd042b2619bc505c12bfdc713808386f2b54d`. | This is a standalone source bundle, not an identified executable or test harness. Its complete metadata tree has 198 entries and no gitlink; the root has `COMMIT`, `LICENSE`, `README.md`, and `config.mk`, but no root CMakeLists or Makefile. `config.mk` is an integration source list, not a self-contained recipe. The README's generic parent-project route recursively fetches Angrylion-Plus and Granite and is excluded. There is no proposed build command until an exact wrapper and its full input closure are established. |

The repository API reports MIT for the standalone bundle's root license. That root-level classification does not clear individual source files, generated shaders, tests/data, or transitive build inputs. The tree names shader material including `slangmosh.hpp` and `slangmosh.json`; its generator and source provenance remain unresolved. The audit therefore marks `parallel-rdp/**`, generated shader paths, Vulkan/util/volk/header inputs and the complete compiler/include closure as blocked. Upstream tests, conformance data, fuzz vectors, and expected outputs are excluded. No implementation or header source was downloaded or inspected for this request.

Revision identity checks used the patch-free Git database commit/tree metadata APIs (`/repos/ata4/angrylion-rdp-plus/git/commits/<sha>` and `/git/trees/<sha>`), repository metadata, and `git ls-remote` against the exact repository URLs above. The patch-bearing `/repos/.../commits/<sha>` endpoint was not used. No implementation source, header, patch, or diff was fetched, stored, or exposed.

## Rights and authority boundary

The exact rights-audit rows and evidence status are in the JSON record. Its `complete` field is false. The Angrylion-Plus notice needs an actual owner/legal decision, and its implementation/generated build-input closure has not been audited. The standalone candidate has only root-license metadata; its per-file, generated, test/data, and transitive build inputs remain blocked. The project root's MIT license is not treated as a blanket clearance for any of those inputs.

The current local decision is exactly blocked. No code or asset acquisition, oracle build, oracle run, upstream test/data use, package installation, private guest/boot use, physical-console test, or paid operation is authorized. Any later local decision must come from a competent owner/legal authority, bind both exact revisions, the complete operation list, the exact corpus and complete cleared rights audit, have a verifiable evidence digest and a bounded expiry, and be explicitly pinned in the reviewed checker source. An agent-created record or synthetic self-test decision cannot supply that pin.

Publication is a separate scope. It is not requested here and remains `not-authorized`; any oracle-derived bytes or expectations stay private under this request. A later publication decision requires its own competent evidence and separate checker pin. A local-use decision cannot imply a publication grant.

## Checker contract

Run `python3 -B scripts/check-rdp-oracle-authority.py --selftest` for authored validator controls and `--expect-blocked` to validate this exact prepared state. `--require-approved` is the live pre-use gate and must refuse while the trusted authority pin is unset. The validator rejects missing or duplicate JSON fields, changed request/audit pins, incomplete corpus identities, partial oracle or operation scope, revision mismatch, expired grants, unresolved rights rows, unready build recipes, and synthetic/test-domain authority. Its synthetic approved control exists only inside self-tests and cannot pass the live gate. Publication is checked separately and cannot inherit a local grant.

Passing these controls proves only that the prepared record is internally bound and the current stop is enforced. It does not authorize oracle use or make VID-04 complete.

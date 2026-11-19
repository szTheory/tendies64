# Current checkpoint: source review fixes before qualification

Task2 wrapper implementation is committed as9c5d0c8d77b780ad9f27c89f02f6bf9c620160f5.
Native, exact hosted collection and cohesive final verification modes now exist.
Focused controls, mocked collector, combined layout rights/privacy and syntax
checks pass. No full current-source normal/ASAN/GCC qualification, live hosted
collection or positive final verification has run.

Independent deep review of all44 changed source files is complete in
[08-REVIEW.md](08-REVIEW.md). CR-01 blocks publication: destination-parent stage
name can be replaced between inode check and name-based no-replace rename.
Fix with a trusted private staging parent and rename from its retained FD;
another pre-rename check is insufficient. WR-01: observation save errors are
ignored and verifier accepts any nonempty audio record; enforce checked saves
and full schema/packet validation, with truncated/resealed controls.

Next actor: Codex fixer, then independent focused source recheck. Full
qualification remains stopped on these concrete findings; no user action is
needed. Preserve original report and evidence, owner edits and all existing
rights/oracle/expired-window/release stops. After fixes/recheck, run cheap
actual prerequisite/source/consumer checks before one coherent full native
normal/ASAN/pinned-GCC13 run and exact two-host CI/collection. Normal gate now
supports unique T64_GATE_JUNIT_OUT; native wrapper reuses full suites and does
not duplicate focused audio CTest execution.

The previous Task1 checkpoint below is historical; its 'not implemented' mode
status has been superseded by the implementation and review above.

---

# Phase08 continuation: qualification implementation

The autonomous phase remains active; no user action is needed. Plans01–04 are
committed with focused normal/instrumented evidence and summaries. Plan05 Task1
is committed; do not recreate it or restart the plan from its beginning.

Task1 commits: ff52d52 (RED witness),2f0e2ff (host-error canonical-state witness
and inventory),a902d31 (verifier, nine controls, safe CI/archive integration).
Focused exact qualification suite passes4/4, with owning private JUnit
`build/gate/08-05-task1-final-qualification-1795043132032804000-41741-qualification-junit.xml`.
Parent spot-check parses it and fresh source/body/object guard passes81 sources.

The local `--emit-hosted --job local` and retained-record `--verify-evidence`
paths pass and explicitly label entry-path validation. They do not qualify the
current source or prove Linux/hosted execution. Original producer source and
binary identities are retained, never relabeled. Original records/projection
smokes remain under `build/gate/08-05-audio-private/` and scoped evidence roots.
The isolated nine-control harness needs no old local evidence and runs no nested
CTest/build. Combined archive selftests validate only the explicit audio
manifest, preserve historical no-audio boot/RDP archives and reject extras.

Task1 also corrected two invalid Phase08 audio fixture manifest rows: pin actual
reachable source commit fb1410892e05fd24a27a5d1b5d229e6bedc9218b with verified
source bytes; use schema-supported expected.kind and preserve wrong-sample
comparator semantics. The manifest checker reports24 entries/0errors; staging
rights scan passes. No schema/scanner/rights policy was relaxed.

## Next actor and order

Parent continues authorized work. The full `--native`, `--collect-hosted`, and
cohesive final `--verify-evidence` modes are NOT implemented yet. Finish those
Task2 source wrappers and focused controls without running full native/CI.
Then perform one independent review of the complete changed source, including
AI/core, runner/capture, verifier/collectors, fixture ledger and workflow; fix
accepted findings and perform bounded relevant retests. Only after review may
the owning final native normal/ASAN/GCC13 qualification run, followed by exact
source hosted jobs/actual Linux+macOS witnesses and authenticated collection.
This sequencing completes source before the planned review, avoiding repeated
broad review. It does not remove the review-before-qualification gate.

Preserve qualified binaries/bindings before replacement. Use unchanged limits:
CTestjobs4, hosted2jobs/10minutes, existing pinned offline/read-only GCC13 image.
No raw ROM/PCM/video upload; only the scanned bounded projection. No new
packages/caches/concurrency or expired-window/oracle authority.

Root owns final SUMMARY/state/roadmap/requirements. Plan05 has no complete
SUMMARY and Phase08 is not complete. Independent security/goal verification and
validation/claims remain required. Phase7/VID04 remain stopped; release/rights/
physical stops persist. Main remote remains the prior green source until final
reviewed/tested Phase08 push; no newer hosted pass exists yet.

Owner edits remain unstaged: AGENTS.md gitblob
7e6fc7df892d2b8f5c47632e7faf5d02b7471730; .planning/config.json gitblob
ded586b5c0e5f32230a8bcb31e09463e184d9a05. Do not edit/stage/revert them.
Sequential root guard build/gate/07-root-pin.sh before writes/every commit.
Original plan base .git/gsd-plan-head-before-08-05 remains97d6b1b for full diff;
continuation starts aftera902d31 plus routine checkpoint docs.

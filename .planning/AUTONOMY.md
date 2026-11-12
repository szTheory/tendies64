# Autonomous operation policy

**Owner direction:** 2026-11-09. The owner wants Tendies64's OpenGSD work to
proceed autonomously after project kickoff, with a rolling roadmap and each
milestone run in autonomous mode. `AGENTS.md` is the project's standing
engineering authority. This file explains how the configured GSD workflow
applies that direction; update it when the owner changes policy.

## 1. Delegated workflow

- During `$gsd-new-project --auto`, research the project and create a rolling
  roadmap that preserves the ambitious emulator goal while sequencing
  verifiable milestones.
- For every milestone: discuss → decision-focused fan-out → synthesize and
  adversarially check recommendations → plan → execute → verify → audit and
  close. Use `$gsd-autonomous` for the milestone's remaining phases. Refresh
  project state and the rolling roadmap as evidence arrives.
- After a completed milestone, continue to the next valuable milestone using
  the installed GSD new-milestone flow, then run its phases autonomously. Do not
  stop for an acknowledgment at ordinary phase or milestone boundaries.
- Self-unblock routine decisions by researching credible options, selecting a
  reversible evidence-backed default, recording rationale and reversal
  evidence in project-owned decisions, and continuing. Batch genuinely
  necessary owner decisions.
- Execution efficiency (owner feedback, 2026-11-10): reuse approved contracts
  and begin the smallest verifying implementation promptly. Reopen research or
  design only for a concrete contradiction or failure. Keep handoffs concise;
  repeat checks only after relevant changes or an unresolved failure warrants
  them. Base progress and elapsed-time claims on commits, test results and
  timestamps rather than perceived activity.
  Once implementation checks pass, close the summary and state promptly; do
  not repeat broad tests or scans solely for bookkeeping changes. Report the
  specific remaining command if a handoff stalls.
  After fixing a failed stage, also execute required later stages that the
  failure prevented from running. An aborted gate provides no evidence for
  its unreached tail; those first executions are not duplicate verification.
  Keep routine summary/state closeout with the orchestrator when execution
  evidence is already saved. A slow bookkeeping handoff is not a reason to
  redispatch implementation or repeatedly recheck a passing hosted pair.
- At each discussion, enumerate actual decision points. Fan out one agent per
  independent decision cluster when it saves time/context. Apply relevant
  CPU/cache/virtual-memory, RSP/microcode, RDP/video, audio, memory/bus,
  accessory, QA/security, product/API, performance and DevOps lenses. Compare
  tradeoffs and failure modes, include an adversarial pass, and stop when more
  research no longer changes the recommendation, risk or evidence plan.
- “Another copy-paste is better than another dependency” is the default;
  document provenance and maintenance ownership for copied code.

## 2. Verification and CI

- Generated GSD state projection `.planning/state.json` stays git-ignored. The
  owning state commands maintain it during routine bookkeeping; preserve human
  edits to configuration/instructions, rather than restoring an obsolete cache
  after a legitimate state update. Do not commit the projection.

- Workflow application (2026-11-10): when installed GSD instructions conflict
  about judgment-tier prohibitions, apply their specific autonomous-mode soft
  gate: report non-authoritative verdicts and every flag prominently, and
  complete with those flags when all automated phase requirements pass and no
  concrete stop is unresolved. A generic human-review recommendation alone is
  not an acknowledgment gate. Preserve the original report and routing rationale;
  never record human validation or risk acceptance that did not occur. Concrete
  rights, authority, physical, publication and evidence gaps still stop work.

- Aim for zero human verification where tools can give useful evidence. Shift
  checks left to component, seam, integration, headless guest and installed
  consumer boundaries as each exists. Throwaway local verification is fine.
- Use deterministic input, fresh isolated state, semantic guest-progress
  assertions, exact test inventories and meaningful negative controls. Keep
  guest time, video, audio and host presentation as distinct evidence. Report
  missing, skipped, timed-out, harness-failed and guest-failed results
  separately.
- Sanitize captured compiler/test evidence before its first commit. Raw JUnit
  and logs can embed checkout paths, temporary paths and hostnames; retain
  portable command/assertion/result records and keep raw captures local.
  Apply the same rule to specifications and source citations: use the title,
  edition, pages, public URL and verified digest, never the workstation cache
  path. Fix a newly generated private-path finding rather than adding a waiver.
  If already committed but still unpublished, repair only those local commits
  while preserving owner edits; the public-history stop remains unchanged.
- Validate a RED run at its first failed behavioral assertion and observed
  values, not merely a well-formed TAP group or a validator's acceptance. A
  setup/addressing failure does not prove the intended defect. Correct the
  setup, demonstrate the intended failure against the pre-feature behavior,
  then confirm GREEN; disclose invalid attempts and reconstructed controls.
  Check newly generated evidence for private paths before committing it.
  A hardcoded false placeholder is not RED evidence. Verification-only work
  over already implemented behavior may start green; remove an inapplicable
  TDD marker with a recorded rationale instead of manufacturing a failure.
  A task explicitly marked tdd=true must capture and classify its actual
  pre-implementation report even when the plan type is execute and the global
  TDD setting is off. Semantic notes alone do not replace that formal guard.
  If the guard was missed, preserve the deviation and label any later isolated
  regression controls as current evidence, never as original RED history.
- A test ROM's repository license does not automatically grant redistribution
  rights for its generated ROM or embedded content. Keep a fixture manifest
  with source/revision, build recipe/toolchain, digest, boot/profile
  requirements, rights, notices and expected-result provenance.
- Add a CI job only for a distinct recurring failure class at a useful wait and
  maintenance cost. Keep one canonical fast local command aligned with the fast
  pull-request gate. Long timing/stress suites and noisy benchmarks belong in
  scheduled or release lanes until their recurring value is shown.
- A guarded distribution-package install still needs its declared legitimacy
  audit before first use. Record package/provider, official provenance,
  acquisition trust, purpose and lifetime then; a later audit must disclose the
  missed pre-use check rather than claim it occurred.
- After every CI run and release workflow, inspect measured critical-path
  delays, duplicate setup/builds, fetches, cache behavior, concurrency and
  matrix cost. Take low-risk wins only when distinct evidence stays intact; log
  the finding so it is not rediscovered at every milestone.
- Benchmark named workloads/builds with correct output held constant. Report
  emulator throughput and end-to-end latency separately from CI queue/setup/
  build/test time.

## 3. Milestone close and release

At each milestone close, audit against the goal and evidence, update the rolling
roadmap and lessons, keep main green, triage pull requests, and target a release
from the exact verified artifact when ready. GSD milestone numbers and
software versions are separate. Once repository visibility, project license
and release channel are decided, routine releases may proceed without a new
approval each time when content rights, tests, package readiness and authority
pass. A blocked release must retain its specific owner and next action without
blocking unrelated authorized work.

## 4. Explicit owner stops

- Copyrighted/protected ROM, IPL/PIF/firmware, 64DD media, commercial game
  content, saves, or test-fixture use/redistribution without documented rights.
- Physical console measurement or testing with owner-held hardware or files.
- PII, credentials, private paths, destructive public-history rewrites, or
  deleting a repository/release.
- Paid services beyond a clearly authorized budget.
- The release channel remains an explicit project decision. Research and local
  implementation continue while it is open; after the owner resolves it, follow
  the release policy above without per-release prompts.
- Resolved by the owner on 2026-11-09: the project license is Apache-2.0, and
  the repository is public under the szTheory GitHub account (creation and first
  push authorized). Routine pushes to it proceed without new approval. The
  PII, credential, private-path and rights stops still apply to every push.
- Any new action outside delegated scope that materially changes project goals,
  rights, authority, risk or cost.

Never ask for a routine acknowledgment or approval already granted in this
policy. Preserve hard stops and keep their concrete blocker, consequence, next
step and responsible actor visible.

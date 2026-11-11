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
- At each discussion, enumerate actual decision points. Fan out one agent per
  independent decision cluster when it saves time/context. Apply relevant
  CPU/cache/virtual-memory, RSP/microcode, RDP/video, audio, memory/bus,
  accessory, QA/security, product/API, performance and DevOps lenses. Compare
  tradeoffs and failure modes, include an adversarial pass, and stop when more
  research no longer changes the recommendation, risk or evidence plan.
- “Another copy-paste is better than another dependency” is the default;
  document provenance and maintenance ownership for copied code.

## 2. Verification and CI

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
- A test ROM's repository license does not automatically grant redistribution
  rights for its generated ROM or embedded content. Keep a fixture manifest
  with source/revision, build recipe/toolchain, digest, boot/profile
  requirements, rights, notices and expected-result provenance.
- Add a CI job only for a distinct recurring failure class at a useful wait and
  maintenance cost. Keep one canonical fast local command aligned with the fast
  pull-request gate. Long timing/stress suites and noisy benchmarks belong in
  scheduled or release lanes until their recurring value is shown.
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

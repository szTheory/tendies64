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
  On a shared branch, a parent documentation commit can contain unfinished
  child RED commits in its ancestry. Defer its push until those source/test
  commits are verified; pin pushes to the intended verified commit.
- Efficiency correction (owner feedback, 2026-11-12): before a broad retest,
  inspect `node scripts/verification-scope.mjs --base <last-tested-source>`.
  It includes staged, unstaged and new source changes, including ignored code
  in production directories. Its output is advisory: required first-time
  checks, dependency effects, rights/publication gates and private input/tool
  qualification still apply. It does not inspect private build inputs.
  `T64_BUILD_ID` already hashes production content and compiler/build policy;
  `T64_SOURCE_REVISION` is a separate provenance annotation. A metadata-only
  commit changing that annotation is not a reason to rerun the full core gate
  or admission fuzzing. Refresh an annotation or artifact-identity check when
  needed, preserve the actual tested artifact identity, and reuse unaffected
  semantic evidence. Do not relabel an old binary as newly tested.
  Executors own implementation and substantive evidence acquisition; the
  orchestrator writes routine SUMMARY/state/roadmap closeout from the saved
  commands, results, commits and limitations in one batch. Review reports,
  private qualification records and required evidence are substantive work,
  not optional bookkeeping. Name the complete audit scope at dispatch,
  including the current verification plan's own evidence threats, and schedule
  final artifact checks explicitly so they do not appear as a late new audit.
  Reserve the shared Git index only immediately around an actual mutation and
  explicitly release it afterward; an intended future commit is not a lock.
  For private qualification, audit source/tool/lock and excluded asset closure
  before the qualified build; scan actual ELF/ROM composition after building
  and before execution. Never require an output hash before its output exists,
  and never use that staging distinction to bypass a pre-use rights check.
  Before freezing a diagnostic inventory contract, derive default feature and
  level selection from the pinned executor, including aliases, repeated names
  and zero-value semantics. Confirm the selected source can populate the
  promised groups before implementing a completeness rule.
  For changed refusal matrices, prove an accepted authored baseline, fail on
  setup/write errors, and assert that every intended case reaches its specific
  refusal. A green test process alone does not establish that its cases ran.
  Preserve and withdraw invalid coverage claims rather than rewriting their
  history; repair the cause before bounded retesting.
  Qualify dependency inventories with a complete partition of every lock entry:
  the exact pinned root and fully audited supported packages, with everything
  else refused. Filtering one source kind at a time is not a complete audit;
  absent source fields can represent external path packages too.
  Persist acquisition URLs, digests, commands, observed results and reviewed
  hook/license facts when an input is first audited. Adapt that saved record to
  a consumer's receipt format later; do not defer evidence capture and then
  repeat unchanged network/source audits to reconstruct it.
  First-use correction (owner's repeated elapsed-time feedback): once concrete
  pre-use source, tool, asset and dependency checks pass, prioritize one bounded
  actual build, artifact check and startup before broadening integration
  automation. Guard and record that exact command; do not make optional wrapper
  abstractions or report prose an additional first-use gate. Integrate the
  reusable wrapper from observed needs in cohesive reviewed slices. Required
  checks and explicit stops still apply. Recheck saved accepted inputs through
  corrected guards without reinstalling or regenerating unchanged inputs.
  After an actual qualified tool startup fails, inspect its runtime search paths
  and already-audited library closure before acquiring or reinstalling tools.
  Prefer a command-scoped fix that preserves the audited binaries and libraries.
  Reuse unchanged qualified build objects for a link-only retry while preserving
  per-attempt logs, lock checks and final artifact identities; ordinary build-tool
  reuse does not require a new cache system or a full dependency rebuild.
  Completion-cost correction (owner's elapsed-time sanity check, 2026-11-13):
  measure time spent after the guest has completed before adding concurrency.
  Treat a run limit as an upper bound when the owning contract permits an
  earlier authenticated completion. Preserve the shared result parser and
  prove the terminal loop through retirement observations; test incomplete
  transcripts and an altered delay slot before removing post-completion work.
  Compare exact canonical result bytes and save before/after measurements.
  Do not weaken refused CPU operations or required membership to improve
  throughput or make qualification complete.
  For subsequent subsystem phases, schedule the qualified tool/source seam and
  a small actual guest attempt before a long native implementation sequence.
  Preserve all pre-use checks, but use an observed startup/refusal to guide the
  next slice instead of postponing the first external integration until the
  subsystem's final plan. Keep expired deadlines authoritative even if a
  process exits successfully during cleanup.
  Authored CI controls must work in a clean workspace without private diagnostic
  directories or inputs. Give synthetic fixtures their own restrictive temporary
  storage under the ignored build tree, and verify that clean setup explicitly;
  a populated developer workspace is not evidence of CI independence.
  Keep long sanitizer/fuzz work separate from wall-capped actual diagnostic
  runs on the shared host. A 2026-11-13 overlapping attempt hit a wall bound;
  the same unchanged case completed in 0.21 seconds after those jobs finished.
  Host contention is an inference, not proof of the cause. Preserve the failed
  attempt and limits, diagnose one bounded case, then serialize the workloads
  for the next justified attempt rather than widening deadlines.
  For focused native CTest selections, use the already approved gate/asan
  test presets and their four-job policy when no wall-capped diagnostic is
  active. A bare --test-dir selection silently runs serially: Plan05-02's
  final instrumented selection spent121s with a54s longest test. This does
  not justify extra concurrency, caches or dependencies; reuse the existing
  preset rather than accidentally overriding it. Keep owning test selection,
  all-target builds, sanitizer environment and exact inventory checks intact.
  Foreground review correction (2026-11-13): if a handoff stays silent,
  request a concrete checkpoint and remaining action. If it is then stopped,
  reconcile the owning durable report/status before resuming, preserve any
  partial evidence, and label an inline fallback as such. Do not redispatch
  the same full review or repeat passing gates merely because no report was
  returned. A timeout or interrupted reviewer never supplies a passing verdict.
  Check attributable artifact and commit progress before interrupting an
  executor for chat silence; productive durable work is not a stall. Preserve
  completed evidence and resume the remaining action after an interruption.
  A Plan05-04 Clang-clean macro failed GCC warnings-as-errors in hosted CI.
  For changed C translation units, use the existing gcc:13 image for a bounded,
  read-only, network-disabled warning check before publication when its headers
  match the owning target. Keep hosted checks; do not install another compiler
  or expand a libc-mismatched probe to unrelated unchanged host code.
- At each discussion, enumerate actual decision points. Fan out one agent per
  independent decision cluster when it saves time/context. Apply relevant
  CPU/cache/virtual-memory, RSP/microcode, RDP/video, audio, memory/bus,
  accessory, QA/security, product/API, performance and DevOps lenses. Compare
  tradeoffs and failure modes, include an adversarial pass, and stop when more
  research no longer changes the recommendation, risk or evidence plan.
- “Another copy-paste is better than another dependency” is the default;
  document provenance and maintenance ownership for copied code.

## Approved CPU compatibility target

Owner direction, 2026-11-13: follow the recommendation to add a separate,
versioned software-compatibility CPU profile and use it for the required CPU
suite completion gate. Preserve the current strict profile, its identity and
refusal behavior. This authorizes the associated fields, versioning, runner
selection, tests, documentation, reviews and bounded diagnostic retests without
per-slice approval. Software expectations and chosen undefined-operation
policies do not establish silicon behavior. Required membership, authenticated
pass/fail completion, resource caps, rights and publication stops remain intact.

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
  For CPU instruction tracers, confirm the intended fetched opcode and inspect
  Cause, full PC and attempt outcome before accepting a result mismatch as
  behavioral RED. An unchanged destination plus non-retirement can be an
  earlier fetch/address fault. Use canonical sign-extended KSEG addresses in
  32-bit kernel fixtures; do not weaken production address validation to make
  a malformed fixture execute.
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

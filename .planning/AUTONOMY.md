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
  Within one planned behavior task, reuse its formal RED milestone while
  adding the remaining source-backed matrix. Retain intermediate failures,
  but do not create another GSD classifier/commit/build cycle for each register.
  Keep tests before the affected behavior and verify cohesive source changes.
  Retain owning CTest logs before inventory discovery or another CTest command;
  discovery also replaces LastTest.log. Capture command output directly and
  verify retained hashes before relying on the archive at closeout.
  Give each manually dispatched aggregate owner CTest batch a unique --output-junit path as a persistent
  backup, and still copy LastTest.log immediately on both success and failure
  before any later CTest/discovery. Plan08 lost a normal aggregate despite
  manual-copy instructions. Preserve that historical gap; the required final
  integrated gate supplies new evidence, not a reconstructed old pass.
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
- Sequential context reuse: a completed typed executor may be reused for the next
  ready plan when the resolved role/model/effort and checkout remain appropriate.
  Treat it as a fresh plan dispatch: recheck completion/readiness, capture the new
  base, record plan-scoped isolation, supply new ownership, read current mutable
  instructions and use the supplied-root guard before writes/commits. Never reuse
  a working executor for a different task or infer completion from an interim
  message. Explicitly retire task-only restrictions when moving from a
  background test repair to a foreground full-plan executor: state the new file
  and shared-build ownership and that the orchestrator is waiting. A stale
  no-shared-build restriction caused an unnecessary stable-point request in
  Plan08 despite the new full-plan authorization. This reduces repeated setup without weakening verification.
- Incremental review efficiency (owner's standing efficiency direction): verify
  the exact shared predicate and one accepted consumer operation before treating
  a static review allegation as a blocker. Distinguish publicly reachable cases
  from explicitly injected defensive states. After independent production review,
  a test-only correction may receive an orchestrator diff/evidence recheck and
  focused normal/instrumented verification; do not repeat the whole source audit
  by default. Preserve the original finding and identify who performed recheck.
  A plan file list is an ownership estimate, not a reason to omit necessary
  narrowly scoped wiring within an already approved behavior/policy. Record the
  dependency correction and its actual accepted-consumer evidence.
- Raw-command conformance preflight: before a decoder implementation expands,
  pin its primary field table and at least one literal valid producer packet
  independently of the new encoder/decoder helpers. Include mode controls and
  nonzero/signed fields that discriminate plausible wrong layouts. Where a
  primary table/diagram conflicts, resolve and record that before decoding.
  A helper and sampler agreeing on pixel output does not validate the input
  contract. Use fixed raw-word regressions before broad suites; retain and
  withdraw invalid coverage claims when this check finds a shared mistake.
  Report full CTest-preset results separately from the full gate wrapper; run
  any required unreached lint/self-test tail rather than relabeling the preset.
- Cheap consistency gates (owner feedback, 2026-11-15): before a long process
  or suite, run fast checks of setup, identity, manifest schema/rights, active
  profile/device wiring
  and representative accepted operations through the actual consumer seam.
  Negative refusals alone do not prove a newly added profile can use an
  existing device. Run those cheap checks before instrumented/full suites;
  retain required checks and limits rather than adding retries or concurrency.
  Decode representative instruction fields from pinned bytes before using a
  derived address as a diagnosis: LUI fills the upper half, and the Phase06
  PI source-offset inference was wrong until checked against runtime values.
  Capture the first exception and its preceding retirement; a later fault in
  an empty exception vector can overwrite the diagnostic pending exception.
  If a delegated investigation stops producing concrete changes, reconcile
  its saved evidence and continue the narrow remaining slice inline rather
  than extending a silent handoff or restarting the whole task.
  Exercise each new subprocess/build helper with a cheap actual authored
  startup and deadline control before the expensive command; syntax-only
  checks missed a missing clock import in the combined BASE builder. Prefer
  `ast.parse` for Python syntax checks: explicit `py_compile` can write bytecode
  despite `-B`, and generated binary files in source directories correctly fail
  the rights scanner. Remove only attributable generated outputs, then recheck.
  Before a diagnostic sweep, prove a successful ROM admission and a single
  canonical tick with the real adapter and a cleared authored input. Refusal-only
  controls missed its host allocator passing byte alignment directly to
  `posix_memalign`; normalize valid core alignment requests to the host minimum.
  Evidence-checker correction: an authored acceptance test must exercise the new
  checker, and test discovery must list every promised mutation. A reduced
  inventory test must reseal context, binding and snapshot too before claiming
  independent-denominator coverage. Bind public results to original command,
  outcome and artifact bytes, never parse a report as result authority. Exercise
  actual saved formats early; handwritten parsers missed unquoted CTest names,
  compact success summaries and the different Tier A/B protocol observations.
  When a handoff has identified enough evidence but produces no runnable patch,
  reconcile its exact diff and finish the narrow remaining slice inline.
  After adding a production file or changing a sealed dispatch function, run
  its exact source/object/body inventory checker before the full gate. Phase06's
  final gate found a stale pre-RDRAM no-HLE roster and two SP seals; keep those
  guards strict, review the concrete changes, and prove mutations still refuse.
  New-descriptor identity check: before broad qualification, mutate every
  applicable policy/boot field through public hash, serialization and known-ID
  canonical construction. Test empty/unterminated policy text and pin retained
  older-profile bytes. Lookup and accepted rendering alone missed descriptor9
  falling through branches that stopped at8; keep version membership explicit.
  For latched device configuration, vary the actual timing and mode fields,
  not only the framebuffer origin. Check consecutive events after the latch,
  immediate-register exceptions, and sampled readback before the next line.
  Keep output-packet capacity separate from addressed source dimensions;
  interlaced rows and source offsets must be bounded by the actual RDRAM span.
  Filter neighborhoods must also distinguish the visible source window from
  valid backing RAM: include a positive one-row window whose only useful tap
  is in the next in-RDRAM row, plus an actual out-of-RDRAM fallback. Equivalent
  helper calls are not a whole/split advance test; make one side issue one
  public bound across multiple fields. Every unexpected test-setup return must
  record a failure before skipping pixel/metadata assertions.
  Before committing public evidence/documentation, run the cheap tree PII
  scan as well as rights/provenance checks. Express local commands with $PWD
  or repo-relative paths. Plan09's evidence commit passed rights/provenance
  but retained real workstation paths; the later history scan then required
  an owner decision even though the commit had never been published.

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
  Before any later all-target rebuild can replace a qualified diagnostic
  consumer, archive its exact executable and binding together and verify their
  recorded hashes. A saved binding alone does not retain the executable. If
  retention was missed, disclose that limit; never relabel a newer binary as
  the historical artifact or replay guests merely to fill the archive.
  Capture that pair when qualification activates a context, before first guest
  use, and require the retained pair at pre-use. Manual archiving at the next
  repair rebuild was missed again in Phase06; existing failed-attempt gaps stay
  explicit even after the forward guard is fixed.
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
  Recovery checks must inspect recent commits and tracked file contents as well
  as dirty/untracked paths; a clean status does not mean an executor's work is
  absent. Label identity algorithms explicitly: `git hash-object` blob IDs and
  SHA-256 artifact digests are different domains, not interchangeable pins.
  Prepare planned evidence-validator code and authored mutation controls before
  the final source-review snapshot where possible. In Phase05 the full review
  preceded the new validator, leaving its source and real-record integration
  for later review. Validate against actual saved output formats as well as
  synthetic fixtures, and bind summaries to original records rather than to
  another summary. Keep later re-review limited to changed code and open rows.
  For public authored output, compare decoded bytes to the independent authored
  contract even when both host records and every dependent hash are resealed.
  Download original hosted ZIP bytes by the selected artifact ID, verify their
  API digest before strict extraction, and retain the archive with its receipt.
  A mock transport must exercise that same path; mock-only extraction checks
  do not qualify a production downloader that already extracted the archive.
  Deadline controls must include a real writing descendant and establish whole
  process-group cleanup and stopped output after return, not just direct-child
  termination. Check source identity before expensive work and after execution;
  a commit annotation alone does not bind a dirty build to that commit.
  Exercise the actual CI emitter entrypoint with isolated public output storage
  before long qualification. A layout-only producer helper missed a distinct
  temporary-directory lifecycle bug in the real emitter. Supply the complete
  workflow identity environment before classifying a local control, and keep
  authored identity values separate from actual server-authenticated hosted
  proof. Probe a new downloader cheaply against an existing cleared artifact
  before spending another CI run on it.
  Anchor required membership to the existing pinned-source qualification
  verifier, not merely two mutually consistent snapshots. An authored control
  must shrink both snapshots and reseal their dependent summaries. Test privacy
  fields from the actual private schemas, including selected-member identities;
  recognizing one generic ROM-hash field does not cover its concrete aliases.
  An accepted qualification fixture must call the complete verifier entry
  through its final dispatch, not only selected helper validators. Phase05's
  v6 helper-only control missed undefined names in the full pre-use verifier;
  the actual runner stopped before guest use. Preserve that setup failure and
  the immutable archive, fix the owning entry test, and use an explicitly
  versioned fresh context for the justified next attempt. Run the independent
  full pre-use verifier before creating a guest run directory.
  Exercise the complete run-wrapper CLI as well as the verifier, using the
  actual context/role shape and normal result dispatch. Feed the production C
  record encoder's output into the real aggregate in an authored control;
  helper-only and replacement-producer fixtures missed Phase06 entry-order and
  literal-key-length defects. Derive fixed JSON key lengths with sizeof, not
  manually counted constants. Select whole-suite BASE levels from the pinned
  global executor; a component's level subset cannot define the denominator.
  A Plan05-04 Clang-clean macro failed GCC warnings-as-errors in hosted CI.
  For changed C translation units, use the existing gcc:13 image for a bounded,
  read-only, network-disabled warning check before publication when its headers
  match the owning target. Keep hosted checks; do not install another compiler
  or expand a libc-mismatched probe to unrelated unchanged host code.
  Before expanding an opcode matrix, anchor representative raw instruction
  words and literal input/output bits against the primary encoding table or
  diagram. Distinguish bit-count subscripts and assembler operand notation
  from encoded fields. An expected-value helper that repeats the production
  transform is not an independent oracle; Plan05-05 exposed this gap.
  T64_STOP_GUEST_TICK is an absolute target. Sequence tests must advance from
  the current tick, then assert the intended opcode actually retired; repeated
  requests for tick1 silently leave later instructions unexecuted. The clip and
  divider test harnesses independently hit this error in Plan05-06/07.
  Finish source review before the combined gate and keep its build tree stable
  until it exits. A new finding invalidates affected evidence; preserve that
  attempt and rerun only after the correction, without rebuilding under CTest.
  Process-test readiness must mean required child/descendant identity is already
  observable. Signal the existing barrier after publishing it; do not hide a
  scheduling race with sleeps, repeated CI attempts or relaxed cleanup checks.
  Diagnose source-fetch failures before retrying CI. An upstream-listed HTTPS
  alternate may supply the same pinned bytes after a bounded transport failure;
  a successful download with a hash mismatch must still fail before use. Preserve
  source/tool versions, fixture identities, rights and source-revision checks.
  Check a new standalone workflow against the repository workflow-policy scanner
  before the combined gate; actionlint alone cannot validate that local contract.
  Admit a planned separate lane through a narrowly reviewed contract and retain
  mutation controls for its command, permissions, uploads, action pin and bounds.
  State-preservation checks must snapshot and compare every named latch; comparison
  to a sentinel constant does not establish that an untouched field stayed equal.
  Bind runner input to approved fixture identities, not a caller-nominated hash.
  Validate saved executable hashes before even version/selftest commands.
  For authored instruction formats, use small named-field packers with linked-word
  anchors before guest runs. POSIX shell function variables are global: use distinct
  helper prefixes and compare installed outputs to checked-candidate identity.
  Load launch identities from checked receipts rather than retyping digest strings.
  Separate per-case child/outer deadlines from a bounded aggregate test timeout;
  one guest's deadline does not cover serial fixture setup and an entire matrix.
  Check manifest distribution channels before wiring a fixture into hosted tests.
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
  Record the owning `ci-review.sh --append` Runs row as well as the finding;
  a prose finding alone omits the tested source and completed-run ledger.
  Before closeout, reconcile finding run IDs against Runs rows and recover
  missing metadata from the actual completed run without relaunching CI.
- CI/CD efficiency and local-first checks (owner direction, 2026-11-19): while
  any CI or release (CD) run is in flight or finished, look for low-hanging
  speed, iteration-time and GitHub Actions cost wins (duplicate setup/builds,
  independent work run serially, oversized matrices, uncached already-audited
  inputs, long waits a cheap check could have pre-empted) and take those that
  leave evidence and quality intact. Do not accept a bottleneck by default:
  step back, measure the critical path, fix the cheapest cause, and log the
  lesson (docs/ci-reviews/LOG.md) so it is not rediscovered. Reproduce CI
  locally before waiting on hosted runs: run the repository's own scanners and
  gates first (check-ci-yml, workflow/policy checks, the gcc:13 warnings route,
  test-inventory, rights/PII), then use `act` for a Linux-job pre-flight
  (`act -l`/`-n` parse and wiring; add `--container-architecture linux/amd64`
  on Apple Silicon; reuse a runner image already on disk, since acquiring a new
  image still needs the package/provenance audit). `act` cannot run macOS jobs
  and is never hosted evidence: authenticated hosted runs remain the proof for
  any hosted claim. In new designs, put cheap consistency checks ahead of long
  tests, processes and suites. This session's lessons: a stale pinned
  inventory, workflow/scanner drift and a missing include path were each found
  only after a long gate; each had a seconds-long local check. Replay every
  verifier or collector tail against real retained artifacts before pushing
  (two Phase 08 defects appeared only on real hosted data, each costing a native
  plus hosted cycle), and build cross-platform comparison controls from real
  two-platform data, not identical synthetic copies. Before accepting a long
  fixed cost, read the per-step timings: the pinned Binutils source build is
  about half of the macOS gate (docs/ci-reviews/LOG.md F-096).
- Benchmark named workloads/builds with correct output held constant. Report
  emulator throughput and end-to-end latency separately from CI queue/setup/
  build/test time.

## 3. Milestone close and release

Release and other CD workflows follow the CI/CD efficiency and local-first rule
in section 2: pre-flight locally, run cheap consistency checks before long
steps, and log measured wins.

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

## Coupled task commit boundaries

When a plan's shared case/build registrations prevent independent coherent
commits, use one verified plan-slice commit and retain per-task evidence.
Do not spend a handoff cycle partially staging cosmetic boundaries or leave
references to uncommitted test targets in an intermediate source commit.


## Approved Phase8 sequencing

Owner direction,2026-11-18: follow the recommendation to start Phase8 Audio
Interface/raw capture from the tested authored Phase7 foundation. This changes
Phase8's dependency from all of Phase7 to its qualified authored output path.
Carry discussion, planning, implementation, review and qualification through
within the approved phase without per-step acknowledgments. Phase7/VID-04 and
Plan13 remain incomplete/stopped; this grants no oracle rights, acquisition,
execution, publication, private-test window, package install or release scope.
Continue cheap actual-consumer/setup checks before long suites and reuse
unchanged evidence. Do not re-ask this sequencing question after a restart.

## Phase9 execution lessons (2026-11-20)

CI/CD efficiency and cheap-first findings from Phase9 execution; they extend section 2.
- Linux-only compile errors (shadowed typedefs, unused results, glibc format
  truncation, build-graph races) surfaced only on hosted runs until the local Linux
  pre-flight existed: compile every gate unit inside the `act` Ubuntu image
  (docs/ci-reviews/LOG.md F-102, about 50 s native, 160 s when the platform is forced
  to amd64 on an arm host, so do not force it). Run it before any push that changes C.
- A manifest row edit moves a hash pinned by the owner-gated Phase7 oracle request.
  After any `fixtures/manifest.json` edit run `ctest -R '^harness\.rdp-oracle-authority$'`
  (0.2 s) at once; the owner-approved identity-only rebind is an orchestrator task,
  never an executor edit. Structural option for the owner: bind per-row hashes
  instead of the whole manifest hash so authored-row additions stop forcing rebinds.
- Before accepting a plan's verify command, check its regex count with `ctest -N -R`
  (plan regexes matched 10 not 11 names more than once) and its direct-run
  environment (`T64_TEST_IPL3_PATH`, mutant inventory env).
- Wrapper time limits must be derived from measured test times: a 6.3 MB hash per
  refusal row costs about 22x under ASan, so one state test reached 632 s while the
  shared qualifier wrapper still killed at 600 s. After raising any test's own
  TIMEOUT, compare the sum of measured sanitizer times with every enclosing limit.
- A canonical digest needs a completeness test: flip every behavior-bearing member
  and require the digest to change. Self-comparing walkers, strip differentials and
  golden tables cannot see a member that is simply absent; the independent review
  found the RDP combiner and coverage gap that way after nine plans had passed.
- Reviewers and auditors must not run `ninja -t clean` or fixture rebuilds in the
  shared tree; use `git archive` scratch builds and re-verify fixture hashes after any
  such incident.
- Replay the CI steps that run after Gate (evidence emit, upload lists) locally before any
  push that touches a retained-evidence producer or a shared test helper: a per-process
  test cleanup scope deleted run directories the hosted emit step reads afterwards and
  only the hosted run showed it (about 20 s to reproduce locally; docs/ci-reviews/LOG.md F-107).
- Before the first push of a phase, audit every registered CTest TIMEOUT against the
  largest native, sanitizer and hosted seconds (hosted macOS ran 2.5x to 3.5x native for
  the state tests; four timeouts were under 2x). F-108.
- Check the CI result of the current head before starting a long (25 minute) native
  qualification rehearsal; a red hosted head makes the native run wasted work.
- A hosted job time gate that is a projection (warm plus the uncached tool build) on one
  noisy runner sample (macOS ranged 349 to 433 s on near-identical code) is checked by
  confirming the cache entries are present and fresh before the final push, not by
  spending another candidate cycle.

## New-milestone ritual (owner direction 2026-11-23)

At every `/gsd-new-milestone`, without being asked: (1) clean up first (consume the handoff, commit stray planning or config edits, reconcile STATE.md, PII scan); (2) fan out read-only subagent lenses per phase group plus a rolling-roadmap lens and an adversarial process lens, each with role perspectives, pros/cons, anti-patterns and primary-source prior art, then synthesize one set of recommendations and follow it; (3) refresh the Rolling Roadmap in ROADMAP.md with priority-ordered horizons (next, near, mid, long) and their promotion evidence; (4) record the takeaways in `research/<milestone>-fanout/`, STATE.md and the owner-memory file; (5) give the owner the exact autonomous launch command and the pre-resolved owner decisions. Lens reports for v1.2 are in `research/v1.2-fanout/`.

## v1.2 execution rules (fan-out 2026-11-23)

- Open every phase with a scripted local preflight under about 90 s that replays the Phase 9 lessons (act-image Linux compile, post-Gate CI steps, CTest regex counts and TIMEOUT audit, oracle-authority check, PII scan, head CI status, host load) and cite it from every plan.
- Review per wave in scratch clones (`git archive`), not once at phase end; Phase 9's blocking defect surfaced only in the late review.
- Plans are capped at 4 tasks and about 45 minutes; no decimal plans; a defect fix is a new numbered plan.
- `parallelization` is false because executors share one build tree and git index.
- Prune closed-phase run directories (about 2,400 under build/gate/08-04-evidence) and check host load before native runs.

## Prompt dispositions for unattended `/gsd-autonomous` runs

| Prompt or state | Disposition |
|-----------------|-------------|
| Discuss-phase hard-to-reverse choice already locked in ROADMAP.md, STATE.md or the fan-out synthesis | Use the locked decision; do not re-ask |
| `gaps_found` after verification | One automatic fix plan and re-verify; then record as tech debt and continue unless it is a must-have |
| `human_needed` (visual or physical check) | Record as a non-blocking owner-attested item; never stop the run; never a must-have |
| Milestone audit `tech_debt` | Accept, list in the audit and RETROSPECTIVE, continue to complete and cleanup |
| Git tag creation | Never (`git.create_tag` is false; D6) |
| Oracle-request identity rebind | Approved for v1.2 manifest rows per STATE.md; orchestrator only |
| Any listed owner stop (rights, PII, hardware, paid, D2/D4/D5/D6/D7/D8) | Stop and batch the question |


# Hosted evidence handoff

Parent preparation after plan 12 and the pre-CI review/fix pass.

- The complete gate currently has 91 tests; sanitizer inventory is 93. State
  format is v8, with exact attempted-output accounting. Use current profile and
  state vectors from `fixtures/boot-contract.json`, not earlier v6/v7 hashes.
  Preserve historical qualification records and three judgment-tier flags.
- A deep explicit-source review through plan 12 found two blockers, now fixed
  and regression-tested. `03-REVIEW.md`, `03-REVIEW-FIX.md` and
  `03-REVIEW-DISPOSITION.md` preserve the evidence, with zero open findings.
  Warm admission now shares read-free span preflight before raw asset hashing;
  malformed real reports are schema-rejected before capture matching, and
  missing log hashes no longer crash the supervisor. Final 91-test gate and
  focused sanitizer regressions passed after both fixes.
- The canonical summary-based evaluation resolver omitted earlier task commits
  from the phase scope. `03-PRECI-REVIEW-SCOPE.json` records the explicit source
  override and reviewed-through commit. The parent will review plan 13's delta
  and confirm these fixes after this plan, then run the security/goal checks.
  Do not duplicate that full source review or mark the phase complete yourself.
- Keep exactly the existing two hosted jobs and one gate command. Verify the
  official upload-action tag/commit before using its reviewed pin. Upload only
  the one allowed sanitized canonical JSON per job, after staged rights/PII
  checks. No ROM, capture, tool binary, manual or raw log uploads.
- The source asset is qualified provenance; runtime hashes the actual embedded
  payload and authored ROM. The nested real result must come from that job's
  actual producer/supervisor execution, not a unit sample or relabeled local
  result. Source revision must equal the exact selected GitHub SHA.
- Normal non-force pushes and corresponding CI are already authorized. Preserve
  the owner's dirty instructions/config. Use no installed-runtime edits, no
  new CI lane, no cache or toolchain changes without a measured need.
- `gh run list` may lag a push. Find the exact push run through the API filtered
  by `head_sha` and event; do not manually dispatch duplicate runs to compensate.
  Record the real run/attempt selectors and poll only its attributable jobs.
  Record every completed run using `scripts/ci-review.sh`, with measured timing
  and the decision about any duplicate work. Previous finding ID was F-029.
- The collector must validate server-side run/job/artifact provenance before
  nested records, and handle attempt selection explicitly. Fresh private output,
  bounded extraction, no symlinks/extra files, atomic pair publication and
  exact selector checks are required. Do not let mocks/local samples satisfy
  the actual two-host proof. Reuse trusted CLI/tools rather than adding an
  archive parser or dependency.
- Run script selftests and checker mutations before hosted publication. Keep
  their deterministic checks in the common gate where needed, without adding
  unreviewed test names outside the exact 91/93 set. A failed CI job needs a
  concrete cause/fix before bounded retest, not blind reruns.
- Task 3 needs all-target sanitizer compilation before execution. One full
  93-test sanitizer run is permitted to establish current-source coverage,
  including the unchanged bounded admission fuzz and intentional fault control;
  set detect_leaks=0 on this macOS host, retaining ASan/UBSan checks. Keep the
  test timeouts/fuzz bounds and headless execution policy.
- After source is final and both hosted records compare, close the plan with
  portable evidence and its committed summary/state metadata promptly. No
  repeated broad checks solely for metadata. The parent performs final review,
  security audit and phase-goal verification; no routine owner approval gate.

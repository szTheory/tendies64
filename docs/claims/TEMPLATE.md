# Claims and evidence-tier template

This is the GOV-04 template. Any statement that says what Tendies64 does or how well it does it
is a claim. A claim names the build, the machine profile, the fixture set, the evidence tier and
the oracle, and says what it does not claim. `scripts/check-claims.sh` checks every claim block
in the repository, and `gov.claims.repo` runs it in every gate run.

Phase 1 ships no real claims. The two example blocks below are illustrative: their ids, digests and
counts are made up, and they are fenced so the repository scan does not treat them as claims.

## Evidence tiers

The tiers are an unordered set. A higher-sounding name is not a stronger claim; each tier says
what kind of evidence stands behind the sentence, nothing more.

| Tier | Meaning |
|------|---------|
| `smoke` | The build starts and a minimal run completes. Nothing about correctness. |
| `regression-pinned` | Outputs are pinned to earlier outputs of this project. A change is caught; a wrong value that was always wrong is not. |
| `unit-tested` | Individual units pass their own tests. No whole-system statement. |
| `self-checking-fixture` | Fixtures that report their own pass or fail verdict were run and passed. The fixture author is the oracle. |
| `emulator-differential` | Results agree with a named emulator at a named revision. The claim names both (`emulator`, `emulator_revision`). The emulator is the oracle, not the hardware. |
| `hardware-derived` | Expected values come from a capture on real hardware with named provenance (`capture_provenance`) inside an explicit `scope`. |

`synthetic` is reserved. It marks results produced by the stub and by the runner's negative
controls. A synthetic result is never support evidence: the tier, the profile `synthetic-stub`
and any build id ending in `+synthetic` are refused in a claim, and an evidence suite that
contains a synthetic result is refused.

## Reserved words

The words "accurate", "compatible" and "supports" (ASCII, any case, whole word) are reserved for
`hardware-derived` claims that have a non-empty `scope`. They are refused anywhere else in a claim
block, and on any prose line that starts with the project name followed by a space (after optional
list or quote markers), because such a line is a claim written outside a block.

## Claim block format

A claim block sits in a Markdown file. It opens with a line that is exactly three backticks
followed by `t64-claim` and closes with a line of exactly three backticks. Each line inside is one
`key: value` pair. Blank lines and other text are not allowed inside a block, and a key may appear
once. Blocks opened with `t64-claim-example` are documentation and are not checked.

Required keys:

| Key | Content |
|-----|---------|
| `build_id` | `MAJOR.MINOR.PATCH`, optionally `+` and lowercase build metadata. |
| `profile_id` | Lowercase letters, digits and `-`, at most 64 characters, starting with a letter or digit. |
| `tier` | One tier name from the table above. |
| `fixture_set` | Name of the fixture set that was run. |
| `manifest_digest` | `sha256:` and 64 lowercase hex digits, the digest of the fixture manifest the set came from. |
| `pass`, `total`, `guest_fail`, `boot_fail`, `timeout`, `missing`, `harness_error`, `skipped` | Non-negative integers. `pass` plus the six other counts must equal `total`. |
| `oracle_kind`, `oracle_revision` | What the results were compared against, and its revision. Neither may be empty. |
| `not_claimed` | What the claim does not cover (see below). |
| `statement` | The rendered sentence, exactly as below. |

Conditional keys: `emulator` and `emulator_revision` for `emulator-differential`; `scope` and
`capture_provenance` for `hardware-derived`. A conditional key on any other tier is refused.
Optional key: `evidence`, a repository-relative path to a `t64.result` suite file.

## The rendered sentence

The `statement` value is generated from the other keys and must match byte for byte. All six
outcomes plus skipped sum to the total, so `boot-fail` is part of the counts (CONTEXT A-4).

```text
Tendies64 {build_id} on profile {profile_id}: {phrase} for {fixture_set} @ {manifest_digest} ({pass}/{total}; missing {missing}, skipped {skipped}, timeout {timeout}, harness-error {harness_error}, guest-fail {guest_fail}, boot-fail {boot_fail}). Oracle: {oracle_kind} {oracle_revision}. Not claimed: {not_claimed}.
```

`{not_claimed}` is written without a trailing period; the template adds one.

The phrase depends on the tier:

| Tier | `{phrase}` |
|------|-----------|
| `smoke` | `smoke-tested` |
| `regression-pinned` | `regression-pinned` |
| `unit-tested` | `unit-tested` |
| `self-checking-fixture` | `passes self-checking fixtures` |
| `emulator-differential` | `agrees with {emulator} {emulator_revision}` |
| `hardware-derived` | `matches hardware-derived expectations within {scope}` |

## What "Not claimed" must cover

Name what a reader could wrongly infer: other profiles, other builds, other fixture sets,
behaviour the fixtures do not exercise, timing the tier cannot show, and any commercial software.
An empty or vague exclusion is a defect in the claim even though the checker only requires text.

## Evidence files

When `evidence` is set, the checker runs `t64-result-check identity` on the file. It must be a
valid `t64.result` suite that ends with its `t64.suite` record (the file passes the consumer's
checks), and every result in it must agree with the claim: the same `profile_id`, the same
`build_id`, and exactly the claimed `pass` count. Synthetic results are refused outright
(`C-EVIDENCE-SYNTHETIC`), and the path is checked lexically and on disk before it is used. The
checker does not compare the suite with a pinned inventory; `t64-result-check suite --inventory`
does that where the suite is sealed.

## Repository scans

Without arguments the checker scans every tracked or untracked-but-not-ignored `*.md` file
except those under `.planning/` and under `tests/seeded/claims/`. The second directory holds
seeded claim files that are invalid on purpose and are driven one at a time by the seeded test
`gov.claims.seeds`. A scan that finds no files fails (`C-ZERO-INPUT`).

```text
sh scripts/check-claims.sh                      scan the repository
sh scripts/check-claims.sh --files FILE...      check the named files
sh scripts/check-claims.sh --selftest           prove the checker and these examples
```

Findings print as `RULE <id> path:line: message`. Exit 0 means clean, 1 means findings, 2 means
the checker could not run (a missing tool, an unreadable file).

## Examples

Both examples are checked by `--selftest` as if they were real blocks, so they stay valid.

```t64-claim-example
build_id: 0.1.0
profile_id: ntsc-cart-4mb-ipl3-open
tier: self-checking-fixture
fixture_set: example-negative-controls
manifest_digest: sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
pass: 17
total: 19
guest_fail: 1
boot_fail: 0
timeout: 0
missing: 0
harness_error: 1
skipped: 0
oracle_kind: guest-self-check
oracle_revision: example-1
not_claimed: any hardware agreement, any commercial software, any timing beyond the fixture set
statement: Tendies64 0.1.0 on profile ntsc-cart-4mb-ipl3-open: passes self-checking fixtures for example-negative-controls @ sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef (17/19; missing 0, skipped 0, timeout 0, harness-error 1, guest-fail 1, boot-fail 0). Oracle: guest-self-check example-1. Not claimed: any hardware agreement, any commercial software, any timing beyond the fixture set.
```

```t64-claim-example
build_id: 0.1.0
profile_id: ntsc-cart-4mb-ipl3-open
tier: hardware-derived
fixture_set: example-hardware-set
manifest_digest: sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
pass: 4
total: 4
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: hardware-capture
oracle_revision: example-capture-0001
scope: example-cpu-subset
capture_provenance: example capture log 0001 (illustrative, not a real capture)
not_claimed: any instruction outside the example subset, any other profile, any graphics or audio behaviour
statement: Tendies64 0.1.0 on profile ntsc-cart-4mb-ipl3-open: matches hardware-derived expectations within example-cpu-subset for example-hardware-set @ sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef (4/4; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: hardware-capture example-capture-0001. Not claimed: any instruction outside the example subset, any other profile, any graphics or audio behaviour.
```

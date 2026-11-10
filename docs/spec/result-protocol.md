# t64.result v1: result protocol

Status: normative for Phase 1. Implemented by `runner/t64_supervisor.c`,
`runner/t64_classify.c` and `runner/t64_synthetic.c`; proven by `unit.classify`
and `tests/controls`. Later phases consume this contract; `schema_version` is
the change seam.

## Scope and evidence stamp

Every run produces one canonical result object. A run is driven by a runner (a
process that boots a guest behind the core facade) and classified by a
supervisor (a separate process that never trusts the runner).

In Phase 1 the only runner is `t64-runner-synthetic`, so every result carries
the synthetic stamp:

| Field | Value |
|-------|-------|
| `profile_id` | `synthetic-stub` |
| `build_id` | `0.0.0+synthetic` (project version, then `+synthetic`) |
| `evidence_tier` | `synthetic` |
| `profile_hash` | SHA-256 of the 73 bytes `{"profile_id":"synthetic-stub","schema":"t64.profile","schema_version":1}` followed by one LF |

Synthetic results are never support evidence; the claims gate refuses them.
The PASS / FAIL / `Done!` verdict grammar in this document is synthetic-only.
Phases 3 and 4 define the nemu64-test verdict adapter for the real core.

## Canonical result object

One JSON object, UTF-8, no whitespace between tokens, keys sorted in unsigned
byte order at every level, integers only, exactly one trailing LF. The same
logical run always yields the same bytes; the bytes do not depend on the slice
size the runner used to advance guest time.

Top-level keys, in order:

| Key | Type | Meaning |
|-----|------|---------|
| `build_id` | string | Synthetic stamp above |
| `counts` | object | See below |
| `detail` | string | Closed enum, see the precedence section |
| `evidence_tier` | string | `synthetic` |
| `fixture_digest` | string or null | SHA-256 of the fixture bytes (64 lowercase hex) when the fixture was read, else null |
| `fixture_id` | string | Caller-supplied id, `[A-Za-z0-9._-]{1,64}`; `none` when the arguments were unusable |
| `guest_tick` | string | Decimal u64 (a string because JSON integers are limited to 2^53-1) |
| `log_digest` | string or null | SHA-256 of the capture bytes (64 lowercase hex), else null |
| `outcome` | string | One of the six outcomes |
| `profile_hash` | string | Synthetic stamp above |
| `profile_id` | string | `synthetic-stub` |
| `schema` | string | `t64.result` |
| `schema_version` | integer | `1` |
| `state_digest` | null | Always null in Phase 1 |
| `stop_reason` | string | `guest-bound`, `guest-halt`, or `none` |

`counts` keys, in order: `fail_reported`, `log_bytes`, `log_bytes_total`,
`log_lines`, `pass_reported`, `unhandled_exception_lines`. All are JSON
integers no larger than 2^53-1.

Null and zero rules:

- For `harness-error` and `missing-fixture`: `log_digest` is null, every count
  is 0, `stop_reason` is `none` and `guest_tick` is `"0"`.
- `fixture_digest` is the actual digest whenever the fixture was read (also for
  a digest mismatch and for harness errors after launch); null when the fixture
  is absent or the arguments were unusable.
- For the four guest outcomes (`guest-fail`, `boot-fail`, `timeout`, `pass`):
  `stop_reason` and `guest_tick` come from the validated runner report,
  `log_digest` is the digest of the captured bytes (the digest of the empty
  string for an empty capture), and counts are derived from the capture.
  `log_bytes` is the capture length; `log_bytes_total` is the number of bytes
  the guest emitted, which exceeds `log_bytes` only when the log limit cut the
  capture.

No wall time, hostname or path appears in the canonical object. Those go to
`run-meta.json`, which is non-canonical, never compared, and never uploaded. It
holds `core_identity`, `host_os`, `wait_status` (the raw wait status, -1 when no
process was reaped) and `wall_ms`.

Consumers (the suite consumer lands in plan 01-07) reject any unknown field and
any `schema_version` other than the ones they implement.

## Outcomes, precedence and exit codes

Six outcomes. The first matching rule wins, in this order. Pass happens only on
a positive match.

| # | Outcome | Exit | Details |
|---|---------|------|---------|
| 1 | `harness-error` | 1 | `usage`, `exec-failed`, `supervisor-signalled`, `wall-backstop`, `runner-signalled`, `runner-exit-nonzero`, `no-result`, `report-invalid`, `fixture-digest-mismatch`, `inventory-mismatch`, `suite-truncated`, `unclassified` |
| 2 | `missing-fixture` | 13 | `fixture-absent` |
| 3 | `guest-fail` | 10 | `log-over-limit`, `log-invalid-utf8`, `fail-reported`, `output-after-done`, `count-mismatch`, `verdict-incomplete` |
| 4 | `boot-fail` | 11 | `empty-output` |
| 5 | `timeout` | 12 | `guest-bound` |
| 6 | `pass` | 0 | `positive-match` |

The supervisor's own exit status is the outcome's exit code. Any other code a
runner returns (including 134 and 139) is never an outcome; it is a harness
error.

Harness-error details, in evaluation order:

1. `usage`: unusable arguments, an unusable work directory, or a fixture that is
   present but not a regular file, larger than 64 MiB or unreadable.
2. `exec-failed`: the runner could not be executed.
3. `supervisor-signalled`: the supervisor received SIGTERM or SIGINT.
4. `wall-backstop`: the wall-clock backstop expired. A wall-clock kill is
   always `harness-error`, never `timeout`: `timeout` means only that the
   guest-time bound was reached, so a hung core cannot satisfy a spin control.
5. `runner-signalled`: the runner died from a signal (for example abort or a
   segmentation fault).
6. `runner-exit-nonzero`: the runner exited with a non-zero status.
7. `no-result`: the runner exited 0 but left no report.
8. `report-invalid`: the report is malformed, or inconsistent with the run: a
   tick above the bound; `guest-bound` with a tick that is not the bound; a
   capture longer than the log limit; or `log_bytes_total` smaller than the
   capture length.
9. `fixture-digest-mismatch`: the fixture's SHA-256 differs from the pinned
   digest. The runner is not launched.
10. `inventory-mismatch`, `suite-truncated`: produced only by suite consumers.
11. `unclassified`: the defensive default for observations that match nothing.

`missing-fixture`: the fixture path does not exist; the runner is not launched.

Guest-fail details, in this order (lines are split on LF; a final unterminated
line counts as a line):

1. `log-over-limit`: `log_bytes_total` is above the log limit.
2. `log-invalid-utf8`: the capture is not valid UTF-8 (RFC 3629).
3. `fail-reported`: a line starts with `FAIL ` (anywhere, including after
   `Done!`).
4. `output-after-done`: any line after the first line that is exactly `Done!`
   and is not an unhandled-exception line.
5. `count-mismatch`: `Done!` was seen and the number of lines starting with
   `PASS ` differs from `--expect-pass`.
6. `verdict-incomplete`: the guest halted, the output is non-empty and `Done!`
   was never seen.

Then `boot-fail` / `empty-output` (the capture is empty, whether the stop was
the bound or a halt), then `timeout` / `guest-bound` (bound reached without
`Done!`), then `pass` / `positive-match` (`Done!` seen, count matches, nothing
else above applied; a guest that spins on after `Done!` still passes).

An unhandled-exception line contains `unhandled exception` (ASCII
case-insensitive). These are counted in `unhandled_exception_lines` and
tolerated after `Done!`.

## Runner report

The runner writes one text file of at most 512 bytes, exactly five LF-terminated
lines in this order, with no CR bytes and no other content:

```
t64-report 1
stop_reason guest-bound|guest-halt
guest_tick <u64>
log_bytes_total <u64>
end
```

Decimals are strict: digits only, no sign, no leading zeros except `0`, no
overflow of u64. `log_bytes_total` must also be at most 2^53-1. Any deviation
makes the supervisor classify the run as `report-invalid`.

## Synthetic script grammar v1

Scripts (`*.t64s`) drive `t64-runner-synthetic`. UTF-8 text with LF line
endings, at most 1 MiB, lines at most 4096 bytes.

- Line 1 is exactly `t64-synthetic 1`. Blank lines and lines starting with `#`
  are ignored.
- `log <tick> <text>`: emits the bytes of `<text>` at the tick. Escapes: `\n`,
  `\\`, `\xHH`. No newline is added automatically. The text must not be empty.
- `logx <tick> <hh> ...`: emits raw bytes given as two-digit hex tokens
  separated by single spaces.
- `end <tick> <directive>`: exactly one, as the last non-comment line.
  Directives in this version: `quiesce` (the guest halts at the tick) and
  `spin` (the guest never halts). Plan 01-11 extends the set.
- Ticks are non-decreasing decimal u64 values across the whole file.

Any grammar violation makes the runner exit 2 with no report.

Guest semantics: `advance(bound)` publishes every record whose tick is at most
the bound and returns `T64_STOP_HALT` at the end tick when a `quiesce` end
tick is at most the bound; otherwise it returns `T64_STOP_BOUND` at exactly the
bound. The drive loop advances in slices (`target = min(tick + slice, bound)`),
drains after each step into the capture, and stops on a halt or when the tick
reaches the bound. The log limit stops writing to the capture but not counting.
The result does not depend on the slice size.

## Command lines

Runner:

```
t64-runner-synthetic --slice TICKS --fixture PATH --bound TICKS
                     --log-limit BYTES --report PATH --capture PATH
```

The caller supplies `--slice`; the supervisor appends the rest. Exit codes: 0
report written, 2 usage or grammar violation, 3 I/O failure.

Supervisor:

```
t64-supervisor --identity synthetic --result PATH --meta PATH --work-dir DIR
               --wall-ms N --bound TICKS --log-limit BYTES --fixture-id ID
               --fixture PATH --fixture-sha256 HEX --expect-pass N
               [--capture-out PATH] -- RUNNER [ARGS...]
```

- Phase 1 accepts only `--identity synthetic`. `--wall-ms` is at least 1;
  `--log-limit` is at most 64 MiB; every option may appear once.
- The fixture is checked with `lstat` before anything runs: it must be a
  regular file (not a symlink, FIFO or device) of at most 64 MiB.
- A private `mkdtemp` directory under `--work-dir` holds the report, capture,
  stdout and stderr files, created with `O_CREAT|O_EXCL|O_NOFOLLOW` and mode
  0600, and is removed at exit.
- The runner runs in its own process group with core dumps disabled and a file
  size limit of the log limit plus 64 KiB, stdout and stderr redirected to
  files. The supervisor polls `waitpid` against a monotonic wall-clock deadline
  (`wall-backstop`), and on expiry or on SIGTERM/SIGINT kills the whole group
  with SIGKILL and reaps it.
- The canonical result and the run-meta are written to `PATH.tmp` and renamed,
  so a result file is never partial. A result is written for every outcome
  whenever `--result` was given.
- The supervisor never uses the runner's exit code as an outcome; it only
  rejects a non-zero one.

## Control matrix

`tests/controls/matrix.tsv` has one TAB-separated row per control; the header
starts with `#`. Columns: `id script expect_pass bound wall_ms slice_a slice_b
fixture_mode fixture_sha256 outcome detail exit`. Goldens live in
`tests/controls/golden/<id>.json` and scripts in `tests/controls/scripts/`;
both directories are directory-owned corpora. Every case runs twice with two
slice sizes and the canonical bytes must be identical to each other and to the
golden. Goldens are derived independently of the code under test (digests from
the host `shasum`/`sha256sum` over bytes produced with `printf`). Crashing
controls can leave host crash reports (on macOS a `.ips` file in the user's
diagnostic reports directory) even with core dumps disabled; that is a host
side effect, not a harness failure.

Phase 1 ships two controls: `positive` (`pass` / `positive-match`) and
`empty-halt` (`boot-fail` / `empty-output`). Plan 01-11 completes the 19-case
matrix. Phases 3 and 4 replay the same matrix against the real core.

# t64.result v1: result protocol

Status: normative for Phase 1. Implemented by `runner/t64_supervisor.c`,
`runner/t64_classify.c`, `runner/t64_synthetic.c` and the consumer
`runner/t64_result_check.c`; proven by `unit.classify`, `tests/controls`,
`unit.result-goldens`, `unit.result-seeds` and `unit.suite-seeds`. Later phases consume this contract; `schema_version` is
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

Consumers (`t64-result-check`, see "Suites and the consumer") reject any unknown
field and any `schema_version` other than the ones they implement.

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
7. `no-result`: the runner exited 0 but left no report (the report file is
   empty or was deleted).
8. `report-invalid`: the report is malformed, or inconsistent with the run: a
   tick above the bound; `guest-bound` with a tick that is not the bound; a
   capture longer than the log limit; `log_bytes_total` smaller than the
   capture length; or a capture length other than
   `min(log_bytes_total, log limit)`, the exact length an honest runner writes
   (a shorter capture could hide a dropped `FAIL ` line). It also covers a
   report or capture path that the runner replaced with something other than
   a regular file (a FIFO, a symlink, a device), a capture that is missing,
   and a capture that could not be read in full (a read error, a short read,
   or no memory to hold it): the supervisor never classifies an empty or
   partial capture as guest output.
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
- `logrep <tick> <count> <text>`: emits `<text>` (same escapes as `log`, not
  empty) `<count>` times back to back at the tick. `count` is at least 1 and
  the total (`count` times the decoded text length) is at most 64 MiB.
- `end <tick> <directive>`: exactly one, as the last non-comment line.
  Directives: `quiesce` (the guest halts at the tick) and `spin` (the guest
  never halts). The negative controls add host-misbehavior directives, which act
  inside `advance` once the bound reaches the end tick: `hang` (sleep forever),
  `abort` (`abort()`), `segv` (`raise(SIGSEGV)`) and `exit0-no-result` (`_exit(0)`
  before any report is written). `exit7` behaves like `quiesce`, writes the
  complete report, and then exits 7.
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
                     [--ignore-log-limit]
```

The caller supplies `--slice` and any flag; the supervisor appends the rest.
`--ignore-log-limit` is test-only: the runner writes the capture past the log
limit so the supervisor's `RLIMIT_FSIZE` backstop can be proven (the
`log-flood-backstop` control). Exit codes: 0 report written (7 for the `exit7`
directive), 2 usage or grammar violation, 3 I/O failure.

Supervisor:

```
t64-supervisor --identity synthetic --result PATH --meta PATH --work-dir DIR
               --wall-ms N --bound TICKS --log-limit BYTES --fixture-id ID
               --fixture PATH --fixture-sha256 HEX --expect-pass N
               [--capture-out PATH] -- RUNNER [ARGS...]
```

- Phase 1 accepts only `--identity synthetic`. `--wall-ms` is at least 1 and
  may be any u64; the deadline saturates at the largest u64 instead of
  wrapping, so a huge value means no practical wall limit, never an immediate
  `wall-backstop`;
  `--log-limit` is at most 64 MiB; every option may appear once.
- The fixture is checked with `lstat` before anything runs: it must be a
  regular file (not a symlink, FIFO or device) of at most 64 MiB.
- A private `mkdtemp` directory under `--work-dir` holds the report, capture,
  stdout and stderr files, created with `O_CREAT|O_EXCL|O_NOFOLLOW` and mode
  0600, and is removed at exit.
- The runner runs in its own process group with core dumps disabled and a file
  size limit of the log limit plus 64 KiB, stdout and stderr redirected to
  files. The supervisor polls for the runner's exit (`waitid` with `WNOWAIT`,
  so the exited runner stays a zombie and its process group id cannot be
  reused) against a monotonic wall-clock deadline (`wall-backstop`). On every
  path, a normal exit, wall-clock expiry or SIGTERM/SIGINT, it then kills the
  whole process group with SIGKILL, before it reads the report and the
  capture, and reaps the runner. No descendant left in the group outlives the
  run or writes to its files after they are read. A descendant that leaves the
  group (`setsid`, `setpgid`) is outside this guarantee.
- After the runner exits, the report and the capture are opened with
  `O_RDONLY|O_NOFOLLOW|O_NONBLOCK` and must be regular files by `fstat`, so a
  runner that plants a FIFO cannot block the supervisor once the wall-clock
  backstop no longer applies.
- The canonical result and the run-meta are written to `PATH.tmp` and renamed,
  so a result file is never partial. A result is written for every outcome
  whenever `--result` was given.
- The supervisor never uses the runner's exit code as an outcome; it only
  rejects a non-zero one.

## Control matrix

`tests/controls/matrix.tsv` has one TAB-separated row per control; the header
starts with `#`. Columns: `id script expect_pass bound wall_ms slice_a slice_b
fixture_mode fixture_sha256 outcome detail exit runner_flags log_limit`.
`runner_flags` is `-` for none or space-separated runner flags; `log_limit` is the
log limit in bytes (default 65536). `fixture_mode` is `ok`, `deleted` (the
fixture path does not exist) or `tampered` (one byte is appended to a copy of the
script). Goldens live in
`tests/controls/golden/<id>.json` and scripts in `tests/controls/scripts/`;
both directories are directory-owned corpora. Every case runs twice with two
slice sizes and the canonical bytes must be identical to each other and to the
golden. Goldens are derived independently of the code under test (digests from
the host `shasum`/`sha256sum` over bytes produced with `printf`). Crashing
controls can leave host crash reports (on macOS a `.ips` file in the user's
diagnostic reports directory) even with core dumps disabled; that is a host
side effect, not a harness failure.

The matrix holds 19 live cases, pinned by `tests/controls/INVENTORY` (the sorted
case ids, the same format as any suite INVENTORY). All six outcomes occur:
`pass` (`positive`, `positive-unhandled`), `timeout` (`spin`), `boot-fail`
(`empty-halt`, `empty-bound`), `guest-fail` (`wrong-counts`, `fail-reported`,
`fail-after-done`, `truncated-verdict`, `log-invalid-utf8`, `log-over-limit`),
`missing-fixture` (`deleted-fixture`) and `harness-error` (`tampered-digest`,
`abort`, `segv`, `exit0-no-result`, `exit-nonzero`, `hang`,
`log-flood-backstop`). `fail-after-done` uses a `PASS` line after `Done!` (not a
`FAIL` line), so it reaches `output-after-done` under the locked order. The live
suite controls (`controls.suite`, `controls.suite-dropped`,
`controls.suite-truncated`, `controls.inventory-drift`, `controls.inventory`) are
driven by `tests/controls/run-suite.sh --bin-dir DIR --mode
suite|dropped|truncated|drift|inventory`. `tests/controls/README.md` lists each
case and what it proves. Phases 3 and 4 replay the same matrix against the real
core.

## Suites and the consumer

A suite is a JSON Lines file: one canonical `t64.result` line per case, in
ascending `fixture_id` order (unsigned byte order), then exactly one final
`t64.suite` record. Every line ends with LF. The suite record is canonical like
any other object:

```
{"case_count":N,"inventory_sha256":"<64 lowercase hex>","schema":"t64.suite","schema_version":1}
```

`INVENTORY` is the pinned case list: the sorted (`LC_ALL=C`) `fixture_id`s, one
per line, each LF-terminated, no blank lines, no duplicates. `inventory_sha256`
is the SHA-256 of the INVENTORY bytes. A missing suite record, or a case set
that differs from INVENTORY, is a `harness-error` (exit 1): the consumer prints
`outcome=harness-error detail=suite-truncated` or
`outcome=harness-error detail=inventory-mismatch` after the rule diagnostic.

`t64-result-check` is the only consumer in Phase 1. Exit 0 is ok, 1 is invalid
or harness-error, 2 is a usage error or an unreadable input. Options always
precede the file argument. Success prints `ok FILE` (except `identity`).
Diagnostics are `RULE-ID path[:line]: message` on stderr, and the first failure
stops the check.

| Subcommand | Behavior |
|------------|----------|
| `result FILE` | One result record (at most 64 KiB, exactly one canonical line). A suite record is refused (R-SCHEMA). |
| `suite --inventory INV FILE` | A whole suite (at most 16 MiB) against the pinned inventory. |
| `seal --inventory INV --out FILE RESULT...` | Validates each result, sorts by `fixture_id`, appends the suite record, runs the `suite` check on the output, and only then writes FILE (via `FILE.tmp` and rename). A set that does not match INVENTORY is refused with S-INVENTORY. |
| `identity FILE` | Validates every line of a result or suite file (the suite record is optional here and no inventory is compared), then prints `fixture_id<TAB>profile_id<TAB>build_id<TAB>evidence_tier<TAB>outcome` per result. Nothing is printed when any line is invalid. The claims gate uses it to refuse synthetic evidence. |

Result rules, checked in this order per record:

| Rule | Fires when |
|------|------------|
| `R-JSON` | The strict parser rejects the bytes (the message names the category, for example `JSON-float` or `JSON-duplicate-key`, and the offset). |
| `R-SCHEMA` | `schema` is not `t64.result` (or, for a suite record, `t64.suite`). |
| `R-VERSION` | `schema_version` is not the integer 1. |
| `R-UNKNOWN-FIELD`, `R-MISSING-FIELD` | The key set of the record or of `counts` differs from the exact sets above. |
| `R-TYPE` | A field has the wrong type or shape: ids and names outside their character sets, digests that are not 64 lowercase hex or null, `guest_tick` that is not a decimal u64 string, counts that are not non-negative integers. |
| `R-ENUM` | `outcome`, `detail`, `evidence_tier` (`smoke`, `regression-pinned`, `unit-tested`, `self-checking-fixture`, `emulator-differential`, `hardware-derived`, `synthetic`) or `stop_reason` is outside its closed set. |
| `R-DETAIL-OUTCOME` | `detail` does not belong to `outcome`. |
| `R-SYNTHETIC-STAMP` | `evidence_tier` synthetic, `profile_id` `synthetic-stub` and a `+synthetic` `build_id` are not all present or all absent, so the stamp cannot be dropped or half-applied. |
| `R-NULL-RULES` | The null and zero rules above are broken, `state_digest` is not null, `log_bytes` exceeds `log_bytes_total`, or `fixture_digest` is null for a detail that happens only after the fixture was read (`fixture-digest-mismatch`, and every harness-error detail after launch: `exec-failed`, `supervisor-signalled`, `wall-backstop`, `runner-signalled`, `runner-exit-nonzero`, `no-result`, `report-invalid`). |
| `R-CONSISTENCY` | A guest outcome breaks a relation the classifier makes deterministic (listed below), so the record was not produced by the classifier. |
| `R-NOT-CANONICAL` | The bytes differ from the canonical re-emission of the parsed record (whitespace, key order, a missing final LF). |

`R-CONSISTENCY` relations, for the four guest outcomes. They follow from the
counts being derived from the capture, the capture holding exactly
`min(log_bytes_total, log limit)` bytes, and the fixed guest-detail order:

- `pass_reported + fail_reported <= log_lines`,
  `unhandled_exception_lines <= log_lines` and `log_lines <= log_bytes`; a
  non-empty capture has at least one line.
- `log_digest` is the SHA-256 of the empty string exactly when `log_bytes` is 0.
- `log-over-limit` requires `log_bytes < log_bytes_total`; every other guest
  detail requires `log_bytes == log_bytes_total`.
- `fail_reported > 0` only with `log-over-limit`, `log-invalid-utf8` or
  `fail-reported`; `fail-reported` requires `fail_reported > 0`.
- `output-after-done` requires at least 2 lines; `verdict-incomplete` requires
  `stop_reason` `guest-halt`; `timeout` requires `stop_reason` `guest-bound`.
- `boot-fail` requires `log_bytes` 0; every other guest detail except
  `log-over-limit` requires `log_bytes > 0`.

Suite rules (the suite record itself also goes through R-JSON, R-SCHEMA,
R-VERSION, R-UNKNOWN-FIELD, R-MISSING-FIELD, R-TYPE and R-NOT-CANONICAL):

| Rule | Fires when | Detail echoed |
|------|------------|---------------|
| `S-TRUNCATED` | The file is empty, the final line has no LF, or the last line is a result (no suite record). | `suite-truncated` |
| `S-EMPTY` | The file holds no result records. Zero results is never a pass. | none |
| `S-ORDER` | A suite record is not the last line, or results are not in ascending `fixture_id` order. | none |
| `S-DUPLICATE` | A `fixture_id` appears twice. Checked before the inventory, so a repeated line reports as a duplicate. | none |
| `S-INVENTORY` | `case_count` differs from the number of results, `inventory_sha256` differs from the SHA-256 of INVENTORY, or the set of `fixture_id`s differs from INVENTORY. | `inventory-mismatch` |

An INVENTORY that is unreadable, empty, unsorted, has duplicates or has an id
outside `[A-Za-z0-9._-]{1,64}` is a usage error (exit 2), never a pass.

Seeds live in `tests/seeded/results/` and `tests/seeded/suites/` (directory-owned
corpora with an `EXPECT.tsv`, driven by `tests/seeded/run-expect.sh`).
`tests/seeded/suites/INVENTORY` is a support file, not a seed.

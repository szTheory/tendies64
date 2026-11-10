# Negative-control matrix

This directory holds the control matrix for the result protocol
(`docs/spec/result-protocol.md`). Each row of `matrix.tsv` is one control: a
synthetic guest script, the exact outcome, detail and exit code it must
produce, and a golden canonical result. A control that cannot fail proves
nothing, so every row asserts the exact outcome, detail and exit code, never
only "not pass".

`scripts/` and `golden/` are directory-owned corpora: a file dropped into either
directory is part of the corpus, and the matrix, INVENTORY and the CTest tests
are what keep the two in step.

## Case table

Rows are listed by outcome. Every row has fixture mode `ok` unless noted, a
5000 ms wall-clock backstop unless noted, and runs the supervisor twice, at
slice sizes 1 and 7.

| id | outcome / detail | exit | What it proves |
|----|------------------|------|----------------|
| `positive` | pass / positive-match | 0 | Two PASS lines, `Done!`, a clean halt: the only way to pass. |
| `positive-unhandled` | pass / positive-match | 0 | Mixed-case unhandled-exception lines after `Done!` are tolerated (ADR-0004), and a guest that spins on after `Done!` still passes. |
| `spin` | timeout / guest-bound | 12 | A guest that never halts reaches the guest-time bound, which is `timeout`. |
| `empty-halt` | boot-fail / empty-output | 11 | A guest that halts without output. |
| `empty-bound` | boot-fail / empty-output | 11 | A guest that spins without output: empty output wins over timeout. |
| `wrong-counts` | guest-fail / count-mismatch | 10 | `Done!` with two PASS lines when three were expected. |
| `fail-reported` | guest-fail / fail-reported | 10 | A `FAIL ` line before `Done!`. |
| `fail-after-done` | guest-fail / output-after-done | 10 | A `PASS c` line after `Done!`. The line is deliberately not a FAIL line, so the locked order (fail-reported before output-after-done) yields output-after-done. |
| `truncated-verdict` | guest-fail / verdict-incomplete | 10 | The verdict line is cut to `Do` (no LF) and the guest halts. |
| `log-invalid-utf8` | guest-fail / log-invalid-utf8 | 10 | The bytes `ff fe` in the capture. |
| `log-over-limit` | guest-fail / log-over-limit | 10 | 100 bytes emitted against a 64-byte limit (`log_limit` column): the digest covers the 64 captured bytes, `log_bytes_total` is 100. |
| `deleted-fixture` | missing-fixture / fixture-absent | 13 | Fixture mode `deleted`: the fixture path does not exist; the runner is never launched. |
| `tampered-digest` | harness-error / fixture-digest-mismatch | 1 | Fixture mode `tampered`: one byte appended to a copy, so the pinned digest no longer matches; the runner is never launched. |
| `abort` | harness-error / runner-signalled | 1 | The runner calls `abort()`. |
| `segv` | harness-error / runner-signalled | 1 | The runner raises SIGSEGV. |
| `exit0-no-result` | harness-error / no-result | 1 | The runner exits 0 without a report: exit 0 alone is never a pass. |
| `exit-nonzero` | harness-error / runner-exit-nonzero | 1 | A fully valid report (it would pass) followed by exit 7: the exit code is checked. |
| `hang` | harness-error / wall-backstop | 1 | The runner hangs inside advance. Wall budget 700 ms; the supervisor must return within about a second. A hung process is never `timeout`. |
| `log-flood-backstop` | harness-error / runner-signalled | 1 | The runner ignores the log limit (`--ignore-log-limit`, test-only) and writes 144000 bytes; the supervisor's RLIMIT_FSIZE kills it with SIGXFSZ. Without the backstop the precedence table would give report-invalid (capture longer than the log limit), so this row proves the backstop itself. |

`tampered-digest` and `deleted-fixture` reuse `scripts/positive.t64s` through the
fixture mode, so the guest behaviour is not what is under test there.

Columns of `matrix.tsv`: `id script expect_pass bound wall_ms slice_a slice_b
fixture_mode fixture_sha256 outcome detail exit runner_flags log_limit`.
`runner_flags` is `-` for none, or space-separated flags passed to the runner
(only `--ignore-log-limit` exists). `log_limit` is the supervisor and runner log
limit in bytes (65536 unless a row needs another).

## Independent golden derivation

A golden is never copied from supervisor output. Each one is assembled by hand
with `printf` from facts read off the script: the fixture digest is `shasum -a
256` over the script bytes, the log digest is `shasum -a 256` over the capture
bytes written out with `printf` (for `log-over-limit`, over the first 64 bytes
only), the counts come from counting the lines in that same text, and the guest
tick comes from the script's `end` line or the bound. Harness-error goldens carry
a null log digest, zero counts, stop reason `none` and guest tick `"0"`. If a
case and the precedence table in the spec disagree, fix `runner/t64_classify.c`
or the script; never edit the golden to match the code.

## Twice-run rule

`run-case.sh` runs every row twice with two slice sizes through the supervisor
and requires: both exit codes equal the row's exit; both canonical results pass
`t64-result-check result`; the two results are byte-identical (`cmp`); and both
are byte-identical to the golden. A result that depended on how the runner sliced
guest time would fail here. Harness-error results null every field that depends
on the capture, so crashes at different points cannot leak nondeterminism into
the bytes.

## Suite controls

`run-suite.sh` runs every matrix row once, seals the results with
`t64-result-check seal --inventory INVENTORY`, and verifies the suite. `INVENTORY`
is the sorted case list. Five CTest tests (label `controls`) use it:

| Test | Proves |
|------|--------|
| `controls.suite` | The sealed live suite verifies against INVENTORY. |
| `controls.suite-dropped` | A suite with one result line removed fails with `S-INVENTORY` and `inventory-mismatch`. |
| `controls.suite-truncated` | A suite without its suite record fails with `S-TRUNCATED` and `suite-truncated`. |
| `controls.inventory-drift` | A pinned inventory with one extra id fails with `S-INVENTORY` and `inventory-mismatch`. |
| `controls.inventory` | The sorted matrix ids are byte-equal to INVENTORY. |

Adding a case means adding the script, the golden, the matrix row and the
INVENTORY line together. Adding only one of them fails `controls.inventory`.

## Host side effects

On macOS, the `abort` and `segv` controls make the system write a crash report
(`.ips`) into the user's diagnostic reports directory even though core dumps are
disabled for the runner (measured: one report per run of each control, none for
`log-flood-backstop`, `hang` or `exit0-no-result`). The files are named after the
runner binary, `t64-runner-synthetic-*.ips`. They are a host side effect, not a
harness failure, and are safe to delete; nothing in the project reads them.

## Replay obligation (Phases 3 and 4)

Phases 3 and 4 replay this matrix against the real core: the same 19 case ids,
the same outcome, detail and exit codes, and the same twice-run and suite rules.
A real-core runner that cannot reproduce a row is a defect in that runner or a
documented, reviewed change to the row, never a silently dropped case; INVENTORY
exists so a dropped case is itself a failing control.

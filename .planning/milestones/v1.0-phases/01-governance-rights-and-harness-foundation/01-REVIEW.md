---
phase: 01-governance-rights-and-harness-foundation
reviewed: 2026-11-09T23:45:00Z
depth: standard
files_reviewed: 121
files_reviewed_list:
  - runner/t64_classify.c
  - runner/t64_classify.h
  - runner/t64_num.h
  - runner/t64_result_check.c
  - runner/t64_supervisor.c
  - runner/t64_synthetic.c
  - tools/common/t64_json.c
  - tools/common/t64_json.h
  - tools/common/t64_json_canon.c
  - tools/common/t64_jsonw.c
  - tools/common/t64_jsonw.h
  - tools/common/t64_sha256.c
  - tools/common/t64_sha256.h
  - tools/common/t64_utf8.c
  - tools/common/t64_utf8.h
  - tools/manifest/t64_manifest_check.c
  - tools/manifest/t64_schema.c
  - tools/manifest/t64_schema.h
  - .github/workflows/ci.yml
  - scripts/check-ci-yml.sh
  - scripts/check-claims.sh
  - scripts/check-docs.sh
  - scripts/check-provenance.sh
  - scripts/ci-review.sh
  - scripts/gate.sh
  - scripts/pii-scan.sh
  - scripts/rights-scan.sh
  - tests/controls/run-case.sh
  - tests/controls/run-suite.sh
  - tools/lint/core-lint.sh
  - tools/lint/lint-objects.sh
  - tools/provenance/t64_provenance_check.c
  - tools/rights/t64_blobscan.c
  - CMakeLists.txt
  - CMakePresets.json
  - LICENSES/MIT.txt
  - core/include/tendies64/t64_facade.h
  - core/t64_core_stub.c
  - docs/spikes/rust-vs-c/admission/admission.c
  - docs/spikes/rust-vs-c/admission/admission.h
  - docs/spikes/rust-vs-c/admission/admission.rs
  - docs/spikes/rust-vs-c/admission/fuzz_admission.c
  - docs/spikes/rust-vs-c/corpus/rust/alloc.rs
  - docs/spikes/rust-vs-c/corpus/rust/asm.rs
  - docs/spikes/rust-vs-c/corpus/rust/build_macro.rs
  - docs/spikes/rust-vs-c/corpus/rust/ctor.rs
  - docs/spikes/rust-vs-c/corpus/rust/entropy.rs
  - docs/spikes/rust-vs-c/corpus/rust/env.rs
  - docs/spikes/rust-vs-c/corpus/rust/extern_host.rs
  - docs/spikes/rust-vs-c/corpus/rust/file_io.rs
  - docs/spikes/rust-vs-c/corpus/rust/float.rs
  - docs/spikes/rust-vs-c/corpus/rust/good.rs
  - docs/spikes/rust-vs-c/corpus/rust/interior_mutable_static.rs
  - docs/spikes/rust-vs-c/corpus/rust/libm.rs
  - docs/spikes/rust-vs-c/corpus/rust/mutable_global.rs
  - docs/spikes/rust-vs-c/corpus/rust/mutable_static.rs
  - docs/spikes/rust-vs-c/corpus/rust/std_escape.rs
  - docs/spikes/rust-vs-c/corpus/rust/stdio.rs
  - docs/spikes/rust-vs-c/corpus/rust/thread.rs
  - docs/spikes/rust-vs-c/corpus/rust/time.rs
  - docs/spikes/rust-vs-c/corpus/rust/tls.rs
  - docs/spikes/rust-vs-c/run-spike.sh
  - fixtures/manifest.json
  - fixtures/manifest.schema.json
  - provenance/copies.json
  - provenance/docs-assets.tsv
  - runner/CMakeLists.txt
  - runner/tests/test_classify.c
  - tests/controls/CMakeLists.txt
  - tests/controls/golden/abort.json
  - tests/controls/golden/deleted-fixture.json
  - tests/controls/golden/empty-bound.json
  - tests/controls/golden/empty-halt.json
  - tests/controls/golden/exit-nonzero.json
  - tests/controls/golden/exit0-no-result.json
  - tests/controls/golden/fail-after-done.json
  - tests/controls/golden/fail-reported.json
  - tests/controls/golden/hang.json
  - tests/controls/golden/log-flood-backstop.json
  - tests/controls/golden/log-invalid-utf8.json
  - tests/controls/golden/log-over-limit.json
  - tests/controls/golden/positive-unhandled.json
  - tests/controls/golden/positive.json
  - tests/controls/golden/segv.json
  - tests/controls/golden/spin.json
  - tests/controls/golden/tampered-digest.json
  - tests/controls/golden/truncated-verdict.json
  - tests/controls/golden/wrong-counts.json
  - tests/controls/matrix.tsv
  - tests/t64_test.h
  - tools/claims/CMakeLists.txt
  - tools/common/CMakeLists.txt
  - tools/common/tests/test_common.c
  - tools/common/tests/test_json.c
  - tools/common/tests/test_json_fuzz.c
  - tools/lint/allowlist.txt
  - tools/lint/rules.txt
  - tools/lint/selftest/bad_asm.c
  - tools/lint/selftest/bad_atomic.c
  - tools/lint/selftest/bad_ctor.c
  - tools/lint/selftest/bad_entropy.c
  - tools/lint/selftest/bad_env.c
  - tools/lint/selftest/bad_extern_host.c
  - tools/lint/selftest/bad_file_io.c
  - tools/lint/selftest/bad_float.c
  - tools/lint/selftest/bad_include_stdio.c
  - tools/lint/selftest/bad_include_unlisted.c
  - tools/lint/selftest/bad_libm.c
  - tools/lint/selftest/bad_malloc.c
  - tools/lint/selftest/bad_mutable_global.c
  - tools/lint/selftest/bad_mutable_static.c
  - tools/lint/selftest/bad_runtime_helper.c
  - tools/lint/selftest/bad_thread.c
  - tools/lint/selftest/bad_time.c
  - tools/lint/selftest/bad_time_macro.c
  - tools/lint/selftest/bad_tls.c
  - tools/lint/selftest/good.c
  - tools/manifest/CMakeLists.txt
  - tools/provenance/CMakeLists.txt
  - tools/repocheck/CMakeLists.txt
  - tools/rights/CMakeLists.txt
findings:
  critical: 4
  warning: 19
  info: 14
  total: 37
status: issues_found
---

# Phase 1: Code Review Report

**Scope:** source changed since the phase base (121 files, about 14.8k lines). Documentation, planning
artifacts and seeded test inputs were excluded. The review ran as three parallel standard-depth
passes over disjoint areas, merged here with non-overlapping finding IDs:

- **Part A (CR-01..WR-07, IN-01..IN-04):** runner, supervisor, classifier, synthetic guest, strict JSON, SHA-256, manifest checker.
- **Part B (CR-11..WR-18, IN-11..IN-14):** scripts, lint, rights and PII scanners, provenance checker, CI workflow.
- **Part C (WR-21..WR-24, IN-21..IN-26):** CMake and presets, core stub and facade, Rust-versus-C spike, tests and goldens.

Reviewer-reported claims were reproduced by the reviewers against built binaries or throwaway
repositories; the orchestrator independently confirmed CR-01 in the source.


<!-- part A -->

**Reviewed:** 2026-11-09
**Depth:** standard (with ASan/UBSan reproductions and a 600k-iteration mutation fuzz of the JSON parser, writer and report parser)
**Files Reviewed:** 18
**Status:** issues_found

## Summary

The JSON parser, canonical writer, SHA-256, UTF-8 validator, report parser and the six-outcome classifier are sound. The classifier precedence matches `docs/spec/result-protocol.md` and ADR-0005, the detail table matches the enum order, and canonical output carries nothing host-dependent. The defects are in the supervisor's trust of the runner's files, the consumer's lack of cross-field checks, and a memory-safety bug in the manifest tool's string helper. Every finding below was reproduced against the built binaries or an ASan build.

What was checked and found clean:

- `t64_json.c`, `t64_jsonw.c` and `t64_utf8.c`: bounds, pool sizing, surrogate handling, integer range, depth and node caps. A 600,000-iteration mutation fuzz under ASan and UBSan found no report. Every accepted mutant re-emitted and re-parsed. Canonical output never exceeded input+16.
- `t64_parse_report` and `t64_parse_u64`: prefix lengths (12, 11, 16) and the overflow guard are correct.
- Classifier guest-fail ordering and `analyze()` line handling, including an unterminated last line and unhandled-exception tolerance after `Done!`.
- `syn_parse` and `syn_decode_text` bounds, `logrep` overflow guard, and the slice-independence of the drive loop.
- SHA-256 padding and endianness.
- The `t64.result` consumer's R-JSON, R-SCHEMA, R-VERSION, key-set, canonical re-emission, suite order, duplicate and inventory logic.

## Critical Issues

### CR-01: `t64_sanitize` writes one byte past its buffer when it truncates

**File:** `tools/manifest/t64_schema.c:78-91`
**Issue:** The loop guard is `n + 4 > cap`. The cut therefore happens at `n == cap-3`. Three `.` are then written, which makes `n == cap`, and `out[n] = '\0'` writes `out[cap]`. The result is a one-byte NUL overflow whenever the input is longer than `cap-3` bytes. The input is manifest content, so it is untrusted. Reproduced under ASan:
- A toolchain item of 120 characters overflows `shown[64]` at `t64_manifest_check.c:374` (stack-buffer-overflow).
- A 100-character `binary_rights` value overflows `tmp[64]` in `describe_value` at `t64_schema.c:318`, reached through `validate` and the enum check (stack-buffer-overflow).
- A direct call with `cap` 64 overflows a 64-byte heap buffer.

The schema allows toolchain items of up to 128 characters, so a schema-valid manifest can trigger this.
**Fix:**
```c
for (i = 0; i < len; i++) {
  unsigned char c = (unsigned char)s[i];
  if (n + 4u >= cap) {   /* keep room for "..." and the NUL */
    cut = 1;
    break;
  }
  out[n++] = (c < 0x20u || c == 0x7fu) ? '?' : (char)c;
}
```
With `n + 4 >= cap` the cut happens at `n == cap-4`, and `n+3` plus the NUL then fits within `cap`. Add a seeded test for a string longer than `cap`.

## Warnings

### WR-01: The supervisor can hang forever on a runner-planted FIFO, defeating the wall backstop

**File:** `runner/t64_supervisor.c:250` and `:666`
**Issue:** `read_small` opens `report`, and the capture reader opens `capture`, with `O_RDONLY|O_NOFOLLOW` and no `O_NONBLOCK`. The fstat check (`S_ISREG`) runs only after the open returns. The runner runs as the same user and may replace either path before it exits. A runner that does `rm report; mkfifo report; exit 0` makes the supervisor block in `open()`, which is after the `waitpid` loop, so the wall deadline no longer applies.

Reproduced. The supervisor stayed in state S with no result written and no output, and left `work-dir/t64sup.*` behind after the `kill -9`. Only SIGTERM released it. SIGTERM interrupts `open` with EINTR, and the supervisor then reports a misleading `no-result`.

The spec says the supervisor "never trusts the runner" and that the wall-clock backstop is the guarantee.
**Fix:** Open with `O_RDONLY|O_NOFOLLOW|O_NONBLOCK`, `fstat`, require `S_ISREG`, then read. Alternatively `lstat` first and refuse non-regular files as `report-invalid` or `no-result`. Add a control to `tests/controls` for a FIFO report.

### WR-02: The supervisor passes a run whose capture is shorter than the runner's own `log_bytes_total`

**File:** `runner/t64_supervisor.c:664-692`, `runner/t64_classify.c:210-215`
**Issue:** `report_invalid` rejects `log_bytes_total < capture_len` but not the reverse. For an honest runner the invariant `capture_len == min(log_bytes_total, log_limit)` is exact. The synthetic runner writes `min(n, room)` per chunk, so it holds. It is not enforced. A short capture can come from three causes:
- an I/O error in the capture read loop (`r <= 0` breaks silently, `:680`);
- a failed `malloc`, where `capture` stays NULL with length 0;
- a runner that did not flush.

The result is a guest verdict computed on missing bytes. A trailing `FAIL ` line that was dropped cannot be seen.

Reproduced. A runner whose report said `log_bytes_total 500` and whose capture was the 6 bytes `Done!\n` produced `outcome=pass detail=positive-match` with `log_bytes:6,log_bytes_total:500`.
**Fix:** In `report_invalid`, add:
```c
(o->capture_len != (o->log_bytes_total < o->log_limit ? o->log_bytes_total
                                                       : o->log_limit))
```
The classifier already rejects `capture_len > log_limit` and `log_bytes_total < capture_len`. Also treat a short read or `malloc` failure on the capture as `report-invalid` or `usage`, not as an empty capture. Update the spec's report-invalid list and add a control.

### WR-03: The consumer accepts internally contradictory records, so a forged or corrupt result passes `t64-result-check`

**File:** `runner/t64_result_check.c:411-462`
**Issue:** R-NULL-RULES checks only null and zero rules and `log_bytes <= log_bytes_total`. It does not tie `outcome`, `detail`, `stop_reason` and `counts` together, although the classifier makes these relations deterministic. Accepted by `t64-result-check result`:
- a `pass` record with `fail_reported:5`;
- a `pass` record with `log_bytes:0` (the classifier would say `boot-fail`);
- a `timeout` record with `stop_reason:"guest-halt"`.

All three were reproduced by editing the positive golden. The claims gate trusts `identity` and `suite`.
**Fix:** Add invariants to the guest-outcome branch:
- `pass` requires `fail_reported == 0`, `log_bytes > 0` and `pass_reported <= log_lines`.
- `boot-fail` requires `log_bytes == 0` and `log_bytes_total == 0`.
- `timeout` requires `stop_reason == guest-bound`.
- `fail-reported` requires `fail_reported > 0`.
- `verdict-incomplete` requires `stop_reason == guest-halt`.
- `log-over-limit` requires `log_bytes < log_bytes_total`.
- Every other guest detail requires `log_bytes == log_bytes_total`.
- Always require `pass_reported + fail_reported <= log_lines` and `unhandled_exception_lines <= log_lines`.

Pin each rule with a seed.

### WR-04: Descendants of a runner that exits normally are never reaped or killed

**File:** `runner/t64_supervisor.c:628-633`
**Issue:** The process-group `SIGKILL` runs only when `!reaped` (wall expiry or SIGTERM). If the runner exits normally, background children in its group survive. A real core runner could spawn helpers. They keep running after the result is written, keep the `stdout`/`stderr` files open, and can write to the capture after it was read. Reproduced. After a run whose runner did `sleep 37 &; exit 0`, `ps` showed `sleep 37` still alive with the runner's pgid.
**Fix:** After the loop, always `kill(-pid, SIGKILL)` and ignore ESRCH. The pgid is guaranteed to stay valid while any member exists, and the call is a no-op when none remain. Do this before reading the report and capture, so a straggler cannot mutate them.

### WR-05: `--wall-ms` overflows the deadline and turns a valid run into `wall-backstop`

**File:** `runner/t64_supervisor.c:574`
**Issue:** `deadline = now_ms() + a.wall_ms` is unchecked u64 addition. The spec requires only `--wall-ms >= 1` and sets no upper bound. A large value wraps the sum below `now_ms()`. Reproduced. With `--wall-ms 18446744073709551615` the positive control was killed at once with `harness-error detail=wall-backstop`. It fails closed, but the diagnosis is wrong.
**Fix:** Reject `wall_ms` above a documented cap (for example 24 h) in `parse_args`, or saturate: `deadline = (a.wall_ms > UINT64_MAX - now) ? UINT64_MAX : now + a.wall_ms`.

### WR-06: The fixture digest the result attests is not the bytes the runner reads (TOCTOU)

**File:** `runner/t64_supervisor.c:376-426` and `:556-557`
**Issue:** The supervisor hashes the fixture and stamps `fixture_digest`, then passes the same path (`--fixture`) to the runner, which reopens it (`t64_synthetic.c:414`). The file can change between the two. This matters for the real core and ROM runners in Phases 3 and 4, where the digest is the identity claim. Nothing in the protocol says the runner must read the pinned bytes.
**Fix:** Copy the fixture into the private `mkdtemp` directory while hashing it, create it with `O_EXCL` and mode 0400, and pass the runner the copy. Alternatively inherit the already-verified descriptor. Record the choice in the spec.

### WR-07: The cross-field rules in `t64-manifest-check` assume schema guarantees that `--schema PATH` does not enforce

**File:** `tools/manifest/t64_manifest_check.c:270-277`, `:288-298`, `:402`
**Issue:** `check_entry` dereferences `item->str` and `sha->str` as if the default schema had validated them. The tool accepts any subset-valid schema. With `{"type":"object"}` as the schema, both of the following were reproduced under ASan:
- `"sha256":"ab"` with commit distribution makes `add_row` do `memcpy(..., sha->str, 64)`, a 64-byte read from a 3-byte pool string (heap-buffer-overflow).
- `"toolchain":[5]` makes `toolchain_pinned` call `strlen(NULL)` (SEGV).

An unconstrained `sha256` string can also hold a TAB or LF. When the bytes are absent, no digest comparison happens, and the string is written verbatim into the allowlist TSV that `rights-scan.sh` consumes. That is row injection into a rights-widening file.
**Fix:** Re-check the load-bearing invariants in code, independent of the schema:
- Require `sha->len == 64` and lowercase hex before `add_row`.
- Require `item->type == T64_JSON_STRING` in the toolchain loop.
- Require `path->type == T64_JSON_STRING` before `path_lexical`.
- Alternatively, refuse any `--schema` other than the pinned one when `--emit-allowlist` is given.

## Info

### IN-01: Predictable `.tmp` names, with no `O_EXCL` or no `O_NOFOLLOW`

**File:** `runner/t64_supervisor.c:79`, `runner/t64_result_check.c:1021`
**Issue:** The supervisor's `atomic_write` uses `O_CREAT|O_TRUNC|O_NOFOLLOW` on `PATH.tmp`. Two supervisors writing the same path share the temp file. `cmd_seal` uses `fopen(tmp_path, "wb")` with no symlink protection. On `fwrite` failure the `FILE*` is leaked, because `fclose` is short-circuited away.
**Fix:** Use `mkstemp` in the target directory (or `O_EXCL` with a pid suffix), and `fclose` on every error path.

### IN-02: `dupstr` results are not checked

**File:** `runner/t64_supervisor.c:556-565`
**Issue:** If `malloc` fails, a NULL lands in the middle of `child_argv`, which silently truncates the runner's arguments at `execvp`.
**Fix:** Build `child_argv` from `char *` literals and `snprintf` buffers on the stack, or check each result and set `usage_error`.

### IN-03: The "anchored" pattern check in the schema subset is superficial

**File:** `tools/manifest/t64_schema.c:179`
**Issue:** `chk_pattern` accepts any pattern that begins with `^` and ends with `$`. `^a|b$` passes but is not fully anchored, so it matches `xb`. Current repo schemas are fine.
**Fix:** Reject top-level `|` outside brackets and groups, or wrap the pattern as `^(...)$` before `regcomp`.

### IN-04: The runner inherits duplicate descriptors for its stdout and stderr files

**File:** `runner/t64_supervisor.c:596-597`
**Issue:** After `dup2(outfd,1)` and `dup2(errfd,2)`, the original `outfd` and `errfd` stay open in the child, because they are not CLOEXEC. They are harmless today but widen what the runner inherits.
**Fix:** Close them in the child after `dup2` (when they are not 1 or 2), or create them with `O_CLOEXEC`.

---

_Reviewed: 2026-11-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_

<!-- part B -->

**Reviewed:** 2026-11-09
**Depth:** standard
**Files Reviewed:** 15
**Status:** issues_found

## Summary

Scope was the 15 files in rv-B.txt: the CI workflow and its static checker, the gate driver, the rights scan
(driver plus `t64-blobscan`), the PII scan, the provenance checker and its driver, the claims and docs
checkers, ci-review, the controls runners, and both determinism-lint layers. The tests/seeded scripts named in
the task text are not in rv-B.txt and were not reviewed.

Every bypass below was reproduced against the real scripts and the built binaries in `build/gate`, using
throwaway repositories under the scratchpad. Nothing in the repository was modified.

The workflow itself is sound: top-level `permissions: {}`, per-job `contents: read`, `pull_request` (not
`pull_request_target`), no secrets, `persist-credentials: false`, full-history checkout, timeouts, and the
checkout SHA resolves to tag v7.0.1 (verified with `gh api`). The rights scanner is robust against hostile file
names (leading dash, spaces, newline, tab all handled; output sanitised), symlinks, history-only blobs,
renames, gitlinks, shallow clones and zero-scanned runs. The failures are concentrated in the two controls that
scan for content by regex over git text output (PII scan) and the two controls that deny by name (extension
rule, lint Layer 2).

Necessity (category 4): read `necessity-review.md`; no finding qualified. The guards that look redundant
(`A-MISSING`, the quoted-path handling in `denied_ext`) are trust-boundary or defense-in-depth code.

CI/CD patterns: read `cicd-review-patterns.md`; the workflow has no deploy leg, so only pinning, permissions,
timeout and trigger checks apply, and they pass.

## Critical Issues

### CR-11: PII scan skips any commit-message line that begins with "COMMIT "

**File:** `scripts/pii-scan.sh:181-189`
**Issue:** The message scan runs `git log --format='COMMIT %h%n%B'` and then treats every line matching
`/^COMMIT /` as a record marker (`c = $2; next`), so the line is discarded and never reaches `msg.txt`. The
comment at lines 164-167 justifies the same trick for the diff section (content lines always start with
space, `+` or `-`), but a commit body is arbitrary text and can start a line with `COMMIT `. Reproduced: a
commit whose body contains `COMMIT contact <real address> or see <per-user home path>` yields
`pii-scan: tree_files=1 commits=1 findings=0` and exit 0. The scan is a security control on a public
repository; a private address or path in a message hides behind one word. It is also a plausible accident
(`COMMIT message drafted from ...` pasted from a tool).
**Fix:** Do not use an in-band marker for free text. Build the message file per commit and prefix every
line, so no input line can be mistaken for a marker:
```sh
: > "$TMP/msg.txt"; : > "$TMP/msg.loc"
while IFS= read -r _c; do
  git show -s --format=%B "$_c" | sed 's/^/M /' > "$TMP/one.msg" || fatal "git show failed"
  sed 's/^M //' "$TMP/one.msg" >> "$TMP/msg.txt"
  awk -v c="$_c" '{ print "message:" c }' "$TMP/one.msg" >> "$TMP/msg.loc"
done < "$TMP/commits.list"
```
Add a seeded case `message-marker-lookalike` to `selftest` (body line `COMMIT ` plus a seeded address, expect
`P-EMAIL`).

### CR-12: PII scan is bypassed by a committed `.gitattributes` `-diff` or `binary` attribute

**File:** `scripts/pii-scan.sh:121,126,168`
**Issue:** The tree scan uses `git grep -I` and the history scan uses `git log -p`. Both honour the `diff`
attribute: a file marked `-diff` or `binary` in `.gitattributes` is treated as binary, so `git grep -I` skips
it and `git log -p` prints `Binary files differ` with no content lines. Reproduced with
`printf '* -diff\n' > .gitattributes` plus a file containing a per-user home path: `git grep -n -I -o -E
--untracked ...` returns 1 (no output) and `git log -m -p` shows only `Binary files /dev/null and b/n.md
differ`. Both the tree and the history pass are blind, for every file the attribute covers, including the
`.gitattributes` file itself. The rights scan reads raw bytes, so it does not catch this and does not look at
`diff`/`-diff` attributes.
**Fix:** Force text handling on both git invocations (verified to restore the matches):
```sh
git grep -a -n -o -E --untracked -e "$PATH_RE"        # was: -I
git -c core.quotepath=false log -m -p --text --no-color --no-ext-diff --no-textconv ...
```
`-a` makes real binaries produce garbage matches rather than silence, so keep the `grep` output filter
tolerant, or add an explicit pass that fails when `.gitattributes` assigns `-diff`/`binary`/`-text` to a text
path. Add a seeded case for the attribute.

### CR-13: Determinism lint Layer 2 does not detect writable state in a custom-named section; both layers pass it

**File:** `tools/lint/lint-objects.sh:105-117` (Mach-O at 110-111, ELF at 113-116)
**Issue:** ADR-0006 states Layer 2 "fails on any non-empty writable data section" and that it "cannot be
bypassed by spelling". The implementation is a name denylist: Mach-O only `__DATA,(__data|__bss|__common|
__thread_)`, ELF only `.data/.bss/.tdata/.tbss/.sdata/...`. A variable placed with
`__attribute__((section(...)))` lands in a section the regex never names. Reproduced on Apple clang:
```c
#define T64_DECL(n) __attribute__((section("__DATA,__st"))) uint32_t n
T64_DECL(t64_hidden_state);
uint32_t bump(void) { return ++t64_hidden_state; }
```
`sh tools/lint/core-lint.sh --files sec3.c` prints `core-lint: files=1 violations=0`, exit 0. Layer 1 also
passes it, because the declaration through a macro is parsed as a function-like declarator
(`fp > 0 && dt[fp+1] != "*"` in `end_decl`). Without the macro Layer 1 does flag it
(`L1-GLOBAL-MUTABLE`), so the hole is the "authoritative" layer, not only the heuristic one. The same holds on
ELF for any section name outside the list. This is the exact "leak passes both layers" outcome the ADR's
Reversal evidence names.
**Fix:** Make Layer 2 allow-by-name for read-only content instead of deny-by-name for writable content.
On Mach-O treat every non-empty section outside `__TEXT,*` and `__DATA_CONST,*`/`__DATA,__const` as writable;
on ELF treat every non-empty section that is not one of `.text* .rodata* .eh_frame* .data.rel.ro* .comment
.note* .symtab .strtab .shstrtab .rela* .rel*` (plus `.init_array` family handled as constructors) as a
violation. Alternatively use `readelf -S`/`objdump -h` flags (WRITE) where available. Add a seed
`bad_custom_section.c` (using the macro form above) to `tools/lint/selftest` and an `L2-W-SECTION` EXPECT line.

## Warnings

### WR-11: `R-DENIED-EXT` is bypassed by a trailing space, newline or other character after the extension

**File:** `tools/rights/t64_blobscan.c:467-492`
**Issue:** `denied_ext` takes the text after the last `.` of the last component and requires it to be exactly
three bytes. A file named `a.sav ` (trailing space) or `b.sav<newline>` has an extension of four bytes and is
not denied. In history the git-quoted form `"b.sav\n"` is judged after dropping only the closing quote, so
`sav\n` is also four bytes. Reproduced: a repository holding `a.sav `, `b.sav<LF>`, `c.eeprom` and `d.srm`
with plain text content scans as `tree=6 history=3 findings=0`, exit 0. The policy says a denied extension
fails "even for harmless text"; content rules still catch real binary saves, so the impact is limited to
text-looking or allowlisted-by-digest payloads, but the control does not do what its selftest says. Related
gap: `.srm`, `.rom`, `.pak` and other common emulator save/ROM suffixes are not in the list; confirm the list
against the GOV-07 text, which names exactly the ten in `exts[]`.
**Fix:** Normalise before comparing: strip trailing whitespace, control bytes and dots from the component (and
from the unquoted git form, including a trailing `\n`/`\t` escape pair) and compare the remainder; or match
case-insensitively with `strcasestr`-style suffix tests on the trimmed name. Add seeded cases `ext-trailing-space`
and `ext-trailing-newline` to the selftest.

### WR-12: PII path pattern misses a bare home directory and several private-path forms

**File:** `scripts/pii-scan.sh:36`
**Issue:** Every alternative in `PATH_RE` needs a path separator after the user name
(`/Users/[A-Za-z0-9._-]+/`), so a user name that ends the token is invisible: `cd /Users/<user>`,
`HOME=/Users/<user>`, `"/home/<user>"`, `C:\Users\<user>` at end of line. Reproduced: `git grep -E` for the
pattern on a line `cd /Users/<user>` returns nothing. The design comment says prose naming a directory must not
match, but `/Users/<name>` names a person, not a directory. Also absent: `/root/`, `/Volumes/<name>/`,
`/mnt/c/Users/<name>/`, `/tmp/claude-*` style session directories that are not under `/private/tmp`.
**Fix:** Match the user name followed by `/`, whitespace, quote, `:` or end of line:
`/Users/[A-Za-z0-9._-]+([^A-Za-z0-9._-]|$)` (and the same for `/home/`, `C:\Users\`), keep the existing
`/home/runner` exemption, and extend the list with `/Volumes/`, `/mnt/[a-z]/Users/`, `/root/`. Add seeded cases
for each new form and for `prose-allowed` (`/home` alone, `/Users` alone).

### WR-13: Provenance "copied-looking" detection is narrower than the policy and is defeated by a BOM or a doubled comment leader

**File:** `tools/provenance/t64_provenance_check.c:341-366`
**Issue:** `scan_line` skips spaces and tabs, then at most one leader from `<!--`, `/*`, `//`, `--`, `*`, `#`,
`;`, then requires the tag at that exact position. `docs/policy/copy-provenance.md` defines copied-looking as
"its first 20 lines carry an SPDX license identifier tag". Reproduced with a license tag naming GPL-2.0-only
on the line: `src/ok.c` (`// <tag>`) and `src/multi.c` (block comment, ` * <tag>`) are flagged, while a file
that starts with a UTF-8 BOM, `/** <tag> */`, `/// <tag>`, `## <tag>` and `% <tag>` all pass with
`copied_looking=2` of 7 files. The BOM case defeats the most common placement (line 1) in files that come
from Windows tooling.
**Fix:** Skip a leading `EF BB BF`, strip any run of leader characters (`/ * # ; - ! % @ < >` and spaces)
instead of one leader, or search each of the first 20 lines for the tag substring (the REUSE rule). Add seeds
for BOM, `/**`, `///`, `##` and `%`, and decide in the policy whether a tag in the middle of a line counts.

### WR-14: Docs-asset allowlist is unvalidated, can waive ROM magic, and waives history blobs by digest alone

**File:** `scripts/rights-scan.sh:205-207`, `tools/rights/t64_blobscan.c:201-208,568,634`
**Issue:** `provenance/docs-assets.tsv` is read from the tree under scan and added to the allowlist with no
constraint. A row `path<TAB>sha256<TAB>reason` clears every content class including `R-ROM-MAGIC`
(`CONTENT_MASK` includes `C_ROM`), for any path (no required `docs/` prefix, no size cap, no extension rule
beyond the unwaivable denied-extension list), and in history the match is by digest only (`allow_match(NULL,
hex)`), so the same bytes pass under any name. The manifest path is validated by `t64-manifest-check`
(rights status, distribution channel); this second path is not. A PR that adds one row for a ROM renamed to
`docs/img.png` makes the gate green. Nothing in the repository (no CODEOWNERS) forces review of the file.
**Fix:** In the driver, reject a docs-assets row unless its path is under an allowed docs prefix, is not a
ROM-magic or container blob (compute with blobscan, or refuse `R-ROM-MAGIC`/`R-CONTAINER` waivers from this
file), and carries a non-empty reason; restrict history waivers to (path, digest) pairs; add a CODEOWNERS
entry for `provenance/docs-assets.tsv`.

### WR-15: CI workflow checker covers one file only and its secrets and trigger rules are bypassable

**File:** `scripts/check-ci-yml.sh:31,74-76,231`
**Issue:** The default target is `.github/workflows/ci.yml`. A second workflow file added under
`.github/workflows/` (for example one with `on: pull_request_target`, `workflow_run`, `issue_comment`, or a
write-scoped token) is never read, so every rule (`CI-PR-TARGET`, `CI-PERMISSIONS-TOP`, `CI-USES-PIN`, ...)
is only as strong as the number of workflow files being exactly one. Within the file: `CI-SECRETS` matches
`secrets[.:]` and `github.token`, so `${{ secrets['NAME'] }}`, `${{ toJSON(secrets) }}` and
`${{ github['token'] }}` pass; the only privileged trigger rejected is `pull_request_target`
(`workflow_run` and `issue_comment` run with repository context and tokens too).
**Fix:** Iterate over every `*.yml`/`*.yaml` file in `.github/workflows/` (fail on zero files) and add a rule
that the `on:` key set is a subset of `{pull_request, push, workflow_dispatch}`. Match `secrets` and
`github.token`/`github['token']` without requiring the following character
(`/secrets([^A-Za-z0-9_]|$)/`, `/github[.[]['"]?token/`). Add selftest mutations for each.

### WR-16: Lint passes a host cycle-counter read (no symbol, no banned token)

**File:** `tools/lint/core-lint.sh:187-197` (name tables), `tools/lint/lint-objects.sh:126-134`
**Issue:** The determinism boundary forbids the host clock, but intrinsics that read host state compile to an
instruction, not a call. Reproduced: `uint64_t t64_leak(void) { return (uint64_t)__builtin_readcyclecounter();
}` gives `core-lint: files=1 violations=0`, exit 0 (Apple clang; `__builtin_ia32_rdtsc`, `__rdtsc`,
`__builtin_arm_rsr64` behave the same). There is no undefined symbol for Layer 2 to see and no entry in the
Layer-1 name tables. Quoted includes of project headers are not checked either (`#include "..."` is skipped
by `check_include`), so a `static inline` wrapper in a header is also invisible to Layer 1.
**Fix:** Add a Layer-1 prefix rule for `__builtin_readcyclecounter`, `__builtin_ia32_rdtsc*`, `__rdtsc*`,
`__builtin_arm_rsr*`, `__builtin_arm_wsr*`, `__builtin_arm_isb`, `__builtin_cpu_*` under `L1-NAME-TIME` (or a
new rule id with a seed); resolve quoted includes against `core/include` and fail on any that leave the core
tree.

### WR-17: A ROM in a text encoding passes the rights scan; the only detector is an advisory

**File:** `tools/rights/t64_blobscan.c:454-461,504-505`, `docs/policy/rights-and-fixtures.md`
**Issue:** The policy lists "ROMs and ROM images, in any extension or encoding" as protected. The scanner
recognises ROM magic at byte 0 only, and the only encoding heuristic (`A-BYTE-LITERAL-RUN`, 256+ consecutive
`0x..` literals) is an advisory that never changes the exit status; decimal byte arrays (`128, 55, 18, 64,`),
base64 and hex text are not examined at all. Any such file is valid UTF-8 with no NUL and passes the GOV-07
classes. This is an inherent limit of byte classification, but the gate currently gives no signal beyond a log
advisory, and the 4032-byte advisory is similarly non-blocking.
**Fix:** Make a text blob above a size threshold (for example 256 KiB) that is not manifest-listed a finding
(`R-LARGE-TEXT`), and promote `A-BYTE-LITERAL-RUN` to a rule with decimal and base64 run detectors; state the
residual risk in the rights policy so "any encoding" is not over-claimed.

### WR-18: `rights-scan.sh` history and range arguments are passed to git as options and history stops at HEAD

**File:** `scripts/rights-scan.sh:144-151`, `scripts/pii-scan.sh:146-153`
**Issue:** `--range` is passed straight to `git rev-list --objects "$_spec"` and `git log ... "$_spec"` with
no `--end-of-options` and no `--` separator, so a value beginning with `-` (or an `--all`-style spelling)
changes what git does or is parsed as a path. CI does not use `--range` today, but both scripts document it for
push hooks. Separately, full-history mode scans only what is reachable from `HEAD`; blobs on other branches,
tags and `refs/pull/*` in a fetch-depth 0 clone are never examined, and the workflow triggers only on pull
requests and pushes to `main`.
**Fix:** Validate the range (`case $RANGE in -*) usage ;; esac`, and run `git rev-parse --verify` on each side)
and use `--end-of-options`; offer `--all` for the history pass and document which refs the gate covers.

## Info

### IN-11: `run-case.sh` and `run-suite.sh` clean up on signals but then keep running

**File:** `tests/controls/run-case.sh:55`, `tests/controls/run-suite.sh:62`
**Issue:** `trap 'rm -rf "$TMPD"' EXIT INT TERM HUP` removes the work directory on SIGINT/SIGTERM and then the
script continues (POSIX shells resume after a trap action), running further supervisor calls against a deleted
directory. `rights-scan.sh` and `pii-scan.sh` use the correct `trap 'exit 130' INT TERM HUP` plus an EXIT
trap. `run-suite.sh` also expands `$RESULTS` unquoted, which breaks on a `TMPDIR` containing a space.
**Fix:** Use `trap 'exit 130' INT TERM HUP` and `trap 'rm -rf "$TMPD"' EXIT`.

### IN-12: `ci-review.sh` does not detect a duplicate finding id within one invocation and rewrites the log non-atomically

**File:** `scripts/ci-review.sh:182-185,149`
**Issue:** Duplicate-id detection greps the log before any row is written, so two `--finding F-001|...` flags
in one call both pass and are both appended. `insert_row` ends with `cat "$_tmp" > "$_f"`, so an interrupt
between truncate and write loses the file content.
**Fix:** Track ids seen in `FINDING_ROWS` and refuse a repeat; write through `mv` from a temp file in the same
directory.

### IN-13: `t64-provenance-check` silently skips an unreadable file and a path behind a symlink

**File:** `tools/provenance/t64_provenance_check.c:779-801`
**Issue:** In `scan_one`, when `read_head` fails (`looking` stays 0) or `fs_check` returns `FS_BAD` for a path
outside `third_party/`, no finding is reported and the file still counts in `g_files`, so the run can end with
`ok`. The symlink case is covered by the rights scan, the unreadable-file case is not.
**Fix:** Report `P-HEADER` (cannot read the file header) on `read_head` failure and `P-PATH` on `FS_BAD`.

### IN-14: PII scan covers messages and author addresses but not author names, tags or non-HEAD refs

**File:** `scripts/pii-scan.sh:191-199`
**Issue:** Only `%ae` and `%ce` are checked. Author and committer names (`%an`, `%cn`), annotated tag messages
and tagger lines, and refs outside the `HEAD` ancestry are not read, although all of them are published with
the repository. Because the rule description says "author and committer addresses", this is a scope note rather
than a defect.
**Fix:** Decide whether names are in scope for AUTONOMY section 4; if so add `%an`/`%cn` against an allowlist
and run the scan over `git for-each-ref` objects.

---

_Reviewed: 2026-11-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_

<!-- part C -->

**Reviewed:** 2026-11-09
**Depth:** standard
**Files Reviewed:** 88
**Status:** issues_found

## Summary

Part C covers the build files, the stub core and facade header, the Rust-vs-C spike (harness, both parsers, the
Rust twins), the test headers and sources, the controls matrix with its goldens, and the manifest, schema and
provenance records. No structural-findings block or external-reviewer evidence was supplied. `necessity:`
assessment was not applied beyond a read of the guards in scope; no necessity finding was raised.

No critical issue was found. The registered spike result is not invalidated by anything below: I re-verified
that the 42 frozen hashes in PREREG.md match the working tree (`run-spike.sh --print-hashes` against the table,
byte-identical), and RESULTS.md shows `fuzz-exit` 0 with 2000040 inputs. The findings are about whether the
mechanism could have produced a different answer, and about one assertion that cannot fail.

What I checked and found sound (so the caller does not need to repeat it):

- **Goldens are independently derivable.** I recomputed every fixture digest (`shasum -a 256` over each script),
  the tampered digest (positive.t64s plus one `#` byte, `dfe78d84...`, matches), every log digest from `printf`
  of the capture text (including the 64-byte truncation for `log-over-limit` and the empty-string digest), the
  log byte and line counts, ticks, and the `logrep` expansion (9000 x 16 = 144000). All 19 goldens agree with
  matrix.tsv and with the scripts, and `INVENTORY` is byte-equal to the sorted matrix ids. `run-case.sh` compares
  goldens byte-for-byte with live output, so a dropped golden fails its own `controls.<id>` test.
- **Builds and tests.** I configured the repo out-of-tree twice (Release with `T64_WERROR=ON`, and the asan
  preset's flags), built with Apple clang under `-Werror`, and ran CTest. Both configurations build clean; the
  asan configuration passes 49 of 49 tests, including the abort, segv and hang controls. 49 tests are
  registered (`ctest -N`), which matches 19 control rows + 5 suite tests + the unit and gov and repo tests.
- **Admission parsers.** `admission.c` reads `buf[0..3]` only after `len >= 4096`; no overread, no UB, `NULL` is
  only reachable with `len == 0`, which returns before any access. C and Rust generators consume the RNG in the
  same order in every mutation arm (checked arm by arm, including the `rnext()` for the limit in case 2). The
  Rust parser is `forbid(unsafe_code)` and `no_std` in its library form.
- **CMake and presets.** Every target in scope calls `t64_target_warnings`; `_POSIX_C_SOURCE` is not defined for
  `t64_core`; the tools glob excludes `common` and adds it first, so `t64_common` exists before consumers;
  `noTestsAction: error` is set on the gate preset and inherited by asan; the matrix loop skips only blank and
  `#` lines and `controls.inventory` pins the row set; `unit.result-goldens` fails on zero goldens.
- **Test vectors.** SHA-256 boundary vectors (55, 56, 63, 64, 65 bytes of `a`) and the one-million-`a` vector
  match `shasum` run by me.
- **Manifest, schema, provenance.** `gov.manifest.repo` passes; the rights statuses (`pending`, `forbidden`,
  `local-only`) are consistent with AGENTS.md (no ROM, no digest, no protected data committed). The MADR copy
  record names license, notice, mode and modifications.

## Warnings

### WR-21: Spike `mechanical-decision` fails open when the fuzz does not complete or when anomalies occur

**File:** `docs/spikes/rust-vs-c/run-spike.sh:426-439, 462, 164-188`
**Issue:** `stay-c17` is the only outcome that auto-accepts ADR-0002 (A-3), yet the decision is
`[ _triggers = none ] && [ _b = no ]`, and `_b` becomes `yes` only if the fuzz exit is non-zero **and**
`fz.err` contains the string `admission.c`. Every other non-zero `FZ_RC` leaves `_b=no` and the decision at
`stay-c17`. That includes the harness's own exits (3 = calloc failed on the 64 MiB blocks, 4 = outcome code out
of range, 5 = `info-consistency` BAD, meaning the parser returned wrong `info` fields), a docker failure (125 to
127), an OOM kill (137) and a timeout. In each case the fuzz did not complete or found a contract bug, and the
script still prints `mechanical-decision: stay-c17`. The script never checks that `inputs: 2000040` was printed.
The same shape exists for rule (a): `print_class_table` counts `anomalies:` but the decision ignores it, so a
broken C lint (exit 2 on every class) or a missing `rustc` (exit 127) yields `rule-a-triggers: none` and
`stay-c17`. Also, the differential compares only the `boundary`, `mutations`, `inputs`, `outcomes` and `fnv1a64`
lines, so the C `info-consistency: BAD` line is not part of it either. The recorded run is clean (RESULTS.md:
`fuzz-exit` 0, 40 + 2,000,000 inputs, anomalies none), so this is a latent defect in the decision procedure, not
a wrong recorded result. It matters because PREREG allows re-runs through the amendment protocol.
**Fix:** Make the decision three-valued. Require, before `stay-c17`: `FZ_RC` equal to 0 (or non-zero with an
attributable report), `inputs: 2000040` and `info-consistency: ok` in `fz.out`, and `anomalies: 0`. Otherwise
print `mechanical-decision: inconclusive` and exit non-zero. For example:
```sh
_in=$(sed -n 's/^inputs: //p' "$T/fz.out")
_an=$(sed -n 's/^anomalies: //p' "$T/table.tsv")
if [ "$_b" = no ] && [ "$FZ_RC" != 0 ]; then _dec=inconclusive
elif [ "$_in" != 2000040 ] || [ "${_an:-1}" != 0 ]; then _dec=inconclusive
elif [ "$_triggers" = none ] && [ "$_b" = no ]; then _dec=stay-c17
else _dec=switch-rust; fi
```
Because the script is frozen, ship this as Amendment 1 with a new `run-spike.sh sha256:` line.

### WR-22: The C gate lint under test is not part of the frozen set, so the registered rule (a) result is not reproducible from the pre-registration

**File:** `docs/spikes/rust-vs-c/run-spike.sh:222-228` (frozen set), `docs/spikes/rust-vs-c/PREREG.md:70-75`
**Issue:** Rule (a) is "the full C gate lint exits 0 on the seed", run as `sh tools/lint/core-lint.sh --files`.
`frozen_paths` hashes the 18 C seeds, the 19 Rust twins, the 4 admission files and `run-spike.sh`, but not
`tools/lint/core-lint.sh`, `tools/lint/lint-objects.sh`, `tools/lint/rules.txt` or `tools/lint/allowlist.txt`.
The thing the rule measures can therefore change between registration and the run (or after it) without
`--verify-frozen` noticing, and RESULTS.md records neither a hash nor a commit for the lint. A later edit of
`rules.txt` or the allowlist can turn "exits 1" into "exits 0" for a class, or the reverse, with the pre-registration
still reporting "frozen inputs: ok". This undercuts the statement in PREREG that the decision is "only the rules
below, applied mechanically". (The same weakness also means the registered result says nothing about the lint
as it stands today; CI selftest covers that separately.)
**Fix:** Record the SHA-256 of the four lint files in RESULTS.md now, as a disclosed gap against PREREG. For any
future spike, add the tool under test to `frozen_paths` and to the table, for example by appending
`tools/lint/core-lint.sh tools/lint/lint-objects.sh tools/lint/rules.txt tools/lint/allowlist.txt` to
`frozen_paths` and adjusting the row-count check.

### WR-23: Rule (b) cannot fire by construction; the 2,000,040-input fuzz adds nothing over the ten-size boundary sweep

**File:** `docs/spikes/rust-vs-c/admission/admission.c:15-31`, `docs/spikes/rust-vs-c/admission/fuzz_admission.c:206-250`, `docs/adr/0002-language-build-and-abi.md:103-105`
**Issue:** After the three length checks, `t64s_admit` reads exactly `buf[0..3]`, with `len >= 4096` already
guaranteed; there is no loop, no length-dependent index and no pointer arithmetic. No input of any length can
make that code touch memory outside the block. The mutation schedule only varies length and the first 36 bytes,
so it exercises the same four loads 2,000,000 times. The only discriminating evidence is the seeded-overread
negative control, which shows the sanitizer is live but not that the fuzz reaches code capable of failing. The
ADR's "the clean result is not vacuous" (and RESULTS.md "Calibration") therefore overstates what was tested:
criterion (b) was nearly certain to read "not triggered" regardless of language. The same holds for the ADR
consequence "Mitigation: sanitizer fuzzing of the admission parser", which currently mitigates nothing about the
real, larger header parser that Phase 2 or 3 will need (header fields at 0x00 to 0x3F, CRC ranges).
**Fix:** Add to ADR-0002 "Evidence limits" that the spike parser's memory-access surface is four bytes and that
rule (b) was structurally unlikely to fire, so (b) is not evidence for C over Rust. Make the real admission
parser (when written) the fuzz target before relying on the mitigation, with a header-field read past 4 bytes
and a CRC or size-dependent loop, under the same exact-size-block harness.

### WR-24: The "more than 512 bytes" report assertion cannot detect removal of the size limit

**File:** `runner/tests/test_classify.c:388-392`
**Issue:** The comment says "More than 512 bytes is invalid even when every line is well formed", but `big` is
`"t64-report 1\n"` followed by 686 `x` characters and a final LF. The second line has no `stop_reason ` prefix,
so `t64_parse_report` returns 0 on line two regardless of `REPORT_MAX_BYTES` (`t64_classify.c:102`). Deleting
the `len > REPORT_MAX_BYTES` guard leaves this test green. A well-formed report longer than 512 bytes cannot be
built from the grammar (every line has a fixed, bounded form), so the guard is only a robustness cap and the
test as written proves nothing about it.
**Fix:** Correct the comment to what is tested ("a report with a malformed second line is rejected") and stop
claiming coverage of the cap. If the cap is meant to be load-bearing, make it testable: expose
`REPORT_MAX_BYTES` in `t64_classify.h` and call `t64_parse_report(valid_report, REPORT_MAX_BYTES + 1, &r)` on a
buffer that is a valid report followed by trailing bytes, asserting the cap (not the grammar) is what rejects it
by temporarily relaxing the grammar check in a dedicated test seam. Otherwise record in the source that the cap
is defence in depth and grammar-redundant.

## Info

### IN-21: Comments say ADR-0002 is still unaccepted, but it is accepted

**File:** `CMakeLists.txt:24-26`, `core/t64_core_stub.c:1-4`, `core/include/tendies64/t64_facade.h:4-6`
**Issue:** All three say core holds only two placeholder paths "until ADR-0002 is accepted" and that behavioral
core code "is not permitted". ADR-0002 now records the opposite ("other core files are legal"). The root
CMakeLists still declares `t64_core` inline with no `core/CMakeLists.txt`, which is correct only for the
pre-acceptance state.
**Fix:** Update the three comments to the accepted state and, when real core code arrives, move the target to
`core/CMakeLists.txt`.

### IN-22: Facade header leaves refused and edge cases undefined while the project rules require them first

**File:** `core/include/tendies64/t64_facade.h:9-16, 23-34`
**Issue:** AGENTS.md asks for refused-operation effects to be defined before an API is promised. The header is
marked provisional, so this is not a defect today, but the semantics already written are incomplete in ways the
synthetic runner will bake in: `advance(bound_tick)` with `bound_tick` below the current tick, `drain_guest_log`
with `cap` smaller than one record (partial record, and which tick `*tick_out` carries on the next call), `buf`
or `tick_out` NULL, and `T64_STOP_BOUND` versus `T64_STOP_HALT` when halt occurs exactly at the bound. The
`t64_stop_reason` enum is also returned by value in a struct with no explicit width, so its size is
implementation-defined; harmless while provisional, a trap once exported.
**Fix:** Add one sentence per case to the header comment now, and use a fixed-width integer for the reason
field at freeze time.

### IN-23: `cmake --workflow --preset gate` is a weaker gate than `scripts/gate.sh`

**File:** `CMakePresets.json:45-54`
**Issue:** The workflow preset is named `gate` but runs configure, build and `ctest --preset gate` only.
`scripts/gate.sh` additionally runs `sh -n`, `tools/lint/core-lint.sh --selftest` and `core-lint.sh` (lines
80-84), none of which are CTest tests (`tools/lint` has no `CMakeLists.txt`). A green workflow preset does not
mean the determinism lint passed.
**Fix:** Rename the preset (for example `build-test`) or register the lint selftest and `core-lint.sh` as CTest
tests (`add_test` in a new `tools/lint/CMakeLists.txt`, which the tools glob picks up).

### IN-24: Fuzz driver accepts a malformed `--mutations` value and reports a clean run

**File:** `docs/spikes/rust-vs-c/admission/fuzz_admission.c:183-185`
**Issue:** `strtoull(argv[++a], NULL, 10)` is unchecked, so `--mutations abc` runs zero mutations and prints
`info-consistency: ok`. The registered run passes no arguments, so it is unaffected, and the control uses a
literal `3000`. The file is frozen and corpus/admission files are never amended per PREREG, so this is
documentation-grade.
**Fix:** If the driver is ever reused, validate with an `endptr` check and exit 2 on trailing characters or an
empty string.

### IN-25: Rule (a) trusts the Rust exit code alone, so a twin that fails for the wrong reason counts as a rejection

**File:** `docs/spikes/rust-vs-c/run-spike.sh:107-114, 178`
**Issue:** A twin that exits 1 for any reason (a typo, a missing file, `build_macro.rs` failing because `HOME`
is unset in the container) is scored as "rejected". The error code is captured and printed (`rcode`) but never
compared with an expected code, so this can only push toward a false trigger, not hide one. PREREG's disclosure
that each twin was compiled once for the intended reason, and the frozen hashes, mitigate it; RESULTS.md also
lists the codes (E0433, E0133, E0599, a lint error), which agree with the intended mechanisms. Note that
`asm.rs` is rejected for E0133 (missing `unsafe`), and `ctor.rs` only because of the `unsafe_code` lint, as
RESULTS already observes.
**Fix:** For future spikes, store the expected rustc code per class next to the class table and fail the row as
an anomaly when the code differs.

### IN-26: Schema title for the forbidden branch promises "no identifying data" but the schema does not enforce it

**File:** `fixtures/manifest.schema.json` (second `oneOf` branch, properties `sha256`, `size`, `source`, `build`,
`expected`, `path`)
**Issue:** The branch title reads "a reason and no identifying data", but those six properties are declared as
`{}`, which accepts any value, so a forbidden entry carrying a SHA-256 is schema-valid. The prohibition lives
only in `t64-manifest-check` (seed `bad_forbidden_digest.json`). That is a legitimate design (the checker can
emit a specific rule id), but the schema title misleads a reader who relies on the schema alone, for example an
external validator.
**Fix:** Reword the title to say the checker enforces the rule, or tighten the schema with `"not": {}` for the
six properties and keep the checker rule as a second layer.

---

_Reviewed: 2026-11-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_

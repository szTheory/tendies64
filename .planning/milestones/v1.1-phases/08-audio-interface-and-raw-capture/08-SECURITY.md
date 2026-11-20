---
phase: 08-audio-interface-and-raw-capture
audited: 2026-11-19
asvs_level: 1
block_on: high
verdict: SECURED
threats_total: 28
threats_closed: 28
threats_open: 0
open_non_blocking: 0
findings_non_blocking: 3
audited_source: e15624cb (HEAD 08e403f is docs only)
---

# Phase 08 Security Audit: Audio Interface and Raw Capture

Verdict: **SECURED**. All 28 declared threats (22 STRIDE threats plus 6 `T-08-SC` package-installation rows) verified against code. No blocking or high finding. Three low or informational findings are recorded below; none counts toward `threats_open`.

Method: each declared mitigation was located in code at the cited file and line (ASVS L1: pattern present at the right boundary; the raw-capture and evidence-path items were additionally traced at L2 depth). `08-REVIEW.md`, `08-REVIEW-FIX.md` and `08-RECHECK-FIX.md` were treated as claims and re-derived from source. No full gate, ASan or `--native` run was started; no network call was made; nothing under `build/` was modified; no source file was edited.

Cheap local checks run (read-only): `sh scripts/check-ci-yml.sh` (ok), `sh scripts/pii-scan.sh --tree` (findings=0, 1111 files), `sh scripts/rights-scan.sh` (findings=0), `git grep` for tracked raw-artifact extensions (none) and workstation paths outside `.planning` (none; the `.planning` hits are `<name>` placeholder prose in Phase 01 pattern documents). Retained local ctest log `build/gate/08-recheck-final-normal-ctest.log` shows the 9 raw-capture and qualification tests passing on the macOS host.

## Threat verification

### Plan 08-01 (AI MMIO and sample event, observer, fixture)

| ID | Category | Sev | Disp | Status | Evidence |
|----|----------|-----|------|--------|----------|
| T-08-01 | Tampering | high | mitigate | CLOSED | `core/t64_ai.c:184-249` `submit_dma`: all refusals precede any mutation. Length masked (`:188`), then address staged, 8-byte aligned, `length <= T64_AUDIO_MAX_PACKET_BYTES` (0x3fff8, `core/t64_internal.h:76`) and `staged + length <= rdram_bytes` in `uint64_t` (`:198-202`). Queue full, sequence exhaustion and reserve-before-commit (`:205-231`). Address write refuses `masked >= rdram_bytes` (`:308-315`). Bus only reaches AI registers at width 4 (`core/t64_bus.c:454,637`). Controls: `core/tests/test_ai.c:1540,1556,1572,702`. |
| T-08-02 | Info disclosure | high | mitigate | CLOSED | `core/t64_ai.c:640-680` `t64_audio_copy`: output and length ranges built by `t64_internal_make_range`, must be disjoint from machine memory and from each other (`:647-658`); capacity checked against the stored packet length before `memcpy` (`:666-669`); `t64_audio_info` (`:623-638`) does the same for the info span. Packet storage is `T64_AUDIO_QUEUE_CAPACITY * T64_AUDIO_MAX_PACKET_BYTES`, zeroed at allocation (`core/t64_machine.c:2348-2362`) and per reservation (`core/t64_ai.c:522-526`). Alias controls: `core/tests/test_ai.c:1824-1893`. |
| T-08-03 | Spoofing | high | mitigate | CLOSED | Authored PCM literal and fixture/profile/input identities are pinned independently of any retained record in `scripts/qualify-audio.py` (`PCM`, `FIXTURE_SHA256`, `PROFILE_SHA256`, `:27-60`; enforced at `_public_projection` `:380-499` and `_verify_audio_observation_tail`). Wrong-sample control and reached-case checks: `scripts/tests/test_audio_qualification.py` (`test_changed_sample_rejected`, `test_wrong_profile_build_input_rejected`). |
| T-08-04 | Tampering | high | mitigate | CLOSED | Profile/source roster pins are carried by the existing digest-pinned inventory (`docs/spec/audio-cases.json`, `EXPECTED_INVENTORY_SHA256`, validated at `scripts/qualify-audio.py:327-378`) and the source/body gate (`scripts/check-rsp-no-hle.py`, 81 reviewed sources, per `08-RECHECK-FIX.md`; not re-run here). |
| T-08-SC | Tampering | high | mitigate | CLOSED | No package manager, download or install command in `.github/`, `scripts/gate.sh`, `scripts/qualify-audio.py` or `scripts/check-audio-cases.py`; `git diff` from the first Phase 08 commit to HEAD touches no package manifest, lockfile or toolchain receipt. |

### Plan 08-02 (registers, DMA state, cadence)

| ID | Category | Sev | Disp | Status | Evidence |
|----|----------|-----|------|--------|----------|
| T-08-05 | Tampering | high | mitigate | CLOSED | Register preflight before mutation: `core/t64_ai.c:293-374`. DACRATE/BITRATE accepted only if `rate_pair_valid` (`:80-87,357-370`); CONTROL accepts only bit 0 (`:319-321`); unknown offsets unsupported (`:371`); reads of write-only registers refused (`:264-267`). Queue indices are always `% T64_AUDIO_QUEUE_CAPACITY` (`:111-119,570-571`). The AI state is never deserialised (digest only, `core/t64_digest.c:747`), so descriptor lengths come only from `submit_dma` (always 8..0x3fff8 and 8-aligned, which makes `descriptor->length - 4u` at `:482` safe). |
| T-08-06 | DoS | high | mitigate | CLOSED | `sample_delay` (`core/t64_ai.c:54-78`): remainder and dacrate bounds, `delay > UINT64_MAX / SOURCE_CLOCK` and `scaled > UINT64_MAX - remainder` guards; `t64_ai_event_can_dispatch` (`:387-455`) pre-checks sample index, history count, generation, tick addition, queue slot and sequence width so an exhausted bound leaves the event queued unchanged. `t64_ai_event` reserves the next event before any state mutation (`:512-516` precede `:518+`). Control: `core/tests/test_ai.c:1540`. |
| T-08-07 | Tampering | high | mitigate | CLOSED | MI mutation limited to `t64_mi_set_pending/clear_pending(T64_MI_INTERRUPT_AI)` (`core/t64_ai.c:355,575,616`); reset cancels only `T64_AI_DEVICE` events and clears the AI state (`:597-621`). |
| T-08-08 | Repudiation | medium | mitigate | CLOSED | Software-policy table and the explicit "not claims about silicon" statements are present in `docs/spec/audio-interface.md` (lines 10-17, 31-60 of the file) and match the code constants (mask `0x3fff8`, ratio `66*(BITRATE+1)`, queue of two). Documentation item; verified by reading doc against code. |
| T-08-SC | Tampering | high | mitigate | CLOSED | As above. |

### Plan 08-03 (observer, output queue, runner drain)

| ID | Category | Sev | Disp | Status | Evidence |
|----|----------|-----|------|--------|----------|
| T-08-09 | Info disclosure | high | mitigate | CLOSED | Same code as T-08-02 (`core/t64_ai.c:623-680`). |
| T-08-10 | DoS | high | mitigate | CLOSED | Output admission: `core/t64_ai.c:486-489` (full queue with no reservation returns `QUEUE_FULL` with nothing committed), `:376-385` (`t64_ai_event_waits_for_output_capacity`), reservation slot recorded before first byte (`:518-527`); the drain frees slots without advancing guest time (`:676-678`). |
| T-08-11 | Tampering | high | mitigate | CLOSED | Observer position (`packet_head/packet_count`) is the only state touched by `t64_audio_copy`; producer progress, packet bytes and `packet_history_sha256` (`hash_completed_packet`, `:157-182`) are independent of it. |
| T-08-12 | Repudiation | medium | mitigate | CLOSED | Runner writes the real observer record (`runner/t64_real.c:1464-1480`), layout pinned by `_Static_assert` at `runner/t64_audio_observation.h:83-95` (LP64 guard `UINTPTR_MAX`, size 3976, region offsets 144/736/1120/1376). The verifier maps the same layout (`scripts/qualify-audio.py:88-175,1797-1990`), requires the exact size, and checks the tail against authored literals. |
| T-08-SC | Tampering | high | mitigate | CLOSED | As above. |

### Plan 08-04 (raw capture, filesystem and supervisor)

| ID | Category | Sev | Disp | Status | Evidence |
|----|----------|-----|------|--------|----------|
| T-08-13 | Tampering | high | mitigate | CLOSED | Descriptor-relative, no-follow path walk: `open_directory_nofollow` (`runner/t64_raw_capture.c:196-210`) and `open_directory_path` (`:212-265`) open each component with `O_DIRECTORY\|O_NOFOLLOW`, reject `.` and `..`; `split_target` (`:267-311`) rejects empty, `.`, `..` and over-long basenames. Target parent FD retained for the capture lifetime (`:1161-1165`). Existing target (including a symlink) refused with `AT_SYMLINK_NOFOLLOW` (`:1166-1170`). A mode-0700 staging parent is created with `mkdirat` and bound by FD with owner, device, inode and mode checks (`:370-427`), and the real bundle is a child of that FD (`:1175-1183`). Files: `O_CREAT\|O_EXCL\|O_NOFOLLOW`, `fstat` regular and `st_nlink==1` (`:571-587`). Publish: `rename_noreplace(stage_parent_fd, "bundle", parent_fd, target)` (`:1335`) with macOS `renameatx_np(RENAME_EXCL)` and Linux `renameat2(RENAME_NOREPLACE)` (`:313-329`); both are probed before guest start (`:429-471`); unsupported hosts fail closed (`:321-327`). The original CR-01 substitution race is closed because the source is resolved relative to the retained staging-parent FD, not by a re-looked-up name. Cleanup is identity gated and non-following (`:344-358,967-1034`). Test-only barrier and fault hooks are compiled out of production: every `getenv` is under `T64_REAL_RUNNER_TESTING` (`:119-177,528-547,1283-1334`), and that macro is defined only for the `t64-runner-test` twin (`runner/CMakeLists.txt:299-309`); production `main` has no `--advance-step` and no env fault (`runner/t64_real.c:1623-1668`); the supervisor `--test-fault` exists only under `T64_SUPERVISOR_TESTING` (`runner/t64_supervisor.c:466-469,623-644,1552-1555`). Evidence caveat: the macOS primitive and race ran locally (see retained ctest log above); Linux `renameat2` runtime evidence is pending the hosted job, as `docs/spec/raw-capture.md:115-117` states. The Linux code path is present and fails closed. |
| T-08-14 | DoS | high | mitigate | CLOSED | Caps: default 64 MiB, ceiling 256 MiB (`runner/t64_raw_capture.h:10-11`), validated in the runner CLI before guest start (`runner/t64_real.c:1637-1645`) and again in the supervisor (`runner/t64_supervisor.c:592-602`); `t64_raw_capture_open` re-validates (`:1110-1113`). All arithmetic is checked: `checked_add_u64` (`:179-184`) in `write_packet_file` (`:565-570`) and `append_manifest` (`:698-702`); pre-allocation capacity check `check_packet_capacity` (`:1063-1077`) bounds the `malloc` in `runner/t64_real.c:511-520,618-626`; entry counts bounded by `RAW_CAPTURE_MAX_PACKETS` (`:1209-1210,1252-1253`); manifest buffer bounded at 8 MiB (`:37,703-714,775`); `valid_video_info` and `valid_audio_info` check width, stride, height and byte-length arithmetic (`:785-842`). Short writes and `EINTR` handled (`:523-556`); write, fsync and close failures are reported, not ignored (`:590-618`). Controls: `controls.real.raw-capture-caps` and `-io-errors` (passing in the retained local log). |
| T-08-15 | Spoofing | high | mitigate | CLOSED | Manifest is generated from the actual packet info and byte lengths (`:690-783`); each file is re-validated by length, identity and link count (`regular_file_matches` `:508-521`, `validate_stream_files` `:917-965`) before the manifest is written last (`:1308-1316`); both streams must be nonempty (`:919-923,1286-1291`); only a guest-completion stop with a non-host-error outcome may finalize (`runner/t64_real.c:1497-1526`). Host failures become `harness-error` with closed-set `capture-*` details (`runner/t64_result_check.c:101-126`; every error code string in `t64_raw_capture.c` and `t64_real.c` appears in that table, checked by diff). Supervisor reports `runner-signalled` rather than inventing success when the child dies (`docs/spec/raw-capture.md:107-113`). |
| T-08-16 | Info disclosure | high | mitigate | CLOSED | Staging parent and bundle are 0700, files 0600 (`:370-427,473-506,571-587,868-892`); target keeps those modes, so the published bundle is owner-only. Stage names are generated (`:331-342`), not caller-chosen. The manifest has no host path, user name or target name; it holds only identities, counts, ticks and generated relative file names (`:712-774`). No capture file is tracked (`git ls-files` finds no `.rgba8`/`.s16be`/`.z64`/`.bin`), `build/` is git-ignored, and CI uploads only the six allowlisted JSON files (T-08-20). Residual (documented, see F-2): an abrupt kill leaves a private 0700 stage directory. |
| T-08-17 | Repudiation | medium | mitigate | CLOSED | Failure paths record a specific stage name in `capture->error` (for example `:1289,1306,1321,1337`) and the runner reports it as the result detail with `harness-error` and the actual guest tick (`runner/t64_real.c:1514-1516`); abort removes only identity-matched entries (`:981-1034`) and success is never reported after a failed publish. |
| T-08-SC | Tampering | high | mitigate | CLOSED | As above; raw files plus existing JSON only. |

### Plan 08-05 (qualification and CI evidence)

| ID | Category | Sev | Disp | Status | Evidence |
|----|----------|-----|------|--------|----------|
| T-08-18 | Spoofing | high | mitigate | CLOSED | `scripts/qualify-audio.py`: CTest command argv must equal the authored list (`_verify_race` `:1590-1620`), commands are only parsed with `shlex` and compared, never executed (`_parse_command` `:239-246`); records must resolve under the explicit build directory (`_under`, `:207-212`); producer result, build IDs, binaries hashes, manifest, observer tail and independent literals are cross-bound (`_capture_record` `:1987-2148`, `_verify_audio_observation` `:1829-1906`, `_verify_audio_observation_tail` `:1913-1985`). Mutation controls: `scripts/tests/test_audio_qualification.py:279,310,867-960` (37 tail mutations). |
| T-08-19 | Repudiation | high | mitigate | CLOSED | Pinned nonempty denominator and inventory (`:327-378`, `case_count > 0` at `:471-480,2392`); `LastTest.log` section must be unique and `Test Passed.` (`_test_section` `:215-224`); JUnit case must be unique and not failed/skipped (`_junit_passed` `:227-236`); discovery `LastTest.log` retention (`:1201`); resealed-omission controls at `scripts/tests/test_audio_qualification.py:927`. |
| T-08-20 | Info disclosure | high | mitigate | CLOSED | Public output is one `manifest.json` per job with an exact key set, hashes, counts and run identifiers only (`_public_projection` `:380-499`, `_projection` `:2334-2403`); `_scan_public` (`:1537-1562`) requires exactly one regular member, size at most 16 KiB, and rejects home/temp paths, e-mail addresses and raw artifact names. CI upload is an exact, SHA-pinned, six-file JSON list (`.github/workflows/ci.yml:45-56,87-98`) enforced by `CI-UPLOAD-ARTIFACT` in `scripts/check-ci-yml.sh:84-130,235-262` (name binds SHA, run, attempt and job; `if-no-files-found: error`; scans run before upload; `check-ci-yml.sh` passes). Private originals live under `build/gate/08-05-audio-private`, created 0700 (`:2366-2372`, `:2282-2331`), and are never in the upload list. Hosted collection is by artifact ID with a size cap of 1 MiB, API digest equality and exit-status check (`scripts/collect-boot-evidence.sh:263-288`, `scripts/qualify-audio.py:519-545`); ZIP extraction refuses absolute, `..`, backslash, NUL, symlink, encrypted, duplicate and unexpected members, bounds expansion, and extracts mode 0600 (`scripts/qualify-rdp-vi.py:1077-1150`, `MAX_UPLOAD_BYTES` 1 MiB); downloaded data is parsed, never executed. `rights-scan` and `pii-scan` also run in CI before upload (`ci.yml:37-43`). Mutation controls: `test_raw_upload_rejected`, `test_pii_projection_rejected`. |
| T-08-21 | Tampering | high | mitigate | CLOSED | Source snapshot taken before and after original verification and compared (`:256-296,2352-2360`, `:812-814`); inventory digest pin (`:327-378`); `check-rsp-no-hle.py` body/object roster (not re-run). `PHASE08_GCC_REQUIRED_UNITS` pins the unit list for the GCC 13 closure (`:1060-1080`, `EXPECTED_GCC13_IMAGE`). |
| T-08-22 | DoS | medium | mitigate | CLOSED | Two jobs, `timeout-minutes: 10` each, `permissions: {}` top level and `contents: read` per job (`.github/workflows/ci.yml:9,18-20,60-62`); ctest `jobs: 4` (`CMakePresets.json:37`); every Python stage is run with an explicit timeout via `_run_bounded` (`scripts/qualify-audio.py:1145-1160` and the plan at `:946-1000`); no new installs, cache or concurrency keys; `CI-TIMEOUT` and `CI-RUNNER-LABEL` enforced by `check-ci-yml.sh`. |
| T-08-SC | Tampering | high | mitigate | CLOSED | As above. |

## CI and command-construction checks (beyond the register)

- Triggers are `pull_request`, `push` to `main`, `workflow_dispatch`; no `pull_request_target`, no secrets or token reference, all actions pinned to 40-hex SHAs, `persist-credentials: false` (`.github/workflows/ci.yml`; rules enforced at `scripts/check-ci-yml.sh`, which passes).
- Run lines contain no `${{ }}` expression; values reach scripts through `$GITHUB_*` environment variables only. The emitter additionally requires its arguments to equal the real `GITHUB_*` environment (`scripts/qualify-audio.py:2406-2440`), and `--artifact-prefix` is regex-restricted (`:2415`, `scripts/check-audio-cases.py:154-159`).
- Every subprocess call is list-form (no shell): `scripts/qualify-audio.py:249,745,779,1145,1338,2531`.
- Hosted collection pins the repository literal (`szTheory/tendies64`) and the workflow path and event of the run record (`:555-572`).

## Findings (non-blocking; none counts toward `threats_open`)

### F-1: `T64_GATE_JUNIT_OUT` containment is lexical and admits `..` (LOW)

`scripts/gate.sh:99-107` accepts any value matching the glob `"$ROOT"/build/gate/08-05-audio-private/*`. A value such as `.../08-05-audio-private/../../../../x` matches the glob (verified with a throwaway `sh` case test using a placeholder root). The Python side resolves paths (`_under`, `scripts/qualify-audio.py:207-212`), but `gate.sh` does not. Exposure is limited: the variable is set only by a local operator or by `qualify-audio.py --native`, it is not set anywhere in CI, there is no privilege boundary, and `[ ! -e ]` prevents overwriting an existing file. Severity low, below `block_on: high`. Suggested fix: reject any value containing `/../` or ending in `/..`, or compare `cd`-resolved parent directories.

### F-2: Abrupt termination leaves a private raw staging directory (INFO)

If the runner is killed before commit (SIGKILL on wall-clock timeout, OOM, `SIGXFSZ`), a `.t64-stage-*` directory with partial raw bytes can remain in the destination parent. This is documented in `docs/spec/raw-capture.md:101-103`. The directory is mode 0700, owned by the runner's user, and never becomes a final bundle. Related note: the supervisor sets `RLIMIT_FSIZE` to `T64_REAL_RESULT_MAX_BYTES + 64 KiB` (about 68 KiB) on the runner (`runner/t64_supervisor.c:44,1534-1537`), so supervised capture of any single file larger than that would be killed by `SIGXFSZ` rather than failing with a `capture-*` detail. This is fail closed and has no security impact for the authored 12x7 fixture, but it is not stated in `docs/spec/raw-capture.md`. Consider documenting it.

### F-3: Raw-struct observer record may carry indeterminate padding bytes (INFO)

`runner/t64_real.c:1464-1470` writes `sizeof observations.audio` bytes of a native-layout struct to a private FIFO. The struct has internal padding (for example after the ninth `uint32_t`). `= {0}` initialisation (`:977`) does not strictly guarantee padding is zeroed, and struct assignment does not copy padding. The record crosses only a private FIFO to the owning test harness and retained build-private evidence, never a trust boundary, and the verifier reads fields rather than padding. No action required; a `memset` before use would make the record byte-deterministic.

## Unregistered flags

None. No `## Threat Flags` section exists in `08-01` to `08-04-SUMMARY.md` (`08-05-SUMMARY.md` is not yet written). No new attack surface was found that lacks a threat mapping: the new filesystem surface maps to T-08-13 to T-08-17, the evidence surface to T-08-18 to T-08-22, and the guest-controlled MMIO surface to T-08-01 to T-08-11.

## Review claims confirmed against code

| Claim | Result |
|-------|--------|
| CR-01 fixed: rename source bound to a retained staging-parent FD | Confirmed (`runner/t64_raw_capture.c:1335`, `:402-422`, `:1317-1322`). |
| W1: cleanup gate removed, identity-checked `rmdir` only | Confirmed (`:1015-1033,1345-1350`); the reasoning (symlink gives `ENOTDIR`, non-empty gives `ENOTEMPTY`, empty gives the swapper nothing) holds. |
| I3: identity recorded before early-return checks | Confirmed (`:389-394,483-489`). The refusal branches themselves have no dedicated control, as the fixer states. |
| I4: `fchmod` swap window in `create_stage` | Re-examined: `mkdirat` mode is at most 0700 (never wider), the opened FD is compared with the `fstatat` identity and owner before and after `fchmod` (`:402-422`), so a substituted entry is refused. No exploitable window found. |
| W2 and I6: layout pinned both sides, verifier checks tail | Confirmed (`runner/t64_audio_observation.h:83-95`; `scripts/qualify-audio.py:1797-1985`). |
| Test-only faults rejected by production CLI | Confirmed (see T-08-13). |

## Not verified here

- Linux runtime behaviour of `renameat2(RENAME_NOREPLACE)` and the hosted two-platform witness (pending hosted run, as documented).
- Full `--native` qualification, ASan run, GCC 13 closure and `check-rsp-no-hle.py` (explicitly out of scope for this audit; relied on the recheck report for those).
- Phase 07 helpers `scripts/qualify-rdp-vi.py` and `scripts/collect-boot-evidence.sh` were read only where Phase 08 depends on them (ZIP extraction, artifact-by-ID download), not audited in full.

threats_open: 0

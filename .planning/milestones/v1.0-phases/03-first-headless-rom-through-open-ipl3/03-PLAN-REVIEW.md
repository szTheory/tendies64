# Phase 3 Plan Review

## VERIFICATION PASSED

**Phase:** First Headless ROM Through Open IPL3  
**Plans verified:** 13 (revision 2)  
**Failed-item recheck:** All four prior findings are resolved; no actionable findings remain.

This pass is limited to those findings and their dependent paths. The revised ledgers record 13 sequential plans, 21 tasks, a maximum of nine files per plan and five per task. No Phase 3 implementation, build, test, CI run, or human validation is claimed.

## Prior findings and recheck

| Prior finding | Current resolution | Recheck result |
|---|---|---|
| Blocker — original 03-01 scope | Split into current 03-01–03 | **Resolved.** Current 03-01 owns seven files across two tasks; its first task remains the first actual IPL3-entry tracer. |
| Blocker — original 03-08 hosted evidence handoff | Current 03-13 tasks 1–2 | **Resolved.** Existing jobs must publish exact-SHA sanitized JSON; a required exact-run collector validates and retrieves both records into one workspace before the comparator runs. |
| Warning — original 03-04 scope | Split into current 03-06, 03-07 and 03-08 | **Resolved.** Each owns five files: ISViewer capture, core output/input submission seam, and canonical input parsing respectively. |
| Warning — original 03-06 scope | Split into current 03-10 and 03-11 | **Resolved.** The five-file v2 checker contract precedes the eight-file real runner/supervisor integration. |

### First real-entry and identity ownership

Current 03-01 Task 1 remains the first implementation task. It owns the private CPU/profile fields in `core/t64_internal.h`, machine initialization and digest changes, boot contract, and existing tracer. It qualifies the pinned bytes and retires an actual entry instruction through the existing per-instance scheduler callback path, with a persistent explicit unsupported stop. Task 2 owns `core/include/tendies64/t64_machine.h` and `core/t64_scheduler.c` for named public stop reasons and stop-aware scheduling. The public/private header and scheduler paths are declared in task `<files>` and read-first ownership; no hidden path is needed.

The three identities are distinct across the plans and result contract:

- `03-01` records the full upstream asset SHA-256/MD5, header region, executable payload offset/length/SHA-256, copied source span, and numeric SP-DMEM entry/destination separately.
- `03-02` verifies the upstream source asset, hashes and embeds the executable payload byte-for-byte, records permitted authored-header transformations, and separately records the normalized whole authored cartridge identity.
- `03-10` defines `ipl3_sha256` as whole source-asset provenance, `ipl3_payload_sha256` as the executed embedded payload, and `fixture_sha256` as the authored whole-ROM identity. `03-11` requires source-asset and payload verification separately during actual descriptor admission.

No task equates the upstream container hash with the embedded payload hash or authored ROM hash.

### Hosted evidence handoff

Current 03-13 Task 1 requires each existing exact-SHA Linux/GCC and macOS/Clang job to pass the real gate, run rights/PII staged-output scans, emit one canonical seven-key wrapper around its actual real-v2 pass, and upload only that JSON at a commit/run/attempt/job-specific artifact name. The upload path is pinned and missing output fails.

Current 03-13 Task 2 requires the collector to validate repository, workflow, head SHA, run ID, attempt, completion/success, exactly one successful job per required name, and exactly one matching artifact per job. It rejects missing, duplicate, wrong-SHA/run/attempt/job, failed, malformed, stale, unsafe, or extra-file inputs. It downloads only the two verified artifacts into bounded private temporary directories, validates the wrapped records against expected selectors, then places both records atomically in the comparator workspace. The comparator validates both records with the strict checker and requires matching guest identities, bound, transcript, and state while allowing different truthful compiler build IDs. Its selftests include fabricated same-host evidence; the actual collector path cannot substitute local samples for hosted records. Final verification requires collection and comparison on the actual selected run.

## Probe verdicts

- Verify-command-path probe: `not_applicable`, 73 commands, zero blockers and warnings, no read error.
- Failing-direction probe: `ok`, 73 commands, zero blockers and warnings, no read error.

These are the supplied authoritative verdicts; this review did not re-run or re-derive either probe. The revised plan checks also report valid structure/dependencies and retain the original decision, threat, privacy and edge-flag coverage.

## Structured Issues

```yaml
issues: []
```

No plan revision is required from this failed-item recheck. All planned runtime evidence remains pending execution.

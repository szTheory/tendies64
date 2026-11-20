# Phase 07 Plan 11 Segment A Evidence

This record covers the qualification helper, actual local producer wiring, exact artifact-layout validation, and CI emission preparation. It does not claim hosted collection, full native/ASAN matrix execution, hardware accuracy, or VID-04 completion.

## Source binding

- Starting source and preserved TDD RED commit: `738aa37b4c37f7a9ed0986f47d6d8f74d1c8b564`.
- Qualification source inventory SHA-256: `c915cc38ce78aeccc0d6718351f4968368f35126366e4b353a90037abc98385b`.
- The final verifier binds commands, process outcomes, executable hashes, profile/fixture/policy, source inventory, and raw bytes to private original records. Its controls accepted the original valid record and rejected missing raw packet, swapped executable hash, resealed empty case snapshot, missing v9 edge case, and a forged success report over a refused producer run.
- The private producer originals are exactly 24 RDRAM bytes at address `0x3000` and 48 VI packet bytes for a 4×3 frame with 16-byte stride. The RDRAM span is the authored fill-color buffer; the packet is the generated frame. The saved observation and file lengths agree. An earlier 384-byte packet statement was a reporting mistake; the saved original is 48 bytes.
- Private `.bin` files and command records stay under ignored `build/gate/07-qualification/`. CI uploads only a canonical JSON hex serialization of those same decoded bytes, with byte lengths, SHA-256 values, the RDRAM address, frame dimensions, and producer/fixture/profile/policy provenance, alongside the existing boot record. The full ROM, boot/IPL3 bytes, and private execution records are not in the upload.

## Commands and observed results

| Command | Result and retained record |
| --- | --- |
| `python3 -B scripts/qualify-rdp-vi.py --selftest` | Passed. Exercises the actual authored-source producer, comparator wrong-pixel refusal, deadline subprocess, and semantic verifier negative controls. |
| `ctest --test-dir build/gate -R '^harness\.rdp-vi-qualification$' --output-on-failure --no-tests=error --output-junit build/gate/07-qualification/harness-ctest-public-raw-final/junit.xml` | Passed 1/1. Retained stdout/stderr, exit code, unique JUnit, and LastTest before/after under `build/gate/07-qualification/harness-ctest-public-raw-final/`. |
| `python3 -B scripts/qualify-rdp-vi.py --deadline-selftest` | Passed: a real child process was terminated and reaped at the configured deadline (about 104 ms for the 100 ms case); no long-running command was used for this control. |
| `sh scripts/collect-boot-evidence.sh --selftest` | Passed actual local boot and RDP/VI producers, then exercised the exact combined archive layout through mocked GitHub transport. Final retained evidence: `build/gate/07-qualification/upload-layout-control.cG2wOQ/`. `control.json` records `transport=mocked-only`, `hosted_proof=false`, actual boot/private-original hashes, separately bound public serialization hashes and lengths, exact captured spans, five relative upload paths per job, clean PII/rights scans, and rejection of 28 archive/record mutations. |
| `sh scripts/rights-scan.sh --staging <job-upload-root>` | Passed for both actual combined text uploads in the selftest. No binary allowlist or rights waiver was added. |
| `python3 -B scripts/qualify-rdp-vi.py --scan-pii <combined-root>` | Passed for both complete combined uploads after strict member and provenance verification; the scanner saw the boot records and all public raw serialization files. |
| `sh scripts/pii-scan.sh --tree` and `sh scripts/rights-scan.sh --tree` | Both passed with 0 findings. Final precommit scan outputs and exit codes are retained under `build/gate/07-qualification/precommit-scans-public-raw-final/`. |
| `sh scripts/check-ci-yml.sh` | Passed workflow contract checks: two existing jobs, pinned actions, original ten-minute job limits, bounded raw emission/scan/upload steps. |
| `sh scripts/check-ci-yml.sh --selftest` | Passed 71 workflow mutation controls. |
| `cmake --preset gate` | Passed normal gate configuration. |
| `sh scripts/check-test-inventory.sh --preset gate` | Passed inventory count `157`; before/after discovery `LastTest.log` records are under `build/gate/07-qualification/test-inventory-yl9djsi9/`. |
| `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite vi-filters --run --artifact-prefix 07-11-segment-a-vi-filters` | Passed focused VI filter CTest (1/1). The source-derived inventory includes all three v9 next-row RDRAM boundary cases. Discovery, test, LastTest, JUnit, and process records were retained under `build/gate/`. |
| Per-binary `--list-cases` checks for the 11 owned RDP/VI binaries | All matched the source-derived suite aggregate. This establishes inventory agreement only; it is not full case-execution evidence. |
| `python3 -B` AST parsing of the edited Python files; `sh -n` on edited shell scripts | Passed. |

## Producer and upload layout

The test-only private export runs only after the actual guest producer reaches the comparator. RDRAM bytes come from the producer's bus read and the VI packet comes from the producer's `t64_video_copy` output; the helper does not replay a saved frame or generate exports from expected arrays. The canonical lowercase hex JSON is copied into the public upload only after the complete original-record verifier passes. The verifier checks that decoded public bytes equal the retained private originals, and the hosted-pair collector compares decoded bytes across hosts while checking each serialization's own length and hash. The collector verifies the existing boot JSON and exact upload whitelist at these archive-relative paths:

```text
<job>.json
rdp-vi/<job>/manifest.json
rdp-vi/<job>/inventory.json
rdp-vi/<job>/observation.json
rdp-vi/<job>/raw-export.json
```

The workflow uploads one combined directory per existing job. The path behavior was checked against the pinned [`actions/upload-artifact` v4.6.2 documentation](https://github.com/actions/upload-artifact/blob/v4.6.2/README.md#upload-using-multiple-paths-and-exclusions) and the [GitHub Actions workflow syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax). The local ZIPs test the same directory-root layout with actual producer output; mocked transport does not establish a hosted run or server-produced artifact.

## Failed attempts and scope limits

- An initial RED setup lacked the `t64-runner-real` sibling binary; this was corrected by retaining the complete executable closure. It was recorded as setup failure, not acceptance evidence.
- An early wrong-pixel control failed because its evidence output directory had not been created. The owning process recorder now creates output directories centrally; fresh successful records were produced afterward.
- The first upload-layout selftest reused an already-created destination directory. It now creates a dedicated `original-run` child; subsequent actual producer and mocked-transport run passed.
- A rights scan of the private binary originals was retained at `build/gate/07-qualification/rights-private-binary-refusal.Bin1/`; it correctly reported `R-NUL-HEAD`/`R-INVALID-UTF8`. The binary files remain private. The canonical textual upload representation then passed rights and PII scans without changing the rights scanner or adding a waiver.
- CMake regeneration during the focused case-check command relinked 59 C targets. No complete native gate, ASAN matrix, or live hosted artifact collection was run in this segment.
- No protected ROM/PIF asset, private guest, Angrylion oracle, physical device, paid service, or destructive action was used.

## Next action

Independent review of this source snapshot should precede the bounded native/ASAN matrix and any live hosted collection. The mocked transport control is local layout evidence only; exact successful hosted SHA/run/attempt/job/artifact bytes remain pending that later segment.

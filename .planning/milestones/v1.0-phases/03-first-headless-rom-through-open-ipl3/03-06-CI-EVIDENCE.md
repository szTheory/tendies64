# Coherent output candidate: hosted evidence

Source `0efc1846903c6d0271bf66f1d02e1af9bd954686` passed both existing jobs in
[push run 43007339324](https://github.com/szTheory/tendies64/actions/runs/43007339324)
on 2026-11-11. Linux x86_64 used GCC 13.3; macOS arm64 used Apple Clang 21.
Each job ran the canonical gate, checked the exact 78-test inventory, passed all
78 tests and core lint, and qualified its source-built tools and generated ROMs.

The tests include actual pinned IPL3-to-authored-hello execution with an exact
30-byte transcript and the actual 4097th-byte overflow refusal. Both hosts also
checked the same independent profile/state-v7 goldens. The guest still stops at
unsupported BREAK; this does not establish a public runner guest-pass or the
later final-state artifact comparison required by PROF-05.

Measured wall time was 301 seconds; the critical path was 299 seconds. Queue/run
times were 2/113 seconds on Linux and 4/295 on macOS. Gate durations were
110/281 seconds; fixture qualification consumed 81/199 seconds and CTest
13.28/44.51 seconds. The real hello/overflow test took 0.65/0.86 seconds.
The measured review and explicit decisions are in `docs/ci-reviews/LOG.md`,
findings F-026 through F-028. No extra hosted lane or timeout increase was used.

Working-tree and reachable-history rights/PII scans passed before the push.
Only the committed candidate was pushed; owner edits to instructions and GSD
configuration stayed local. Raw logs remain outside committed evidence.

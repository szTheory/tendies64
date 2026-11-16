# Hosted input/output candidate

Source `8b308cb4580c2c1f6fedbab0bd100ad6ddad3262` passed both existing jobs in
[push run 43020272408](https://github.com/szTheory/tendies64/actions/runs/43020272408)
on 2026-11-11. Linux x86_64/GCC and macOS arm64/Clang each qualified the updated
fixture producer, checked the exact 79-test inventory, passed all 79 tests, and
passed core lint.

This includes checked public output draining, timed trace-only input and the
production `t64.input` parser. The actual IPL3/hello input-partition control
passes on both hosts: exact output, bound inclusion, equal-tick source order,
no trace loss and equal canonical final states across tested partitions and
trace modes. It does not establish SI/controller consumption, real-runner
guest-pass, or the final cross-job state artifact comparison.

Linux queue/run was 2/106 seconds; macOS was 2/235 seconds. Script durations were
101/227 seconds, fixture qualification 74/171 seconds, and the real input control
0.25/0.22 seconds. The measured aggregate and decision are recorded in
`docs/ci-reviews/LOG.md`, finding F-029. Standard jobs and existing timeouts were
preserved. Tree and reachable-history rights/PII scans passed before the push;
raw logs were not committed.

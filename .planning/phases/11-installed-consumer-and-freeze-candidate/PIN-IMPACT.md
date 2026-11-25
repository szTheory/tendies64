# Phase 11 pin-impact record

Planning record of Phase 11 (Installed Consumer and Freeze Candidate), opened by plan
11-01 (phase-wave W0, no production edit). Each later plan appends its own rows; the
sections below are written in the order the plans fill them. Evidence tier: unit-tested,
authored scope; a software-interface claim only.

## Wave bases

Format and rule (standing rule "Base records" of the Phase 11 plans): a base is the
40-hex output of `git rev-parse HEAD`, written here as exactly one line per label,
`- <label>: <40 lowercase hex>`, in a commit that changes only this file. The Wave 0
record below is the one exception: it is committed with Task 1 of plan 11-01, whose
commit has this base as its parent. Later labels ("Install base" by plan 11-15,
"Freeze base" by plan 11-21, "Qualification base" by plan 11-25) are appended by their
plans. No check uses the moving reference HEAD as a base.

- Wave 0 base: f9124ae9fce134f31b645c6f2911a6f27fa41b9a

# Phase 12 pin-impact record

Planning record of Phase 12 (Minimal Host, Playable Homebrew and Final Bind), opened by plan
12-01 (phase-wave W0, no production edit). Each later plan appends its own rows; the sections
below are written in the order the plans fill them. Evidence tier: unit-tested, authored scope;
a software-interface claim only.

## Wave bases

Format and rule (standing rule "Base records" of the Phase 12 plans): a base is the 40-hex
output of `git rev-parse HEAD`, written here as exactly one line per label,
`- <label>: <40 lowercase hex>`, in a commit that changes only this file. The Wave 0 record
below is the one exception: it is committed with Task 1 of plan 12-01, whose commit has this
base as its parent, and it is taken only after `git status --porcelain -- .planning` printed
nothing, so the base commit contains every Phase 12 plan, `12-VALIDATION.md`, the research and
the context. Later labels ("Host base" by plan 12-06, "Bind base" by plan 12-12,
"Qualification base" by plan 12-18) are appended by their plans. No check uses the moving
reference HEAD as a base.

- Wave 0 base: 60b9e3c34fc0efc0ff64296cfa31415565b847b2

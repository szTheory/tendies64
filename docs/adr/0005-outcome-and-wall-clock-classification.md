---
status: accepted
date: 2026-11-09
decision-makers: szTheory
consulted: none
informed: none
---

# Classify every run into six outcomes, and treat a wall-clock kill as a harness error

**Status:** accepted

## Context and Problem Statement

The runner skeleton and every later emulator run must turn a process outcome into one result that cannot be
mistaken for evidence of something it is not. A guest that spins forever, a core that hangs, a runner that
crashes and a guest that prints nothing are different facts. The research default classified a wall-clock
kill as `timeout`. Negative controls assert `timeout` for a spin-forever guest, so a hung core must not be
able to satisfy that assertion. How are outcomes ordered, and what does a wall-clock kill mean?

## Decision Drivers

- Pass must require a positive match, never the absence of a failure (AGENTS.md: a boot or an exit status
  alone is not evidence).
- A supervisor must produce a result even when the runner is killed or crashes, and must not trust the
  runner's own exit code.
- Guest time and host time are separate. Only guest time describes the guest.
- The classification must be one ordered function that unit tests can pin, with a closed set of detail
  values.

## Considered Options

- A wall-clock kill is `timeout` (the research default).
- A wall-clock kill is `harness-error` with detail `wall-backstop`.

## Decision Outcome

Chosen option: "A wall-clock kill is `harness-error` with detail `wall-backstop`", because `timeout` then
means only that the guest-time bound was reached. A hung core cannot satisfy a spin control, and host load
cannot turn a slow machine into a guest verdict.

Locked rules:

- Six outcomes, with this precedence (the first match wins): `harness-error`, `missing-fixture`,
  `guest-fail`, `boot-fail`, `timeout`, `pass`.
- `pass` is reported only on a positive match of the expected verdict.
- A wall-clock kill is `harness-error` with detail `wall-backstop`. `timeout` means only that the guest-time
  bound was reached.
- Empty guest output is `boot-fail`, whether the stop was the bound or a guest halt.
- Exit codes: pass 0, harness-error 1, guest-fail 10, boot-fail 11, timeout 12, missing-fixture 13.
- Any other code, including 134 and 139, is a `harness-error`. The supervisor never trusts the runner's exit
  code; it writes the result and derives its own exit status from the outcome.

| Outcome | Exit | Meaning |
|---------|------|---------|
| `pass` | 0 | the expected verdict matched positively |
| `harness-error` | 1 | the harness, runner or wall-clock backstop failed; no guest verdict |
| `guest-fail` | 10 | the guest reported failure or malformed output |
| `boot-fail` | 11 | the guest produced no output |
| `timeout` | 12 | the guest-time bound was reached |
| `missing-fixture` | 13 | the fixture is absent |

### Consequences

- Good: a hung or crashed core is always visible as a harness error, never as a guest result.
- Good: one precedence table serves the supervisor, the suite consumer and the claims gate.
- Bad: a real core that is merely slow on the host reports `harness-error`, so the wall budget must be set
  generously and reported separately from guest time.

### Confirmation

`docs/spec/result-protocol.md` states the protocol and the detail enum. `tests/controls/matrix.tsv` is the
control matrix: each row names the exact expected outcome and detail, not merely a non-zero exit, and phases 3
and 4 replay the same matrix against the real core. `unit.classify` pins the precedence function.

## Reversal evidence

This decision is wrong if a real core case exists where guest progress cannot be observed independently of
wall time, so that the guest-time bound can never be reached and only a wall-clock kill ends the run. Such a
case would need a documented stop condition that is not a harness fault, and this record would be superseded
by one that defines it. A preference for a shorter outcome list is not evidence.

## Pros and Cons of the Options

### A wall-clock kill is `timeout`

- Good: fewer outcomes to explain; matches common test runners.
- Bad: a hung core satisfies a negative control that asserts `timeout`, so the control cannot detect it.
- Bad: host speed leaks into a guest verdict.

### A wall-clock kill is `harness-error` with detail `wall-backstop`

- Good: `timeout` keeps one meaning, guest-time bound reached.
- Good: a hang is distinguishable from a spin in the control matrix.
- Bad: a slow host needs a larger wall budget.

## More Information

- Related: `docs/spec/result-protocol.md`, `runner/t64_classify.c`, `tests/controls/matrix.tsv`.
- Detail values are a closed enum in the protocol; this record fixes the outcomes, the precedence and the
  exit codes.

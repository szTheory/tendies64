# Local whole-BASE diagnostic protocol

Status: implementation contract for the private, local-only Phase 06 BASE
diagnostic. Authored controls contain synthetic members only. External source
identities, assets, tool receipts, inventories, and results remain in ignored
owner-only build storage.

## One phase-wide source authority

`runner/t64_base_diag_protocol.c` owns the complete phase-wide member
denominator and outcome completeness. CPU and RSP protocols keep their existing
schemas and component contracts; neither is used to infer the whole-BASE
member set. A member is identified by its source registration index and value
index. Display names are descriptive and may repeat.

The private source derivation uses the independently accepted global CPU
projection and the accepted RSP partition of those same source ordinals.
Required members preserve source value order. Non-CPU members remain
in the denominator, with execution gated by their exact accepted component
module/asset closure. Missing device or asset closure remains an individual
blocked reason. A zero-value registration has one
`once` member with an empty value identifier. Optional CPU lanes outside the
default BASE dispatch are not promoted into the required denominator. Supported
timing/cycle/CP0-hazard and stress rows retain separate non-required lanes.
One compiled constructor represents each registration; its value ordinals use
the shared four-byte selector rather than duplicate constructor instances.

The canonical `t64.base-source-authority` v1 document contains the complete
ordered `members` array and `source_revision`. The canonical
`t64.base-inventory` v1 document repeats the exact member array, source
revision, and `source_authority_sha256`. The parser accepts an independently
checked expected authority digest from the immutable context. It refuses
reduced source and inventory pairs even when both are resealed. Exact private
counts and member identities are not published.

Each member has exactly `execution`, `group`, `index`, `lane`, `level`, `name`,
`reason`, `required`, `value_id`, and `value_index`. `execution` is one of
`values`, `once`, or `blocked`. Arrays preserve source order; canonical JSON
uses closed keys and a trailing newline. Inventory and authority inputs are
bounded to 4 MiB, 131,072 members, and 512 bytes per text field.

## Original member records

The legacy canonical `t64.base-member-record` v1 document binds the inventory digest,
source registration/value indices, one outcome (`pass`, `fail`, `incomplete`,
`not-run`, `guest-stop`, or `harness-error`), and the SHA-256 of the original
member bytes. Records are bounded to 8 KiB. The C aggregate verifies the exact
original bytes before accepting a result, rejects unknown or duplicate
members, and poisons itself after malformed or mismatched input. A claimed pass
or fail additionally requires the canonical shared guest transcript, matching
source/value ordinals and verdict. Legacy records cannot establish a bound
whole-suite pass.

Version 2 binds the immutable context SHA-256, actual core build identity,
profile ID and profile SHA-256, selected ROM SHA-256, original capture SHA-256,
and execution observations. The expected selector image digest comes from the
independently qualified image closure, separately from the record. Capture is
bounded to 4,096 bytes. Tick and attempted capture total are canonical unsigned
decimal strings; tick cannot exceed the existing ten-million-tick case limit.
A pass/fail requires exact, lossless capture, observed guest entry, and at least
two consecutive observed terminal jump/NOP pairs at the qualified park address.
Changed delay-slot instructions and incomplete sequences are refused.

The C aggregate exposes three independent predicates:

- `source_complete`: every source row, including informational and blocked
  members, has one valid identity-bound disposition.
- `authenticated_execution_complete`: source-complete, with every required
  member producing a genuine authenticated pass or fail.
- `all_pass`: authenticated execution is complete and every required member passes.

The closed version-2 outcomes add `timeout`, `blocked`, `unexecuted` and
`refused`; legacy `not-run` is refused in version 2. Blocked receipts require
the exact source-owned reason and no guest progress or selected image.
Unexecuted receipts require a bounded reason (`not-selected`, `interrupted` or
`window-exhausted`) and no guest progress. Attempted refusals, incomplete
results, timeouts and harness errors retain their separate reasons and expected
selected-image identity. Missing records remain missing; they are never silently
converted into unexecuted results. Invalid, duplicate or foreign additions poison
completion.

`t64.base-classification` v1 is a C-produced canonical report with source-order
member rows, group/lane counts, target identities and the three predicates.
Arrival order cannot change its bytes. `reached` counts attempted records,
excluding blocked/unexecuted receipts; it does not assert guest-entry execution.
Metadata reports have a separate 64 MiB bound, without increasing guest capture.
Python derives source membership and prepares private files; C alone calculates
counts and completeness.

`--classify-manifest SOURCE SHA INVENTORY CONTEXT_SHA INPUT REPORT` consumes
canonical `t64.base-classification-input` v1. Each `members` row has exactly
`expected_rom_sha256`, `original`, `outcome`, `reason`, `record`,
`registration_index` and `value_index`. `record` authenticates existing v2 bytes;
`blocked`, `unexecuted`, `timeout` and `harness-error` produce and retain C-encoded
receipts plus empty original captures. Output paths must be fresh private files.
Exit zero means source-complete classification, including honest blockers; the
private report states execution completeness and passing separately. Exit three
retains a source-incomplete report; malformed inputs exit four.

## Immutable context and pre-use ordering

The private context is fixed under `build/private-base-diagnostic` with
owner-only directory and file permissions. A separate binding records the
canonical context, inventory, source authority, and accepted source projection
digests. The verifier independently checks the source projection seal and, in
the production repository, revalidates it through the existing CPU
qualification authority. It then derives the expected BASE authority and
checks the exact source, tool, lock, runtime, boot, image, composition, profile,
build, compiler, asset, executable, and verifier file bindings. Missing files,
symlinks, unsafe write permissions, changed bytes, an unapproved profile, or an
already-created run directory stop dispatch.

Before activating a context, the builder retains an immutable owner-only pair
under `build/private-base-diagnostic/execution-snapshots/<context-sha256>/`:
the exact executable bytes and
the exact context and binding bytes. The verifier derives that location from
the active context bytes, checks the pair against the active binding, and
requires the retained executable to match the active executable hash before
dispatch. Retention preserves build evidence; it does not authorize an older
context or executable after the live binding changes. Existing snapshots are
never repaired or overwritten.

`verify_and_dispatch` completes every check before invoking normal dispatch.
The callback can receive the already verified context and exact input bytes,
so dispatch need not reopen a pathname after verification. The caller supplies
a fixed in-process dispatch path; context data cannot name
an arbitrary command. Executable bytes are hashed before use. Pre-use refusal
occurs before a case directory exists. Later preparation/write failures launch
no guest and retain any partial private inputs as a harness error.

Context version 2 adds the independently verified RSP source projection and
fixed RSP qualification, source-closure and overlay roles. Its source provider
re-derives the RSP partition from the fixed CPU authority and verifies the
existing component's source/assets/tools/locks without invoking its old identity
consumer. A new combined image is a separate Tier B/T1 artifact; the component
qualifiers and their historical results are not relabeled.

Before re-derivation, original pre-combined state remains immutable. Later
partially staged or qualified control inputs are retained in content-addressed
private history with a `forensics-only` purpose. This permits a corrected
qualification to preserve failed intermediate inputs without overwriting or
repairing historical evidence. Each activated context retains its exact
executable/context/binding pair before first use.

## Serial dispatch and lanes

`run-base-diagnostic.py --run-all --local-only --adapter
build/gate/runner/t64-base-diagnostic` performs one serial required sweep.
Each fresh case passes the full independent pre-use gate, then consumes frozen
authority/inventory bytes and a verified exact copy of the retained consumer.
The guest image must match its independent selector identity before launch.
The native worker has the existing ten-second wall limit inside the
twelve-second host deadline, with ten million guest ticks and 4,096 capture
bytes. Native worker reaping is observed explicitly. A timeout remains a
timeout even if cleanup returns zero; uncertain cleanup is a harness error.

An immutable context/lane marker prevents a repeated invocation from silently
retrying or overwriting cases. Stops preserve completed and partial original
files, and remaining source rows receive explicit dispositions. A launched but
uncertain case is not relabeled as unexecuted. `--verify-complete --local-only`
re-authenticates the original records through C and reports source completeness,
not passing execution. Source, tool or target changes require a distinct
qualified context; historical results are never pooled into it.

## Phase 06 evidence verification

`scripts/check-base-evidence.py --check-current` verifies retained local BASE
records without starting a guest. The execution-summary index is used only to
locate the separate required, informational and stress batches. For each batch,
the checker authenticates the copied context and binding against the immutable
execution snapshot, checks the bound source-authority and inventory bytes, and
passes the retained member records and captures back through that batch's exact
archived C consumer. It compares the regenerated C classification with the
retained C report. Python checks the source-derived member authority; it does not aggregate
result totals or decide completeness.
The C report remains the sole source for those fields; the command prints only a
sanitized verification status.

`--check-artifacts` performs the same saved context, binding and snapshot checks for
the saved qualified artifact. The separate `--check-preuse` mode also checks
the live executable against its bound digest. It does not rebuild or dispatch a guest. Both modes reject missing
or changed archive components. A matching hash establishes byte identity within
the saved binding; it does not establish legal rights or same-user execution
attestation.

The fixed operator commands are:

```
python3 -B scripts/base-diagnostic-lanes.py --run-required --local-only
python3 -B scripts/base-diagnostic-lanes.py --run-informational --local-only
python3 -B scripts/base-diagnostic-lanes.py --run-stress --local-only
```

The wrapper supplies only a fixed lane enum to the normal qualified runner.
It exposes no selector, profile, tick, capture or command override. CI and
required-fixture environments refuse before dispatch. Its private invocation
receipt references the C report and records the observed operator identity;
it does not calculate completeness or copy diagnostic counts. An
`authenticated_terminal_observed` flag requires a bound C pass/fail from the
selected lane, which already requires actual guest entry and terminal-loop
observations. Definitions, blocked/setup receipts and other lanes cannot set
that flag. Partial guest progress without a terminal remains in original records.

The stress command is the explicit local scheduled entry: the owner invokes it
under the existing limits. It installs no recurring host service or external
CI job. Existing authored hosted stress is separate evidence. Defining this
command, or retaining a blocked/unexecuted receipt, establishes no external
stress execution.

## Scope and evidence

Phase 6 authorizes the permitted required partition on the selected Tier B/T1
target and separate informational/stress selections, under the existing CPU/RSP
bounds and local rights controls. Original output, captures, source/tool bindings,
member identities/counts and exact consumer artifacts remain private. An
unsupported target or blocked member is an honest retained observation. A lane
definition or authored harness control does not establish external execution.
This protocol does not authorize another tier, physical hardware, uncleared
graphics assets or Phase 7 device implementation, and establishes no hardware
accuracy claim.

The saved-artifact mode validates the immutable archive independently of the
current build tree. `--check-preuse` additionally requires the live executable
to match the saved binding, and refuses drift after a rebuild. Neither mode
launches guests. Historical contexts retain their own host/tool identities;
independently sealed CPU/RSP source derivation authenticates their full member
authority before original records return to C for classification.

The public authored receipt is checked against original CTest blocks, unique
wrapper command/outcome records, their retained log digests, and the exact
archived runner/test/ROM binding. Runtime profile hashes, terminal exceptions,
retirement tails and transcript bytes come from those original observations.
Model/state-version fields absent from those records are not receipt results.
Reports and specifications cannot substitute for the original observations.
These integrity checks assume the reviewed producer and a trusted local owner;
they are not cryptographic execution attestation against that owner.

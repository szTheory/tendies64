# Input replay: t64.replay v1, t64.replay-report v1 and the runner surface

## Scope and evidence

This document specifies the host-side operation log that records the input calls
a run made, the player that replays them, the identity binding, the closed
report record and the runner flags that drive them (plan 09-07, D-34 and D-35).
Everything here is runner-side: the core knows nothing of replays and no core
source, public header or result record key is added. No API or ABI freeze is
implied; format keys, flag names, the module API and fixture row fields are
provisional until the Phase 11 freeze candidate (final ABI bind at the end of
Phase 12).

What the evidence is. The behavior is regression-pinned and unit-tested: an
authored ROM, driven by an independently authored input script, is recorded
through the real supervised runner and replayed to the same checkpoint digests.
The checkpoint digests are regression pins taken from a reference path that does
not use the recorder; they are not hardware values. Portability across builds,
platforms or profiles is not claimed, equality across submission schedules is
not claimed, and no hardware, oracle or differential statement is made.

What is not implemented: state-anchored replay (`boot` other than `cold-admit`),
cross-build or cross-profile replay, a binary replay schema, import of a save
during a run and rewind.

## File format

A replay is one canonical JSON document: keys sorted in ascending byte order, no
whitespace, one trailing LF, integers only, every u64 a decimal string and every
digest 64 lowercase hex characters. The parser is closed: unknown, missing and
duplicate keys, wrong types, nesting beyond depth 4, strings beyond 160 bytes,
numbers beyond 2^53 and any non-canonical number are refused. The identity of a
replay is the SHA-256 of its canonical bytes.

Root keys (all required):

| Key | Type | Rule |
| --- | --- | --- |
| `boot` | string | `cold-admit`; `state` is reserved and refused with `boot-unsupported` |
| `build_id` | string | the build identity grammar (fixed prefix plus 64 hex) |
| `content_sha256` | string | 64 lowercase hex, SHA-256 of the admitted image |
| `content_size` | decimal string | size of the admitted image in bytes |
| `descriptor_version` | integer | at least 1 |
| `input_schema_version` | integer | 1 |
| `machine_model_version` | integer | at least 1 |
| `ops` | array | the operations, see below |
| `profile_digest` | string | 64 lowercase hex, the profile hash |
| `profile_id` | string | 1 to 63 characters of a-z, 0-9 and `-` |
| `save_init` | object | `{kind: blank}` or `{kind: image, length, sha256}` |
| `schema` | string | `t64.replay` |
| `schema_version` | integer | 1 |
| `source_revision` | string | 40 lowercase hex; informational, not bound |
| `state_serialization_version` | integer | at least 1 |

`save_init` is `blank` (no save device, or a device whose bytes equal the blank
pattern, 0xff for EEPROM and 0x00 for SRAM) or `image` with the length (a
decimal string from 1 to 1,048,576) and the SHA-256 of the content the run
started with. The image form is verified in process only: the real runner
refuses replay together with the save flags, so a supervised run records `blank`
only.

Operations, each an object with a closed key set:

| Op | Keys | Ranges |
| --- | --- | --- |
| input | `at`, `buttons`, `event_tick`, `op` = `input`, `port`, `x`, `y` | `at` and `event_tick` decimal strings; `buttons` 0 to 65535; `port` 0 only (K7); `x` and `y` -128 to 127 |
| checkpoint | `at`, `digest`, `op` = `checkpoint` | digest is the machine digest after the advance to `at` |
| end | `at`, `digest`, `op` = `end`, `stop` | `stop` is `guest-bound` or `guest-completion` |

## Operation semantics

- `at` is the guest tick at which the host submits the op (the completed tick of
  the previous advance). `at` never decreases along the array; equal `at` runs in
  array order.
- An input op calls the real `t64_machine_queue_input` with `event_tick`; the
  event is stamped `event_tick`, which is never below `at`. The core latches the
  value so that a poll sampled at tick E sees the last input stamped strictly
  before E (the strictly-after rule of the controller device).
- A checkpoint is taken after the advance to `at` and before later ops at that
  tick.
- Exactly one end op exists and it is last. The end op records the terminal stop
  kind and the final digest.
- Boot is cold admit only; port 0 only; the t64.input v1 parser and its port
  rule are unchanged.

## Caps

| Cap | Value |
| --- | --- |
| document bytes | 4,194,304 |
| ops, the end op included | 65,536 |
| checkpoints | 4,096 |
| report bytes | 4,096 |

The byte cap is checked before any parse. The op and checkpoint counts are taken
from the parsed node counts and checked before the decoded op array exists; the
node limit is derived from the op cap (8 per op plus 64), so the transient node
array is bounded before allocation. The op cap is not reachable at the default
byte cap with input ops (a minimal canonical input op is about 75 bytes against
4,194,304 bytes), so the parser takes an explicit limits structure and the cap
refusals are exercised through lowered limits on the same code path; the default
constants are asserted by value and the 4,097th checkpoint is refused at the
defaults. Measured once: parsing a 49,000-op document of 3,626,654 bytes peaked
at about 43.2 million bytes of resident memory.

The recorder refuses an append that would pass a cap before the machine call
(one op slot stays free for the end) and the replay is then not published.

## Identity binding

Before any guest advance the live machine must equal the recorded header. The
fields are compared in a fixed order and the first mismatch is reported with its
name:

1. profile: `profile-id`, `profile-digest`, `descriptor-version`,
   `machine-model-version`, `state-serialization-version`
2. build: `build-id`
3. content: `content-sha256`, `content-size`
4. `save-init`

The three families are separate statuses (profile, build, content) and the save
binding is a fourth; a replay with both the profile and the build altered
reports the profile field. A mismatch ends the run before the first advance with
zero advance calls and the live tick 0, and the machine digest equals the digest
taken right after admission. The checkpoint digests exclude the build id and the
profile id (state serialization v25), so they are build independent; the binding
nevertheless refuses a replay recorded by another build.

## Report record

`t64.replay-report` v1 is a closed record, canonical like the replay, at most
4,096 bytes. Keys (all required, sorted): `actual_digest` (hex or null), `at`
(decimal string or null), `checkpoints_matched`, `detail`, `expected_digest`
(hex or null), `field`, `live_tick` (decimal string), `op_index` (integer or
null), `ops_applied`, `replay_sha256` (hex or null when the artifact did not
parse), `result`, `schema` (`t64.replay-report`), `schema_version`.

- `result`: `match`, `diverged`, `identity-mismatch`, `refused`.
- `detail`: `none`, `artifact-invalid`, `input-refused`, `queue-full`,
  `advance-invalid`, `stop-early`, `stop-kind`, `digest`.
- `field`: `none` or one of the nine mismatch names above.

Cross-field rules, each with its own checker status: `checkpoints_matched` is at
most `ops_applied`; `match` has field `none`, detail `none` and null `op_index`,
`at` and digests; `diverged` has detail `digest`, `stop-early` or `stop-kind`,
field `none` and all of `op_index`, `at`, `expected_digest` and `actual_digest`;
`identity-mismatch` has a mismatch field other than `none`, detail `none` and
null `op_index`, `at` and digests; `refused` has detail `artifact-invalid`,
`input-refused`, `queue-full` or `advance-invalid`, field `none` and no digests.

A driver op the machine refuses (`T64_STATUS_QUEUE_FULL`) ends the play with
`refused` and `queue-full` at the op index (the count of ops applied before it);
the refused call changes nothing and no further op is applied. A completion
stop before the end op is `diverged` with `stop-early`; a different terminal
kind is `stop-kind`.

## Runner surface

Supervisor options (child options in parentheses):

| Option | Meaning |
| --- | --- |
| `--replay-record LEAF` | record; the leaf ends in `rpl` (`--replay-record`) |
| `--replay-checkpoint-interval TICKS` | a positive multiple of 16 not above the bound (`--replay-checkpoint-interval`); see the checkpoint cap below |
| `--replay-play PATH` | play; copied into a verified private snapshot (`--replay-fd`) |
| `--replay-play-size N` | exact size, at most 4,194,304 |
| `--replay-play-sha256 HEX` | exact SHA-256 |
| `--replay-report LEAF` | the report leaf, ending in `rpr` (`--replay-report`) |

The format version strings are `t64.replay/1` and `t64.replay-report/1`.

Defined combinations: record with the artifact pair and the interval; play with
the artifact pair, the descriptor triple and the report leaf, and an empty input
schedule (the replay supplies every input; a schedule with events is refused).
Refused combinations, all before any guest starts: record with play, record
without the pair or the interval, play without the report leaf or the triple, a
size above the cap, an interval of 0, not a multiple of 16 or above the bound, a
leaf with a slash, an uppercase letter or a four-letter extension, replay with
raw capture, with the save flags and in synthetic mode. The child-argument slot
sum grows by 8 and its compile-time assertion still holds.

The checkpoint cap and the interval (a stated limit, plan 09-10, L3 IN-01). The
recorder holds 4,096 checkpoints, and a checkpoint is a full machine digest taken at
every multiple of the interval, so a run that ends at tick `T` (authored completion or
the bound) takes `floor(T / interval)` of them. When that exceeds the cap, the 4,097th
checkpoint latches `checkpoint-limit` in the recorder, the run ends in the generic
`host-error` after the guest has run, and nothing is published (the unit cases of
`unit.replay.suite` pin the recorder side: the cap, the latched error and the refused
finish). The supervisor and the runner do not refuse such an interval at option parse
because the end tick of a guest is not known there. Measured on the authored mix row:
it completes at guest tick 1,141,425, so an interval of 352 takes 3,242 checkpoints
(4,261 would be demanded by the bound of 1,500,000), the run completes and its cost is
the digests, about a minute natively, and with `--artifact-max-bytes` 65,536 it ended in
`artifact-cap` because the document no longer fit; the artifact maximum is therefore the
first practical limit for a dense interval, the checkpoint cap the second. The rule is
the stated limit `floor(T / interval) <= 4096`: choose an interval of at least `T / 4096`
(`T` at most the bound) and an artifact maximum that holds the document. The byte cap
(4,194,304) and the op cap (65,536) are not reachable first. Naming the recorder failures
in the closed result vocabulary is a Phase 11 item (it adds closed detail names to the
result protocol and the checker).

Mapping onto the existing `t64.result`: a record run is the ordinary result and
the replay is an extra private artifact published only when the loop ended at
guest-completion or guest-bound. A play run is the ordinary result when the
replay matches. A mismatch, a parse refusal or a refused schedule before the
guest starts is the existing prelaunch refusal (guest tick 0; through the
supervisor it is reported as its post-launch classification, harness-error with
report-error and capture-invalid, as for the other prelaunch refusals). A
divergence or a driver refusal after the guest started is the existing
harness-error with stop kind and detail `host-error`; the specific cause lives
only in the report. No `t64.result` key or detail value is added.

## Recorder and driver

The recorder wraps the real `t64_machine_queue_input` and records an op only
when the call is accepted. The real runner pre-queues the whole script before
the first advance, so its replays hold every input at `at` 0 in script order
(scripted pre-queue); checkpoints are taken at every multiple of the interval
and one end closes the log. The in-process reference and the driver also support
just-in-time submission at completed ticks (a lead of 5,000 ticks in the
authored reference), so `at` greater than 0 is exercised. The digests of the two
submission modes differ at a checkpoint where an event is pending; the
documents assert that difference as a witness and make no claim of equality.

The player applies, in array order, every op whose `at` equals the completed
tick. The in-process driver advances with inclusive tick bounds to each next op
`at` in slices of the given step, continues a resource-limit stop under the
progress rule of the runner (64 yields without progress give `advance-invalid`)
and binds the replay before the first advance. The runner loop targets the
minimum of the slice target and the next op `at`.

## Goldens

The committed literals hold ops and checkpoint digests only; the header is
composed at test time from the live identity, so the goldens do not move with the
build id or the source revision. They were produced once by the reference path
of `runner/tests/test_replay.c` (events queued through the real call, inclusive
bounds, the machine digest; the replay module is not linked into that path) and
reviewed against a second run at advance step 7 against 4096. The canonical
sizes and identity values of the fixed-header documents were derived separately
with a short script over the same literals, not with the C encoder. They
regenerate only through a recorded defect fix.

## Privacy

A replay or report is a private artifact: published as one single file through
the artifact publisher (mode 0600, no clobber, an owner-only directory), never
in the upload allowlist and never committed. The replay extension `rpl` is a
denied extension of the rights scanner; the report extension `rpr` holds digests,
ticks and field names only and is not denied. Header values come only from the
live identity: no host path, time or user name enters either record.

## Authored fixture and independent oracle

The ROM `phase09-replay-mix-si-v1` (source `fixtures/replay/mix.S`, one assembler
variable `T64_MIX_SPIN`) polls the controller six times with a delay loop
between polls and prints six 12-byte lines (72 bytes). The input script
`fixtures/replay/mix.input.json` stamps one state in each gap between two
measured poll execute ticks (the third gap holds two states at one tick, the
later row winning). `fixtures/replay/mix_oracle.py` holds the scenario literals
and the strictly-after rule outside the ROM and derives the transcript; its
value and the independent `shasum` of the hand-derived text agree. The test
checks, from the neutral run, that every stamp is at least 8,192 ticks from every
execute tick and lies strictly in its gap, so the transcript depends on ordering
only; the absolute start ticks are measured, never pinned.

## Mutation rows

The rows asserted by `mutation-matrix`, each after an accepted baseline of the
same bytes (the list equals the table of `runner/tests/test_replay.c`):

unknown-root-key, missing-boot, missing-build_id, missing-content_sha256,
missing-content_size, missing-descriptor_version, missing-input_schema_version,
missing-machine_model_version, missing-ops, missing-profile_digest,
missing-profile_id, missing-save_init, missing-schema, missing-schema_version,
missing-source_revision, missing-state_serialization_version,
duplicate-root-key, duplicate-op-key, type-string-for-integer,
type-integer-for-string, type-object-for-array, type-array-for-object,
schema-text, schema-version-2, input-schema-version-2, boot-state, boot-other,
profile-digest-uppercase, profile-digest-63, profile-digest-nonhex,
content-sha-uppercase, content-size-leading-zero, content-size-above-u64,
content-size-json-integer, build-id-grammar, source-revision-uppercase,
descriptor-version-zero, save-init-kind-unknown, save-init-blank-extra-key,
save-init-image-accepted, save-init-image-no-sha, save-init-image-length-zero,
number-above-2-53, number-fraction, number-exponent, port-1, port-3,
buttons-65536, x-128, y-minus-129, at-leading-zero, event-before-at,
at-reversed, end-missing, end-not-last, second-end, unknown-op-name,
input-with-digest, op-digest-uppercase, end-stop-unknown, depth-5,
trailing-bytes.

Further checks in the same case: a 161-byte string, the byte cap, the caps at the
limit and one above through lowered limits, the 4,097th checkpoint at the default
limits, the recorder cap refusals and a sampled single-bit-flip sweep over every
5th byte of the golden file (at least 400 flips; an accepted mutant must have a
different canonical identity).

The rows of `report-record`: match-field, match-detail, match-op-index,
match-at, count-order, schema-text, schema-version, unknown-key, missing-key,
duplicate-key, result-outside-set, detail-outside-set, field-outside-set,
diverged-no-op-index, diverged-no-at, diverged-no-expected,
diverged-no-actual, identity-field-none, identity-detail,
identity-field-unknown, refused-detail-none, refused-with-digest, and the
oversize record.

## Evidence limits

- The original run comes from an independently authored script and the golden
  op lists are literals derived from it; the digests are regression pins from a
  reference path, so a consistently wrong machine would pass.
- A hash proves identity, not rights; no ROM, save, firmware or protected
  material is used.
- The `save_init` image form is verified in process only.
- State-anchored replay, cross-build or cross-profile replay, a binary schema,
  rewind and any hardware, oracle or differential statement are out of scope.
- The replay suite runs natively only (D-40): no hosted run line is added.

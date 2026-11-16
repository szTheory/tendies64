# Output and input seam reconciliation for plan 03-07

The current warm profile uses state v7 and has canonically represented capture,
magic/count/buffer and sticky loss. Plan 03-06 and its hosted candidate have
qualified exact hello output. `unit.core.isviewer` is already present in the
78-test gate and 80-test ASan inventories; do not add a duplicate registration.
`core/t64_observe.c` does not exist yet and is an intentional new module.

Drain must retain the lifetime capture and loss policy: successful host copying
must not free guest capture capacity or change its canonical digest. Add an
observer-only per-instance cursor, reset with the observer/output lifecycle and
excluded from v7 encoding. Repeated drain with no new bytes returns empty.
Define total produced as the retained, successfully produced lifetime bytes;
the sticky loss bit separately reports refused output. Compute a captured-byte
digest from that retained prefix without inventing unrepresented hash state.
Validate all caller spans and pairwise aliases before changing caller bytes or
the cursor; short capacity reports the required size without consuming data.

Timed input is port-0 trace only, not SI/controller consumption. Existing queue
records already encode tick, sequence, device, kind, value and token in v7;
buttons and bounded signed axes fit in the value. Use distinct stable input
tags, reserve then commit, and preserve queue/sequence on refusal. Define trace
payload packing and check source plus kind when classifying records. Trace
enable/drain choices remain observer-only. No new canonical layout is needed
merely for an observer cursor or an event kind whose payload is fully encoded.
If concrete future state is genuinely missing, identify it before adding it.

Supporting ownership extends to `core/t64_internal.h` for observer state and
private declarations, plus `core/t64_machine.c`, `fixtures/boot-contract.json`,
`core/tests/test_boot_entry.c`, `core/tests/test_tracer.c` and
`docs/spec/machine-contract.md` for lifecycle and active trace-only input/output
policy identity and documentation. Bind the new explicit input policy and keep
state v7 when its wire layout is unchanged; independently update affected
profile/state goldens using the established reference encoder. Preserve old
v6/v5 meanings and synthetic identities. No RSP/SI engine, physical input,
frozen ABI, extra dependency or host callback during advance is implied.

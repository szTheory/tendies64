# Phase 2 final local verification after CI portability fix

The only production change since the reviewed fixes is the equivalent checked
allocation-size expression in `core/t64_machine.c`. Its tested Git blob is
`692f32af8a88b9d9e18e9c8cb461cbbc0d93f42f`; ASan build ID is `t64-build-v1-7f6450394419bfe477918ea33c6970f915eb59ca3b8dcd7c9e620caaf9302993`.
The embedded source revision is the preceding committed base (`5b277e9`); the
build fingerprint includes the changed production source bytes.

Commands and results, 2026-11-10:

- `cmake --build --preset gate --target test_lifecycle test_profile` succeeded.
- `ctest --preset gate --no-tests=error -R '^unit.core.(lifecycle|profile)$' --output-on-failure`: both passed.
- `sh tools/lint/core-lint.sh`: 15 source files, zero violations.
- `cmake --build --preset asan`: full build, all changed core and linked test/probe targets refreshed.
- `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^(unit\.core\..*|unit\.model-result|fuzz\.rom-admission|sanitizer\.negative-control)$' --timeout 120 --output-on-failure`: 14/14 passed, 44.58 seconds wall; full fuzz 44.57 seconds. This run supersedes the earlier split component/fuzz record for final-source evidence.
- The negative control passed with the deliberately faulty child; no sanitizer finding occurred in the production path. LeakSanitizer remains unavailable on this host; address and undefined instrumentation are active.

Sanitized production fuzz assertion output:

```text
# rom admission: seed=0x7a64c0de mutations=250000 fixed_cases=148 accepted=316 refused=249832 orders=83334/83334/83332 fields=60 inventory=fixed-v1 sanitizer=asan+ubsan total_calls=250148 branches=2/64/2/2/1/4/1/1/1/1/1/2
ok 1 - production admission campaign has exact mutations, inventory, and refusal invariants
```

The transcript's individual test outcomes and invocation were inspected before
writing this portable record. Raw local test logs are excluded from publication.
Hosted Linux/macOS qualification remains pending the replacement push run.

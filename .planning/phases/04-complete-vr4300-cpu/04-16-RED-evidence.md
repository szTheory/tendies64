# Plan 16 Authored RED Evidence

This control uses an authored invalid receipt and a fake compiler that writes a
marker if invoked. It exercises only the local build-script refusal boundary;
it does not install or invoke a diagnostic package, build a diagnostic ROM, or
run a guest.

Command:

```sh
ctest --preset gate --no-tests=error -R '^runner\.cpu-diagnostic$' --output-on-failure
```

The registered `runner.cpu-diagnostic` test executed the new assertion. Before
the qualification gate was implemented, the assertion failed because the
script returned `73`, the fake compiler emitted `FAKE_CC_INVOKED`, and its
side-effect marker existed. The expected behavior is refusal with exit `4`
before the compiler runs. The failure therefore demonstrates the specific
pre-use ordering defect, rather than a setup or test-discovery failure.

The unchanged JUnit report was accepted by the RED evidence classifier as
`RED_EVIDENCE_OK`. Its private raw record and report remain in the ignored
qualification workspace.

## Source-faithful inventory contract RED control

The authored protocol control preserves the pinned `Weird` level on a CPU
registration assigned to `required-base`, and checks that an otherwise-valid
inventory using the superseded schema version is refused. This follows the
upstream runner: the default `base` feature dispatches `Weird` tests through the
base profile. The prior protocol version rejected that source-faithful row and
accepted a stale-version fixture. The focused `runner.cpu-diagnostic-protocol`
test failed on these behavior assertions and was accepted by the RED evidence
classifier as `RED_EVIDENCE_OK`. It used only authored native protocol data;
no external diagnostic or guest execution was involved. The raw JUnit and
record remain private.

## Authored qualification-gate controls

The registered `runner.cpu-diagnostic` control uses self-contained temporary
files. It accepts an authored compiler and target-tool hash baseline, then
checks individual hash and selected-path refusals, registry versus Git-source
lock handling, missing-receipt refusal, and symlink-ancestor confinement. These
controls exercise the production validation seams without private source,
archive, or diagnostic data.

A separate local-only integration control runs the production pre-use verifier
with an authored synthetic receipt and fake compiler/binutils over audited
private inputs. It does not invoke Rust, build the external diagnostic, or run a
guest. Its detailed report remains in ignored private storage.

The gate build and focused authored controls passed. The all-target ASan build
and the three diagnostic CTest controls also passed. No private receipt, ROM,
capture, inventory, batch, or diagnostic-derived status is included here.

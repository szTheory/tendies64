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

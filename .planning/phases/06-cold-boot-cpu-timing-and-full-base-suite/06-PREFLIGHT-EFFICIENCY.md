# Pre-use efficiency follow-up

The current qualified required sweep is advancing and must keep its exact
context. Implement these changes after it finishes, before later lane contexts;
do not interrupt or replay completed cases merely to benchmark host changes.

Observed on this local run: median member-process time was 0.216 seconds,
median full dispatch 1.023 seconds, and an independent full pre-use invocation
took 0.615 seconds. These measure host work, not N64 timing or accuracy.
Original logs and records remain in ignored local evidence directories.

## Remove duplicate work within one full pre-use entry

For version-2 contexts, `_verify_rsp_source_authority` invokes the fixed RSP
provider with `verify_current_identity=False`. That provider already calls the
independent CPU source validator. `_verify_live_source_authority` then calls
the CPU validator again in the same entry.

Keep the fixed CPU revision/projection-anchor check and one complete CPU/RSP
source/tool/asset/lock verification. Suppress only the second CPU validation
after the first provider returns successfully. The control must be internal
to the verified call chain, never nominated by context fields or CLI input.
Version-1 behavior remains unchanged. This adds no cache or concurrency.

Check accepted full entry, source/projection/tool drift, reduced-and-resealed
membership, and a failing RSP provider. Prove the CPU authority still runs once
and the independent anchor cannot be skipped. Measure read-only pre-use calls
serially after the current sweep, without re-executing guest cases.

## Reuse the batch's already frozen source documents

Each case currently copies the complete authority and inventory even though
the batch already retains identical frozen documents. Pass the batch copies
to the C member producer instead. Preserve the independent expected authority
digest and context binding, and verify the frozen input paths before launch.
Keep the existing per-case variant ROM and original record/capture retention.
Do not rewrite or deduplicate historical case files after execution.

Check the real accepted admission path, target/source mismatches and source
drift between cases. Compare source and original-record identities before and
after the change. Counts and completeness remain solely in C.

Neither change alters the profile, ten-million-tick case limit, native
ten-second/host twelve-second deadlines, capture/event limits, rights controls,
one-shot markers, stopped attempts or publication policy. Include both changes
and their evidence in the final source/security review.

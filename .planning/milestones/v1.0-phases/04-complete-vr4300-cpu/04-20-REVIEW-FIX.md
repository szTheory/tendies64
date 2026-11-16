# COP1 conversion destination correction

The initial Plan20 review correctly identified that one alias flag was being
used for both conversion sources and destinations. This was treated as
blocking for publication, despite its warning label.

The selected software policy does include scalar conversion source aliases,
including CVT.S.W and CVT.W.S. The original suggestion to leave every scalar
source untouched would remove an intended policy. The narrower remedy keeps
that source selection but normalizes destinations only for 64-bit pair
results. A 32-bit result retains its requested destination and upper word.
This is a policy clarification and functional correction, not a risk waiver
or a claim about physical floating-point register behavior.

## Meaningful regression

The new authored CVT.S.W control uses distinct source values, an odd 32-bit
destination and an even-neighbor sentinel. It confirms fetched full PC
`0xffffffff80001000`, opcode `0x46804960`, retirement and Cause0.

Before the fix, conversion reads the intended even source value7, but writes
the float bits to even neighbor4 (`0x1122334440e00000`) and leaves odd
destination5 at `0x55667788deadbeef`. The requested-destination and unchanged
neighbor assertions fail. After the fix, neighbor4 remains
`0x11223344a5a5a5a5` and destination5 is `0x5566778840e00000`. Paired
conversion and old-profile controls remain in the passing owning arithmetic
test. This regression is actual pre-fix behavior, not a reconstructed RED.

Raw RED and verbose GREEN remain ignored in
`build/compat-v2-scalar-cvt-dest-red.log` and
`build/compat-v2-cop1-arithmetic-verbose.log`.

Both complete normal/instrumented target builds pass after the correction.
The owning COP1/three-diagnostic selection passes in both configurations
(six tests each).
The earlier full native gate and 39-test instrumented run retain their actual
pre-correction build identities; they are not relabeled as this corrected
binary. Unchanged subsystem evidence is reused, with fresh owning checks.

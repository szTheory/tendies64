# Plan07-10 implementation contract

The [primary preflight](07-10-VI-FILTER-FIELDS.md) establishes STATUS fields,
coverage interpolation and conditional AA/dither selection. It does not
establish exact silicon kernels, tables, rounding or a total stage order.
The plan explicitly permits named deterministic software approximations for
these gaps. The choices below are reversible software policies; authored
agreement does not qualify hardware accuracy or VID-04. Preserve D2/D5/D6
and all expired resource limits.

## Identity and first actual consumer

Add one composite successor of v8, preserving every older profile's semantics,
public hash/serialization and state domain. Reuse descriptor10 if its layout
does not change; advance the affected machine/state versions. Bind the complete
filter policy in the existing output-policy text, with its exact equations
documented here and in the VI spec. Keep the v8 rational clock/latch policy,
source unpacking, geometry and IRQ behavior. No API/ABI freeze or version per
fix. Public metadata mutation/canonical admission and older-profile goldens
precede broad tests.

Before production behavior, register the full promised case roster and run one
valid nonuniform producer through actual MMIO and raw packet copy. Retain a
semantic RED, not a setup refusal. Use RGBA8888 for the exact primary AA witness:
center RGB(200,80,40), logical coverage2, fourteen full-coverage neighbors
RGB(40,120,200), STATUS0x00000003. Expected RGB(80,110,160) follows the primary
equation exactly. Mode2 STATUS0x00000203 bypasses AA and yields(200,80,40) at
integer coordinates. Set every fourth source byte to0x1f: inherited low-five-bit
alpha expansion yields255, so the expected center RGBA is(80,110,160,255),
or(200,80,40,255) for mode2. Seed coverage through the actual RDP ownership seam and
also exercise a fetched-command producer; do not infer coverage from alpha.

## Selected integer software pipeline

All coordinates, neighborhoods and arithmetic are checked integers. RGB stages
do not change the nearest center's retained source-alpha interpretation.
Keep source pixels immutable. The chosen total order is backend AA-or-dither,
then optional divot, fractional resampling, optional gamma-dither, then gamma.
This order is a software choice, with the documented conditional AA/dither
selection preserved.

- AA modes0/1 use the same visible-color policy; do not invent fetch timing.
  For partial coverage c in0..7, search the 5x3 neighborhood for samples with
  logical coverage8. Select maximum RGB Manhattan distance from foreground;
  ties select first in top-to-bottom, left-to-right order. If none exists,
  use foreground. Each channel is floor((c*foreground+(8-c)*background)/8).
  Unknown coverage remains the existing logical0. Full coverage bypasses AA.
  Mode2 treats coverage as8. Mode3 bypasses AA and fractional resampling.
- Dither-filter bit16 selects a software smoothing approximation only for
  RGBA5551 full-coverage samples when AA is not selected. Average the expanded
  RGB channels over a 3x3 neighborhood, rounding down. This does not claim
  recovery of hardware's discarded bits. It never runs on an AA-selected
  partial sample; RGBA8888 bypasses it.
- Divot bit4 applies only to partial-coverage centers in AA modes0/1. Take the
  component-wise median of the backend-filtered left, center and right RGB.
  This median and its position in the pipeline are software choices.
- Modes0/1/2 use bilinear RGB resampling at the existing Q10 source coordinate.
  Retain `(offset + i*step)` including its fractional bits through tap selection;
  do not reconstruct fractions from the already floored nearest index.
  Use the low10 coordinate bits as fractions; sum the four Q20 weighted
  products and floor once by2^20. Test both fractions nonzero and distinguish
  one final floor from two intermediate floors. Mode3 uses the nearest floor
  sample. Serrate selects the existing parity row and the next same-parity
  source row for vertical interpolation; it does not adopt an OS mode preset.
- Gamma bit3 uses integer gamma2 approximation floor(sqrt(255*channel)).
  Generate the lookup using a bounded integer square root, never host libm.
  Gamma-dither bit2 has effect only with gamma enabled: before lookup, add -1
  for rank<8 or +1 otherwise, saturating0..255, using the explicit rank matrix
  [0,8,2,10;12,4,14,6;3,11,1,9;15,7,13,5] indexed by output x/y modulo4.
  It is a static spatial software pattern, with no host entropy or shared RDP
  RNG consumption. Exact gamma curve/pattern are not primary hardware facts.

Neighborhoods use source coordinates and WIDTH pitch, with row steps1 in
progressive and2 for same-field serrate neighbors. Clamp horizontal taps to
0..WIDTH-1 and top taps to the first row of the selected parity (row0 for
progressive). Check every resulting byte span against
RDRAM; if an otherwise out-of-RDRAM neighbor is needed, replicate the valid
center instead. Never read an invalid neighbor, fabricate source height from
packet capacity, or refuse a valid center merely because a neighbor is beyond
RDRAM. Bind this edge rule as software policy. Test first/last RAM samples,
partial final rows, offsets and serrate independently under sanitizers.
For the successor only, `source_bytes` reports the bounding span from ORIGIN
through the highest valid source-color tap actually read for the packet,
including enabled filter/resample neighbors. It must not silently retain a
nearest-center-only span when additional source pixels are consumed. Invalid
neighbors use the valid-center fallback and do not enlarge the span. This is
metadata about the software sampler, not a hardware memory-fetch timing model.

Packet filter metadata is a documented bitset of stages actually selected
somewhere in that packet, rather than a raw STATUS echo. Define the public
constants and include all new metadata in only the new state domain. Assert
actual copied-packet source format, field, geometry, timing, profile/build and
selected-filter constants in the assertions; keep v8 packet serialization exact.
Include AA/dither exclusivity, gamma-dither gating, mixed coverage, stage order and
deliberate wrong-pixel/first-difference controls. Pin independent literal
vectors before helpers; never move goldens to fit implementation output.

## Lossless output and closeout

Use the existing two-packet storage. Reserve before a producing field event;
full leaves its exact tick/sequence/phase/pixels pending. Copy is atomic after
validating all destination/length/metadata spans, overlap and capacity; failed
copy cannot move the cursor or partially write pixels. Host drain consumes no
guest time and does not change the canonical digest. Equal completed guest
boundaries must have equal bytes and digests across drain cadence, split
advance and interleaved instances. Test actual runner packet drains, not saved
frame replay. Reuse existing contracts where they already pass; do not add a
second output system.

Run actual profile/runner acceptance, wrong-pixel control and strict body/source/
object/mutation inventories before long suites. Preserve owning LastTest plus
unique JUnit on success/failure, and archive exact binaries/libraries/source
bindings before later all-target replacement. Parent owns independent review,
integrated gate, hosted qualification and routine summary/state. No private
guest, oracle, physical test, dependency, cache or concurrency expansion.

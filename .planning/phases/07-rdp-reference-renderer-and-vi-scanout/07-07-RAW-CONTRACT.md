# Plan07-07 execution contract

The [primary field preflight](07-07-BLENDER-FIELDS.md) and [complete write-route inventory](07-07-WRITE-ROUTES.md) are the inputs. Primary fields must not be changed to fit a decoder. This contract chooses reversible software behavior where those records identify gaps; it supplies no hardware/oracle evidence. D2/D5/D6 and all existing limits remain unchanged.

## Numeric policy to implement and pin

Use a named v6 composite-profile blender policy, preserving all older profiles. Apply the approved compatibility policy to concrete descriptor/model/state fields and identity strings without another approval. Necessary profile/runner wiring and allocation/reset/hash files are within the approved Plan07 behavior even where its file estimate omitted them.

- Retain the existing 16-position software sample mask separately. Convert its population to logical0..8 coverage using `(n+1)/2`; this is a software conversion, not the hardware sample/dither mask.
- Use RGB8 and integer blend factors with unity32: source alpha byte maps to `alpha>>3` (0..31), complement is `32-A`, literal one is32. Use nonnegative wide numerators, floor division by A+B and explicit zero-denominator RGB0. Normalize/clamp each cycle; cycle1 input color receives normalized cycle0 RGB. The primary text qualifies feedback as a numerator working for B=1-A; normalized feedback for all selectors is software policy. Distinguish all selector slots/cycles and coefficient boundaries.
- Alpha compare accepts equality. Nonrandom comparison uses blend-register alpha; the alpha-compare random-threshold bit uses the already generated per-covered-pixel xorshift byte, without another RNG advance. Alpha dither pattern/selector effects, coverage-times-alpha rounding, alpha-to-coverage and memory-alpha conversion must be named precise integer rules in docs/tests before implementing them. Do not silently ignore those modes or present a software default as a primary fact.
- Coverage destination clamp is min(8,sum), wrap is sum0→0 and otherwise `1+((sum-1)%8)`, full is8, save is old memory coverage. Overflow is sum>8, including exact8 as a discriminating nonoverflow test. Preserve independent color and coverage gating effects under color-on-coverage and alpha rejection.
- Force-blend, antialias/partial-coverage enable, image-read disable, two-cycle behavior and fill/copy bypass need reached raw-command controls. Pin exact memory/color/alpha effects in the software policy rather than deriving them from a convenient helper.

## Address sidecar policy

Prefer a bounded byte per physical16-bit RDRAM granule with a validity bit and logical0..8 count. New/reset/unloaded/unwritten and externally invalidated entries read as coverage0. A16-bit RDP output authors its granule. A32-bit output authors both; define its read as the minimum of the two valid halfword counts (invalid either means0), so cross-format aliases need no hidden epoch or host-global map. A partial byte overlap invalidates only intersecting granules; masked stores invalidate selected bytes only. Account for an odd custom RAM byte count safely using ceiling granule count and checked allocation arithmetic.

Keep physical 16-bit hidden packing unclaimed. For32-bit output, primary alpha[7:5] stores the three-bit coverage-minus-one code; define zero-coverage/no-write and remaining low5 alpha bits explicitly. The sidecar remains the software alias/validity authority. Document any necessary deviation before RED rather than inventing physical RDRAM lanes.

Notify only after successful CPU scalar/masked/cache-line writeback, PI completion and both SP DMA commit routes. Cached stores do not invalidate backing metadata until writeback. Fill/copy author full coverage under this profile; older profiles remain unchanged. Allocation/admission/reset/unload/destruction and canonical digest own all future-affecting sidecar state. Failed operations do not invalidate; hooks cannot fail after accepted backing mutation. All spans are prechecked, and row work remains reserved before mutation.

## Cheap literal before broad suites

Independent primary/SDK packed word `0xEF1000FF0A7041C8` has two-cycle mode, dither disabled, c0 P/A/M/B=(0,2,1,0), c1=(0,2,3,0), force blend, image read, antialias, color-on-coverage and wrap. With shade-alpha128→factor16, input RGB(200,100,40), memory RGB(40,80,120), fog RGB(20,60,100), each complementary factor is16. Cycle0=(120,90,80); cycle1=(70,75,90). New logicalcoverage3 plus old6 wraps9→1 and permits color write. Assert RGB and hidden count separately; pin final quantization/remaining alpha bits from the chosen software policy. This vector avoids any rounding ambiguity. The preflight's different rational-factor witness is illustrative, not an assertion that factors must use /255.

Reach at least one actual fetched packet to raster to RDRAM/raw VI before expanding helper matrices. Then cover all promised modes, 16/32 aliases, actual CPU/PI/SP writes, cache writeback and masked-store lane ownership, first/last/odd custom span bounds, recovery, reset/unload, split/interleaved instances and digest sensitivity. Source guards and exact nonempty case inventory precede long suites. Tests must reach intended operations, and goldens must discriminate plausible adjacent-field/layout errors.

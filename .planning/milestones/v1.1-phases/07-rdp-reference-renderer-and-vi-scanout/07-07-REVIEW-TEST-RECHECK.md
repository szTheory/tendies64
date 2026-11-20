# Plan07-07 test-oracle recheck

Orchestrator recheck of WR01/WR02, following independent production review and the separate independent profile repair recheck. This is a test-only diff/evidence recheck, not a second full source audit.

WR01 is resolved: the registered selector case now submits32 fixed EF literals through actual command fetch, a constant primitive combiner/shaded triangle, RGBA8888 RDRAM and raw VI. Each primary P/A/M/B slot takes all four codes in each cycle. The oracle does not call the production decoder, blender_modes or blend_direct; distinct fixed pixel/memory/blend/fog/shade inputs and literal RGB tables discriminate alternatives. A separate Python integer calculation checked every literal's primary-table fields and all32 outputs; retained JSON: `build/gate/07-07-review-literal-matrix-check.json`. The pre-existing fetched RGBA5551 dither case separately checks quantization.

WR02 is resolved: fresh lower and upper instances independently assert exact0x8cef/0x7310 masks, populations10/6, intersection0 and union0xffff. A third instance then asserts accumulated full mask and logical coverage8. This distinguishes a second primitive incorrectly owning all samples.

Final registered10-case normal and ASAN/UBSan suites pass, with owning logs `build/{gate,asan}/07-07-review-final-blender-LastTest.log`. GCC13 test-target warnings-as-errors compile passes; command, diagnostic log and explicit outcome are `build/gate/07-07-review-blender-gcc-*`. No production-owner batch was repeated for these test-only corrections. Original broader production checks remain in execution evidence; repaired descriptor9 has its own public-API regression and independent recheck.

IN01 clarification: the initial introduction really omitted v6 and gave an inconsistent count. The corrected introduction lists all FIVE registered RDP profiles v2..v6. The initial review's recommendation to list six is withdrawn; the actual omission/count error is fixed.

No unresolved finding remains in this scoped review. Final combined gate/hosted qualification are separate pending evidence, and hardware/VID04 remains unqualified.

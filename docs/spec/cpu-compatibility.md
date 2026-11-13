# VR4300 software compatibility profile

## Identity

`ntsc-cart-4m-openipl3-vr4300-compat-v1` is a separately versioned software
expectations profile for the VR4300 model. Its descriptor schema remains v3;
its machine model is v4, state serialization is v10, and the CPU semantic
policy marker is v2. The strict profile
`ntsc-cart-4m-openipl3-vr4300-v1` remains model v3, state v9, and semantic
policy v1.

The compatibility profile has its own profile identity and digest. It keeps
the same prepared IPL3 entry, seed reset policy, and 4 MiB flat-image
admission policy as the strict VR4300 profile. The profile version distinguishes
software behavior; it does not add a new boot seed or a different ROM limit.

## State identity

State v10 retains the v9 field sequence, including its complete CPU state and
retained guest output, then appends the 64-bit `delay_origin_pc_full` field
immediately after the v9 CPU-state block. This branch-origin state is separate
from the delay-slot address so a nested branch-delay case can retain the
origin needed for exception EPC reporting. State v9 does not serialize this
field, so its existing bytes remain unchanged.

The v10 digest uses the `t64.state.v10` domain and binds the v10 number and
compatibility profile digest, so it remains distinct from v9 even when the
prepared machine fields match.

## Evidence boundary

This profile describes versioned software expectations for an unqualified
target. Its identity, seed, execution results, and compatibility outcomes are
software-model evidence; none establishes NUS-CPU-03 silicon behavior or broad
cartridge compatibility. Any support statement still needs to name the exact
profile and cite evidence for the workload and behavior being claimed.

The strict profile remains available for callers that require its existing
refusal behavior. A compatibility completion gate should select the v1
compatibility profile explicitly and record that selection with the tested
profile and build identity.

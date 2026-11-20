/*
 * Build-independent literals of the replay-mix scenario (plan 09-07, D-34).
 * The files and tests include only ops and checkpoint digests: the header of a
 * replay is composed at test time from the live identity, so these literals do
 * not move with the build id or the source revision.
 *
 *  - The input rows are the events of fixtures/replay/mix.input.json written
 *    out (event tick, buttons, x, y; the file is authored independently of the
 *    recorder and cross-checked by fixtures/replay/mix_oracle.py).
 *  - The checkpoint and end digests are regression pins taken once from the
 *    reference path of test_replay.c (`test_replay --print-goldens`: events
 *    queued through the real t64_machine_queue_input, inclusive tick-bound
 *    advances, t64_machine_digest; the replay module is not linked into that
 *    path) and reviewed against a second run at advance step 7 against 4096.
 *    They are not hardware values. They regenerate only through a recorded F1
 *    defect fix: they were regenerated once by plan 09-10 (the composite
 *    family state gained the combiner block and the memory coverage table,
 *    STATE-CONTENT-F1; reference path at advance steps 4096 and 7, both modes,
 *    equal), and the canonical identity values were re-derived by the same
 *    Python procedure after it reproduced the previous pins byte for byte.
 *  - The canonical sizes and identity SHA-256 values belong to a documented
 *    fixed header (see test_replay.c) plus these ops and were derived
 *    separately with a short Python script (json.dumps with sorted keys and
 *    compact separators over the same literals), not with the C encoder.
 *
 * The pre-queue mode (the real runner pre-queues the whole script before the
 * first advance) holds every input at 0 in script order, checkpoints at every
 * multiple of 160,000 below the completion tick and one end. The just-in-time
 * mode submits each stamp 5,000 ticks before its event tick and takes one
 * checkpoint 2,500 ticks before it. The two modes are both pinned; no equality
 * between them is claimed.
 */
#ifndef T64_REPLAY_MIX_GOLDEN_H
#define T64_REPLAY_MIX_GOLDEN_H

#include <stdint.h>

typedef struct {
  uint64_t event_tick;
  uint16_t buttons;
  int32_t x;
  int32_t y;
} golden_event;

#define GOLDEN_SCRIPT_COUNT 6u
static const golden_event golden_script[GOLDEN_SCRIPT_COUNT] = {
    {UINT64_C(602000), 32768u, 1, -1},
    {UINT64_C(702000), 16417u, 127, -128},
    {UINT64_C(802000), 8208u, -128, 127},
    {UINT64_C(802000), 6148u, -2, 2},
    {UINT64_C(902000), 258u, 0, 64},
    {UINT64_C(1002000), 3267u, 5, -5},
};

/* The completion stop of the authored ROM (guest-completion). */
#define GOLDEN_END_TICK UINT64_C(1141425)

#define GOLDEN_PRE_INTERVAL UINT64_C(160000)
#define GOLDEN_PRE_CHECKPOINTS 7u
static const uint64_t golden_pre_ticks[GOLDEN_PRE_CHECKPOINTS] = {
    UINT64_C(160000),  UINT64_C(320000),  UINT64_C(480000), UINT64_C(640000),
    UINT64_C(800000),  UINT64_C(960000),  UINT64_C(1120000)};
static const char *const golden_pre_digests[GOLDEN_PRE_CHECKPOINTS] = {
    "d6a7fc24f6472cf37f04247fcda42af9eb14fde2d840da035e697eb2ef58a9c9",
    "cd98976555d09a87ac60cc15a5fe44c4a5233e7b9b93f1e1688dbee56902002f",
    "776f755bbe5662cf641fe42d84d667d69c0f28694fe7399ebf484568120bf969",
    "cbed7dd4b31c430a14b78955dc73dc9e95ebd270bfaee73d88e79b9d82c29b02",
    "14e251e85e48de863a27ba5b85664c7d74e031ad038ec413808d7cc907ea7dcb",
    "7cc7fe8ba79ddd7d89f801cf3fb8e85b45b34f0a24d89c6230ff0396bd137ae6",
    "59b349e7e65c2bb87aaaf3dc2768144776d4c4ca521fca31449099ea7ad7779f"};
#define GOLDEN_PRE_END_DIGEST \
  "ec8addbaac0cd2b42f2c6f4a1fac79e4d0fea1d69bf9f0733cf7cb38382505d3"

/* Just in time: the checkpoint ticks are the distinct stamp ticks less 2,500;
 * the submission ticks are the event ticks less 5,000. */
#define GOLDEN_JIT_LEAD UINT64_C(5000)
#define GOLDEN_JIT_CHECKPOINTS 5u
static const uint64_t golden_jit_stamps[GOLDEN_JIT_CHECKPOINTS] = {
    UINT64_C(602000), UINT64_C(702000), UINT64_C(802000), UINT64_C(902000),
    UINT64_C(1002000)};
static const uint64_t golden_jit_ticks[GOLDEN_JIT_CHECKPOINTS] = {
    UINT64_C(599500), UINT64_C(699500), UINT64_C(799500), UINT64_C(899500),
    UINT64_C(999500)};
static const char *const golden_jit_digests[GOLDEN_JIT_CHECKPOINTS] = {
    "a0b80493f951da8051f8dff4c1d9e8668e273faf95175708bbca315f5bd3a4b0",
    "40eacf15cb260819cef5c7a126829ac81afece91ae8e7929282eca4e931baa49",
    "49ae92d346979b3c06e1b11c02438f2f5a16025086710bba950099446382bcdf",
    "f79243ff3bf80d7ee351cfa1d2705ba85b7ac5234c3f593fc3718c4422818215",
    "e1bef67e25376ca0c5b2a5511a1d8f84310401770e741f90ba9f89381a360b84"};
#define GOLDEN_JIT_END_DIGEST \
  "ec8addbaac0cd2b42f2c6f4a1fac79e4d0fea1d69bf9f0733cf7cb38382505d3"

/* The fixed-header canonical documents (python-derived). */
#define GOLDEN_PRE_CANONICAL_BYTES 2013u
#define GOLDEN_PRE_CANONICAL_SHA256 \
  "ef300beefd56b2d41e51cdcb2187396915dbc3d777fafaede8974fcb4c5bd644"
#define GOLDEN_JIT_CANONICAL_BYTES 1822u
#define GOLDEN_JIT_CANONICAL_SHA256 \
  "9decbf291bbfcbc9d08afc806d0df6b2b4afb97612fdc0ae681b8e6c714a6bb5"

#endif /* T64_REPLAY_MIX_GOLDEN_H */

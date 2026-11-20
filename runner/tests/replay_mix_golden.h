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
 *    defect fix.
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
    "4a873dd8c04ab57e8f5aa64a95268d2f897269ee5a44c03bc498b8a05abcd452",
    "6f16cabb0d8112a11cd6d167fa9e13db3a0f803e9665e7a4bc0ec60b80f8d101",
    "b60649433dd49bcccc2f39f332c7e47cfa65f734cf207ba7dc3c9e1cae0e6e17",
    "622d61c5b16316438f7c0d706123134bc78eed44bcfdb91a65389e571c954618",
    "584d2aa0028bc2d534fa1cb011e25a4c409298a8787b6617501b683259991fbe",
    "aeabb524bc9f61bf71386dd50f121a4da6fd22f0006bc81e45cc03e9c33dec4e",
    "1ece46e6821648d9b3b8720ef5575585d58c0895ce7d33b78e842b1e69d67bed"};
#define GOLDEN_PRE_END_DIGEST \
  "3a72b2a985b72a52bb63479856af8c44433f3777bf8be536037d707995f0fa48"

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
    "30aabf97cd1583eab70a6ff4ec4b6c02515d245f6eb1f693ee11dbf6d927a52a",
    "ac4f18bb03e5c8bc06c0f0d9c302c9cd17b3104e0f54caa47336d9086dd0699e",
    "5728c464cf280472b143891bc9ce8221811936126528f25bb7e20419eb36a28d",
    "b2712dd7e998e34849c0aa2bab9529aefdde7c043345be574035619d7a1de901",
    "5bf4c296b37ed28d288a1939e0cd3db0a0bd28e24d27e8007265d2b34b8fd574"};
#define GOLDEN_JIT_END_DIGEST \
  "3a72b2a985b72a52bb63479856af8c44433f3777bf8be536037d707995f0fa48"

/* The fixed-header canonical documents (python-derived). */
#define GOLDEN_PRE_CANONICAL_BYTES 2013u
#define GOLDEN_PRE_CANONICAL_SHA256 \
  "10d20e4aae1ce96445fbad417c63e668be4dfce56a7a9ce23bd7a649b488d06a"
#define GOLDEN_JIT_CANONICAL_BYTES 1822u
#define GOLDEN_JIT_CANONICAL_SHA256 \
  "dd7d36c15d9ef114431c84e6cc880fc3359b9e8d46d77510bdd1ee7dc920eafe"

#endif /* T64_REPLAY_MIX_GOLDEN_H */

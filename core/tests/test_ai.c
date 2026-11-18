#include <stdio.h>
#include <string.h>

#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define AI_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"

static void case_cpu_two_dma_literal_pcm(void) {
  const t64_profile_descriptor *profile = NULL;
  const t64_status status = t64_profile_lookup(AI_PROFILE_ID, &profile);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return;
  T64_CHECK_STR(profile->profile_id, AI_PROFILE_ID);
  T64_CHECK_INT(profile->descriptor_version, 11u);
  T64_CHECK_INT(profile->machine_model_version, 19u);
  T64_CHECK_INT(profile->state_serialization_version, 24u);
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    puts("cpu-two-dma-literal-pcm");
    return 0;
  }
  case_cpu_two_dma_literal_pcm();
  return T64_TEST_FINISH("unit.core.ai.tracer");
}

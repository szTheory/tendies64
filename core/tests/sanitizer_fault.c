#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int main(void) {
  volatile size_t one_past_end = 1u;
  uint8_t *owned = (uint8_t *)malloc(sizeof *owned);

  if (owned == NULL) {
    return 2;
  }
  owned[one_past_end] = UINT8_C(0xa5);
  free(owned);
  return 0;
}

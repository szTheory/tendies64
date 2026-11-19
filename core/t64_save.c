#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*
 * Save media of the Phase 9 composite family: the device is selected by the
 * profile save_kind alone (never by header, title, game code, checksum or file
 * name). Every figure and rule here is a named software-profile policy; no
 * hardware accuracy or hardware timing is claimed.
 */

#define SAVE_BLANK_EEPROM UINT8_C(0xFF)
#define SAVE_BLANK_SRAM UINT8_C(0x00)

size_t t64_save_storage_bytes(uint32_t kind) {
  switch (kind) {
  case T64_SAVE_EEPROM_4K:
    return T64_SAVE_EEPROM_4K_BYTES;
  case T64_SAVE_EEPROM_16K:
    return T64_SAVE_EEPROM_16K_BYTES;
  case T64_SAVE_SRAM_32K:
    return T64_SAVE_SRAM_BYTES;
  default:
    return 0u;
  }
}

/* Blank fill and a cleared deadline; no effect when the profile has no save
 * device. Runs at create and at the reset site of the admission path. */
void t64_save_reset_machine_state(t64_machine *machine) {
  size_t size;

  if (machine == NULL) {
    return;
  }
  machine->save.busy_until = 0u;
  size = t64_save_storage_bytes(machine->profile.save_kind);
  if (size == 0u || machine->save.bytes == NULL) {
    return;
  }
  (void)memset(machine->save.bytes,
               machine->profile.save_kind == T64_SAVE_SRAM_32K
                   ? SAVE_BLANK_SRAM
                   : SAVE_BLANK_EEPROM,
               size);
}

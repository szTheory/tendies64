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

/*
 * EEPROM on the cartridge channel (channel 4). Classification is static and
 * outcome is dynamic: the handshake total of an RD64B start is computed from
 * the table before the execute tick exists, so the class looks only at the
 * profile kind, the declared counts and the command byte. Only the exact
 * triples (0x00, 1, 3), (0x04, 2, 8) and (0x05, 10, 1) are VALID on an EEPROM
 * profile; everything else there is TIMEOUT; a profile without an EEPROM is
 * ABSENT. A read while busy is VALID here and answers the timeout flag at the
 * execute tick (named policy). All of it is named software-profile policy.
 */
#define SAVE_CMD_INFO UINT8_C(0x00)
#define SAVE_CMD_READ UINT8_C(0x04)
#define SAVE_CMD_WRITE UINT8_C(0x05)
#define SAVE_FLAG_NO_DEVICE UINT8_C(0x80)
#define SAVE_FLAG_TIMEOUT UINT8_C(0x40)
#define SAVE_BUSY_STATUS UINT8_C(0x80)
#define SAVE_ID_4K UINT8_C(0x80)
#define SAVE_ID_16K UINT8_C(0xC0)
#define SAVE_MIRROR_MASK_4K UINT8_C(0x3F)

t64_save_channel4_class t64_save_channel4_classify(const t64_machine *machine,
                                                   uint8_t command,
                                                   uint32_t tx_count,
                                                   uint32_t rx_count) {
  if (!t64_has_eeprom_save(machine)) {
    return T64_SAVE_CH4_ABSENT;
  }
  if ((command == SAVE_CMD_INFO && tx_count == 1u && rx_count == 3u) ||
      (command == SAVE_CMD_READ && tx_count == 2u && rx_count == 8u) ||
      (command == SAVE_CMD_WRITE && tx_count == 10u && rx_count == 1u)) {
    return T64_SAVE_CH4_VALID;
  }
  return T64_SAVE_CH4_TIMEOUT;
}

/* The block index selects eight bytes; the 4 Kbit part mirrors block indexes
 * 64 to 255 onto 0 to 63 (named policy resting on one community source). */
static uint8_t *eeprom_block(t64_machine *machine, uint8_t index) {
  uint32_t block = index;

  if (machine->profile.save_kind == T64_SAVE_EEPROM_4K) {
    block &= (uint32_t)SAVE_MIRROR_MASK_4K;
  }
  return machine->save.bytes + (size_t)block * T64_SAVE_EEPROM_BLOCK_BYTES;
}

/* Executes one channel 4 transaction at the execute tick `tick`. Returns the
 * receive flag bits: 0x00, 0x40 (timeout) or 0x80 (no device). Never reads tx
 * beyond tx_count and never writes rx beyond rx_count. No scheduler event is
 * involved: the busy window is a stored deadline compared at later ticks. */
uint8_t t64_save_channel4_execute(t64_machine *machine, uint64_t tick,
                                  const uint8_t *tx, uint32_t tx_count,
                                  uint8_t *rx, uint32_t rx_count) {
  const uint8_t command = (tx != NULL && tx_count >= 1u) ? tx[0] : UINT8_C(0xFF);
  bool busy;

  if (machine == NULL || tx == NULL || rx == NULL ||
      machine->save.bytes == NULL) {
    return SAVE_FLAG_NO_DEVICE;
  }
  switch (t64_save_channel4_classify(machine, command, tx_count, rx_count)) {
  case T64_SAVE_CH4_ABSENT:
    return SAVE_FLAG_NO_DEVICE;
  case T64_SAVE_CH4_TIMEOUT:
    return SAVE_FLAG_TIMEOUT;
  case T64_SAVE_CH4_VALID:
    break;
  }
  busy = tick < machine->save.busy_until;
  if (command == SAVE_CMD_INFO) {
    rx[0] = UINT8_C(0x00);
    rx[1] = machine->profile.save_kind == T64_SAVE_EEPROM_4K ? SAVE_ID_4K
                                                             : SAVE_ID_16K;
    rx[2] = busy ? SAVE_BUSY_STATUS : UINT8_C(0x00);
    return 0u;
  }
  if (command == SAVE_CMD_READ) {
    if (busy) {
      return SAVE_FLAG_TIMEOUT;
    }
    (void)memcpy(rx, eeprom_block(machine, tx[1]),
                 T64_SAVE_EEPROM_BLOCK_BYTES);
    return 0u;
  }
  /* Write: data commit at acceptance, deadline from this execute tick. */
  if (busy) {
    rx[0] = SAVE_BUSY_STATUS;
    return 0u;
  }
  if (tick > UINT64_MAX - T64_SAVE_EEPROM_BUSY_TICKS) {
    return SAVE_FLAG_TIMEOUT;
  }
  (void)memcpy(eeprom_block(machine, tx[1]), tx + 2,
               T64_SAVE_EEPROM_BLOCK_BYTES);
  machine->save.busy_until = tick + T64_SAVE_EEPROM_BUSY_TICKS;
  rx[0] = UINT8_C(0x00);
  return 0u;
}

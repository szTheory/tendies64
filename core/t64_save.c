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

/* Blank fill, a cleared deadline and a cleared import generation; no storage
 * effect when the profile has no save device. Runs at create and at the reset
 * site of the admission path (D-25: a successful replacement restores blank
 * save content; a refused admission never reaches it). */
void t64_save_reset_machine_state(t64_machine *machine) {
  size_t size;

  if (machine == NULL) {
    return;
  }
  machine->save.busy_until = 0u;
  machine->save.write_generation = 0u;
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

void t64_save_generation_advance(t64_machine *machine) {
  if (machine != NULL && machine->profile.save_kind != 0u &&
      machine->save.write_generation != UINT64_MAX) {
    machine->save.write_generation++;
  }
}

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

/*
 * SRAM window [0x08000000, 0x08008000): reached by aligned 32-bit access and
 * by PI DMA only, for the explicit SRAM profile. No mirroring, no banking.
 * Named software-profile policy (single community source for the base
 * address and size); no hardware accuracy is claimed.
 */
t64_status t64_save_sram_read_word(const t64_machine *machine,
                                   uint32_t address, uint32_t *value_out) {
  const uint8_t *bytes;
  uint32_t offset;

  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_sram_save(machine) || machine->save.bytes == NULL ||
      address < T64_SRAM_BASE ||
      address - T64_SRAM_BASE > T64_SAVE_SRAM_BYTES - 4u) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if ((address & UINT32_C(3)) != 0u) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  offset = address - T64_SRAM_BASE;
  bytes = machine->save.bytes + offset;
  *value_out = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
               ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
  return T64_STATUS_OK;
}

t64_status t64_save_sram_write_word(t64_machine *machine, uint32_t address,
                                    uint32_t value) {
  uint8_t *bytes;
  uint32_t offset;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_sram_save(machine) || machine->save.bytes == NULL ||
      address < T64_SRAM_BASE ||
      address - T64_SRAM_BASE > T64_SAVE_SRAM_BYTES - 4u) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if ((address & UINT32_C(3)) != 0u) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  offset = address - T64_SRAM_BASE;
  bytes = machine->save.bytes + offset;
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  return T64_STATUS_OK;
}

/* The PI DMA effect, applied at the completion event: SRAM to RDRAM copies
 * the storage into RDRAM and invalidates the RDRAM derived state; RDRAM to
 * SRAM copies RDRAM into the storage (no RDRAM invalidation). The spans were
 * validated at the start; they are checked again here with checked sums. */
t64_status t64_save_sram_dma_apply(t64_machine *machine, uint32_t direction,
                                   uint32_t dram_address,
                                   uint32_t sram_offset, uint32_t length) {
  if (machine == NULL || !t64_has_sram_save(machine) ||
      machine->save.bytes == NULL || machine->ram == NULL ||
      (uint64_t)dram_address + (uint64_t)length >
          (uint64_t)machine->profile.rdram_bytes ||
      (uint64_t)sram_offset + (uint64_t)length >
          (uint64_t)T64_SAVE_SRAM_BYTES) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (direction == T64_PI_DIRECTION_SRAM_TO_RDRAM) {
    (void)memcpy(machine->ram + dram_address,
                 machine->save.bytes + sram_offset, (size_t)length);
    t64_rdp_memory_coverage_invalidate(machine, dram_address, length);
    t64_cpu_link_invalidate_range(machine, dram_address, length);
    return T64_STATUS_OK;
  }
  if (direction == T64_PI_DIRECTION_RDRAM_TO_SRAM) {
    (void)memcpy(machine->save.bytes + sram_offset,
                 machine->ram + dram_address, (size_t)length);
    return T64_STATUS_OK;
  }
  return T64_STATUS_INVALID_ARGUMENT;
}

/* True while an EEPROM deadline is in the future at the machine tick, or an
 * SRAM DMA descriptor (a non-ROM PI direction) is active. */
bool t64_save_in_flight(const t64_machine *machine) {
  if (machine == NULL) {
    return false;
  }
  return machine->tick < machine->save.busy_until ||
         (machine->pi.active_valid &&
          machine->pi.active_save_direction !=
              T64_PI_DIRECTION_ROM_TO_RDRAM);
}

/*
 * The D-24 rows R3 to R6 of a save device request, shared by the info, import
 * and export functions so they cannot drift: the profile has a device, the
 * kind is known, the kind is supported, the kind equals the profile's. A
 * profile with no device answers DEVICE_MISMATCH for any kind (the requested
 * kind versus the profile's none), which keeps the order observable: a
 * no-device profile with an unknown kind reports the mismatch. The helper
 * reads the profile only and never writes anything.
 */
#define SAVE_KIND_LAST_KNOWN UINT32_C(7)

static t64_status save_request_prefix(const t64_machine *machine,
                                      uint32_t requested_kind) {
  if (machine->profile.save_kind == T64_SAVE_NONE) {
    return T64_STATUS_SAVE_DEVICE_MISMATCH;
  }
  if (requested_kind == T64_SAVE_NONE ||
      requested_kind > SAVE_KIND_LAST_KNOWN) {
    return T64_STATUS_SAVE_UNKNOWN_DEVICE;
  }
  if (requested_kind > T64_SAVE_SRAM_32K) {
    return T64_STATUS_SAVE_UNSUPPORTED_DEVICE;
  }
  if (requested_kind != machine->profile.save_kind) {
    return T64_STATUS_SAVE_DEVICE_MISMATCH;
  }
  if (machine->save.bytes == NULL) {
    /* Unreachable while the create path sizes the region from the profile. */
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  return T64_STATUS_OK;
}

/*
 * The D-24 check prefix of the info request, in this exact order: pointers,
 * admitted, then rows R3 to R6. (Import and export check admission before the
 * pointer and span row, because they read or write a caller span; the info
 * function must write its output, so its pointer row comes first. The
 * difference is recorded in docs/spec/save-media.md.) The function never
 * writes the machine and writes the output only on success.
 */
t64_status t64_machine_save_info(const t64_machine *machine,
                                 uint32_t requested_kind,
                                 t64_save_info *info_out) {
  t64_address_range output_range;
  t64_status status;

  T64_ENTRY_GUARD(machine);
  if (machine == NULL || info_out == NULL ||
      !t64_internal_make_range(info_out, sizeof *info_out, &output_range) ||
      !t64_internal_observation_range_is_disjoint(machine, output_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  status = save_request_prefix(machine, requested_kind);
  if (status != T64_STATUS_OK) {
    return status;
  }
  info_out->kind = machine->profile.save_kind;
  info_out->size_bytes =
      (uint32_t)t64_save_storage_bytes(machine->profile.save_kind);
  info_out->flags = t64_save_in_flight(machine) ? T64_SAVE_INFO_BUSY : 0u;
  info_out->write_generation = machine->save.write_generation;
  return T64_STATUS_OK;
}

/*
 * Byte-order selections (assumption A1: authored policy, not a hardware fact;
 * no other emulator's convention is asserted). RAW copies; SWAP16 swaps the
 * two bytes inside every 16-bit unit; WORDREV32 reverses the four bytes inside
 * every 32-bit unit. Both transforms are involutions, so import and export
 * apply the same permutation. The three storage sizes are multiples of 4, so
 * there is no padding. Source and destination are disjoint spans.
 */
static bool save_format_valid(uint32_t format) {
  return format == T64_SAVE_FORMAT_RAW || format == T64_SAVE_FORMAT_SWAP16 ||
         format == T64_SAVE_FORMAT_WORDREV32;
}

static void save_transform_copy(uint8_t *destination, const uint8_t *source,
                                size_t length, uint32_t format) {
  size_t offset;

  if (format == T64_SAVE_FORMAT_SWAP16) {
    for (offset = 0u; offset + 2u <= length; offset += 2u) {
      destination[offset] = source[offset + 1u];
      destination[offset + 1u] = source[offset];
    }
    return;
  }
  if (format == T64_SAVE_FORMAT_WORDREV32) {
    for (offset = 0u; offset + 4u <= length; offset += 4u) {
      destination[offset] = source[offset + 3u];
      destination[offset + 1u] = source[offset + 2u];
      destination[offset + 2u] = source[offset + 1u];
      destination[offset + 3u] = source[offset];
    }
    return;
  }
  (void)memcpy(destination, source, length);
}

/*
 * Export, D-24 rows R0 to R8 in this exact order (export has no R9: the
 * committed bytes are always readable). Every check precedes the first write
 * to the caller buffer, so a refusal leaves the buffer and the machine
 * untouched; only the too-small row writes *length_out (the required size). A
 * zero capacity constructs no span (an empty span cannot overlap or overflow)
 * and asks for the required length.
 */
t64_status t64_machine_save_export(const t64_machine *machine,
                                   uint32_t device_kind, uint32_t format,
                                   uint8_t *bytes_out, size_t capacity,
                                   size_t *length_out) {
  t64_address_range length_range;
  t64_address_range bytes_range;
  t64_status status;
  size_t size;

  T64_ENTRY_GUARD(machine);
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (length_out == NULL ||
      !t64_internal_make_range(length_out, sizeof *length_out,
                               &length_range) ||
      !t64_internal_observation_range_is_disjoint(machine, length_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (bytes_out == NULL) {
    if (capacity != 0u) {
      return T64_STATUS_INVALID_ARGUMENT;
    }
  } else if (capacity != 0u &&
             (!t64_internal_make_range(bytes_out, capacity, &bytes_range) ||
              !t64_internal_observation_range_is_disjoint(machine,
                                                          bytes_range) ||
              t64_internal_ranges_overlap(bytes_range, length_range))) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = save_request_prefix(machine, device_kind);
  if (status != T64_STATUS_OK) {
    return status;
  }
  if (!save_format_valid(format)) {
    return T64_STATUS_SAVE_BAD_FORMAT;
  }
  size = t64_save_storage_bytes(machine->profile.save_kind);
  if (capacity < size) {
    *length_out = size;
    return T64_STATUS_BUFFER_TOO_SMALL;
  }
  save_transform_copy(bytes_out, machine->save.bytes, size, format);
  *length_out = size;
  return T64_STATUS_OK;
}

/*
 * Import, D-24 rows R0 to R9 in this exact order. A length of 0 is handled
 * before any span is constructed (an empty span cannot overlap or overflow and
 * is then refused at row R8). Every check precedes the first read of a caller
 * byte and the first mutation, so atomicity is structural and no staging
 * buffer exists. On success the normalized bytes are stored, the deadline is
 * cleared (an import is not a guest write), the scheduler queue and the guest
 * tick are untouched, no trace record is written and the observer-only
 * write_generation advances by one.
 */
t64_status t64_machine_save_import(t64_machine *machine, uint32_t device_kind,
                                   uint32_t format, const uint8_t *bytes,
                                   size_t length) {
  t64_address_range span;
  t64_status status;
  size_t size;

  T64_ENTRY_GUARD(machine);
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (bytes == NULL ||
      (length != 0u &&
       (!t64_internal_make_range(bytes, length, &span) ||
        !t64_internal_observation_range_is_disjoint(machine, span)))) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = save_request_prefix(machine, device_kind);
  if (status != T64_STATUS_OK) {
    return status;
  }
  if (!save_format_valid(format)) {
    return T64_STATUS_SAVE_BAD_FORMAT;
  }
  size = t64_save_storage_bytes(machine->profile.save_kind);
  if (length != size) {
    return T64_STATUS_SAVE_SIZE_MISMATCH;
  }
  if (t64_save_in_flight(machine)) {
    return T64_STATUS_SAVE_BUSY;
  }
  save_transform_copy(machine->save.bytes, bytes, size, format);
  machine->save.busy_until = 0u;
  t64_save_generation_advance(machine);
  return T64_STATUS_OK;
}

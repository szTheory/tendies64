#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*
 * Joybus frame parser, tick planner and executor for the modeled PIF RAM.
 *
 * The parser is a total, bounded function over every 64-byte image: it reads
 * only offsets 0 to 62 (offset 63 is the PIF command byte), keeps at most five
 * channel entries and never reports an error; a malformed or truncated frame
 * is a guest-visible outcome, not a host refusal. Every figure here (escape
 * handling, count and flag masks, the handshake bound, response lengths, the
 * tick formulas) is named software-profile policy of the composite family.
 * No hardware accuracy or hardware timing is claimed.
 */

#define JOYBUS_ESCAPE_SKIP UINT8_C(0x00)
#define JOYBUS_ESCAPE_RESET UINT8_C(0xFD)
#define JOYBUS_ESCAPE_END UINT8_C(0xFE)
#define JOYBUS_ESCAPE_NOP UINT8_C(0xFF)
#define JOYBUS_COUNT_MASK UINT8_C(0x3F)
#define JOYBUS_FLAG_MASK UINT8_C(0xC0)
#define JOYBUS_FLAG_SKIP UINT8_C(0x80)
#define JOYBUS_FLAG_RESET UINT8_C(0x40)
#define JOYBUS_RX_NO_DEVICE UINT8_C(0x80)
#define JOYBUS_RX_TIMEOUT UINT8_C(0x40)
#define JOYBUS_LAST_HANDSHAKE_OFFSET 62u
#define JOYBUS_CMD_INFO UINT8_C(0x00)
#define JOYBUS_CMD_STATE UINT8_C(0x01)
#define JOYBUS_CMD_RESET_INFO UINT8_C(0xFF)
#define JOYBUS_BUTTONS_LOW_MASK UINT8_C(0x3F)

typedef enum {
  JOYBUS_OUTCOME_ABSENT = 0,
  JOYBUS_OUTCOME_TIMEOUT,
  JOYBUS_OUTCOME_RESPONSE
} joybus_outcome;

typedef struct {
  joybus_outcome outcome;
  uint8_t data[4];
  uint32_t length;
} joybus_response;

void t64_joybus_parse(const uint8_t pif_ram[T64_PIF_RAM_BYTES],
                      t64_joybus_table *table_out) {
  uint32_t position = 0u;
  uint32_t channel = 0u;

  if (pif_ram == NULL || table_out == NULL) {
    return;
  }
  (void)memset(table_out, 0, sizeof *table_out);
  table_out->parsed = true;
  while (position <= JOYBUS_LAST_HANDSHAKE_OFFSET &&
         channel < T64_JOYBUS_CHANNELS) {
    uint8_t first = pif_ram[position];
    uint32_t tx_count;
    uint32_t rx_count;
    t64_joybus_entry *entry;

    if (first == JOYBUS_ESCAPE_END) {
      break;
    }
    if (first == JOYBUS_ESCAPE_NOP) {
      position++;
      continue;
    }
    if (first == JOYBUS_ESCAPE_SKIP) {
      position++;
      channel++;
      continue;
    }
    if (first == JOYBUS_ESCAPE_RESET) {
      entry = &table_out->entries[table_out->entry_count];
      entry->kind = T64_JOYBUS_ENTRY_RESET;
      entry->channel = channel;
      entry->offset = position;
      table_out->entry_count++;
      position++;
      channel++;
      continue;
    }
    tx_count = (uint32_t)(first & JOYBUS_COUNT_MASK);
    if (tx_count == 0u || position + 1u > JOYBUS_LAST_HANDSHAKE_OFFSET) {
      break;
    }
    rx_count = (uint32_t)(pif_ram[position + 1u] & JOYBUS_COUNT_MASK);
    if (position + 2u + tx_count + rx_count > T64_PIF_RAM_BYTES - 1u) {
      break;
    }
    entry = &table_out->entries[table_out->entry_count];
    entry->kind = T64_JOYBUS_ENTRY_TRANSACT;
    entry->channel = channel;
    entry->offset = position;
    entry->tx_count = tx_count;
    entry->rx_count = rx_count;
    entry->flags = (uint32_t)(first & JOYBUS_FLAG_MASK);
    table_out->entry_count++;
    position += 2u + tx_count + rx_count;
    channel++;
  }
}

/* The cartridge channel (channel 4) is dispatched through this one function.
 * In this plan no profile has a device there, so it answers absent for every
 * command; the save devices replace the body behind the same interface. */
static bool cartridge_channel_response(const t64_joybus_entry *entry,
                                       uint8_t command,
                                       joybus_response *response_out) {
  (void)entry;
  (void)command;
  (void)response_out;
  return false;
}

/* Resolve what a non-skipped transaction yields. Policy: a controller answers
 * info (0x00 and 0xFF) with 05 00 02 and state (0x01) with four bytes when
 * exactly one command byte was sent; every other command or command length
 * times out with no data; a channel without a device reports no device. */
static void resolve_transaction(const t64_joybus_entry *entry,
                                uint8_t command,
                                const t64_joybus_controller *port0,
                                joybus_response *response_out) {
  (void)memset(response_out, 0, sizeof *response_out);
  if (entry->channel == 4u) {
    if (!cartridge_channel_response(entry, command, response_out)) {
      response_out->outcome = JOYBUS_OUTCOME_ABSENT;
    }
    return;
  }
  if (entry->channel != 0u || port0 == NULL || !port0->present) {
    response_out->outcome = JOYBUS_OUTCOME_ABSENT;
    return;
  }
  response_out->outcome = JOYBUS_OUTCOME_TIMEOUT;
  if (entry->tx_count != 1u) {
    return;
  }
  if (command == JOYBUS_CMD_INFO || command == JOYBUS_CMD_RESET_INFO) {
    response_out->outcome = JOYBUS_OUTCOME_RESPONSE;
    response_out->data[0] = UINT8_C(0x05);
    response_out->data[1] = UINT8_C(0x00);
    response_out->data[2] = UINT8_C(0x02);
    response_out->length = 3u;
  } else if (command == JOYBUS_CMD_STATE) {
    response_out->outcome = JOYBUS_OUTCOME_RESPONSE;
    response_out->data[0] = (uint8_t)(port0->buttons >> 8);
    response_out->data[1] =
        (uint8_t)((uint8_t)port0->buttons & JOYBUS_BUTTONS_LOW_MASK);
    response_out->data[2] = port0->x;
    response_out->data[3] = port0->y;
    response_out->length = 4u;
  }
}

static uint8_t entry_command(const uint8_t pif_ram[T64_PIF_RAM_BYTES],
                             const t64_joybus_entry *entry) {
  if ((entry->flags & (uint32_t)JOYBUS_FLAG_RESET) != 0u) {
    return JOYBUS_CMD_RESET_INFO;
  }
  return pif_ram[entry->offset + 2u];
}

static bool entry_is_valid(const t64_joybus_entry *entry) {
  return entry->offset <= JOYBUS_LAST_HANDSHAKE_OFFSET &&
         entry->tx_count <= (uint32_t)JOYBUS_COUNT_MASK &&
         entry->rx_count <= (uint32_t)JOYBUS_COUNT_MASK &&
         entry->offset + 2u + entry->tx_count + entry->rx_count <=
             T64_PIF_RAM_BYTES - 1u;
}

static uint64_t transaction_ticks(const t64_joybus_entry *entry,
                                  const joybus_response *response) {
  uint64_t sent = (uint64_t)entry->tx_count * T64_JOYBUS_BYTE_TICKS;

  if (response->outcome == JOYBUS_OUTCOME_RESPONSE) {
    uint32_t transferred =
        entry->rx_count < response->length ? entry->rx_count
                                           : response->length;

    return sent + T64_JOYBUS_CONSOLE_STOP_TICKS +
           T64_JOYBUS_TX_RX_GAP_TICKS +
           (uint64_t)transferred * T64_JOYBUS_BYTE_TICKS +
           T64_JOYBUS_DEVICE_STOP_TICKS;
  }
  return sent + T64_JOYBUS_CONSOLE_STOP_TICKS + T64_JOYBUS_TIMEOUT_TICKS;
}

uint64_t t64_joybus_plan(const uint8_t pif_ram[T64_PIF_RAM_BYTES],
                         const t64_joybus_table *table, bool port0_present) {
  t64_joybus_controller port0;
  uint64_t total = 0u;
  uint32_t index;

  if (pif_ram == NULL || table == NULL || !table->parsed) {
    return 0u;
  }
  (void)memset(&port0, 0, sizeof port0);
  port0.present = port0_present;
  for (index = 0u;
       index < table->entry_count && index < T64_JOYBUS_CHANNELS; index++) {
    const t64_joybus_entry *entry = &table->entries[index];
    joybus_response response;

    if (entry->kind == T64_JOYBUS_ENTRY_RESET) {
      total += T64_JOYBUS_RESET_TICKS;
      continue;
    }
    if (entry->kind != T64_JOYBUS_ENTRY_TRANSACT || !entry_is_valid(entry) ||
        (entry->flags & (uint32_t)JOYBUS_FLAG_SKIP) != 0u) {
      continue;
    }
    resolve_transaction(entry, entry_command(pif_ram, entry), &port0,
                        &response);
    total += transaction_ticks(entry, &response);
  }
  return total;
}

void t64_joybus_execute(uint8_t pif_ram[T64_PIF_RAM_BYTES],
                        const t64_joybus_table *table,
                        const t64_joybus_controller *port0) {
  uint32_t index;

  if (pif_ram == NULL || table == NULL || !table->parsed) {
    return;
  }
  for (index = 0u;
       index < table->entry_count && index < T64_JOYBUS_CHANNELS; index++) {
    const t64_joybus_entry *entry = &table->entries[index];
    joybus_response response;
    uint8_t flags = 0u;
    uint32_t written = 0u;

    if (entry->kind != T64_JOYBUS_ENTRY_TRANSACT || !entry_is_valid(entry) ||
        (entry->flags & (uint32_t)JOYBUS_FLAG_SKIP) != 0u) {
      continue;
    }
    resolve_transaction(entry, entry_command(pif_ram, entry), port0,
                        &response);
    if (response.outcome == JOYBUS_OUTCOME_ABSENT) {
      flags = JOYBUS_RX_NO_DEVICE;
    } else if (response.outcome == JOYBUS_OUTCOME_TIMEOUT) {
      flags = JOYBUS_RX_TIMEOUT;
    } else {
      written = entry->rx_count < response.length ? entry->rx_count
                                                  : response.length;
      if (entry->rx_count > response.length) {
        flags = JOYBUS_RX_TIMEOUT;
      }
      (void)memcpy(pif_ram + entry->offset + 2u + entry->tx_count,
                   response.data, written);
    }
    /* The count is rebuilt from the parsed table on every execution so a
       frame executed again never accumulates flag bits. */
    pif_ram[entry->offset + 1u] = (uint8_t)(entry->rx_count | flags);
  }
}

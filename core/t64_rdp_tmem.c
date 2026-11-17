#include "t64_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RDP_OP_TEX_RECT UINT32_C(0x24)
#define RDP_OP_TEX_RECT_FLIP UINT32_C(0x25)
#define RDP_OP_SET_CONVERT UINT32_C(0x2c)
#define RDP_OP_SET_TILE_SIZE UINT32_C(0x32)
#define RDP_OP_LOAD_BLOCK UINT32_C(0x33)
#define RDP_OP_LOAD_TILE UINT32_C(0x34)
#define RDP_OP_SET_TILE UINT32_C(0x35)
#define RDP_OP_SET_TEXTURE_IMAGE UINT32_C(0x3d)
#define RDP_OP_LOAD_TLUT UINT32_C(0x30)

static uint32_t command_tile(uint64_t word) {
  return (uint32_t)((word >> 24) & UINT64_C(7));
}

static uint32_t field12(uint64_t word, uint32_t shift) {
  return (uint32_t)((word >> shift) & UINT64_C(0xfff));
}

static int32_t sign_extend16(uint32_t value) {
  value &= UINT32_C(0xffff);
  return (value & UINT32_C(0x8000)) != 0u
             ? (int32_t)value - INT32_C(65536)
             : (int32_t)value;
}

static int64_t floor_div_pow2(int64_t value, uint32_t shift) {
  int64_t divisor = (int64_t)(UINT32_C(1) << shift);
  int64_t quotient = value / divisor;
  int64_t remainder = value % divisor;

  if (value < 0 && remainder != 0) quotient--;
  return quotient;
}

static uint32_t bits_per_texel(uint32_t size) {
  return UINT32_C(4) << size;
}

static bool format_size_supported(uint32_t format, uint32_t size) {
  switch (format) {
  case 0u: return size == 2u || size == 3u; /* RGBA16/32 */
  case 1u: return size == 2u;               /* YUV16 */
  case 2u: return size == 0u || size == 1u; /* CI4/8 */
  case 3u: return size <= 2u;               /* IA4/8/16 */
  case 4u: return size <= 1u;               /* I4/8 */
  default: return false;
  }
}

static int32_t signed9(uint32_t value) {
  value &= UINT32_C(0x1ff);
  return (value & UINT32_C(0x100)) != 0u
             ? (int32_t)value - INT32_C(512)
             : (int32_t)value;
}

static int32_t coeff(const t64_rdp_state *rdp, size_t index) {
  return signed9(rdp->convert_coefficients[index]);
}

static uint8_t clamp_u8(int32_t value) {
  if (value < 0) return 0u;
  if (value > 255) return UINT8_MAX;
  return (uint8_t)value;
}

void t64_rdp_tmem_convert_yuv(const t64_rdp_state *rdp, uint8_t y,
                              uint8_t u, uint8_t v, uint8_t rgb_out[3]) {
  int32_t red, green, blue;
  int32_t u_delta, v_delta;

  if (rdp == NULL || rgb_out == NULL) return;
  u_delta = (int32_t)u - INT32_C(128);
  v_delta = (int32_t)v - INT32_C(128);
  red = (int32_t)y + (coeff(rdp, 0u) * v_delta) / 128;
  green = (int32_t)y + (coeff(rdp, 1u) * u_delta +
                        coeff(rdp, 2u) * v_delta) /
                           128;
  blue = (int32_t)y + (coeff(rdp, 3u) * u_delta) / 128;
  rgb_out[0] = clamp_u8(red);
  rgb_out[1] = clamp_u8(green);
  rgb_out[2] = clamp_u8(blue);
}

static bool source_byte_address(const t64_machine *machine, uint32_t x,
                                uint32_t y, uint64_t *address_out) {
  const t64_rdp_state *rdp = &machine->rdp;
  const uint32_t bits = bits_per_texel(rdp->texture_image_size);
  const uint64_t row_bits = (uint64_t)rdp->texture_image_width * bits;
  const uint64_t row_bytes = (row_bits + UINT64_C(7)) / UINT64_C(8);
  const uint64_t texel_bits = (uint64_t)x * bits;
  const uint64_t byte_offset = texel_bits / UINT64_C(8);
  const uint64_t address = (uint64_t)rdp->texture_image_address +
                           (uint64_t)y * row_bytes + byte_offset;

  if (x >= rdp->texture_image_width || address >= machine->profile.rdram_bytes)
    return false;
  *address_out = address;
  return true;
}

static bool source_texel(const t64_machine *machine, uint32_t x, uint32_t y,
                         uint32_t *value_out) {
  const t64_rdp_state *rdp = &machine->rdp;
  uint64_t address;
  uint32_t bits = bits_per_texel(rdp->texture_image_size);
  uint32_t value;

  if (!source_byte_address(machine, x, y, &address)) return false;
  switch (bits) {
  case 4u: {
    uint8_t byte = machine->ram[(size_t)address];
    value = (x & 1u) == 0u ? byte >> 4 : byte & UINT8_C(0x0f);
    break;
  }
  case 8u: value = machine->ram[(size_t)address]; break;
  case 16u:
    if (address + UINT64_C(2) > machine->profile.rdram_bytes) return false;
    value = ((uint32_t)machine->ram[(size_t)address] << 8) |
            machine->ram[(size_t)address + 1u];
    break;
  case 32u:
    if (address + UINT64_C(4) > machine->profile.rdram_bytes) return false;
    value = ((uint32_t)machine->ram[(size_t)address] << 24) |
            ((uint32_t)machine->ram[(size_t)address + 1u] << 16) |
            ((uint32_t)machine->ram[(size_t)address + 2u] << 8) |
            machine->ram[(size_t)address + 3u];
    break;
  default: return false;
  }
  *value_out = value;
  return true;
}

static bool load_source_span_valid(const t64_machine *machine, uint32_t x0,
                                   uint32_t y0, uint32_t x1, uint32_t y1) {
  const t64_rdp_state *rdp = &machine->rdp;
  uint32_t bits = bits_per_texel(rdp->texture_image_size);
  uint64_t row_bits = (uint64_t)rdp->texture_image_width * bits;
  uint64_t row_bytes = (row_bits + UINT64_C(7)) / UINT64_C(8);
  uint64_t end;

  if (x0 > x1 || x1 >= rdp->texture_image_width || y0 > y1) return false;
  end = (uint64_t)rdp->texture_image_address +
        (uint64_t)y1 * row_bytes +
        (((uint64_t)x1 + UINT64_C(1)) * bits + UINT64_C(7)) / UINT64_C(8);
  return end <= machine->profile.rdram_bytes;
}

static uint64_t tmem_base(const t64_rdp_tile *tile) {
  return (uint64_t)tile->tmem_word * UINT64_C(8);
}

static bool tmem_store(t64_machine *machine, const t64_rdp_tile *tile,
                       uint64_t byte_offset, uint32_t bits, uint32_t value,
                       uint32_t row, uint32_t texel_index) {
  uint64_t address = tmem_base(tile) + byte_offset;
  uint8_t *tmem = machine->rdp.tmem;

  if ((row & 1u) != 0u) address ^= UINT64_C(4);
  if (bits == 4u) {
    uint8_t *byte;
    if (address >= T64_RDP_TMEM_BYTES) return false;
    byte = &tmem[(size_t)address];
    if ((texel_index & 1u) == 0u)
      *byte = (uint8_t)((*byte & UINT8_C(0x0f)) | ((value & 15u) << 4));
    else
      *byte = (uint8_t)((*byte & UINT8_C(0xf0)) | (value & 15u));
  } else if (bits == 8u) {
    if (address >= T64_RDP_TMEM_BYTES) return false;
    tmem[(size_t)address] = (uint8_t)value;
  } else if (bits == 16u) {
    if (address + UINT64_C(2) > T64_RDP_TMEM_BYTES) return false;
    tmem[(size_t)address] = (uint8_t)(value >> 8);
    tmem[(size_t)address + 1u] = (uint8_t)value;
  } else {
    if (address + UINT64_C(4) > T64_RDP_TMEM_BYTES) return false;
    tmem[(size_t)address] = (uint8_t)(value >> 24);
    tmem[(size_t)address + 1u] = (uint8_t)(value >> 16);
    tmem[(size_t)address + 2u] = (uint8_t)(value >> 8);
    tmem[(size_t)address + 3u] = (uint8_t)value;
  }
  return true;
}

static uint32_t tmem_read_bits(const t64_machine *machine, uint64_t address,
                               uint32_t bits, uint32_t texel_index) {
  const uint8_t *tmem = machine->rdp.tmem;

  if (address >= T64_RDP_TMEM_BYTES) return 0u;
  if (bits == 4u) {
    uint8_t byte = tmem[(size_t)address];
    return (texel_index & 1u) == 0u ? byte >> 4 : byte & 15u;
  }
  if (bits == 8u) return tmem[(size_t)address];
  if (bits == 16u) {
    if (address + UINT64_C(2) > T64_RDP_TMEM_BYTES) return 0u;
    return ((uint32_t)tmem[(size_t)address] << 8) | tmem[(size_t)address + 1u];
  }
  if (address + UINT64_C(4) > T64_RDP_TMEM_BYTES) return 0u;
  return ((uint32_t)tmem[(size_t)address] << 24) |
         ((uint32_t)tmem[(size_t)address + 1u] << 16) |
         ((uint32_t)tmem[(size_t)address + 2u] << 8) |
         tmem[(size_t)address + 3u];
}

bool t64_rdp_tmem_handles_opcode(uint32_t opcode) {
  switch (opcode) {
  case RDP_OP_SET_TEXTURE_IMAGE:
  case RDP_OP_SET_TILE:
  case RDP_OP_SET_TILE_SIZE:
  case RDP_OP_LOAD_TILE:
  case RDP_OP_LOAD_BLOCK:
  case RDP_OP_LOAD_TLUT:
  case RDP_OP_SET_CONVERT:
    return true;
  default: return false;
  }
}

bool t64_rdp_tmem_validate_command(const t64_machine *machine,
                                   uint32_t opcode, uint64_t word,
                                   uint32_t *reason_out) {
  const t64_rdp_state *rdp;
  uint32_t format, size, tile_index;

  if (machine == NULL || reason_out == NULL) return false;
  rdp = &machine->rdp;
  format = (uint32_t)((word >> 53) & UINT64_C(7));
  size = (uint32_t)((word >> 51) & UINT64_C(3));
  tile_index = command_tile(word);
  *reason_out = T64_RDP_REFUSAL_INVALID_TEXTURE;

  if (opcode == RDP_OP_SET_TEXTURE_IMAGE) {
    uint32_t width = (uint32_t)((word >> 32) & UINT64_C(0xfff)) + 1u;
    uint32_t address = (uint32_t)word & UINT32_C(0x03ffffff);
    if (!format_size_supported(format, size)) return false;
    if (width == 0u || width > 4096u ||
        (uint64_t)address >= machine->profile.rdram_bytes) {
      *reason_out = T64_RDP_REFUSAL_TEXTURE_RANGE;
      return false;
    }
    return true;
  }
  if (opcode == RDP_OP_SET_TILE) {
    uint32_t tmem_word = (uint32_t)((word >> 32) & UINT64_C(0x1ff));
    if (!format_size_supported(format, size) || tile_index >= T64_RDP_TILE_COUNT ||
        tmem_word >= T64_RDP_TMEM_BYTES / 8u) return false;
    return true;
  }
  if (opcode == RDP_OP_SET_TILE_SIZE) {
    uint32_t uls = field12(word, 44u);
    uint32_t ult = field12(word, 32u);
    uint32_t lrs = field12(word, 12u);
    uint32_t lrt = field12(word, 0u);
    if (tile_index >= T64_RDP_TILE_COUNT) return false;
    if (!rdp->tiles[tile_index].valid) return false;
    if (lrs < uls || lrt < ult) return false;
    return true;
  }
  if (opcode == RDP_OP_SET_CONVERT) return true;
  if (tile_index >= T64_RDP_TILE_COUNT || !rdp->tiles[tile_index].valid ||
      rdp->texture_image_valid == 0u) return false;
  if (opcode == RDP_OP_LOAD_TLUT) {
    uint32_t uls = field12(word, 44u) >> 2;
    uint32_t ult = field12(word, 32u) >> 2;
    uint32_t lrs = field12(word, 12u) >> 2;
    uint32_t count = lrs >= uls ? lrs - uls + 1u : 0u;
    uint64_t base = tmem_base(&rdp->tiles[tile_index]);
    if ((rdp->texture_image_format != 0u &&
         rdp->texture_image_format != 3u) ||
        rdp->texture_image_size != 2u || count == 0u || count > 256u ||
        base < UINT64_C(2048) || base + (uint64_t)count * 8u >
                                    T64_RDP_TMEM_BYTES ||
        !load_source_span_valid(machine, uls, ult, lrs, ult)) {
      *reason_out = T64_RDP_REFUSAL_TEXTURE_RANGE;
      return false;
    }
    return true;
  }
  if (rdp->tiles[tile_index].format != rdp->texture_image_format ||
      rdp->tiles[tile_index].size != rdp->texture_image_size) return false;
  if (opcode == RDP_OP_LOAD_TILE) {
    uint32_t uls = field12(word, 44u) >> 2;
    uint32_t ult = field12(word, 32u) >> 2;
    uint32_t lrs = field12(word, 12u) >> 2;
    uint32_t lrt = field12(word, 0u) >> 2;
    const t64_rdp_tile *tile = &rdp->tiles[tile_index];
    uint32_t bits = bits_per_texel(tile->size);
    uint64_t row_words = tile->line_words;
    uint64_t row_bytes = ((uint64_t)(lrs - uls + 1u) * bits + 7u) / 8u;
    uint64_t last = tmem_base(tile) +
                    (uint64_t)(lrt - ult + 1u) * row_words * 8u;
    if (lrs < uls || lrt < ult || tile->line_words == 0u ||
        row_bytes > (uint64_t)tile->line_words * UINT64_C(8) ||
        !load_source_span_valid(machine, uls, ult, lrs, lrt) ||
        last > T64_RDP_TMEM_BYTES) {
      *reason_out = T64_RDP_REFUSAL_TEXTURE_RANGE;
      return false;
    }
    return true;
  }
  if (opcode == RDP_OP_LOAD_BLOCK) {
    uint32_t uls = field12(word, 44u) >> 2;
    uint32_t ult = field12(word, 32u) >> 2;
    uint32_t lrs = field12(word, 12u);
    uint32_t count = lrs >= uls ? lrs - uls + 1u : 0u;
    uint64_t bits = bits_per_texel(rdp->tiles[tile_index].size);
    uint64_t end = UINT64_C(0);
    uint64_t first_pixel = (uint64_t)ult * rdp->texture_image_width + uls;
    uint64_t last_pixel = first_pixel + (count == 0u ? 0u : count - 1u);
    uint64_t last_y = last_pixel / rdp->texture_image_width;
    uint64_t last_x = last_pixel % rdp->texture_image_width;
    uint64_t row_bytes = ((uint64_t)rdp->texture_image_width *
                          bits_per_texel(rdp->texture_image_size) +
                          UINT64_C(7)) / UINT64_C(8);
    uint64_t source_end = (uint64_t)rdp->texture_image_address +
                          last_y * row_bytes +
                          ((last_x + UINT64_C(1)) * bits + UINT64_C(7)) /
                              UINT64_C(8);
    uint32_t texels_per_word = UINT32_C(64) / (uint32_t)bits;
    uint32_t dxt = field12(word, 0u);
    uint32_t accumulator = 0u;
    uint32_t word_count = count == 0u ? 0u :
        (count + texels_per_word - 1u) / texels_per_word;
    uint32_t word_index;
    const t64_rdp_tile *tile = &rdp->tiles[tile_index];

    for (word_index = 0u; word_index < word_count; word_index++) {
      uint32_t line = accumulator >> 11;
      uint64_t word_address = tmem_base(tile) +
          ((uint64_t)word_index + (uint64_t)line * tile->line_words) * 8u;
      end = word_address + UINT64_C(8);
      if (end > T64_RDP_TMEM_BYTES) break;
      accumulator += dxt;
    }
    if (count == 0u || count > 2048u ||
        uls >= rdp->texture_image_width ||
        source_end > machine->profile.rdram_bytes ||
        end > T64_RDP_TMEM_BYTES) {
      *reason_out = T64_RDP_REFUSAL_TEXTURE_RANGE;
      return false;
    }
    return true;
  }
  return false;
}

static void apply_load_tile(t64_machine *machine, uint32_t tile_index,
                            uint64_t word) {
  t64_rdp_state *rdp = &machine->rdp;
  t64_rdp_tile *tile = &rdp->tiles[tile_index];
  uint32_t uls = field12(word, 44u) >> 2;
  uint32_t ult = field12(word, 32u) >> 2;
  uint32_t lrs = field12(word, 12u) >> 2;
  uint32_t lrt = field12(word, 0u) >> 2;
  uint32_t bits = bits_per_texel(tile->size);
  uint32_t y;

  for (y = 0u; y <= lrt - ult; y++) {
    uint32_t x;
    uint64_t row_offset = (uint64_t)y * tile->line_words * 8u;
    for (x = 0u; x <= lrs - uls; x++) {
      uint32_t value = 0u;
      uint64_t byte_offset = row_offset + ((uint64_t)x * bits) / 8u;
      if (source_texel(machine, uls + x, ult + y, &value))
        (void)tmem_store(machine, tile, byte_offset, bits, value, y, x);
    }
  }
  tile->uls = uls << 2;
  tile->ult = ult << 2;
  tile->lrs = lrs << 2;
  tile->lrt = lrt << 2;
  tile->size_valid = true;
  rdp->dpc_tmem_counter = (rdp->dpc_tmem_counter + 1u) &
                          UINT32_C(0x00ffffff);
}

static void apply_load_block(t64_machine *machine, uint32_t tile_index,
                             uint64_t word) {
  t64_rdp_state *rdp = &machine->rdp;
  t64_rdp_tile *tile = &rdp->tiles[tile_index];
  uint32_t uls = field12(word, 44u) >> 2;
  uint32_t ult = field12(word, 32u) >> 2;
  uint32_t lrs = field12(word, 12u);
  uint32_t count = lrs - uls + 1u;
  uint32_t dxt = field12(word, 0u);
  uint32_t bits = bits_per_texel(tile->size);
  uint32_t accumulator = 0u;
  uint32_t texels_per_word = UINT32_C(64) / bits;
  uint32_t x;

  for (x = 0u; x < count; x++) {
    uint32_t value = 0u;
    uint32_t word_index = x / texels_per_word;
    uint32_t line = accumulator >> 11;
    uint32_t source_x = (uls + x) % rdp->texture_image_width;
    uint32_t source_y = ult + (uls + x) / rdp->texture_image_width;
    uint32_t texel_in_word = x % texels_per_word;
    uint64_t byte_offset;

    byte_offset = ((uint64_t)word_index +
                   (uint64_t)line * tile->line_words) * 8u +
                  ((uint64_t)texel_in_word * bits) / 8u;
    if (source_texel(machine, source_x, source_y, &value))
      (void)tmem_store(machine, tile, byte_offset, bits, value, line,
                       texel_in_word);
    if (texel_in_word + 1u == texels_per_word) accumulator += dxt;
  }
  tile->uls = uls << 2;
  tile->ult = ult << 2;
  tile->lrs = lrs << 2;
  tile->lrt = ult << 2;
  tile->size_valid = true;
  rdp->dpc_tmem_counter = (rdp->dpc_tmem_counter + 1u) &
                          UINT32_C(0x00ffffff);
}

static void apply_load_tlut(t64_machine *machine, uint32_t tile_index,
                            uint64_t word) {
  t64_rdp_state *rdp = &machine->rdp;
  const t64_rdp_tile *tile = &rdp->tiles[tile_index];
  uint32_t uls = field12(word, 44u) >> 2;
  uint32_t ult = field12(word, 32u) >> 2;
  uint32_t lrs = field12(word, 12u) >> 2;
  uint32_t x;
  uint64_t base = tmem_base(tile);

  for (x = 0u; x <= lrs - uls; x++) {
    uint32_t value = 0u;
    uint64_t address = base + (uint64_t)x * 8u;
    if (!source_texel(machine, uls + x, ult, &value)) continue;
    rdp->tmem[(size_t)address] = (uint8_t)(value >> 8);
    rdp->tmem[(size_t)address + 1u] = (uint8_t)value;
    rdp->tmem[(size_t)address + 2u] = (uint8_t)(value >> 8);
    rdp->tmem[(size_t)address + 3u] = (uint8_t)value;
    rdp->tmem[(size_t)address + 4u] = (uint8_t)(value >> 8);
    rdp->tmem[(size_t)address + 5u] = (uint8_t)value;
    rdp->tmem[(size_t)address + 6u] = (uint8_t)(value >> 8);
    rdp->tmem[(size_t)address + 7u] = (uint8_t)value;
  }
  rdp->tlut_valid = true;
  rdp->dpc_tmem_counter = (rdp->dpc_tmem_counter + 1u) &
                          UINT32_C(0x00ffffff);
}

void t64_rdp_tmem_apply_command(t64_machine *machine, uint32_t opcode,
                                uint64_t word) {
  t64_rdp_state *rdp;
  uint32_t tile_index;

  if (machine == NULL) return;
  rdp = &machine->rdp;
  tile_index = command_tile(word);
  if (opcode == RDP_OP_SET_TEXTURE_IMAGE) {
    rdp->texture_image_format = (uint32_t)((word >> 53) & UINT64_C(7));
    rdp->texture_image_size = (uint32_t)((word >> 51) & UINT64_C(3));
    rdp->texture_image_width =
        (uint32_t)((word >> 32) & UINT64_C(0xfff)) + 1u;
    rdp->texture_image_address =
        (uint32_t)word & UINT32_C(0x03ffffff);
    rdp->texture_image_valid = 1u;
  } else if (opcode == RDP_OP_SET_TILE) {
    t64_rdp_tile *tile = &rdp->tiles[tile_index];
    uint32_t cms = (uint32_t)((word >> 8) & UINT64_C(3));
    uint32_t cmt = (uint32_t)((word >> 18) & UINT64_C(3));
    tile->format = (uint32_t)((word >> 53) & UINT64_C(7));
    tile->size = (uint32_t)((word >> 51) & UINT64_C(3));
    tile->line_words = (uint32_t)((word >> 41) & UINT64_C(0x1ff));
    tile->tmem_word = (uint32_t)((word >> 32) & UINT64_C(0x1ff));
    tile->palette = (uint32_t)((word >> 20) & UINT64_C(15));
    tile->clamp_t = (cmt & 2u) != 0u;
    tile->mirror_t = (cmt & 1u) != 0u;
    tile->mask_t = (uint32_t)((word >> 14) & UINT64_C(15));
    tile->shift_t = (uint32_t)((word >> 10) & UINT64_C(15));
    tile->clamp_s = (cms & 2u) != 0u;
    tile->mirror_s = (cms & 1u) != 0u;
    tile->mask_s = (uint32_t)((word >> 4) & UINT64_C(15));
    tile->shift_s = (uint32_t)(word & UINT64_C(15));
    tile->valid = true;
    tile->size_valid = false;
  } else if (opcode == RDP_OP_SET_TILE_SIZE) {
    t64_rdp_tile *tile = &rdp->tiles[tile_index];
    tile->uls = field12(word, 44u);
    tile->ult = field12(word, 32u);
    tile->lrs = field12(word, 12u);
    tile->lrt = field12(word, 0u);
    tile->size_valid = tile->lrs >= tile->uls && tile->lrt >= tile->ult;
  } else if (opcode == RDP_OP_LOAD_TILE) {
    apply_load_tile(machine, tile_index, word);
  } else if (opcode == RDP_OP_LOAD_BLOCK) {
    apply_load_block(machine, tile_index, word);
  } else if (opcode == RDP_OP_LOAD_TLUT) {
    apply_load_tlut(machine, tile_index, word);
  } else if (opcode == RDP_OP_SET_CONVERT) {
    rdp->convert_coefficients[0] = (uint32_t)((word >> 45) & UINT64_C(0x1ff));
    rdp->convert_coefficients[1] = (uint32_t)((word >> 36) & UINT64_C(0x1ff));
    rdp->convert_coefficients[2] = (uint32_t)((word >> 27) & UINT64_C(0x1ff));
    rdp->convert_coefficients[3] = (uint32_t)((word >> 18) & UINT64_C(0x1ff));
    rdp->convert_coefficients[4] = (uint32_t)((word >> 9) & UINT64_C(0x1ff));
    rdp->convert_coefficients[5] = (uint32_t)(word & UINT64_C(0x1ff));
  }
}

bool t64_rdp_tmem_validate_copy(const t64_machine *machine, uint64_t first,
                                uint64_t second, uint32_t x0, uint32_t y0,
                                uint32_t x1, uint32_t y1,
                                uint32_t *reason_out) {
  const t64_rdp_state *rdp;
  uint32_t tile_index;
  const t64_rdp_tile *tile;
  uint64_t last_pixel, end;
  uint32_t opcode;

  if (machine == NULL || reason_out == NULL) return false;
  rdp = &machine->rdp;
  opcode = (uint32_t)((first >> 56) & UINT64_C(0x3f));
  tile_index = command_tile(first);
  *reason_out = T64_RDP_REFUSAL_RENDER_RANGE;
  if ((opcode != RDP_OP_TEX_RECT && opcode != RDP_OP_TEX_RECT_FLIP) ||
      !rdp->color_image_valid || !rdp->scissor_valid ||
      !rdp->other_modes_valid ||
      ((rdp->other_modes >> 52) & UINT64_C(3)) != 2u ||
      rdp->color_image_size != 2u || rdp->color_image_format != 0u ||
      tile_index >= T64_RDP_TILE_COUNT) return false;
  if (((second >> 16) & UINT64_C(0xffff)) != UINT64_C(4096)) return false;
  tile = &rdp->tiles[tile_index];
  if (!tile->valid || !tile->size_valid ||
      !format_size_supported(tile->format, tile->size) ||
      (tile->format == 0u && tile->size == 3u) || tile->format == 1u) {
    *reason_out = T64_RDP_REFUSAL_INVALID_TEXTURE;
    return false;
  }
  if (tile->format == 2u &&
      (!rdp->tlut_valid ||
       (rdp->tlut_format != 2u && rdp->tlut_format != 3u))) {
    *reason_out = T64_RDP_REFUSAL_INVALID_TEXTURE;
    return false;
  }
  {
    uint32_t bits = bits_per_texel(tile->size);
    uint64_t tile_width =
        ((uint64_t)(tile->lrs - tile->uls) >> 2) + UINT64_C(1);
    uint64_t tile_height =
        ((uint64_t)(tile->lrt - tile->ult) >> 2) + UINT64_C(1);
    uint64_t row_bytes = (tile_width * bits + UINT64_C(7)) / UINT64_C(8);
    uint64_t line_bytes = (uint64_t)tile->line_words * UINT64_C(8);
    uint64_t tmem_end = tmem_base(tile) + tile_height * line_bytes;
    if (tmem_end > T64_RDP_TMEM_BYTES || tile->line_words == 0u ||
        row_bytes > line_bytes) {
      *reason_out = T64_RDP_REFUSAL_TEXTURE_RANGE;
      return false;
    }
    if (tile->format == 2u) {
      uint64_t palette_end = UINT64_C(2048) +
          (uint64_t)(tile->size == 0u ? tile->palette * 16u + 15u : 255u) * 8u +
          8u;
      if (palette_end > T64_RDP_TMEM_BYTES) {
        *reason_out = T64_RDP_REFUSAL_TEXTURE_RANGE;
        return false;
      }
    }
  }
  if (x0 > x1 || y0 > y1) return true;
  if (x1 >= rdp->color_image_width) return false;
  last_pixel = (uint64_t)y1 * rdp->color_image_width + x1;
  end = (uint64_t)rdp->color_image_address + (last_pixel + 1u) * 2u;
  if (end > machine->profile.rdram_bytes) return false;
  (void)second;
  return true;
}

void t64_rdp_tmem_begin_copy(t64_machine *machine, uint64_t first,
                             uint64_t second, uint32_t x0, uint32_t y0,
                             uint32_t x1, uint32_t y1) {
  t64_rdp_state *rdp = &machine->rdp;
  uint32_t opcode = (uint32_t)((first >> 56) & UINT64_C(0x3f));
  int32_t start_s = sign_extend16((uint32_t)(second >> 48));
  int32_t start_t = sign_extend16((uint32_t)(second >> 32));
  int32_t dsdx = sign_extend16((uint32_t)(second >> 16));
  int32_t dtdy = sign_extend16((uint32_t)second);

  rdp->render_active = x0 <= x1 && y0 <= y1;
  rdp->render_kind = 2u;
  rdp->render_x_begin = x0;
  rdp->render_x_end = x1;
  rdp->render_y_begin = y0;
  rdp->render_y_end = y1;
  rdp->render_y = y0;
  rdp->render_texture_tile = command_tile(first);
  rdp->render_texture_s = start_s;
  rdp->render_texture_t = start_t;
  rdp->render_texture_dsdx = dsdx;
  rdp->render_texture_dtdy = dtdy;
  rdp->render_texture_flip = opcode == RDP_OP_TEX_RECT_FLIP;
  if (rdp->render_active) {
    int64_t origin_x = (int64_t)(field12(first, 44u) >> 2);
    int64_t origin_y = (int64_t)(field12(first, 32u) >> 2);
    int64_t clipped_dx = (int64_t)x0 - origin_x;
    int64_t clipped_dy = (int64_t)y0 - origin_y;
    int64_t s_pixels = rdp->render_texture_flip ? clipped_dy : clipped_dx;
    int64_t t_pixels = rdp->render_texture_flip ? clipped_dx : clipped_dy;
    int64_t clipped_s = (int64_t)start_s +
                         s_pixels * floor_div_pow2(dsdx, 7u);
    int64_t clipped_t = (int64_t)start_t +
                         t_pixels * floor_div_pow2(dtdy, 5u);

    rdp->render_texture_s = (int32_t)clipped_s;
    rdp->render_texture_t = (int32_t)clipped_t;
  }
}

static int32_t wrap_coordinate(int32_t fixed, uint32_t lo_fixed,
                               uint32_t hi_fixed, uint32_t mask,
                               uint32_t shift, bool clamp, bool mirror) {
  int64_t coord = floor_div_pow2(fixed, 5u);
  int64_t lo = (int64_t)(lo_fixed >> 2);
  int64_t hi = (int64_t)(hi_fixed >> 2);

  if (shift <= 10u) {
    coord = floor_div_pow2(coord, shift);
  } else {
    int64_t scale = (int64_t)(UINT32_C(1) << (16u - shift));
    coord = (coord * scale) % INT64_C(65536);
    if (coord < 0) coord += INT64_C(65536);
  }
  if (clamp) {
    if (coord < lo) coord = lo;
    if (coord > hi) coord = hi;
    return (int32_t)coord;
  }
  if (mask != 0u) {
    int64_t period = INT64_C(1) << mask;
    int64_t span = period * (mirror ? INT64_C(2) : INT64_C(1));
    int64_t phase = (coord - lo) % span;
    if (phase < 0) phase += span;
    if (mirror && phase >= period) phase = span - phase - 1;
    coord = lo + phase;
  }
  return (int32_t)coord;
}

static uint16_t rgba5551(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  uint32_t value = ((uint32_t)(r >> 3) << 11) |
                   ((uint32_t)(g >> 3) << 6) |
                   ((uint32_t)(b >> 3) << 1) |
                   (a >= 128u ? UINT32_C(1) : UINT32_C(0));
  return (uint16_t)value;
}

static bool read_tile_texel(const t64_machine *machine,
                            const t64_rdp_tile *tile, uint32_t x, uint32_t y,
                            uint16_t *pixel_out) {
  uint64_t width, height, row, offset;
  uint32_t bits;

  if (machine == NULL || tile == NULL || pixel_out == NULL ||
      tile->lrs < tile->uls || tile->lrt < tile->ult) return false;
  width = ((uint64_t)(tile->lrs - tile->uls) >> 2) + UINT64_C(1);
  height = ((uint64_t)(tile->lrt - tile->ult) >> 2) + UINT64_C(1);
  if ((uint64_t)x >= width || (uint64_t)y >= height) return false;
  bits = bits_per_texel(tile->size);
  row = (uint64_t)y * tile->line_words * 8u;
  offset = row + ((uint64_t)x * bits) / 8u;
  if ((y & 1u) != 0u) offset ^= UINT64_C(4);
  offset += tmem_base(tile);

  if (tile->format == 0u && tile->size == 2u) {
    *pixel_out = (uint16_t)tmem_read_bits(machine, offset, 16u, x);
    return true;
  }
  if (tile->format == 0u && tile->size == 3u) {
    uint32_t rgba = tmem_read_bits(machine, offset, 32u, x);
    *pixel_out = rgba5551((uint8_t)(rgba >> 24), (uint8_t)(rgba >> 16),
                          (uint8_t)(rgba >> 8), (uint8_t)rgba);
    return true;
  }
  if (tile->format == 2u) {
    uint32_t index = tmem_read_bits(machine, offset, bits, x);
    uint32_t palette_index = tile->size == 0u
                                 ? tile->palette * 16u + index
                                 : index;
    uint64_t tlut_address = UINT64_C(2048) +
                            (uint64_t)palette_index * UINT64_C(8);
    if (tlut_address + 2u > T64_RDP_TMEM_BYTES) return false;
    uint16_t palette_value =
        (uint16_t)tmem_read_bits(machine, tlut_address, 16u, 0u);
    if (machine->rdp.tlut_format == 3u) {
      uint8_t intensity = (uint8_t)(palette_value >> 8);
      uint8_t alpha = (uint8_t)palette_value;
      *pixel_out = rgba5551(intensity, intensity, intensity, alpha);
    } else {
      *pixel_out = palette_value;
    }
    return true;
  }
  if (tile->format == 3u || tile->format == 4u) {
    uint32_t value = tmem_read_bits(machine, offset, bits, x);
    uint8_t intensity, alpha = UINT8_MAX;
    if (tile->format == 3u && tile->size == 0u) {
      intensity = (uint8_t)(((value >> 1) & 7u) * 255u / 7u);
      alpha = (value & 1u) != 0u ? UINT8_MAX : 0u;
    } else if (tile->format == 3u && tile->size == 1u) {
      intensity = (uint8_t)(((value >> 4) & 15u) * 17u);
      alpha = (uint8_t)((value & 15u) * 17u);
    } else if (tile->format == 3u) {
      intensity = (uint8_t)(value >> 8);
      alpha = (uint8_t)value;
    } else if (tile->size == 0u) {
      intensity = (uint8_t)((value & 15u) * 17u);
    } else {
      intensity = (uint8_t)value;
    }
    *pixel_out = rgba5551(intensity, intensity, intensity, alpha);
    return true;
  }
  if (tile->format == 1u && tile->size == 2u) {
    uint32_t pair_x = x & ~UINT32_C(1);
    uint64_t pair_offset = tmem_base(tile) + row +
                           ((uint64_t)pair_x * 16u) / 8u;
    uint32_t y0, y1, u, v;
    uint8_t rgb[3];
    if ((y & 1u) != 0u) pair_offset ^= UINT64_C(4);
    y0 = tmem_read_bits(machine, pair_offset, 8u, 0u);
    u = tmem_read_bits(machine, pair_offset + 1u, 8u, 0u);
    y1 = tmem_read_bits(machine, pair_offset + 2u, 8u, 0u);
    v = tmem_read_bits(machine, pair_offset + 3u, 8u, 0u);
    t64_rdp_tmem_convert_yuv(&machine->rdp,
                             (uint8_t)((x & 1u) == 0u ? y0 : y1),
                             (uint8_t)u, (uint8_t)v, rgb);
    *pixel_out = rgba5551(rgb[0], rgb[1], rgb[2], UINT8_MAX);
    return true;
  }
  return false;
}

static uint8_t expand5_rgba8(uint32_t value) {
  return (uint8_t)((value << 3) | (value >> 2));
}

static uint32_t pack_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return ((uint32_t)r << 24) | ((uint32_t)g << 16) |
         ((uint32_t)b << 8) | (uint32_t)a;
}

static bool read_tile_texel_rgba8(const t64_machine *machine,
                                  const t64_rdp_tile *tile, uint32_t x,
                                  uint32_t y, uint32_t *rgba_out) {
  uint64_t width, height, row, offset;
  uint32_t bits;

  if (machine == NULL || tile == NULL || rgba_out == NULL ||
      tile->lrs < tile->uls || tile->lrt < tile->ult) return false;
  width = ((uint64_t)(tile->lrs - tile->uls) >> 2) + UINT64_C(1);
  height = ((uint64_t)(tile->lrt - tile->ult) >> 2) + UINT64_C(1);
  if ((uint64_t)x >= width || (uint64_t)y >= height) return false;
  bits = bits_per_texel(tile->size);
  row = (uint64_t)y * tile->line_words * 8u;
  offset = row + ((uint64_t)x * bits) / 8u;
  if ((y & 1u) != 0u) offset ^= UINT64_C(4);
  offset += tmem_base(tile);

  if (tile->format == 0u && tile->size == 2u) {
    uint16_t pixel = (uint16_t)tmem_read_bits(machine, offset, 16u, x);
    *rgba_out = pack_rgba8(
        expand5_rgba8((pixel >> 11) & UINT16_C(0x1f)),
        expand5_rgba8((pixel >> 6) & UINT16_C(0x1f)),
        expand5_rgba8((pixel >> 1) & UINT16_C(0x1f)),
        (pixel & UINT16_C(1)) != 0u ? UINT8_MAX : 0u);
    return true;
  }
  if (tile->format == 0u && tile->size == 3u) {
    *rgba_out = tmem_read_bits(machine, offset, 32u, x);
    return true;
  }
  if (tile->format == 2u) {
    uint32_t index = tmem_read_bits(machine, offset, bits, x);
    uint32_t palette_index = tile->size == 0u
                                 ? tile->palette * 16u + index
                                 : index;
    uint64_t tlut_address = UINT64_C(2048) +
                            (uint64_t)palette_index * UINT64_C(8);
    uint16_t palette_value;
    if (tlut_address + 2u > T64_RDP_TMEM_BYTES) return false;
    palette_value = (uint16_t)tmem_read_bits(machine, tlut_address, 16u, 0u);
    if (machine->rdp.tlut_format == 3u) {
      *rgba_out = pack_rgba8((uint8_t)(palette_value >> 8),
                             (uint8_t)(palette_value >> 8),
                             (uint8_t)(palette_value >> 8),
                             (uint8_t)palette_value);
    } else {
      *rgba_out = pack_rgba8(
          expand5_rgba8((palette_value >> 11) & UINT16_C(0x1f)),
          expand5_rgba8((palette_value >> 6) & UINT16_C(0x1f)),
          expand5_rgba8((palette_value >> 1) & UINT16_C(0x1f)),
          (palette_value & UINT16_C(1)) != 0u ? UINT8_MAX : 0u);
    }
    return true;
  }
  if (tile->format == 3u || tile->format == 4u) {
    uint32_t value = tmem_read_bits(machine, offset, bits, x);
    uint8_t intensity, alpha = UINT8_MAX;
    if (tile->format == 3u && tile->size == 0u) {
      intensity = (uint8_t)(((value >> 1) & 7u) * 255u / 7u);
      alpha = (value & 1u) != 0u ? UINT8_MAX : 0u;
    } else if (tile->format == 3u && tile->size == 1u) {
      intensity = (uint8_t)(((value >> 4) & 15u) * 17u);
      alpha = (uint8_t)((value & 15u) * 17u);
    } else if (tile->format == 3u) {
      intensity = (uint8_t)(value >> 8);
      alpha = (uint8_t)value;
    } else if (tile->size == 0u) {
      intensity = (uint8_t)((value & 15u) * 17u);
    } else {
      intensity = (uint8_t)value;
    }
    *rgba_out = pack_rgba8(intensity, intensity, intensity, alpha);
    return true;
  }
  if (tile->format == 1u && tile->size == 2u) {
    uint32_t pair_x = x & ~UINT32_C(1);
    uint64_t pair_offset = tmem_base(tile) + row +
                           ((uint64_t)pair_x * 16u) / 8u;
    uint32_t y0, y1, u, v;
    uint8_t rgb[3];
    if ((y & 1u) != 0u) pair_offset ^= UINT64_C(4);
    y0 = tmem_read_bits(machine, pair_offset, 8u, 0u);
    u = tmem_read_bits(machine, pair_offset + 1u, 8u, 0u);
    y1 = tmem_read_bits(machine, pair_offset + 2u, 8u, 0u);
    v = tmem_read_bits(machine, pair_offset + 3u, 8u, 0u);
    t64_rdp_tmem_convert_yuv(&machine->rdp,
                             (uint8_t)((x & 1u) == 0u ? y0 : y1),
                             (uint8_t)u, (uint8_t)v, rgb);
    *rgba_out = pack_rgba8(rgb[0], rgb[1], rgb[2], UINT8_MAX);
    return true;
  }
  return false;
}

static bool sample_texel(const t64_machine *machine, const t64_rdp_tile *tile,
                         int32_t s, int32_t t, uint16_t *pixel_out) {
  int32_t wrapped_x = wrap_coordinate(s, tile->uls, tile->lrs, tile->mask_s,
                                      tile->shift_s, tile->clamp_s,
                                      tile->mirror_s);
  int32_t wrapped_y = wrap_coordinate(t, tile->ult, tile->lrt, tile->mask_t,
                                      tile->shift_t, tile->clamp_t,
                                      tile->mirror_t);
  int32_t relative_x = wrapped_x - (int32_t)(tile->uls >> 2);
  int32_t relative_y = wrapped_y - (int32_t)(tile->ult >> 2);

  if (relative_x < 0 || relative_y < 0) return false;
  return read_tile_texel(machine, tile, (uint32_t)relative_x,
                         (uint32_t)relative_y, pixel_out);
}

bool t64_rdp_tmem_copy_pixel(const t64_machine *machine, uint32_t x,
                             uint32_t y, uint16_t *pixel_out) {
  const t64_rdp_state *rdp;
  const t64_rdp_tile *tile;
  int64_t dx, dy, s, t;

  if (machine == NULL || pixel_out == NULL ||
      machine->rdp.render_texture_tile >= T64_RDP_TILE_COUNT) return false;
  rdp = &machine->rdp;
  tile = &rdp->tiles[rdp->render_texture_tile];
  dx = (int64_t)x - (int64_t)rdp->render_x_begin;
  dy = (int64_t)y - (int64_t)rdp->render_y_begin;
  if (rdp->render_texture_flip) {
    s = (int64_t)rdp->render_texture_s +
        dy * floor_div_pow2(rdp->render_texture_dsdx, 7u);
    t = (int64_t)rdp->render_texture_t +
        dx * floor_div_pow2(rdp->render_texture_dtdy, 5u);
  } else {
    s = (int64_t)rdp->render_texture_s +
        dx * floor_div_pow2(rdp->render_texture_dsdx, 7u);
    t = (int64_t)rdp->render_texture_t +
        dy * floor_div_pow2(rdp->render_texture_dtdy, 5u);
  }
  if (s < INT32_MIN || s > INT32_MAX || t < INT32_MIN || t > INT32_MAX)
    return false;
  return sample_texel(machine, tile, (int32_t)s, (int32_t)t, pixel_out);
}

bool t64_rdp_tmem_validate_triangle(const t64_machine *machine,
                                    uint32_t tile_index) {
  const t64_rdp_state *rdp;
  const t64_rdp_tile *tile;
  uint64_t width, height, row_bytes, end;
  uint32_t bits;

  if (machine == NULL || !t64_has_rdp_texture_profile(machine) ||
      tile_index >= T64_RDP_TILE_COUNT) return false;
  rdp = &machine->rdp;
  tile = &rdp->tiles[tile_index];
  if (!tile->valid || !tile->size_valid ||
      !format_size_supported(tile->format, tile->size) ||
      tile->line_words == 0u || tile->lrs < tile->uls ||
      tile->lrt < tile->ult) return false;
  if (tile->format == 2u &&
      (!rdp->tlut_valid ||
       (rdp->tlut_format != 2u && rdp->tlut_format != 3u))) return false;

  bits = bits_per_texel(tile->size);
  width = ((uint64_t)(tile->lrs - tile->uls) >> 2) + UINT64_C(1);
  height = ((uint64_t)(tile->lrt - tile->ult) >> 2) + UINT64_C(1);
  row_bytes = (width * bits + UINT64_C(7)) / UINT64_C(8);
  end = tmem_base(tile) + height * (uint64_t)tile->line_words * UINT64_C(8);
  if (row_bytes > (uint64_t)tile->line_words * UINT64_C(8) ||
      end > T64_RDP_TMEM_BYTES) return false;

  /* An unclamped, unmasked axis has no bounded address transform. */
  if ((!tile->clamp_s && tile->mask_s == 0u) ||
      (!tile->clamp_t && tile->mask_t == 0u)) return false;
  if ((!tile->clamp_s &&
       (UINT64_C(1) << tile->mask_s) > width) ||
      (!tile->clamp_t &&
       (UINT64_C(1) << tile->mask_t) > height)) return false;
  return true;
}

static int64_t floor_div_i64(int64_t numerator, int64_t denominator) {
  int64_t quotient = numerator / denominator;
  int64_t remainder = numerator % denominator;
  if (remainder != 0 && numerator < 0) quotient--;
  return quotient;
}

static int64_t positive_mod_i64(int64_t value, int64_t modulus) {
  int64_t result = value % modulus;
  if (result < 0) result += modulus;
  return result;
}

static bool normalize_triangle_axis(int64_t fixed_q16, int64_t lower,
                                    int64_t upper, uint32_t shift,
                                    uint32_t mask, bool clamp, bool mirror,
                                    int64_t *relative_q16_out) {
  int64_t shifted;
  int64_t relative;
  int64_t maximum;

  if (relative_q16_out == NULL || upper < lower) return false;
  if (shift <= 10u) {
    shifted = floor_div_pow2(fixed_q16, shift);
  } else {
    int64_t scale = (int64_t)(UINT32_C(1) << (16u - shift));
    if (fixed_q16 > INT64_MAX / scale || fixed_q16 < INT64_MIN / scale)
      return false;
    shifted = fixed_q16 * scale;
  }
  relative = shifted - lower * INT64_C(65536);
  maximum = (upper - lower) * INT64_C(65536);
  if (clamp) {
    if (relative < 0) relative = 0;
    if (relative > maximum) relative = maximum;
  } else {
    int64_t period = (int64_t)(UINT32_C(1) << mask) * INT64_C(65536);
    int64_t span = period * (mirror ? INT64_C(2) : INT64_C(1));
    relative = positive_mod_i64(relative, span);
    if (mirror && relative >= period) relative = span - relative;
  }
  *relative_q16_out = relative;
  return true;
}

static bool map_triangle_index(int64_t index, uint32_t length,
                               uint32_t mask, bool clamp, bool mirror,
                               uint32_t *mapped_out) {
  int64_t mapped = index;
  if (mapped_out == NULL || length == 0u) return false;
  if (clamp) {
    if (mapped < 0) mapped = 0;
    if (mapped >= (int64_t)length) mapped = (int64_t)length - 1;
  } else {
    int64_t period = INT64_C(1) << mask;
    int64_t span = period * (mirror ? INT64_C(2) : INT64_C(1));
    mapped = positive_mod_i64(mapped, span);
    if (mirror && mapped >= period) mapped = span - mapped - 1;
  }
  if (mapped < 0 || mapped >= (int64_t)length) return false;
  *mapped_out = (uint32_t)mapped;
  return true;
}

static bool read_triangle_neighbor(const t64_machine *machine,
                                   const t64_rdp_tile *tile,
                                   int64_t x, int64_t y,
                                   uint16_t *pixel_out) {
  uint32_t width = ((tile->lrs - tile->uls) >> 2) + 1u;
  uint32_t height = ((tile->lrt - tile->ult) >> 2) + 1u;
  uint32_t mapped_x, mapped_y;
  if (!map_triangle_index(x, width, tile->mask_s, tile->clamp_s,
                          tile->mirror_s, &mapped_x) ||
      !map_triangle_index(y, height, tile->mask_t, tile->clamp_t,
                          tile->mirror_t, &mapped_y)) return false;
  return read_tile_texel(machine, tile, mapped_x, mapped_y, pixel_out);
}

static bool read_triangle_neighbor_rgba8(const t64_machine *machine,
                                         const t64_rdp_tile *tile,
                                         int64_t x, int64_t y,
                                         uint32_t *rgba_out) {
  uint32_t width = ((tile->lrs - tile->uls) >> 2) + 1u;
  uint32_t height = ((tile->lrt - tile->ult) >> 2) + 1u;
  uint32_t mapped_x, mapped_y;
  if (!map_triangle_index(x, width, tile->mask_s, tile->clamp_s,
                          tile->mirror_s, &mapped_x) ||
      !map_triangle_index(y, height, tile->mask_t, tile->clamp_t,
                          tile->mirror_t, &mapped_y)) return false;
  return read_tile_texel_rgba8(machine, tile, mapped_x, mapped_y, rgba_out);
}

static uint8_t rgba8_channel(uint32_t rgba, unsigned channel) {
  return (uint8_t)(rgba >> (24u - channel * 8u));
}

static uint32_t average_four_rgba8(const uint32_t texels[4]) {
  uint32_t output[4] = {0u, 0u, 0u, 0u};
  unsigned texel, channel;
  for (texel = 0u; texel < 4u; texel++)
    for (channel = 0u; channel < 4u; channel++)
      output[channel] += rgba8_channel(texels[texel], channel);
  for (channel = 0u; channel < 4u; channel++) output[channel] /= 4u;
  return pack_rgba8((uint8_t)output[0], (uint8_t)output[1],
                    (uint8_t)output[2], (uint8_t)output[3]);
}

static uint32_t three_point_rgba8(const uint32_t texels[4], uint32_t fs,
                                  uint32_t ft) {
  uint8_t output[4];
  unsigned channel;
  for (channel = 0u; channel < 4u; channel++) {
    uint32_t tl = rgba8_channel(texels[0], channel);
    uint32_t tr = rgba8_channel(texels[1], channel);
    uint32_t bl = rgba8_channel(texels[2], channel);
    uint32_t br = rgba8_channel(texels[3], channel);
    uint64_t value;
    if (fs >= ft) {
      value = (uint64_t)tl * (UINT32_C(65536) - fs) +
              (uint64_t)tr * (fs - ft) + (uint64_t)br * ft;
    } else {
      value = (uint64_t)tl * (UINT32_C(65536) - ft) +
              (uint64_t)bl * (ft - fs) + (uint64_t)br * fs;
    }
    output[channel] = (uint8_t)(value / UINT64_C(65536));
  }
  return pack_rgba8(output[0], output[1], output[2], output[3]);
}

static uint32_t channel5(uint16_t pixel, unsigned channel) {
  if (channel == 0u) return (pixel >> 11) & UINT16_C(0x1f);
  if (channel == 1u) return (pixel >> 6) & UINT16_C(0x1f);
  return (pixel >> 1) & UINT16_C(0x1f);
}

static uint16_t average_four_texels(const uint16_t texels[4]) {
  uint32_t channels[3] = {0u, 0u, 0u};
  unsigned texel, channel;
  uint32_t alpha = 0u;
  for (texel = 0u; texel < 4u; texel++) {
    for (channel = 0u; channel < 3u; channel++)
      channels[channel] += channel5(texels[texel], channel);
    alpha += texels[texel] & UINT16_C(1);
  }
  return (uint16_t)(((channels[0] / 4u) << 11) |
                    ((channels[1] / 4u) << 6) |
                    ((channels[2] / 4u) << 1) | (alpha / 4u));
}

static uint16_t three_point_texels(const uint16_t texels[4], uint32_t fs,
                                   uint32_t ft) {
  uint32_t channels[3] = {0u, 0u, 0u};
  uint32_t alpha = 0u;
  unsigned channel;

  for (channel = 0u; channel < 4u; channel++) {
    uint32_t tl, tr, bl, br;
    uint64_t value;
    if (channel == 3u) {
      tl = texels[0] & 1u;
      tr = texels[1] & 1u;
      bl = texels[2] & 1u;
      br = texels[3] & 1u;
    } else {
      tl = channel5(texels[0], channel);
      tr = channel5(texels[1], channel);
      bl = channel5(texels[2], channel);
      br = channel5(texels[3], channel);
    }
    if (fs >= ft) {
      value = (uint64_t)tl * (UINT32_C(65536) - fs) +
              (uint64_t)tr * (fs - ft) + (uint64_t)br * ft;
    } else {
      value = (uint64_t)tl * (UINT32_C(65536) - ft) +
              (uint64_t)bl * (ft - fs) + (uint64_t)br * fs;
    }
    if (channel == 3u) alpha = (uint32_t)value;
    else channels[channel] = (uint32_t)(value / UINT64_C(65536));
  }
  return (uint16_t)((channels[0] << 11) | (channels[1] << 6) |
                    (channels[2] << 1) |
                    (alpha / UINT32_C(65536)));
}

bool t64_rdp_tmem_triangle_pixel(const t64_machine *machine,
                                 uint32_t tile_index, int64_t s_q16,
                                 int64_t t_q16, int64_t w_q16,
                                 bool perspective, uint16_t *pixel_out) {
  const t64_rdp_tile *tile;
  int64_t s, t, s_relative, t_relative;
  int64_t s_index, t_index;
  uint32_t s_fraction, t_fraction;

  if (machine == NULL || pixel_out == NULL ||
      tile_index >= T64_RDP_TILE_COUNT ||
      !t64_rdp_tmem_validate_triangle(machine, tile_index) ||
      s_q16 < INT32_MIN || s_q16 > INT32_MAX ||
      t_q16 < INT32_MIN || t_q16 > INT32_MAX ||
      w_q16 < INT32_MIN || w_q16 > INT32_MAX) return false;
  s = s_q16;
  t = t_q16;
  if (perspective) {
    int64_t s_numerator, t_numerator;
    int64_t s_quotient, t_quotient;
    int64_t s_remainder, t_remainder;
    if (w_q16 == 0) return false;
    s_numerator = s * INT64_C(65536);
    t_numerator = t * INT64_C(65536);
    s_quotient = s_numerator / w_q16;
    t_quotient = t_numerator / w_q16;
    s_remainder = s_numerator % w_q16;
    t_remainder = t_numerator % w_q16;
    if (s_remainder != 0 && ((s_numerator < 0) != (w_q16 < 0)))
      s_quotient--;
    if (t_remainder != 0 && ((t_numerator < 0) != (w_q16 < 0)))
      t_quotient--;
    s = s_quotient;
    t = t_quotient;
  }
  if (s < INT32_MIN || s > INT32_MAX || t < INT32_MIN || t > INT32_MAX)
    return false;
  tile = &machine->rdp.tiles[tile_index];
  if (!normalize_triangle_axis(s, (int64_t)(tile->uls >> 2),
                               (int64_t)(tile->lrs >> 2), tile->shift_s,
                               tile->mask_s, tile->clamp_s, tile->mirror_s,
                               &s_relative) ||
      !normalize_triangle_axis(t, (int64_t)(tile->ult >> 2),
                               (int64_t)(tile->lrt >> 2), tile->shift_t,
                               tile->mask_t, tile->clamp_t, tile->mirror_t,
                               &t_relative)) return false;
  s_index = floor_div_i64(s_relative, INT64_C(65536));
  t_index = floor_div_i64(t_relative, INT64_C(65536));
  s_fraction = (uint32_t)(s_relative - s_index * INT64_C(65536));
  t_fraction = (uint32_t)(t_relative - t_index * INT64_C(65536));
  if (((machine->rdp.other_modes >> 45) & UINT64_C(1)) == 0u) {
    return read_triangle_neighbor(machine, tile, s_index, t_index, pixel_out);
  }
  {
    uint16_t texels[4];
    if (!read_triangle_neighbor(machine, tile, s_index, t_index, &texels[0]) ||
        !read_triangle_neighbor(machine, tile, s_index + 1, t_index, &texels[1]) ||
        !read_triangle_neighbor(machine, tile, s_index, t_index + 1, &texels[2]) ||
        !read_triangle_neighbor(machine, tile, s_index + 1, t_index + 1,
                                &texels[3])) return false;
    if (((machine->rdp.other_modes >> 44) & UINT64_C(1)) != 0u) {
      *pixel_out = average_four_texels(texels);
    } else {
      *pixel_out = three_point_texels(texels, s_fraction, t_fraction);
    }
  }
  return true;
}

bool t64_rdp_tmem_triangle_pixel_rgba8(const t64_machine *machine,
                                       uint32_t tile_index, int64_t s_q16,
                                       int64_t t_q16, int64_t w_q16,
                                       bool perspective,
                                       uint32_t *rgba_out) {
  const t64_rdp_tile *tile;
  int64_t s, t, s_relative, t_relative;
  int64_t s_index, t_index;
  uint32_t s_fraction, t_fraction;

  if (machine == NULL || rgba_out == NULL ||
      tile_index >= T64_RDP_TILE_COUNT ||
      !t64_has_rdp_combiner_profile(machine) ||
      !t64_rdp_tmem_validate_triangle(machine, tile_index) ||
      s_q16 < INT32_MIN || s_q16 > INT32_MAX ||
      t_q16 < INT32_MIN || t_q16 > INT32_MAX ||
      w_q16 < INT32_MIN || w_q16 > INT32_MAX) return false;
  s = s_q16;
  t = t_q16;
  if (perspective) {
    int64_t s_numerator, t_numerator;
    int64_t s_quotient, t_quotient;
    int64_t s_remainder, t_remainder;
    if (w_q16 == 0) return false;
    s_numerator = s * INT64_C(65536);
    t_numerator = t * INT64_C(65536);
    s_quotient = s_numerator / w_q16;
    t_quotient = t_numerator / w_q16;
    s_remainder = s_numerator % w_q16;
    t_remainder = t_numerator % w_q16;
    if (s_remainder != 0 && ((s_numerator < 0) != (w_q16 < 0)))
      s_quotient--;
    if (t_remainder != 0 && ((t_numerator < 0) != (w_q16 < 0)))
      t_quotient--;
    s = s_quotient;
    t = t_quotient;
  }
  if (s < INT32_MIN || s > INT32_MAX || t < INT32_MIN || t > INT32_MAX)
    return false;
  tile = &machine->rdp.tiles[tile_index];
  if (!normalize_triangle_axis(s, (int64_t)(tile->uls >> 2),
                               (int64_t)(tile->lrs >> 2), tile->shift_s,
                               tile->mask_s, tile->clamp_s, tile->mirror_s,
                               &s_relative) ||
      !normalize_triangle_axis(t, (int64_t)(tile->ult >> 2),
                               (int64_t)(tile->lrt >> 2), tile->shift_t,
                               tile->mask_t, tile->clamp_t, tile->mirror_t,
                               &t_relative)) return false;
  s_index = floor_div_i64(s_relative, INT64_C(65536));
  t_index = floor_div_i64(t_relative, INT64_C(65536));
  s_fraction = (uint32_t)(s_relative - s_index * INT64_C(65536));
  t_fraction = (uint32_t)(t_relative - t_index * INT64_C(65536));
  if (((machine->rdp.other_modes >> 45) & UINT64_C(1)) == 0u) {
    return read_triangle_neighbor_rgba8(machine, tile, s_index, t_index,
                                        rgba_out);
  }
  {
    uint32_t texels[4];
    if (!read_triangle_neighbor_rgba8(machine, tile, s_index, t_index,
                                      &texels[0]) ||
        !read_triangle_neighbor_rgba8(machine, tile, s_index + 1, t_index,
                                      &texels[1]) ||
        !read_triangle_neighbor_rgba8(machine, tile, s_index, t_index + 1,
                                      &texels[2]) ||
        !read_triangle_neighbor_rgba8(machine, tile, s_index + 1,
                                      t_index + 1, &texels[3])) return false;
    if (((machine->rdp.other_modes >> 44) & UINT64_C(1)) != 0u) {
      *rgba_out = average_four_rgba8(texels);
    } else {
      *rgba_out = three_point_rgba8(texels, s_fraction, t_fraction);
    }
  }
  return true;
}

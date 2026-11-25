/*
 * Save import and export flag vocabulary shared by the supervisor and the real
 * runner (Phase 9, plan 09-05). Header only, so the supervisor needs no core
 * link: the numeric kinds and formats below are asserted equal to the public
 * T64_SAVE_* and T64_SAVE_FORMAT_* constants in runner/t64_real.c.
 *
 * The tokens are closed lists. A format token carries its version suffix (the
 * D-35 per-format version string); there is no unversioned token, no default
 * and no autodetect. The byte-order selections are authored policy (assumption
 * A1), not a hardware fact. The runner is not installed and is not part of the
 * ABI or the freeze record; these names stay provisional.
 */
#ifndef T64_SAVE_FLAGS_H
#define T64_SAVE_FLAGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Kind numbers equal the public T64_SAVE_EEPROM_4K, _EEPROM_16K and
 * _SRAM_32K constants; sizes are the exact storage sizes (no padding). */
#define T64_SAVE_FLAGS_KIND_EEPROM_4K 1u
#define T64_SAVE_FLAGS_KIND_EEPROM_16K 2u
#define T64_SAVE_FLAGS_KIND_SRAM_32K 3u
#define T64_SAVE_FLAGS_FORMAT_RAW 1u
#define T64_SAVE_FLAGS_FORMAT_SWAP16 2u
#define T64_SAVE_FLAGS_FORMAT_WORDREV32 3u

/* Kind token to the numeric kind and the exact size in bytes. */
static inline bool t64_save_flags_kind(const char *token, uint32_t *kind_out,
                                       uint32_t *size_out) {
  if (token == NULL || kind_out == NULL || size_out == NULL) return false;
  if (strcmp(token, "eeprom4k") == 0) {
    *kind_out = T64_SAVE_FLAGS_KIND_EEPROM_4K;
    *size_out = 512u;
    return true;
  }
  if (strcmp(token, "eeprom16k") == 0) {
    *kind_out = T64_SAVE_FLAGS_KIND_EEPROM_16K;
    *size_out = 2048u;
    return true;
  }
  if (strcmp(token, "sram32k") == 0) {
    *kind_out = T64_SAVE_FLAGS_KIND_SRAM_32K;
    *size_out = 32768u;
    return true;
  }
  return false;
}

/* Format token (with its version suffix) to the numeric format. */
static inline bool t64_save_flags_format(const char *token,
                                         uint32_t *format_out) {
  if (token == NULL || format_out == NULL) return false;
  if (strcmp(token, "raw-v1") == 0) {
    *format_out = T64_SAVE_FLAGS_FORMAT_RAW;
    return true;
  }
  if (strcmp(token, "swap16-v1") == 0) {
    *format_out = T64_SAVE_FLAGS_FORMAT_SWAP16;
    return true;
  }
  if (strcmp(token, "wordrev32-v1") == 0) {
    *format_out = T64_SAVE_FLAGS_FORMAT_WORDREV32;
    return true;
  }
  return false;
}

/* The export leaf extension rule: exactly one of the lowercase extensions eep,
 * sra and sav, all of which are in the rights scanner's denied-extension list,
 * so an exported save is never mistaken for a publishable file. The leaf
 * grammar itself (stem, one dot, three lowercase letters) is validated by the
 * artifact publisher before any filesystem effect. */
static inline bool t64_save_flags_leaf_extension_ok(const char *leaf) {
  const char *dot;

  if (leaf == NULL) return false;
  dot = strrchr(leaf, '.');
  if (dot == NULL || dot == leaf) return false;
  dot++;
  return strcmp(dot, "eep") == 0 || strcmp(dot, "sra") == 0 ||
         strcmp(dot, "sav") == 0;
}

#endif /* T64_SAVE_FLAGS_H */

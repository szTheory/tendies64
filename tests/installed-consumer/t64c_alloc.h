/*
 * Counting host allocator of the installed consumer (Phase 11, plans 11-16 and
 * 11-18). Own code: it includes only the public header and system headers. The
 * ledger is the per-instance allocator context, so a release of an unknown
 * pointer, a double release and a leak are all visible, and an allocation can
 * be made to fail on purpose (fail_in) for the ALLOCATION_FAILED cases. The
 * storage is aligned by arithmetic over malloc, not by aligned_alloc (a C11
 * function): the consumer, like the public header, compiles as C99 (the plain cc
 * control of controls.tsv uses -std=c99).
 */
#ifndef T64C_ALLOC_H
#define T64C_ALLOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <tendies64/t64_machine.h>

#define T64C_MAX_SLOTS 1024u

typedef struct {
  void *data;  /* the aligned block the library sees */
  void *raw;   /* what malloc returned, for free */
  size_t size; /* the size the library asked for (the sequence harness scans it) */
  bool active;
} t64c_slot;

typedef struct {
  t64c_slot slots[T64C_MAX_SLOTS];
  size_t slot_count;
  size_t allocations;
  size_t releases;
  size_t bad_releases;
  size_t outstanding;
  size_t refusals;
  /* When non-zero, the fail_in-th next allocate call is refused (1 fails the
   * next call); the counter then returns to zero. */
  size_t fail_in;
  size_t calls;
} t64c_ledger;

static inline bool t64c_allocate(void *context, size_t size, size_t alignment,
                                 t64_host_storage *storage_out) {
  t64c_ledger *ledger = (t64c_ledger *)context;
  size_t host_alignment = alignment;
  size_t rounded;
  void *raw;
  void *memory;

  if (ledger == NULL || storage_out == NULL || size == 0u || alignment == 0u ||
      (alignment & (alignment - 1u)) != 0u) {
    return false;
  }
  ledger->calls++;
  if (ledger->fail_in != 0u && --ledger->fail_in == 0u) {
    ledger->refusals++;
    return false;
  }
  if (host_alignment < sizeof(void *)) {
    host_alignment = sizeof(void *);
  }
  if (ledger->slot_count >= T64C_MAX_SLOTS) {
    ledger->refusals++;
    return false;
  }
  rounded = ((size + host_alignment - 1u) / host_alignment) * host_alignment;
  raw = malloc(rounded + host_alignment);
  if (raw == NULL) {
    ledger->refusals++;
    return false;
  }
  memory = (void *)((((uintptr_t)raw + host_alignment - 1u) / host_alignment) *
                    host_alignment);
  ledger->slots[ledger->slot_count].data = memory;
  ledger->slots[ledger->slot_count].raw = raw;
  ledger->slots[ledger->slot_count].size = size;
  ledger->slots[ledger->slot_count].active = true;
  ledger->slot_count++;
  ledger->allocations++;
  ledger->outstanding++;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  return true;
}

static inline void t64c_release(void *context, t64_host_storage storage) {
  t64c_ledger *ledger = (t64c_ledger *)context;
  size_t i;

  if (ledger == NULL) {
    return;
  }
  for (i = 0u; i < ledger->slot_count; i++) {
    if (ledger->slots[i].active && ledger->slots[i].data == storage.data) {
      ledger->slots[i].active = false;
      ledger->releases++;
      ledger->outstanding--;
      free(ledger->slots[i].raw);
      return;
    }
  }
  ledger->bad_releases++;
}

#endif /* T64C_ALLOC_H */

/*
 * The consumer's own copy of the status classes of the public API (Phase 11,
 * plan 11-18; plan 11-20 compares it with docs/abi/status-classes.txt). One
 * row per T64_STATUS_* and T64_STOP_REASON_* constant, written by hand from
 * that table and never generated from the header or the library:
 *   X(NAME, CLASS, RESUME)
 * CLASS is T64C_CLASS_OK, _REFUSED, _GUEST_STOP, _DEFECT or _RESERVED; RESUME
 * is T64C_RESUME_NA, _RESUMABLE or _TERMINAL.
 */
#ifndef T64C_CLASSES_H
#define T64C_CLASSES_H

#include <stdint.h>

#include <tendies64/t64_machine.h>

typedef enum {
  T64C_CLASS_OK = 0,
  T64C_CLASS_REFUSED,
  T64C_CLASS_GUEST_STOP,
  T64C_CLASS_DEFECT,
  T64C_CLASS_RESERVED,
  T64C_CLASS_UNKNOWN
} t64c_class;

typedef enum {
  T64C_RESUME_NA = 0,
  T64C_RESUME_RESUMABLE,
  T64C_RESUME_TERMINAL
} t64c_resume;

#define T64C_STATUS_CLASS_TABLE(X)                                             \
  X(T64_STATUS_OK, T64C_CLASS_OK, T64C_RESUME_NA)                              \
  X(T64_STATUS_INVALID_ARGUMENT, T64C_CLASS_REFUSED, T64C_RESUME_NA)           \
  X(T64_STATUS_UNKNOWN_PROFILE, T64C_CLASS_REFUSED, T64C_RESUME_NA)            \
  X(T64_STATUS_ALLOCATION_FAILED, T64C_CLASS_REFUSED, T64C_RESUME_NA)          \
  X(T64_STATUS_BAD_STORAGE, T64C_CLASS_REFUSED, T64C_RESUME_NA)                \
  X(T64_STATUS_INVALID_ROM, T64C_CLASS_REFUSED, T64C_RESUME_NA)                \
  X(T64_STATUS_UNSUPPORTED_ROM_ORDER, T64C_CLASS_RESERVED, T64C_RESUME_NA)     \
  X(T64_STATUS_NOT_READY, T64C_CLASS_REFUSED, T64C_RESUME_NA)                  \
  X(T64_STATUS_TIME_REVERSED, T64C_CLASS_REFUSED, T64C_RESUME_NA)              \
  X(T64_STATUS_BUFFER_TOO_SMALL, T64C_CLASS_REFUSED, T64C_RESUME_NA)           \
  X(T64_STATUS_LIMIT_REVERSED, T64C_CLASS_REFUSED, T64C_RESUME_NA)             \
  X(T64_STATUS_QUEUE_FULL, T64C_CLASS_REFUSED, T64C_RESUME_NA)                 \
  X(T64_STATUS_SEQUENCE_EXHAUSTED, T64C_CLASS_REFUSED, T64C_RESUME_TERMINAL)   \
  X(T64_STATUS_TICK_OVERFLOW, T64C_CLASS_REFUSED, T64C_RESUME_NA)              \
  X(T64_STATUS_RESOURCE_EXHAUSTED, T64C_CLASS_REFUSED, T64C_RESUME_NA)         \
  X(T64_STATUS_BUS_UNMAPPED, T64C_CLASS_REFUSED, T64C_RESUME_NA)               \
  X(T64_STATUS_BUS_UNSUPPORTED, T64C_CLASS_REFUSED, T64C_RESUME_NA)            \
  X(T64_STATUS_BUS_ALIGNMENT, T64C_CLASS_REFUSED, T64C_RESUME_NA)              \
  X(T64_STATUS_BUS_RANGE, T64C_CLASS_REFUSED, T64C_RESUME_NA)                  \
  X(T64_STATUS_BUS_DEVICE_REFUSED, T64C_CLASS_REFUSED, T64C_RESUME_NA)         \
  X(T64_STATUS_SAVE_UNKNOWN_DEVICE, T64C_CLASS_REFUSED, T64C_RESUME_NA)        \
  X(T64_STATUS_SAVE_UNSUPPORTED_DEVICE, T64C_CLASS_REFUSED, T64C_RESUME_NA)    \
  X(T64_STATUS_SAVE_DEVICE_MISMATCH, T64C_CLASS_REFUSED, T64C_RESUME_NA)       \
  X(T64_STATUS_SAVE_BAD_FORMAT, T64C_CLASS_REFUSED, T64C_RESUME_NA)            \
  X(T64_STATUS_SAVE_SIZE_MISMATCH, T64C_CLASS_REFUSED, T64C_RESUME_NA)         \
  X(T64_STATUS_SAVE_BUSY, T64C_CLASS_REFUSED, T64C_RESUME_NA)                  \
  X(T64_STATUS_STATE_FORMAT, T64C_CLASS_REFUSED, T64C_RESUME_NA)               \
  X(T64_STATUS_STATE_INTEGRITY, T64C_CLASS_REFUSED, T64C_RESUME_NA)            \
  X(T64_STATUS_STATE_VERSION, T64C_CLASS_REFUSED, T64C_RESUME_NA)              \
  X(T64_STATUS_STATE_PROFILE, T64C_CLASS_REFUSED, T64C_RESUME_NA)              \
  X(T64_STATUS_STATE_CONTENT, T64C_CLASS_REFUSED, T64C_RESUME_NA)              \
  X(T64_STATUS_STATE_BUILD, T64C_CLASS_REFUSED, T64C_RESUME_NA)                \
  X(T64_STATUS_STATE_RANGE, T64C_CLASS_REFUSED, T64C_RESUME_NA)                \
  X(T64_STATUS_STATE_INVARIANT, T64C_CLASS_REFUSED, T64C_RESUME_NA)            \
  X(T64_STATUS_ABI_MISMATCH, T64C_CLASS_REFUSED, T64C_RESUME_NA)               \
  X(T64_STATUS_STRUCT_TOO_SMALL, T64C_CLASS_REFUSED, T64C_RESUME_NA)           \
  X(T64_STATUS_REENTRANT, T64C_CLASS_REFUSED, T64C_RESUME_NA)                  \
  X(T64_STATUS_POISONED, T64C_CLASS_DEFECT, T64C_RESUME_NA)                    \
  X(T64_STATUS_INTERNAL_DEFECT, T64C_CLASS_DEFECT, T64C_RESUME_NA)

#define T64C_STOP_CLASS_TABLE(X)                                               \
  X(T64_STOP_REASON_BOUND, T64C_CLASS_GUEST_STOP, T64C_RESUME_RESUMABLE)       \
  X(T64_STOP_REASON_INSTRUCTION_COUNT, T64C_CLASS_GUEST_STOP,                  \
    T64C_RESUME_RESUMABLE)                                                     \
  X(T64_STOP_REASON_FRAME_EVENT_COUNT, T64C_CLASS_GUEST_STOP,                  \
    T64C_RESUME_RESUMABLE)                                                     \
  X(T64_STOP_REASON_HALT, T64C_CLASS_RESERVED, T64C_RESUME_TERMINAL)           \
  X(T64_STOP_REASON_QUIESCENT, T64C_CLASS_GUEST_STOP, T64C_RESUME_RESUMABLE)   \
  X(T64_STOP_REASON_RESOURCE_LIMIT, T64C_CLASS_GUEST_STOP,                     \
    T64C_RESUME_RESUMABLE)                                                     \
  X(T64_STOP_REASON_CPU_UNSUPPORTED, T64C_CLASS_GUEST_STOP,                    \
    T64C_RESUME_TERMINAL)                                                      \
  X(T64_STOP_REASON_GUEST_COMPLETION, T64C_CLASS_GUEST_STOP,                   \
    T64C_RESUME_TERMINAL)

typedef struct {
  uint32_t value;
  t64c_class cls;
  t64c_resume resume;
} t64c_class_row;

#define T64C_CLASS_ROW(name, cls_value, resume_value)                          \
  {(uint32_t)(name), (cls_value), (resume_value)},

/* The class of a status value; T64C_CLASS_UNKNOWN for a value with no row. */
static inline t64c_class t64c_status_class(uint32_t status) {
  static const t64c_class_row rows[] = {T64C_STATUS_CLASS_TABLE(T64C_CLASS_ROW)};
  unsigned i;

  for (i = 0u; i < sizeof rows / sizeof rows[0]; i++) {
    if (rows[i].value == status) {
      return rows[i].cls;
    }
  }
  return T64C_CLASS_UNKNOWN;
}

/* The class of an advance stop reason; T64C_CLASS_UNKNOWN without a row. */
static inline t64c_class t64c_stop_class(uint32_t reason) {
  static const t64c_class_row rows[] = {T64C_STOP_CLASS_TABLE(T64C_CLASS_ROW)};
  unsigned i;

  for (i = 0u; i < sizeof rows / sizeof rows[0]; i++) {
    if (rows[i].value == reason) {
      return rows[i].cls;
    }
  }
  return T64C_CLASS_UNKNOWN;
}

/* The resume column of a stop reason. */
static inline t64c_resume t64c_stop_resume(uint32_t reason) {
  static const t64c_class_row rows[] = {T64C_STOP_CLASS_TABLE(T64C_CLASS_ROW)};
  unsigned i;

  for (i = 0u; i < sizeof rows / sizeof rows[0]; i++) {
    if (rows[i].value == reason) {
      return rows[i].resume;
    }
  }
  return T64C_RESUME_NA;
}

#endif /* T64C_CLASSES_H */

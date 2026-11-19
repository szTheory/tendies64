/* Bounded private pipe record for the authored CPU-to-AI tracer. */
#ifndef T64_AUDIO_OBSERVATION_H
#define T64_AUDIO_OBSERVATION_H

#include <stddef.h>
#include <stdint.h>

#include "tendies64/t64_machine.h"

#define T64_AUDIO_OBSERVATION_MAGIC UINT32_C(0x54363441)
#define T64_AUDIO_OBSERVATION_VERSION UINT32_C(2)
#define T64_AUDIO_OBSERVATION_PACKET_COUNT 2u
#define T64_AUDIO_OBSERVATION_PACKET_BYTES 16u
#define T64_AUDIO_OBSERVATION_MAX_BUS_WRITES 16u
#define T64_AUDIO_OBSERVATION_MAX_SW_RETIREMENTS 16u
#define T64_AUDIO_OBSERVATION_MAX_YIELDS 65u

typedef struct {
  uint64_t tick;
  uint32_t address;
  uint32_t value;
  uint32_t kind;
} t64_audio_bus_write;

typedef struct {
  uint64_t tick;
  uint32_t pc;
  uint32_t opcode;
} t64_audio_sw_retirement;

typedef struct {
  t64_audio_packet_info info;
  uint8_t bytes[T64_AUDIO_OBSERVATION_PACKET_BYTES];
} t64_audio_packet_observation;

typedef struct {
  uint64_t target_tick;
  uint64_t before_tick;
  uint64_t reached_tick;
  uint32_t video_before;
  uint32_t video_after;
  uint32_t audio_before;
  uint32_t audio_after;
} t64_audio_yield_observation;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t packet_count;
  uint32_t bus_write_count;
  uint32_t sw_retirement_count;
  uint32_t trace_overflow;
  uint32_t mi_ai_read_seen;
  uint32_t guest_mi_snapshot_seen;
  uint32_t guest_mi_snapshot;
  uint64_t trace_records;
  uint64_t mi_ai_read_tick;
  uint64_t guest_last_cpu_tick;
  uint64_t guest_stop_tick;
  uint64_t guest_first_escape_tick;
  uint64_t guest_first_exception_tick;
  uint64_t guest_first_exception_epc;
  uint64_t guest_first_exception_preceding_tick;
  uint64_t guest_first_exception_state;
  uint64_t guest_first_exception_mi;
  uint32_t guest_last_cpu_pc;
  uint32_t guest_first_escape_pc;
  uint32_t guest_first_exception_preceding_pc;
  uint32_t guest_first_exception_seen;
  uint32_t guest_stop_reason;
  uint32_t yield_count;
  t64_audio_packet_observation packets[T64_AUDIO_OBSERVATION_PACKET_COUNT];
  t64_audio_bus_write bus_writes[T64_AUDIO_OBSERVATION_MAX_BUS_WRITES];
  t64_audio_sw_retirement
      sw_retirements[T64_AUDIO_OBSERVATION_MAX_SW_RETIREMENTS];
  t64_audio_yield_observation
      yields[T64_AUDIO_OBSERVATION_MAX_YIELDS];
} t64_audio_observation;

/* The retained record is a raw native-layout copy that scripts/qualify-audio.py
 * maps with ctypes. Pin the 64-bit version-2 layout so a field edit cannot
 * silently change the retained 3,976-byte record or its region offsets. */
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
_Static_assert(sizeof(t64_audio_packet_info) == 280u,
               "audio packet info layout is part of observation version 2");
_Static_assert(sizeof(t64_audio_observation) == 3976u,
               "audio observation version 2 record size changed");
_Static_assert(offsetof(t64_audio_observation, packets) == 144u,
               "audio observation packets offset changed");
_Static_assert(offsetof(t64_audio_observation, bus_writes) == 736u,
               "audio observation bus-write offset changed");
_Static_assert(offsetof(t64_audio_observation, sw_retirements) == 1120u,
               "audio observation retirement offset changed");
_Static_assert(offsetof(t64_audio_observation, yields) == 1376u,
               "audio observation yield offset changed");
#endif

#endif

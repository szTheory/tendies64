#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define RSP_SMOKE_PROFILE "ntsc-cart-4m-openipl3-rsp-functional-v1"
#define RSP_SMOKE_TICK_LIMIT UINT64_C(10000000)
#define RSP_SMOKE_CHILD_LIMIT_MS UINT64_C(10000)
#define RSP_SMOKE_CAPTURE_LIMIT 4096u
#define RSP_SMOKE_ROM_LIMIT UINT64_C(0x00401000)
#define RSP_SMOKE_BREAK_PC UINT32_C(0x9c)
#define RSP_SMOKE_RSP_BRANCH_PC UINT32_C(0x10)
#define RSP_SMOKE_RSP_DELAY_PC UINT32_C(0x14)
#define RSP_SMOKE_RSP_PAIR_PC UINT32_C(0x18)
#define RSP_SMOKE_RSP_PAIR_SECOND_PC UINT32_C(0x1c)
#define RSP_SMOKE_GUEST_FAIL_PC UINT32_C(0x800005c8)
#define RSP_SMOKE_GUEST_PASS_PC UINT32_C(0x80000610)
#define RSP_SMOKE_TERMINAL_PC UINT32_C(0x800006c0)
#define RSP_SMOKE_TERMINAL_OPCODE UINT32_C(0x1000ffff)
#define RSP_SMOKE_DELAY_PC UINT32_C(0x800006c4)
#define RSP_SMOKE_DELAY_OPCODE UINT32_C(0x00000000)
#define RSP_SMOKE_BOOT_STEP_MAX UINT64_C(16)
#define RSP_SMOKE_PAYLOAD_START UINT32_C(0x80000400)
#define RSP_SMOKE_PAYLOAD_TEXT_END UINT32_C(0x800006c8)

typedef struct {
  uint8_t *bytes;
  size_t length;
  char sha256[T64_SHA256_HEX_SIZE];
} rom_input;

typedef struct {
  uint64_t cpu_retired;
  uint64_t rsp_retired;
  uint32_t before_previous_cpu_pc;
  uint32_t before_previous_cpu_opcode;
  uint32_t previous_cpu_pc;
  uint32_t previous_cpu_opcode;
  uint32_t guest_fail_from_pc;
  uint32_t guest_fail_from_opcode;
  bool have_previous_cpu;
  bool cpu_entered_payload;
  bool terminal_branch_retired;
  bool terminal_delay_retired;
  bool terminal_delay_mismatch;
  uint32_t terminal_delay_opcode;
  bool missing_break_seen;
  bool rsp_taken_branch_retired;
  bool rsp_branch_delay_retired;
  bool rsp_pair_retired;
  bool rsp_load_use_stall;
  bool rsp_break_retired;
  bool cpu_unsupported;
  bool cpu_pending_exception_seen;
  bool saw_guest_check;
  bool saw_guest_fail;
  bool saw_guest_pass;
  bool saw_terminal_branch;
  uint32_t unsupported_cpu_pc;
  uint32_t unsupported_cpu_opcode;
  uint32_t first_exception_code;
  uint64_t first_exception_epc;
  uint64_t first_exception_tick;
  uint32_t last_rsp_pc;
  uint32_t last_rsp_opcode;
} observations;

static bool allocate_machine(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *data = NULL;
  size_t actual_alignment = alignment < sizeof(void *) ? sizeof(void *) : alignment;
  (void)context;
  if (storage_out == NULL || posix_memalign(&data, actual_alignment, size) != 0) {
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = actual_alignment;
  return true;
}

static void release_machine(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void digest_bytes(const void *bytes, size_t length,
                         uint8_t digest[T64_SHA256_DIGEST_SIZE]) {
  t64_sha256_ctx hash;
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, bytes, length);
  t64_sha256_final(&hash, digest);
}

static bool valid_sha256(const char *value) {
  size_t index;
  if (value == NULL || strlen(value) != 64u) return false;
  for (index = 0u; index < 64u; index++) {
    if (!((value[index] >= '0' && value[index] <= '9') ||
          (value[index] >= 'a' && value[index] <= 'f'))) return false;
  }
  return true;
}

static const char *read_rom(const char *path, rom_input *rom) {
  struct stat metadata;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  size_t offset = 0u;
  int fd;
  if (path == NULL || rom == NULL) return "path";
  fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) return "open";
  if (fstat(fd, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
      metadata.st_size < 4096 ||
      (uint64_t)metadata.st_size > RSP_SMOKE_ROM_LIMIT) {
    close(fd);
    return "metadata";
  }
  rom->length = (size_t)metadata.st_size;
  rom->bytes = (uint8_t *)malloc(rom->length);
  if (rom->bytes == NULL) {
    close(fd);
    return "allocation";
  }
  while (offset < rom->length) {
    ssize_t count = read(fd, rom->bytes + offset, rom->length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      close(fd);
      free(rom->bytes);
      rom->bytes = NULL;
      return "read";
    }
    offset += (size_t)count;
  }
  if (close(fd) != 0) {
    free(rom->bytes);
    rom->bytes = NULL;
    return "close";
  }
  digest_bytes(rom->bytes, rom->length, digest);
  t64_sha256_hex(digest, rom->sha256);
  return NULL;
}

static bool observe_cpu(observations *seen, const t64_trace_record *record) {
  uint32_t pc;
  uint32_t opcode;
  if (record->device != T64_TRACE_DEVICE_MACHINE) return true;
  if (record->kind == T64_TRACE_KIND_CPU_UNSUPPORTED) {
    seen->cpu_unsupported = true;
    seen->unsupported_cpu_pc = (uint32_t)(record->value >> 32);
    seen->unsupported_cpu_opcode = (uint32_t)record->value;
    return true;
  }
  if (record->kind != T64_TRACE_KIND_CPU_RETIREMENT) return true;
  pc = (uint32_t)(record->value >> 32);
  opcode = (uint32_t)record->value;
  seen->cpu_retired++;
  if (pc >= RSP_SMOKE_PAYLOAD_START &&
      pc < RSP_SMOKE_PAYLOAD_TEXT_END) {
    seen->cpu_entered_payload = true;
  }
  if (pc >= UINT32_C(0x800004cc) && pc < RSP_SMOKE_GUEST_FAIL_PC) {
    seen->saw_guest_check = true;
  }
  if (pc == RSP_SMOKE_GUEST_FAIL_PC) seen->saw_guest_fail = true;
  if (pc == RSP_SMOKE_GUEST_PASS_PC) seen->saw_guest_pass = true;
  if (pc == RSP_SMOKE_TERMINAL_PC) seen->saw_terminal_branch = true;
  if (seen->have_previous_cpu &&
      seen->previous_cpu_pc == RSP_SMOKE_TERMINAL_PC &&
      seen->previous_cpu_opcode == RSP_SMOKE_TERMINAL_OPCODE &&
      pc == RSP_SMOKE_DELAY_PC && opcode == RSP_SMOKE_DELAY_OPCODE) {
    seen->terminal_branch_retired = true;
    seen->terminal_delay_retired = true;
  } else if (seen->have_previous_cpu &&
             seen->previous_cpu_pc == RSP_SMOKE_TERMINAL_PC &&
             seen->previous_cpu_opcode == RSP_SMOKE_TERMINAL_OPCODE &&
             pc == RSP_SMOKE_DELAY_PC) {
    seen->terminal_delay_mismatch = true;
    seen->terminal_delay_opcode = opcode;
  }
  if (pc == RSP_SMOKE_GUEST_FAIL_PC) {
    seen->guest_fail_from_pc = seen->before_previous_cpu_pc;
    seen->guest_fail_from_opcode = seen->before_previous_cpu_opcode;
  }
  seen->before_previous_cpu_pc = seen->previous_cpu_pc;
  seen->before_previous_cpu_opcode = seen->previous_cpu_opcode;
  seen->previous_cpu_pc = pc;
  seen->previous_cpu_opcode = opcode;
  seen->have_previous_cpu = true;
  return true;
}

static bool drain_trace(t64_machine *machine, observations *seen) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t index;
  if (t64_machine_trace_drain(machine, records,
                              sizeof records / sizeof records[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) return false;
  for (index = 0u; index < count; index++) {
    if (!observe_cpu(seen, &records[index])) return false;
  }
  return true;
}

static void observe_rsp(t64_machine *machine, observations *seen) {
  if (!machine->rsp_observation_valid ||
      machine->rsp_observed_tick != machine->tick) return;
  if (machine->rsp_observed_outcome == T64_RSP_ATTEMPT_NONE &&
      machine->rsp_observed_pc == UINT32_C(0x44) &&
      machine->rsp_observed_stall_reason == T64_RSP_STALL_SCALAR_READY) {
    seen->rsp_load_use_stall = true;
  }
  if (machine->rsp_observed_outcome == T64_RSP_ATTEMPT_RETIRED) {
    seen->rsp_retired++;
    seen->last_rsp_pc = machine->rsp_observed_pc;
    seen->last_rsp_opcode = machine->rsp_observed_opcode;
    if (machine->rsp_observed_pc == UINT32_C(16) &&
        machine->rsp_observed_opcode == UINT32_C(0x10000001)) {
      seen->rsp_taken_branch_retired = true;
    }
    if (machine->rsp_observed_pc == UINT32_C(24) &&
        machine->rsp_observed_opcode == UINT32_C(0x24040007) &&
        machine->rsp_observed_second_pc == UINT32_C(28) &&
        machine->rsp_observed_second_opcode == UINT32_C(0x4a0208d0)) {
      seen->rsp_pair_retired = true;
    }
    if (machine->rsp_observed_pc == RSP_SMOKE_BREAK_PC &&
        machine->rsp_observed_opcode == UINT32_C(0x0000000d)) {
      seen->rsp_break_retired = true;
    } else if (machine->rsp_observed_pc == RSP_SMOKE_BREAK_PC) {
      seen->missing_break_seen = true;
    }
    if (machine->rsp_observed_pc == UINT32_C(20) &&
        machine->rsp_observed_opcode == UINT32_C(0x24030009)) {
      seen->rsp_branch_delay_retired = true;
    }
    if (machine->rsp_observed_second_pc != 0u) seen->rsp_retired++;
  }
}

static bool drain_output(t64_machine *machine, uint8_t *capture,
                         size_t *length, uint64_t *attempted_total) {
  t64_output_drain_result output = {0};
  size_t available = RSP_SMOKE_CAPTURE_LIMIT - *length;
  if (t64_machine_output_drain(machine, capture + *length, available,
                               &output) != T64_STATUS_OK ||
      output.lost_output || !output.total_attempted_bytes_exact ||
      output.copied_bytes > available ||
      output.total_attempted_bytes > RSP_SMOKE_CAPTURE_LIMIT) return false;
  *length += output.copied_bytes;
  *attempted_total = output.total_attempted_bytes;
  return *attempted_total == *length;
}

static void print_guest_failure_detail(const t64_machine *machine,
                                      const observations *seen) {
  size_t index;
  (void)printf("guest_compare_actual=%02x guest_compare_expected=%02x dmem_80_9f=",
               (unsigned int)(uint8_t)machine->cpu.gpr[12],
               (unsigned int)(uint8_t)machine->cpu.gpr[13]);
  for (index = 0u; index < 32u; index++) {
    (void)printf("%02x", machine->sp_dmem[UINT32_C(0x80) + index]);
  }
  (void)printf(" flag_word=%08x load_word=%08x rsp_stalls=%llu\n",
               (unsigned int)((uint32_t)machine->sp_dmem[0x20] << 24 |
                              (uint32_t)machine->sp_dmem[0x21] << 16 |
                              (uint32_t)machine->sp_dmem[0x22] << 8 |
                              (uint32_t)machine->sp_dmem[0x23]),
               (unsigned int)((uint32_t)machine->sp_dmem[0x24] << 24 |
                              (uint32_t)machine->sp_dmem[0x25] << 16 |
                              (uint32_t)machine->sp_dmem[0x26] << 8 |
                              (uint32_t)machine->sp_dmem[0x27]),
               (unsigned long long)machine->rsp.stall_slots);
  (void)seen;
}

static uint64_t monotonic_ms(void) {
  struct timespec value;
  if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0u;
  return (uint64_t)value.tv_sec * UINT64_C(1000) +
         (uint64_t)value.tv_nsec / UINT64_C(1000000);
}

static int run_smoke(const char *path, const char *expected_sha,
                     uint64_t boot_step_max) {
  static const t64_host_allocator allocator = {
      allocate_machine, release_machine, NULL};
  static const uint8_t expected_output[] = "T64-RSP-SMOKE:PASS\n";
  static const uint8_t expected_guest_failure[] = "FAIL\n";
  rom_input rom = {0};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  observations seen = {0};
  uint8_t output[T64_SHA256_DIGEST_SIZE];
  uint8_t capture[RSP_SMOKE_CAPTURE_LIMIT] = {0};
  size_t capture_length = 0u;
  uint64_t attempted_total = 0u;
  uint64_t current_tick = 0u;
  uint64_t start_ms = monotonic_ms();
  int result = 4;

  const char *rom_error = NULL;
  if (!valid_sha256(expected_sha)) {
    size_t index;
    (void)printf("classification=harness-error reason=invalid-rom-digest-argument length=%zu\n",
                 strlen(expected_sha));
    for (index = 0u; index < strlen(expected_sha) && index < 80u; index++) {
      if (!((expected_sha[index] >= '0' && expected_sha[index] <= '9') ||
            (expected_sha[index] >= 'a' && expected_sha[index] <= 'f'))) {
        (void)printf("invalid_character_index=%zu value=%u\n", index,
                     (unsigned int)(unsigned char)expected_sha[index]);
        break;
      }
    }
    return 4;
  }
  rom_error = read_rom(path, &rom);
  if (rom_error != NULL) {
    (void)printf("classification=harness-error reason=rom-%s\n", rom_error);
    return 4;
  }
  if (strcmp(rom.sha256, expected_sha) != 0) {
    (void)puts("classification=harness-error reason=rom-digest");
    goto cleanup;
  }
  if (t64_profile_lookup(RSP_SMOKE_PROFILE, &profile) != T64_STATUS_OK ||
      profile == NULL || profile->machine_model_version != 6u ||
      profile->state_serialization_version != 12u ||
      strcmp(profile->sp_model_version,
             "rsp-functional-issue-dma-divider-integer-v1") != 0 ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK ||
      t64_machine_admit_rom(machine, rom.bytes, rom.length) != T64_STATUS_OK) {
    (void)puts("classification=harness-error reason=profile-or-admission");
    goto cleanup;
  }
  if (t64_machine_content_digest(machine, output) != T64_STATUS_OK) {
    (void)puts("classification=harness-error reason=content-identity");
    goto cleanup;
  }
  {
    char admitted_sha[T64_SHA256_HEX_SIZE];
    t64_sha256_hex(output, admitted_sha);
    if (strcmp(admitted_sha, rom.sha256) != 0) {
      (void)puts("classification=harness-error reason=admitted-rom-digest");
      goto cleanup;
    }
  }

  while (current_tick < RSP_SMOKE_TICK_LIMIT) {
    t64_stop_result stop = {0u, 0u};
    uint64_t delta = seen.cpu_entered_payload ? UINT64_C(1) : boot_step_max;
    uint64_t remaining = RSP_SMOKE_TICK_LIMIT - current_tick;
    uint64_t target = current_tick + (delta < remaining ? delta : remaining);
    uint64_t now_ms = monotonic_ms();
    if (start_ms == 0u || now_ms < start_ms ||
        now_ms - start_ms >= RSP_SMOKE_CHILD_LIMIT_MS) {
      (void)puts("classification=timeout reason=child-wall-deadline");
      result = 3;
      goto cleanup;
    }
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
    if (!seen.cpu_pending_exception_seen &&
        machine->cpu.pending_exception_valid) {
      seen.cpu_pending_exception_seen = true;
      seen.first_exception_code = machine->cpu.pending_exception_code;
      seen.first_exception_epc = machine->cpu.pending_exception_epc;
      seen.first_exception_tick = stop.tick;
    }
    if (status != T64_STATUS_OK) {
      (void)printf("classification=harness-error reason=advance-status status=%u\n",
                   (unsigned int)status);
      goto cleanup;
    }
    if (!drain_trace(machine, &seen)) {
      (void)puts("classification=harness-error reason=trace-drain");
      goto cleanup;
    }
    if (stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED &&
        seen.cpu_unsupported) {
      t64_cpu_stop_detail detail = {0u, 0u, 0u};
      uint64_t store_base = machine->cpu.gpr[20];
      uint64_t store_address = store_base + (uint64_t)(int64_t)(int16_t)
          (seen.unsupported_cpu_opcode & UINT32_C(0xffff));
      if (t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_OK ||
          detail.pc != seen.unsupported_cpu_pc ||
          detail.opcode != seen.unsupported_cpu_opcode ||
          detail.retired_instructions != seen.cpu_retired) {
        (void)puts("classification=harness-error reason=cpu-stop-observation-mismatch");
        goto cleanup;
      }
      (void)printf("classification=guest-unsupported cpu_pc=%08x cpu_opcode=%08x cpu_retired=%llu store_base=%016llx store_address=%016llx store_value=%08x rsp_valid=%u rsp_outcome=%u rsp_pc=%03x rsp_opcode=%08x rsp_reason=%u rsp_tick=%llu tick=%llu\n",
                   detail.pc, detail.opcode,
                   (unsigned long long)detail.retired_instructions,
                   (unsigned long long)store_base,
                   (unsigned long long)store_address,
                   (unsigned int)(uint32_t)machine->cpu.gpr[8],
                   machine->rsp_observation_valid ? 1u : 0u,
                   machine->rsp_observed_outcome,
                   machine->rsp_observed_pc,
                   machine->rsp_observed_opcode,
                   machine->rsp_observed_refusal_reason,
                   (unsigned long long)machine->rsp_observed_tick,
                   (unsigned long long)stop.tick);
      result = 2;
      goto cleanup;
    }
    if (stop.tick != target) {
      (void)printf("classification=harness-error reason=advance-stop reason=%u tick=%llu target=%llu\n",
                   stop.reason, (unsigned long long)stop.tick,
                   (unsigned long long)target);
      goto cleanup;
    }
    observe_rsp(machine, &seen);
    if (seen.missing_break_seen) {
      (void)printf("classification=missing-break rsp_pc=%03x opcode=%08x retired=%llu tick=%llu\n",
                   machine->rsp_observed_pc,
                   machine->rsp_observed_opcode,
                   (unsigned long long)seen.rsp_retired,
                   (unsigned long long)stop.tick);
      result = 2;
      goto cleanup;
    }
    if (!drain_output(machine, capture, &capture_length, &attempted_total)) {
      (void)puts("classification=harness-error reason=capture-limit");
      goto cleanup;
    }
    current_tick = stop.tick;
    if (seen.terminal_delay_mismatch) {
      (void)printf("classification=completion-delay-mismatch branch_pc=%08x branch_opcode=%08x delay_pc=%08x delay_opcode=%08x tick=%llu\n",
                   RSP_SMOKE_TERMINAL_PC, RSP_SMOKE_TERMINAL_OPCODE,
                   RSP_SMOKE_DELAY_PC, seen.terminal_delay_opcode,
                   (unsigned long long)current_tick);
      result = 2;
      goto cleanup;
    }
    if (seen.terminal_delay_retired) break;
    if (machine->rsp_observation_valid &&
        machine->rsp_observed_outcome == T64_RSP_ATTEMPT_REFUSED) {
      (void)printf("classification=refused rsp_pc=%03x opcode=%08x reason=%u tick=%llu\n",
                   machine->rsp_observed_pc, machine->rsp_observed_opcode,
                   machine->rsp_observed_refusal_reason,
                   (unsigned long long)current_tick);
      result = 2;
      goto cleanup;
    }
  }
  if (current_tick >= RSP_SMOKE_TICK_LIMIT && !seen.terminal_delay_retired) {
    (void)printf("classification=incomplete reason=guest-tick-bound cpu_pc=%08x cpu_pc_full=%016llx cpu_last_pc=%08x cpu_last_opcode=%08x cpu_retired=%llu guest_check_seen=%u guest_fail_seen=%u guest_pass_seen=%u terminal_branch_seen=%u cpu_terminal_seen=%u cpu_delay_seen=%u first_exception=%u first_exception_epc=%016llx first_exception_tick=%llu cpu_exception=%u cpu_epc=%016llx cpu_status=%08x capture=%zu rsp_pc=%03x rsp_status=%08x rsp_retired=%llu rsp_stalls=%llu rsp_last_pc=%03x rsp_last_opcode=%08x rsp_outcome=%u rsp_reason=%u rsp_observation_tick=%llu next_issue_tick=%llu dma_active=%u dma_pending=%u tick=%llu\n",
                 machine->cpu.pc,
                 (unsigned long long)machine->cpu.pc_full,
                 seen.previous_cpu_pc, seen.previous_cpu_opcode,
                 (unsigned long long)seen.cpu_retired,
                 seen.saw_guest_check ? 1u : 0u,
                 seen.saw_guest_fail ? 1u : 0u,
                 seen.saw_guest_pass ? 1u : 0u,
                 seen.saw_terminal_branch ? 1u : 0u,
                 seen.terminal_branch_retired ? 1u : 0u,
                 seen.terminal_delay_retired ? 1u : 0u,
                 seen.first_exception_code,
                 (unsigned long long)seen.first_exception_epc,
                 (unsigned long long)seen.first_exception_tick,
                 machine->cpu.pending_exception_valid ?
                     machine->cpu.pending_exception_code : UINT32_C(0),
                 (unsigned long long)machine->cpu.pending_exception_epc,
                 (unsigned int)(uint32_t)machine->cpu.cp0[12],
                 capture_length,
                 machine->rsp.pc, machine->rsp.sp_status,
                 (unsigned long long)machine->rsp.retired_instructions,
                 (unsigned long long)machine->rsp.stall_slots,
                 machine->rsp_observed_pc, machine->rsp_observed_opcode,
                 machine->rsp_observed_outcome,
                 machine->rsp_observed_refusal_reason,
                 (unsigned long long)machine->rsp_observed_tick,
                 (unsigned long long)machine->rsp.next_issue_tick,
                 machine->sp.active.valid ? 1u : 0u,
                 machine->sp.pending.valid ? 1u : 0u,
                 (unsigned long long)current_tick);
    result = 3;
    goto cleanup;
  }
  if (capture_length != sizeof expected_output - 1u ||
      memcmp(capture, expected_output, sizeof expected_output - 1u) != 0) {
    if (capture_length == sizeof expected_guest_failure - 1u &&
        memcmp(capture, expected_guest_failure,
               sizeof expected_guest_failure - 1u) == 0 &&
        seen.saw_guest_fail) {
    (void)printf("classification=guest-fail failed_check_pc=%08x failed_check_opcode=%08x rsp_pc=%03x rsp_opcode=%08x rsp_retired=%llu cpu_retired=%llu capture=%zu tick=%llu\n",
                 seen.guest_fail_from_pc, seen.guest_fail_from_opcode,
                 seen.last_rsp_pc, seen.last_rsp_opcode,
                 (unsigned long long)seen.rsp_retired,
                 (unsigned long long)seen.cpu_retired, capture_length,
                 (unsigned long long)current_tick);
    print_guest_failure_detail(machine, &seen);
    } else {
      (void)printf("classification=unexpected-transcript capture=%zu cpu_guest_fail_seen=%u rsp_pc=%03x rsp_opcode=%08x tick=%llu\n",
                   capture_length, seen.saw_guest_fail ? 1u : 0u,
                   seen.last_rsp_pc, seen.last_rsp_opcode,
                   (unsigned long long)current_tick);
    }
    result = 2;
    goto cleanup;
  }
  if (!seen.rsp_taken_branch_retired || !seen.rsp_branch_delay_retired ||
      !seen.rsp_pair_retired || !seen.rsp_load_use_stall ||
      !seen.rsp_break_retired ||
      seen.rsp_retired < UINT64_C(40) || machine->rsp.stall_slots == 0u ||
      machine->rsp.pc != RSP_SMOKE_BREAK_PC ||
      (machine->rsp.sp_status & UINT32_C(3)) != UINT32_C(3)) {
    (void)printf("classification=incomplete reason=retirement-evidence rsp_branch=%u rsp_delay=%u rsp_pair=%u rsp_load_use_stall=%u rsp_break=%u rsp_retired=%llu rsp_stalls=%llu rsp_pc=%03x status=%08x cpu_retired=%llu tick=%llu\n",
                 seen.rsp_taken_branch_retired ? 1u : 0u,
                 seen.rsp_branch_delay_retired ? 1u : 0u,
                 seen.rsp_pair_retired ? 1u : 0u,
                 seen.rsp_load_use_stall ? 1u : 0u,
                 seen.rsp_break_retired ? 1u : 0u,
                 (unsigned long long)seen.rsp_retired,
                 (unsigned long long)machine->rsp.stall_slots,
                 machine->rsp.pc, machine->rsp.sp_status,
                 (unsigned long long)seen.cpu_retired,
                 (unsigned long long)current_tick);
    result = 2;
    goto cleanup;
  }
  {
    size_t index;
    uint32_t flag_word = (uint32_t)machine->sp_dmem[0x20] << 24 |
                         (uint32_t)machine->sp_dmem[0x21] << 16 |
                         (uint32_t)machine->sp_dmem[0x22] << 8 |
                         (uint32_t)machine->sp_dmem[0x23];
    uint32_t load_use_word = (uint32_t)machine->sp_dmem[0x24] << 24 |
                             (uint32_t)machine->sp_dmem[0x25] << 16 |
                             (uint32_t)machine->sp_dmem[0x26] << 8 |
                             (uint32_t)machine->sp_dmem[0x27];
    (void)printf("classification=pass rom_sha256=%s profile=%s model=6 state=12 step_max=%llu rsp_branch_pc=%03x rsp_branch_opcode=10000001 rsp_delay_pc=%03x rsp_delay_opcode=24030009 rsp_pair_first_pc=%03x rsp_pair_second_pc=%03x rsp_load_use_stall=1 rsp_break_pc=%03x rsp_retired=%llu rsp_stall_slots=%llu rsp_status=%08x rsp_semaphore=%u cpu_retired=%llu cpu_terminal_pc=%08x cpu_terminal_opcode=%08x delay_pc=%08x delay_opcode=%08x guest_tick=%llu capture_bytes=%zu transcript_hex=",
                 rom.sha256, RSP_SMOKE_PROFILE,
                 (unsigned long long)boot_step_max,
                 RSP_SMOKE_RSP_BRANCH_PC, RSP_SMOKE_RSP_DELAY_PC,
                 RSP_SMOKE_RSP_PAIR_PC, RSP_SMOKE_RSP_PAIR_SECOND_PC,
                 machine->rsp.pc,
                 (unsigned long long)seen.rsp_retired,
                 (unsigned long long)machine->rsp.stall_slots,
                 machine->rsp.sp_status,
                 machine->rsp.sp_semaphore ? 1u : 0u,
                 (unsigned long long)seen.cpu_retired,
                 RSP_SMOKE_TERMINAL_PC, RSP_SMOKE_TERMINAL_OPCODE,
                 RSP_SMOKE_DELAY_PC, RSP_SMOKE_DELAY_OPCODE,
                 (unsigned long long)current_tick, capture_length);
    for (index = 0u; index < capture_length; index++) {
      (void)printf("%02x", capture[index]);
    }
    (void)printf(" dmem_80_9f=");
    for (index = 0u; index < 32u; index++) {
      (void)printf("%02x", machine->sp_dmem[UINT32_C(0x80) + index]);
    }
    (void)printf(" flag_word=%08x load_use_word=%08x input_word=%08x vsar=%02x%02x,%02x%02x,%02x%02x divider=%02x%02x,%02x%02x\n",
                 flag_word, load_use_word,
                 (unsigned int)((uint32_t)machine->sp_dmem[0x100] << 24 |
                                (uint32_t)machine->sp_dmem[0x101] << 16 |
                                (uint32_t)machine->sp_dmem[0x102] << 8 |
                                (uint32_t)machine->sp_dmem[0x103]),
                 machine->sp_dmem[0xa0], machine->sp_dmem[0xa1],
                 machine->sp_dmem[0xb0], machine->sp_dmem[0xb1],
                 machine->sp_dmem[0xc0], machine->sp_dmem[0xc1],
                 machine->sp_dmem[0xd0], machine->sp_dmem[0xd1],
                 machine->sp_dmem[0xe0], machine->sp_dmem[0xe1]);
  }
  result = 0;
cleanup:
  if (machine != NULL) t64_machine_destroy(machine);
  free(rom.bytes);
  return result;
}

int main(int argc, char **argv) {
  if (argc == 4 && strcmp(argv[1], "--run") == 0) {
    return run_smoke(argv[2], argv[3], RSP_SMOKE_BOOT_STEP_MAX);
  }
  if (argc == 6 && strcmp(argv[1], "--run") == 0 &&
      strcmp(argv[4], "--boot-step-max") == 0 &&
      (strcmp(argv[5], "1") == 0 || strcmp(argv[5], "16") == 0)) {
    uint64_t boot_step_max = strcmp(argv[5], "1") == 0
                                 ? UINT64_C(1)
                                 : RSP_SMOKE_BOOT_STEP_MAX;
    return run_smoke(argv[2], argv[3], boot_step_max);
  }
  (void)fprintf(stderr,
                "usage: t64-rsp-smoke --run ROM EXPECTED_ROM_SHA256 [--boot-step-max 1|16]\n");
  return 64;
}

#include "t64_internal.h"

#include "t64_sha256.h"

#include <stddef.h>
#include <stdint.h>

static void hash_u32(t64_sha256_ctx *context, uint32_t value) {
  uint8_t bytes[4];

  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  t64_sha256_update(context, bytes, sizeof bytes);
}

static void hash_u64(t64_sha256_ctx *context, uint64_t value) {
  uint8_t bytes[8];
  unsigned i;

  for (i = 0u; i < 8u; i++) {
    unsigned shift = 56u - (i * 8u);
    bytes[i] = (uint8_t)(value >> shift);
  }
  t64_sha256_update(context, bytes, sizeof bytes);
}

static void hash_event(t64_sha256_ctx *context, const t64_event *event) {
  hash_u64(context, event->tick);
  hash_u64(context, event->sequence);
  hash_u32(context, event->device);
  hash_u32(context, event->kind);
  hash_u64(context, event->value);
  hash_u64(context, event->token);
}

static void hash_sp_descriptor(t64_sha256_ctx *context,
                               const t64_sp_dma_descriptor *descriptor) {
  hash_u32(context, descriptor->valid ? UINT32_C(1) : UINT32_C(0));
  hash_u32(context, descriptor->dram_to_sp ? UINT32_C(1) : UINT32_C(0));
  hash_u32(context, descriptor->imem ? UINT32_C(1) : UINT32_C(0));
  hash_u32(context, descriptor->raw_mem_address);
  hash_u32(context, descriptor->raw_dram_address);
  hash_u32(context, descriptor->raw_length);
  hash_u32(context, descriptor->aligned_dram_start);
  hash_u32(context, descriptor->internal_start);
  hash_u32(context, descriptor->bytes_per_line);
  hash_u32(context, descriptor->line_count);
  hash_u32(context, descriptor->skip);
  hash_u32(context, descriptor->payload_bytes);
  hash_u64(context, descriptor->start_tick);
  hash_u64(context, descriptor->end_tick);
  hash_u64(context, descriptor->token);
}

static void hash_vr4300_cpu_state(t64_sha256_ctx *context,
                                  const t64_cpu_state *cpu) {
  size_t i;

  hash_u64(context, cpu->pc_full);
  hash_u64(context, cpu->last_pc_full);
  hash_u64(context, cpu->delay_slot_pc_full);
  hash_u64(context, cpu->delay_target_full);
  hash_u64(context, cpu->stop_pc_full);
  hash_u32(context, cpu->attempt_outcome);
  hash_u32(context, cpu->semantic_policy_version);
  hash_u32(context, cpu->cp0_config_init_window ? UINT32_C(1) : UINT32_C(0));
  hash_u32(context, cpu->tlb_shutdown ? UINT32_C(1) : UINT32_C(0));
  hash_u32(context, cpu->pending_exception_valid ? UINT32_C(1) : UINT32_C(0));
  hash_u32(context, cpu->pending_exception_code);
  hash_u64(context, cpu->pending_exception_address);
  hash_u64(context, cpu->pending_exception_epc);
  hash_u32(context, cpu->pending_exception_bd ? UINT32_C(1) : UINT32_C(0));

  hash_u32(context, T64_VR4300_TLB_ENTRY_COUNT);
  for (i = 0u; i < T64_VR4300_TLB_ENTRY_COUNT; i++) {
    const t64_vr4300_tlb_pair *entry = &cpu->tlb[i];

    hash_u64(context, entry->page_mask);
    hash_u64(context, entry->vpn2);
    hash_u64(context, entry->region);
    hash_u32(context, entry->asid);
    hash_u64(context, entry->entry_lo0);
    hash_u64(context, entry->entry_lo1);
  }

  hash_u64(context, cpu->count_sync_tick);
  hash_u32(context, cpu->count_half_clock_phase);
  hash_u64(context, cpu->count_compare_deadline);
  hash_u64(context, cpu->count_equality_latch_generation);
  hash_u32(context, cpu->count_equality_latched ? UINT32_C(1) : UINT32_C(0));
  hash_u32(context, cpu->linked_physical_address);
  hash_u32(context, cpu->linked_width);
  hash_u32(context, cpu->linked_valid ? UINT32_C(1) : UINT32_C(0));

  hash_u32(context, T64_VR4300_ICACHE_LINE_COUNT);
  for (i = 0u; i < T64_VR4300_ICACHE_LINE_COUNT; i++) {
    const t64_vr4300_icache_line *line = &cpu->icache[i];

    hash_u32(context, line->physical_tag);
    hash_u32(context, line->valid ? UINT32_C(1) : UINT32_C(0));
    t64_sha256_update(context, line->data, sizeof line->data);
  }
  hash_u32(context, T64_VR4300_DCACHE_LINE_COUNT);
  for (i = 0u; i < T64_VR4300_DCACHE_LINE_COUNT; i++) {
    const t64_vr4300_dcache_line *line = &cpu->dcache[i];

    hash_u32(context, line->physical_tag);
    hash_u32(context, line->valid ? UINT32_C(1) : UINT32_C(0));
    hash_u32(context, line->dirty ? UINT32_C(1) : UINT32_C(0));
    t64_sha256_update(context, line->data, sizeof line->data);
  }
  hash_u32(context, 32u);
  for (i = 0u; i < 32u; i++) {
    hash_u64(context, cpu->fpr[i]);
  }
  hash_u32(context, cpu->fcr0);
  hash_u32(context, cpu->fcr31);
}

t64_status t64_machine_digest(const t64_machine *machine,
                              uint8_t digest_out[T64_DIGEST_SIZE]) {
  static const uint8_t domain_v5[] = {'t', '6', '4', '.', 's', 't', 'a', 't',
                                      'e', '.', 'v', '5'};
  static const uint8_t domain_v6[] = {'t', '6', '4', '.', 's', 't', 'a', 't',
                                      'e', '.', 'v', '6'};
  static const uint8_t domain_v7[] = {'t', '6', '4', '.', 's', 't', 'a', 't',
                                      'e', '.', 'v', '7'};
  static const uint8_t domain_v8[] = {'t', '6', '4', '.', 's', 't', 'a', 't',
                                      'e', '.', 'v', '8'};
  static const uint8_t domain_v9[] = {'t', '6', '4', '.', 's', 't', 'a', 't',
                                      'e', '.', 'v', '9'};
  static const uint8_t domain_v10[] = {'t', '6', '4', '.', 's', 't', 'a',
                                       't', 'e', '.', 'v', '1', '0'};
  static const uint8_t domain_v11[] = {'t', '6', '4', '.', 's', 't', 'a',
                                       't', 'e', '.', 'v', '1', '1'};
  static const uint8_t domain_v12[] =
      "t64.state.rsp.functional.12";
  t64_sha256_ctx context;
  t64_address_range output_range;
  const bool has_v6_state =
      machine != NULL && machine->profile.state_serialization_version == 6u;
  const bool has_v7_state =
      machine != NULL && machine->profile.state_serialization_version == 7u;
  const bool has_v8_state =
      machine != NULL && machine->profile.state_serialization_version == 8u;
  const bool has_v9_state =
      machine != NULL && machine->profile.state_serialization_version == 9u;
  const bool has_v10_state =
      machine != NULL && machine->profile.state_serialization_version == 10u;
  const bool has_v11_state =
      machine != NULL && machine->profile.state_serialization_version == 11u;
  const bool has_v12_state = t64_has_rsp_functional_profile(machine);
  const bool has_warm_state =
      has_v6_state || has_v7_state || has_v8_state || has_v9_state ||
      has_v10_state || has_v11_state || has_v12_state;
  size_t i;

  if (machine == NULL || digest_out == NULL ||
      !t64_internal_make_range(digest_out, T64_DIGEST_SIZE, &output_range) ||
      !t64_internal_observation_range_is_disjoint(machine, output_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }

  t64_sha256_init(&context);
  if (has_v12_state) {
    t64_sha256_update(&context, domain_v12, sizeof domain_v12 - 1u);
  } else if (has_v11_state) {
    t64_sha256_update(&context, domain_v11, sizeof domain_v11);
  } else if (has_v10_state) {
    t64_sha256_update(&context, domain_v10, sizeof domain_v10);
  } else if (has_v9_state) {
    t64_sha256_update(&context, domain_v9, sizeof domain_v9);
  } else if (has_v8_state) {
    t64_sha256_update(&context, domain_v8, sizeof domain_v8);
  } else if (has_v7_state) {
    t64_sha256_update(&context, domain_v7, sizeof domain_v7);
  } else if (has_v6_state) {
    t64_sha256_update(&context, domain_v6, sizeof domain_v6);
  } else {
    t64_sha256_update(&context, domain_v5, sizeof domain_v5);
  }
  hash_u32(&context, machine->profile.state_serialization_version);
  t64_sha256_update(&context, machine->profile_digest,
                    sizeof machine->profile_digest);
  t64_sha256_update(&context, machine->content_digest,
                    sizeof machine->content_digest);
  hash_u32(&context, machine->has_image ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, (uint32_t)machine->image_length);
  t64_sha256_update(&context, machine->rom_header,
                    sizeof machine->rom_header);
  t64_sha256_update(&context, machine->rom_title,
                    sizeof machine->rom_title);
  t64_sha256_update(&context, machine->rom_game_code,
                    sizeof machine->rom_game_code);
  hash_u32(&context, machine->rom_version);
  hash_u64(&context, machine->tick);
  hash_u64(&context, machine->instruction_count);
  hash_u64(&context, machine->frame_event_count);
  hash_u32(&context, machine->sticky_error_status);
  hash_u32(&context, machine->halted ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, machine->profile.rdram_bytes);
  t64_sha256_update(&context, machine->ram, machine->profile.rdram_bytes);
  hash_u32(&context, (uint32_t)sizeof machine->sp_dmem);
  t64_sha256_update(&context, machine->sp_dmem, sizeof machine->sp_dmem);
  hash_u32(&context, (uint32_t)sizeof machine->sp_imem);
  t64_sha256_update(&context, machine->sp_imem, sizeof machine->sp_imem);
  hash_u32(&context, machine->mi_pending & T64_MI_INTERRUPT_BITS);
  hash_u32(&context, machine->mi_mask & T64_MI_INTERRUPT_BITS);
  hash_u32(&context, machine->mi_irq_line ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, machine->pi.dram_address);
  hash_u32(&context, machine->pi.cart_address);
  hash_u32(&context, machine->pi.read_length);
  hash_u32(&context, machine->pi.write_length);
  hash_u32(&context, machine->pi.domain1_latency);
  hash_u32(&context, machine->pi.domain1_pulse_width);
  hash_u32(&context, machine->pi.domain1_page_size);
  hash_u32(&context, machine->pi.domain1_release_duration);
  hash_u32(&context, machine->pi.domain2_latency);
  hash_u32(&context, machine->pi.domain2_pulse_width);
  hash_u32(&context, machine->pi.domain2_page_size);
  hash_u32(&context, machine->pi.domain2_release_duration);
  hash_u32(&context, machine->pi.error ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, machine->pi.active_valid ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context,
           machine->pi.active_canceled ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, machine->pi.active_dram_address);
  hash_u32(&context, machine->pi.active_cart_address);
  hash_u32(&context, machine->pi.active_length);
  hash_u64(&context, machine->pi.active_token);
  hash_u64(&context, machine->pi.next_token);
  hash_u32(&context,
           machine->pi.token_exhausted ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, machine->sp.mem_address);
  hash_u32(&context, machine->sp.dram_address);
  hash_u32(&context, machine->sp.read_length);
  hash_u32(&context, machine->sp.write_length);
  hash_sp_descriptor(&context, &machine->sp.active);
  hash_sp_descriptor(&context, &machine->sp.pending);
  hash_u64(&context, machine->sp.next_token);
  hash_u32(&context,
           machine->sp.token_exhausted ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, (uint32_t)machine->queue_count);
  hash_u64(&context, machine->next_sequence);
  hash_u32(&context,
           machine->sequence_exhausted ? UINT32_C(1) : UINT32_C(0));
  for (i = 0u; i < machine->queue_count; i++) {
    size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;

    hash_event(&context, &machine->events[index]);
  }
  if (has_warm_state) {
    for (i = 0u; i < sizeof machine->boot.asset_sha256; i++) {
      t64_sha256_update(&context, &machine->boot.asset_sha256[i], 1u);
    }
    for (i = 0u; i < sizeof machine->boot.payload_sha256; i++) {
      t64_sha256_update(&context, &machine->boot.payload_sha256[i], 1u);
    }
    for (i = 0u; i < sizeof machine->boot.policy_sha256; i++) {
      t64_sha256_update(&context, &machine->boot.policy_sha256[i], 1u);
    }
    hash_u32(&context, machine->boot.source_offset);
    hash_u32(&context, machine->boot.payload_length);
    hash_u32(&context, machine->boot.dmem_offset);
    hash_u32(&context, machine->boot.entry_pc);
    hash_u32(&context, machine->boot.seed_reset_version);
    hash_u32(&context, machine->boot.seed_s3_rom_type);
    hash_u32(&context, machine->boot.seed_s4_tv_type);
    hash_u32(&context, machine->boot.seed_s5_reset_type);
    hash_u32(&context, machine->boot.seed_s6_entropy);
    hash_u32(&context, machine->boot.seed_s7_boot_info_version);
    hash_u32(&context, machine->boot.prepared ? UINT32_C(1) : UINT32_C(0));
    for (i = 0u; i < 32u; i++) {
      hash_u64(&context, machine->cpu.gpr[i]);
    }
    hash_u64(&context, machine->cpu.hi);
    hash_u64(&context, machine->cpu.lo);
    for (i = 0u; i < 32u; i++) {
      hash_u64(&context, machine->cpu.cp0[i]);
    }
    hash_u64(&context, machine->cpu.retired_instructions);
    hash_u32(&context, machine->cpu.pc);
    hash_u32(&context, machine->cpu.last_pc);
    hash_u32(&context, machine->cpu.last_opcode);
    hash_u32(&context, machine->cpu.delay_slot_pc);
    hash_u32(&context, machine->cpu.delay_target);
    hash_u32(&context,
             machine->cpu.delay_slot_pending ? UINT32_C(1) : UINT32_C(0));
    hash_u32(&context, machine->cpu.stopped ? UINT32_C(1) : UINT32_C(0));
    hash_u32(&context, machine->cpu.stop_reason);
    hash_u32(&context, machine->cpu.stop_pc);
    hash_u32(&context, machine->cpu.stop_opcode);
    if (has_v9_state || has_v10_state || has_v11_state || has_v12_state) {
      hash_vr4300_cpu_state(&context, &machine->cpu);
      if (has_v10_state || has_v11_state || has_v12_state) {
        hash_u64(&context, machine->cpu.delay_origin_pc_full);
      }
      if (has_v11_state || has_v12_state) {
        hash_u64(&context, machine->cpu.cp2_transfer_latch);
      }
    }
    hash_u32(&context, machine->cache.policy_version);
    hash_u32(&context, machine->cache.last_operation);
    hash_u32(&context, machine->cache.last_pc);
    hash_u32(&context, machine->cache.last_address);
    hash_u32(&context,
             machine->cache.operation_seen ? UINT32_C(1) : UINT32_C(0));
    hash_u32(&context, machine->guest_devices.policy_version);
    hash_u32(&context, machine->guest_devices.mi_version);
    hash_u32(&context, machine->guest_devices.ri_select);
    hash_u32(&context, machine->guest_devices.mi_version_known
                           ? UINT32_C(1)
                           : UINT32_C(0));
    hash_u32(&context, machine->guest_devices.ri_select_known
                           ? UINT32_C(1)
                           : UINT32_C(0));
    hash_u32(&context, machine->output.policy_version);
    hash_u32(&context, machine->output.video_output_ready ? UINT32_C(1)
                                                          : UINT32_C(0));
    hash_u32(&context, machine->output.audio_output_ready ? UINT32_C(1)
                                                          : UINT32_C(0));
    if (has_v7_state || has_v8_state || has_v9_state || has_v10_state ||
        has_v11_state || has_v12_state) {
      /* Retained guest output is future state; the host drain cursor is not. */
      hash_u32(&context, machine->output.magic_register);
      hash_u32(&context, machine->output.byte_count);
      hash_u32(&context, T64_ISVIEWER_BUFFER_BYTES);
      t64_sha256_update(&context, machine->output.buffer,
                        sizeof machine->output.buffer);
      hash_u32(&context, machine->output.captured_bytes);
      hash_u32(&context, T64_ISVIEWER_CAPTURE_BYTES);
      t64_sha256_update(&context, machine->output.captured,
                        sizeof machine->output.captured);
      hash_u32(&context,
               machine->output.lost_output ? UINT32_C(1) : UINT32_C(0));
      if (has_v8_state || has_v9_state || has_v10_state || has_v11_state ||
          has_v12_state) {
        hash_u64(&context, machine->output.total_attempted_bytes);
        hash_u32(&context,
                 machine->output.total_attempted_bytes_saturated
                     ? UINT32_C(1)
                     : UINT32_C(0));
      }
    }
    if (has_v12_state) {
      const t64_rsp_state *rsp = &machine->rsp;

      for (i = 0u; i < 32u; i++) {
        hash_u32(&context, rsp->gpr[i]);
      }
      t64_sha256_update(&context, (const uint8_t *)rsp->vector,
                        sizeof rsp->vector);
      for (i = 0u; i < 8u; i++) {
        hash_u64(&context, rsp->accumulator[i] & UINT64_C(0x0000ffffffffffff));
      }
      hash_u32(&context, rsp->vco);
      hash_u32(&context, rsp->vcc);
      hash_u32(&context, rsp->vce);
      hash_u32(&context, rsp->pc);
      hash_u32(&context, rsp->delay_target);
      hash_u32(&context, rsp->delay_slot_pending ? 1u : 0u);
      hash_u32(&context, rsp->branch_bubble_pending ? 1u : 0u);
      hash_u32(&context, rsp->divider_input);
      hash_u32(&context, rsp->divider_result);
      hash_u32(&context, rsp->divider_high_valid ? 1u : 0u);
      for (i = 0u; i < 32u; i++) {
        hash_u64(&context, rsp->scalar_ready_tick[i]);
        hash_u64(&context, rsp->vector_ready_tick[i]);
      }
      for (i = 0u; i < 8u; i++) {
        hash_u64(&context, rsp->accumulator_ready_tick[i]);
      }
      hash_u64(&context, rsp->memory_access_ready_tick);
      hash_u64(&context, rsp->next_issue_tick);
      hash_u64(&context, rsp->issue_sequence);
      hash_u64(&context, rsp->retired_instructions);
      hash_u64(&context, rsp->stall_slots);
      hash_u32(&context, rsp->sp_status);
      hash_u32(&context, rsp->sp_semaphore ? 1u : 0u);
      hash_u64(&context, rsp->dma_beat_cursor);
      hash_u64(&context, rsp->dma_setup_remaining);
      hash_u64(&context, rsp->active_dma_token);
      hash_u64(&context, rsp->pending_dma_token);
      hash_u64(&context, rsp->dma_event_sequence);
    }
  }
  t64_sha256_final(&context, digest_out);
  return T64_STATUS_OK;
}

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

static void hash_rdram_protocol_state(
    t64_sha256_ctx *context, const t64_rdram_protocol_state *protocol) {
  size_t device_index;
  size_t register_index;

  hash_u32(context, protocol->ri_config);
  hash_u32(context, protocol->ri_config_known ? 1u : 0u);
  hash_u32(context, protocol->ri_current_load);
  hash_u32(context, protocol->ri_current_load_known ? 1u : 0u);
  hash_u64(context, protocol->ri_config_tick);
  hash_u64(context, protocol->ri_current_load_deadline_tick);
  hash_u32(context, protocol->ri_mode);
  hash_u32(context, protocol->ri_mode_known ? 1u : 0u);
  hash_u64(context, protocol->ri_mode_tick);
  hash_u64(context, protocol->ri_mode_deadline_tick);
  hash_u32(context, protocol->ri_refresh);
  hash_u32(context, protocol->ri_refresh_known ? 1u : 0u);
  hash_u32(context, protocol->mi_repeat_enabled ? 1u : 0u);
  hash_u32(context, protocol->mi_repeat_known ? 1u : 0u);
  hash_u32(context, protocol->mi_repeat_length);
  hash_u32(context, protocol->mi_upper_mode ? 1u : 0u);
  hash_u32(context, protocol->broadcast_registers_known);
  for (register_index = 0u;
       register_index < T64_RDRAM_PROTOCOL_REGISTER_COUNT;
       register_index++) {
    hash_u32(context, protocol->broadcast_register_values[register_index]);
  }
  hash_u32(context, protocol->logical_device_count);
  hash_u32(context, protocol->capacity_scan_complete ? 1u : 0u);
  hash_u32(context, protocol->logical_capacity_bytes);
  for (device_index = 0u; device_index < T64_RDRAM_PROTOCOL_SLOT_COUNT;
       device_index++) {
    const t64_rdram_device_state *device =
        &protocol->devices[device_index];

    hash_u32(context, device->device_id);
    hash_u32(context, device->calibration_state);
    hash_u32(context, device->present ? 1u : 0u);
    hash_u32(context, device->device_id_known ? 1u : 0u);
    hash_u32(context, device->registers_known);
    for (register_index = 0u;
         register_index < T64_RDRAM_PROTOCOL_REGISTER_COUNT;
         register_index++) {
      hash_u32(context, device->register_values[register_index]);
    }
  }
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
  static const uint8_t domain_v13[] = "t64.state.rdram.t1.13";
  static const uint8_t domain_v16[] = "t64.state.rdp.vi.tmem.16";
  static const uint8_t domain_v17[] = "t64.state.rdp.vi.triangle.17";
  static const uint8_t domain_v18[] = "t64.state.rdp.vi.texture.18";
  static const uint8_t domain_v19[] = "t64.state.rdp.vi.combiner.19";
  static const uint8_t domain_v20[] = "t64.state.rdp.vi.blender.20";
  static const uint8_t domain_v21[] = "t64.state.rdp.vi.depth.21";
  static const uint8_t domain_v22[] = "t64.state.rdp.vi.scanout.22";
  static const uint8_t domain_v23[] = "t64.state.rdp.vi.filter.23";
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
  const bool has_v12_state =
      machine != NULL && machine->profile.state_serialization_version == 12u;
  const bool has_v13_state = t64_has_t1_policy(machine);
  const bool has_v16_state =
      machine != NULL && machine->profile.machine_model_version == 11u &&
      machine->profile.state_serialization_version == 16u;
  const bool has_v17_state =
      machine != NULL && machine->profile.machine_model_version == 12u &&
      machine->profile.state_serialization_version == 17u;
  const bool has_v18_state = t64_has_rdp_texture_profile(machine);
  const bool has_v19_state =
      machine != NULL && machine->profile.machine_model_version == 14u &&
      machine->profile.state_serialization_version == 19u;
  const bool has_v20_state =
      machine != NULL && machine->profile.machine_model_version == 15u &&
      machine->profile.state_serialization_version == 20u;
  const bool has_v21_state = t64_has_rdp_depth_profile(machine);
  const bool has_v22_state = t64_has_vi_scanout_v8_profile(machine);
  const bool has_v23_state = t64_has_vi_filter_v9_profile(machine);
  const bool has_rdp_state = has_v16_state || has_v17_state ||
                             has_v18_state || has_v19_state || has_v20_state ||
                             has_v21_state || has_v22_state || has_v23_state;
  const bool has_rsp_state = has_v12_state || has_v13_state;
  const bool has_warm_state =
      has_v6_state || has_v7_state || has_v8_state || has_v9_state ||
      has_v10_state || has_v11_state || has_rsp_state;
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
  if (has_v23_state) {
    t64_sha256_update(&context, domain_v23, sizeof domain_v23 - 1u);
  } else if (has_v22_state) {
    t64_sha256_update(&context, domain_v22, sizeof domain_v22 - 1u);
  } else if (has_v21_state) {
    t64_sha256_update(&context, domain_v21, sizeof domain_v21 - 1u);
  } else if (has_v20_state) {
    t64_sha256_update(&context, domain_v20, sizeof domain_v20 - 1u);
  } else if (has_v19_state) {
    t64_sha256_update(&context, domain_v19, sizeof domain_v19 - 1u);
  } else if (has_v18_state) {
    t64_sha256_update(&context, domain_v18, sizeof domain_v18 - 1u);
  } else if (has_v17_state) {
    t64_sha256_update(&context, domain_v17, sizeof domain_v17 - 1u);
  } else if (has_v16_state) {
    t64_sha256_update(&context, domain_v16, sizeof domain_v16 - 1u);
  } else if (has_v13_state) {
    t64_sha256_update(&context, domain_v13, sizeof domain_v13 - 1u);
  } else if (has_v12_state) {
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
    if (has_v9_state || has_v10_state || has_v11_state || has_rsp_state) {
      hash_vr4300_cpu_state(&context, &machine->cpu);
      if (has_v10_state || has_v11_state || has_rsp_state) {
        hash_u64(&context, machine->cpu.delay_origin_pc_full);
      }
      if (has_v11_state || has_rsp_state) {
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
        has_v11_state || has_rsp_state) {
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
          has_rsp_state) {
        hash_u64(&context, machine->output.total_attempted_bytes);
        hash_u32(&context,
                 machine->output.total_attempted_bytes_saturated
                     ? UINT32_C(1)
                     : UINT32_C(0));
      }
    }
    if (has_rsp_state) {
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
    if (has_v13_state) {
      hash_rdram_protocol_state(
          &context, &machine->guest_devices.rdram_protocol);
    }
    if (has_rdp_state) {
      const t64_rdp_state *rdp = &machine->rdp;
      const t64_vi_state *vi = &machine->vi;
      size_t logical;

      hash_u32(&context, rdp->dpc_start);
      hash_u32(&context, rdp->dpc_end);
      hash_u32(&context, rdp->dpc_current);
      hash_u32(&context, rdp->dpc_status);
      hash_u32(&context, rdp->dpc_clock_counter);
      hash_u32(&context, rdp->dpc_buffer_busy_counter);
      hash_u32(&context, rdp->dpc_pipe_busy_counter);
      hash_u32(&context, rdp->dpc_tmem_counter);
      hash_u32(&context, rdp->pending_command_address);
      hash_u32(&context, rdp->pending_command_byte);
      hash_u32(&context, rdp->pending_opcode_field);
      hash_u64(&context, rdp->pending_command_word);
      hash_u64(&context, rdp->fence_generation);
      hash_u32(&context, rdp->color_image_address);
      hash_u32(&context, rdp->color_image_width);
      hash_u32(&context, rdp->color_image_format);
      hash_u32(&context, rdp->color_image_size);
      hash_u32(&context, rdp->scissor_xh);
      hash_u32(&context, rdp->scissor_yh);
      hash_u32(&context, rdp->scissor_xl);
      hash_u32(&context, rdp->scissor_yl);
      hash_u64(&context, rdp->other_modes);
      hash_u32(&context, rdp->fill_color);
      hash_u32(&context, rdp->texture_image_address);
      hash_u32(&context, rdp->texture_image_width);
      hash_u32(&context, rdp->texture_image_format);
      hash_u32(&context, rdp->texture_image_size);
      hash_u32(&context, rdp->texture_image_valid);
      hash_u32(&context, rdp->tlut_format);
      hash_u32(&context, rdp->tlut_valid ? 1u : 0u);
      hash_u32(&context, rdp->render_kind);
      hash_u32(&context, rdp->render_texture_tile);
      hash_u32(&context, (uint32_t)rdp->render_texture_s);
      hash_u32(&context, (uint32_t)rdp->render_texture_t);
      hash_u32(&context, (uint32_t)rdp->render_texture_dsdx);
      hash_u32(&context, (uint32_t)rdp->render_texture_dtdy);
      hash_u64(&context, rdp->copy_count);
      for (i = 0u; i < 6u; i++)
        hash_u32(&context, rdp->convert_coefficients[i]);
      hash_u64(&context, rdp->pending_texrect_word);
      if (has_v17_state || has_v18_state || has_v19_state || has_v20_state ||
          has_v21_state) {
        size_t packet_index;
        hash_u32(&context, rdp->pending_triangle_word_count);
        for (packet_index = 0u;
             packet_index < rdp->pending_triangle_word_count &&
             packet_index < T64_RDP_TRIANGLE_MAX_WORDS;
             packet_index++) {
          hash_u64(&context, rdp->pending_triangle_words[packet_index]);
        }
        hash_u32(&context, rdp->primitive_color);
        hash_u32(&context, rdp->primitive_color_valid ? 1u : 0u);
        hash_u32(&context, rdp->triangle_count);
        hash_u32(&context, rdp->render_triangle ? 1u : 0u);
        hash_u32(&context, rdp->shade_triangle ? 1u : 0u);
        hash_u32(&context, T64_RDP_TRIANGLE_COVERAGE_WIDTH);
        hash_u32(&context, T64_RDP_TRIANGLE_COVERAGE_HEIGHT);
        t64_sha256_update(&context, machine->rdp_triangle_coverage,
                          T64_RDP_TRIANGLE_COVERAGE_BYTES);
      }
      if (has_v20_state || has_v21_state) {
        hash_u32(&context,
                 (uint32_t)T64_RDP_MEMORY_COVERAGE_BYTES(
                     machine->profile.rdram_bytes));
        t64_sha256_update(&context, machine->rdp_memory_coverage,
                          T64_RDP_MEMORY_COVERAGE_BYTES(
                              machine->profile.rdram_bytes));
      }
      if (has_v19_state || has_v20_state || has_v21_state) {
        hash_u32(&context, rdp->primitive_lod_fraction);
        hash_u32(&context, rdp->environment_color);
        hash_u32(&context, rdp->fog_color);
        hash_u32(&context, rdp->blend_color);
        hash_u64(&context, rdp->combine_mode);
        hash_u32(&context, rdp->combine_mode_valid ? 1u : 0u);
        hash_u32(&context, rdp->environment_color_valid ? 1u : 0u);
        hash_u32(&context, rdp->fog_color_valid ? 1u : 0u);
        hash_u32(&context, rdp->blend_color_valid ? 1u : 0u);
        hash_u32(&context, rdp->key_center_valid ? 1u : 0u);
        hash_u32(&context, rdp->key_scale_valid ? 1u : 0u);
        hash_u32(&context, rdp->key_width_valid ? 1u : 0u);
        for (i = 0u; i < 3u; i++) {
          hash_u32(&context, rdp->key_center[i]);
          hash_u32(&context, rdp->key_scale[i]);
          hash_u32(&context, rdp->key_width[i]);
        }
      }
      if (has_v21_state) {
        hash_u32(&context, rdp->depth_image_address);
        hash_u32(&context, rdp->depth_image_valid ? 1u : 0u);
        hash_u32(&context, rdp->primitive_z);
        hash_u32(&context, rdp->primitive_dz);
        hash_u32(&context, rdp->primitive_depth_valid ? 1u : 0u);
        hash_u32(&context,
                 (uint32_t)T64_RDP_DEPTH_HIDDEN_BYTES(
                     machine->profile.rdram_bytes));
        t64_sha256_update(&context, machine->rdp_depth_hidden,
                          T64_RDP_DEPTH_HIDDEN_BYTES(
                              machine->profile.rdram_bytes));
      }
      hash_u32(&context, rdp->work_policy_version);
      hash_u32(&context, rdp->random_state);
      hash_u32(&context, rdp->refusal_reason);
      hash_u64(&context, rdp->command_count);
      hash_u64(&context, rdp->fill_count);
      hash_u64(&context, rdp->fullsync_count);
      hash_u64(&context, rdp->last_original_word);
      hash_u32(&context, rdp->last_command_byte);
      hash_u32(&context, rdp->last_opcode_field);
      hash_u32(&context, rdp->render_x_begin);
      hash_u32(&context, rdp->render_x_end);
      hash_u32(&context, rdp->render_y_begin);
      hash_u32(&context, rdp->render_y_end);
      hash_u32(&context, rdp->render_y);
      hash_u32(&context, rdp->color_image_valid ? 1u : 0u);
      hash_u32(&context, rdp->scissor_valid ? 1u : 0u);
      hash_u32(&context, rdp->other_modes_valid ? 1u : 0u);
      hash_u32(&context, rdp->fill_color_valid ? 1u : 0u);
      hash_u32(&context, rdp->render_active ? 1u : 0u);
      hash_u32(&context, rdp->fence_pending ? 1u : 0u);
      hash_u32(&context, rdp->busy ? 1u : 0u);
      hash_u32(&context, rdp->dpc_xbus ? 1u : 0u);
      hash_u32(&context, rdp->dpc_freeze ? 1u : 0u);
      hash_u32(&context, rdp->dpc_flush ? 1u : 0u);
      hash_u32(&context, rdp->dpc_start_valid ? 1u : 0u);
      hash_u32(&context, rdp->dpc_end_valid ? 1u : 0u);
      hash_u32(&context, rdp->command_pending ? 1u : 0u);
      hash_u32(&context, rdp->texrect_pending ? 1u : 0u);
      hash_u32(&context, rdp->render_texture_flip ? 1u : 0u);

      t64_sha256_update(&context, rdp->tmem, sizeof rdp->tmem);
      for (i = 0u; i < T64_RDP_TILE_COUNT; i++) {
        const t64_rdp_tile *tile = &rdp->tiles[i];
        hash_u32(&context, tile->format);
        hash_u32(&context, tile->size);
        hash_u32(&context, tile->line_words);
        hash_u32(&context, tile->tmem_word);
        hash_u32(&context, tile->palette);
        hash_u32(&context, tile->mask_s);
        hash_u32(&context, tile->mask_t);
        hash_u32(&context, tile->shift_s);
        hash_u32(&context, tile->shift_t);
        hash_u32(&context, tile->uls);
        hash_u32(&context, tile->ult);
        hash_u32(&context, tile->lrs);
        hash_u32(&context, tile->lrt);
        hash_u32(&context, tile->clamp_s ? 1u : 0u);
        hash_u32(&context, tile->mirror_s ? 1u : 0u);
        hash_u32(&context, tile->clamp_t ? 1u : 0u);
        hash_u32(&context, tile->mirror_t ? 1u : 0u);
        hash_u32(&context, tile->valid ? 1u : 0u);
        hash_u32(&context, tile->size_valid ? 1u : 0u);
      }

      for (i = 0u; i < sizeof vi->registers / sizeof vi->registers[0];
           i++) {
        hash_u32(&context, vi->registers[i]);
      }
      hash_u64(&context, vi->phase_remainder);
      hash_u64(&context, vi->phase_cycles);
      hash_u64(&context, vi->last_guest_tick);
      hash_u64(&context, vi->next_sequence);
      hash_u64(&context, vi->event_generation);
      hash_u32(&context, vi->field);
      hash_u32(&context, vi->refusal_reason);
      hash_u32(&context, vi->enabled ? 1u : 0u);
      hash_u32(&context, vi->sequence_exhausted ? 1u : 0u);
      if (has_v22_state || has_v23_state) {
        for (i = 0u; i < sizeof vi->active_registers /
                            sizeof vi->active_registers[0];
             i++) {
          hash_u32(&context, vi->active_registers[i]);
        }
        hash_u64(&context, vi->phase_units);
        hash_u64(&context, vi->next_boundary_units);
        hash_u64(&context, vi->field_start_tick);
        hash_u32(&context, vi->current_readback);
        hash_u32(&context, vi->latch_pending ? 1u : 0u);
      }
      /*
       * Packet bytes and metadata retain produced-output history in slots
       * chosen from the producer sequence. packet_head/count are observer
       * state and intentionally stay out of the digest; advancing the head
       * even when the queue becomes empty keeps those physical slots stable
       * across different host drain cadences.
       */
      for (logical = 0u; logical < T64_VIDEO_QUEUE_CAPACITY; logical++) {
        size_t slot = logical;
        const t64_video_packet_info *info = &vi->packet_info[slot];
        size_t length = vi->packet_lengths[slot];

        hash_u64(&context, length);
        if (machine->video_packets != NULL &&
            length <= T64_VIDEO_MAX_PACKET_BYTES) {
          t64_sha256_update(&context,
                            machine->video_packets +
                                slot * T64_VIDEO_MAX_PACKET_BYTES,
                            length);
        }
        hash_u32(&context, info->width);
        hash_u32(&context, info->height);
        hash_u32(&context, info->stride_bytes);
        hash_u32(&context, info->source_address);
        hash_u32(&context, info->source_format);
        hash_u32(&context, info->output_encoding);
        hash_u32(&context, info->field);
        hash_u32(&context, info->parity);
        hash_u32(&context, info->control);
        hash_u32(&context, info->burst);
        hash_u32(&context, info->h_sync);
        hash_u32(&context, info->v_sync);
        hash_u32(&context, info->h_start);
        hash_u32(&context, info->v_start);
        hash_u32(&context, info->x_scale);
        hash_u32(&context, info->y_scale);
        hash_u64(&context, info->start_tick);
        hash_u64(&context, info->end_tick);
        hash_u64(&context, info->sequence);
        hash_u64(&context, info->commands_completed);
        hash_u64(&context, info->fullsync_completed);
        hash_u64(&context, info->last_command_word);
        hash_u32(&context, info->last_command_byte);
        hash_u32(&context, info->last_opcode_field);
        hash_u64(&context, info->source_bytes);
        hash_u64(&context, info->packet_bytes);
        t64_sha256_update(&context, (const uint8_t *)info->profile_id,
                          sizeof info->profile_id);
        t64_sha256_update(&context, (const uint8_t *)info->build_id,
                          sizeof info->build_id);
        t64_sha256_update(&context, (const uint8_t *)info->vi_clock_policy,
                          sizeof info->vi_clock_policy);
        if (has_v22_state || has_v23_state) {
          hash_u32(&context, info->h_end);
          hash_u32(&context, info->v_end);
          hash_u32(&context, info->source_width);
          hash_u32(&context, info->source_pitch_bytes);
          hash_u32(&context, info->source_x_offset);
          hash_u32(&context, info->source_y_offset);
          hash_u32(&context, info->filter);
          t64_sha256_update(&context,
                            (const uint8_t *)info->vi_output_policy,
                            sizeof info->vi_output_policy);
        }
      }
    }
  }
  t64_sha256_final(&context, digest_out);
  return T64_STATUS_OK;
}

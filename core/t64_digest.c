#include "t64_internal.h"

#include "t64_sha256.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Every byte of the canonical state goes through digest_bytes_class. The sink
 * copies it out, hashes it, or only counts it; a non-body class is also
 * recorded as a span so a stream can be compared against an older stream with
 * its framing removed. A write that would pass the buffer or the span array
 * sets the overflow flag, copies nothing and still advances the offset.
 */
static void digest_bytes_class(t64_digest_sink *sink, const void *data,
                               size_t length, uint32_t class_id) {
  if (length == 0u) {
    return;
  }
  if (sink->buffer != NULL) {
    if (sink->offset <= sink->capacity &&
        length <= sink->capacity - sink->offset) {
      (void)memcpy(sink->buffer + sink->offset, data, length);
    } else {
      sink->overflow = true;
    }
  }
  if (sink->hash != NULL) {
    t64_sha256_update((t64_sha256_ctx *)sink->hash, data, length);
  }
  if (class_id != T64_DIGEST_CLASS_BODY && sink->spans != NULL) {
    t64_digest_span *last =
        sink->span_count > 0u ? &sink->spans[sink->span_count - 1u] : NULL;

    if (last != NULL && last->class == class_id &&
        last->offset + last->length == sink->offset) {
      last->length += length;
    } else if (sink->span_count < sink->span_capacity) {
      sink->spans[sink->span_count].offset = sink->offset;
      sink->spans[sink->span_count].length = length;
      sink->spans[sink->span_count].class = class_id;
      sink->span_count++;
    } else {
      sink->overflow = true;
    }
  }
  sink->offset += length;
}

static void digest_bytes(t64_digest_sink *sink, const void *data,
                         size_t length) {
  digest_bytes_class(sink, data, length, T64_DIGEST_CLASS_BODY);
}

static void hash_u32_class(t64_digest_sink *sink, uint32_t value,
                           uint32_t class_id) {
  uint8_t bytes[4];

  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  digest_bytes_class(sink, bytes, sizeof bytes, class_id);
}

static void hash_u32(t64_digest_sink *sink, uint32_t value) {
  hash_u32_class(sink, value, T64_DIGEST_CLASS_BODY);
}

static void hash_u64(t64_digest_sink *sink, uint64_t value) {
  uint8_t bytes[8];
  unsigned i;

  for (i = 0u; i < 8u; i++) {
    unsigned shift = 56u - (i * 8u);
    bytes[i] = (uint8_t)(value >> shift);
  }
  digest_bytes(sink, bytes, sizeof bytes);
}

/* Section 10 (SI, PIF and latch) is a new section: its fields are streamed
 * in the new-span class so a stripped v25 stream equals the older stream. */
static void hash_u32_new(t64_digest_sink *sink, uint32_t value) {
  hash_u32_class(sink, value, T64_DIGEST_CLASS_NEW);
}

static void hash_u64_new(t64_digest_sink *sink, uint64_t value) {
  hash_u32_new(sink, (uint32_t)(value >> 32));
  hash_u32_new(sink, (uint32_t)value);
}

/* v25 framing: a section id (marker class) precedes each section body. Every
 * marker goes through this function, which also feeds the section recorder of
 * the sink; the bytes written are exactly the marker bytes. */
static void section_marker(t64_digest_sink *sink, bool framed, uint32_t id) {
  if (framed) {
    if (sink->section_count < T64_DIGEST_MAX_SECTIONS) {
      sink->section_ids[sink->section_count] = id;
      sink->section_offsets[sink->section_count] = sink->offset;
      sink->section_count++;
    } else {
      sink->section_overflow = true;
    }
    hash_u32_class(sink, id, T64_DIGEST_CLASS_MARKER);
  }
}

static void hash_event(t64_digest_sink *sink, const t64_event *event) {
  hash_u64(sink, event->tick);
  hash_u64(sink, event->sequence);
  hash_u32(sink, event->device);
  hash_u32(sink, event->kind);
  hash_u64(sink, event->value);
  hash_u64(sink, event->token);
}

static void hash_sp_descriptor(t64_digest_sink *sink,
                               const t64_sp_dma_descriptor *descriptor) {
  hash_u32(sink, descriptor->valid ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, descriptor->dram_to_sp ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, descriptor->imem ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, descriptor->raw_mem_address);
  hash_u32(sink, descriptor->raw_dram_address);
  hash_u32(sink, descriptor->raw_length);
  hash_u32(sink, descriptor->aligned_dram_start);
  hash_u32(sink, descriptor->internal_start);
  hash_u32(sink, descriptor->bytes_per_line);
  hash_u32(sink, descriptor->line_count);
  hash_u32(sink, descriptor->skip);
  hash_u32(sink, descriptor->payload_bytes);
  hash_u64(sink, descriptor->start_tick);
  hash_u64(sink, descriptor->end_tick);
  hash_u64(sink, descriptor->token);
}

static void hash_rdram_protocol_state(
    t64_digest_sink *sink, const t64_rdram_protocol_state *protocol) {
  size_t device_index;
  size_t register_index;

  hash_u32(sink, protocol->ri_config);
  hash_u32(sink, protocol->ri_config_known ? 1u : 0u);
  hash_u32(sink, protocol->ri_current_load);
  hash_u32(sink, protocol->ri_current_load_known ? 1u : 0u);
  hash_u64(sink, protocol->ri_config_tick);
  hash_u64(sink, protocol->ri_current_load_deadline_tick);
  hash_u32(sink, protocol->ri_mode);
  hash_u32(sink, protocol->ri_mode_known ? 1u : 0u);
  hash_u64(sink, protocol->ri_mode_tick);
  hash_u64(sink, protocol->ri_mode_deadline_tick);
  hash_u32(sink, protocol->ri_refresh);
  hash_u32(sink, protocol->ri_refresh_known ? 1u : 0u);
  hash_u32(sink, protocol->mi_repeat_enabled ? 1u : 0u);
  hash_u32(sink, protocol->mi_repeat_known ? 1u : 0u);
  hash_u32(sink, protocol->mi_repeat_length);
  hash_u32(sink, protocol->mi_upper_mode ? 1u : 0u);
  hash_u32(sink, protocol->broadcast_registers_known);
  for (register_index = 0u;
       register_index < T64_RDRAM_PROTOCOL_REGISTER_COUNT;
       register_index++) {
    hash_u32(sink, protocol->broadcast_register_values[register_index]);
  }
  hash_u32(sink, protocol->logical_device_count);
  hash_u32(sink, protocol->capacity_scan_complete ? 1u : 0u);
  hash_u32(sink, protocol->logical_capacity_bytes);
  for (device_index = 0u; device_index < T64_RDRAM_PROTOCOL_SLOT_COUNT;
       device_index++) {
    const t64_rdram_device_state *device =
        &protocol->devices[device_index];

    hash_u32(sink, device->device_id);
    hash_u32(sink, device->calibration_state);
    hash_u32(sink, device->present ? 1u : 0u);
    hash_u32(sink, device->device_id_known ? 1u : 0u);
    hash_u32(sink, device->registers_known);
    for (register_index = 0u;
         register_index < T64_RDRAM_PROTOCOL_REGISTER_COUNT;
         register_index++) {
      hash_u32(sink, device->register_values[register_index]);
    }
  }
}

static void hash_vr4300_cpu_state(t64_digest_sink *sink,
                                  const t64_cpu_state *cpu) {
  size_t i;

  hash_u64(sink, cpu->pc_full);
  hash_u64(sink, cpu->last_pc_full);
  hash_u64(sink, cpu->delay_slot_pc_full);
  hash_u64(sink, cpu->delay_target_full);
  hash_u64(sink, cpu->stop_pc_full);
  hash_u32(sink, cpu->attempt_outcome);
  hash_u32(sink, cpu->semantic_policy_version);
  hash_u32(sink, cpu->cp0_config_init_window ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, cpu->tlb_shutdown ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, cpu->pending_exception_valid ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, cpu->pending_exception_code);
  hash_u64(sink, cpu->pending_exception_address);
  hash_u64(sink, cpu->pending_exception_epc);
  hash_u32(sink, cpu->pending_exception_bd ? UINT32_C(1) : UINT32_C(0));

  hash_u32(sink, T64_VR4300_TLB_ENTRY_COUNT);
  for (i = 0u; i < T64_VR4300_TLB_ENTRY_COUNT; i++) {
    const t64_vr4300_tlb_pair *entry = &cpu->tlb[i];

    hash_u64(sink, entry->page_mask);
    hash_u64(sink, entry->vpn2);
    hash_u64(sink, entry->region);
    hash_u32(sink, entry->asid);
    hash_u64(sink, entry->entry_lo0);
    hash_u64(sink, entry->entry_lo1);
  }

  hash_u64(sink, cpu->count_sync_tick);
  hash_u32(sink, cpu->count_half_clock_phase);
  hash_u64(sink, cpu->count_compare_deadline);
  hash_u64(sink, cpu->count_equality_latch_generation);
  hash_u32(sink, cpu->count_equality_latched ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, cpu->linked_physical_address);
  hash_u32(sink, cpu->linked_width);
  hash_u32(sink, cpu->linked_valid ? UINT32_C(1) : UINT32_C(0));

  hash_u32(sink, T64_VR4300_ICACHE_LINE_COUNT);
  for (i = 0u; i < T64_VR4300_ICACHE_LINE_COUNT; i++) {
    const t64_vr4300_icache_line *line = &cpu->icache[i];

    hash_u32(sink, line->physical_tag);
    hash_u32(sink, line->valid ? UINT32_C(1) : UINT32_C(0));
    digest_bytes(sink, line->data, sizeof line->data);
  }
  hash_u32(sink, T64_VR4300_DCACHE_LINE_COUNT);
  for (i = 0u; i < T64_VR4300_DCACHE_LINE_COUNT; i++) {
    const t64_vr4300_dcache_line *line = &cpu->dcache[i];

    hash_u32(sink, line->physical_tag);
    hash_u32(sink, line->valid ? UINT32_C(1) : UINT32_C(0));
    hash_u32(sink, line->dirty ? UINT32_C(1) : UINT32_C(0));
    digest_bytes(sink, line->data, sizeof line->data);
  }
  hash_u32(sink, 32u);
  for (i = 0u; i < 32u; i++) {
    hash_u64(sink, cpu->fpr[i]);
  }
  hash_u32(sink, cpu->fcr0);
  hash_u32(sink, cpu->fcr31);
}

t64_status t64_digest_stream(const t64_machine *machine,
                             t64_digest_sink *sink) {
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
  static const uint8_t domain_v24[] = "t64.state.audio.ai.24";
  static const uint8_t domain_v25[] = "t64.state.si.25";
  static const uint8_t domain_v26[] = "t64.state.si.26";
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
  const bool has_v24_state = t64_has_ai_profile(machine);
  /* The two composite digest domains are exact tuples; the v25 framing (a
   * section id before each section, no packet identity strings, the new-span
   * tail) is shared by both. */
  const bool has_v25_state = t64_has_si_v25_profile(machine);
  const bool has_v26_state = t64_has_si_v26_profile(machine);
  const bool has_v25_framing = has_v25_state || has_v26_state;
  /* The depth block of the v21 body (descriptor 10 profiles). The v26 family
   * has depth active but keeps the v25 body: its depth scalars are new-span
   * members of the RDP block and its hidden table ends section 8. */
  const bool has_v21_body = has_v21_state && !has_v26_state;
  const bool has_audio_state = has_v24_state || has_v25_framing;
  const bool has_rdp_state = has_v16_state || has_v17_state ||
                             has_v18_state || has_v19_state || has_v20_state ||
                             has_v21_state || has_v22_state || has_v23_state;
  const bool has_rsp_state = has_v12_state || has_v13_state;
  const bool has_warm_state =
      has_v6_state || has_v7_state || has_v8_state || has_v9_state ||
      has_v10_state || has_v11_state || has_rsp_state;
  size_t i;

  if (machine == NULL || sink == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (has_v25_framing && machine->rdp_memory_coverage == NULL) {
    /*
     * Every family machine owns its coverage table from creation. A walker
     * that skipped the bytes would return a digest that no longer covers the
     * member plan 09-10 added, so the stream refuses instead (re-check R1
     * RC-01). An internal inconsistency: T64_STATUS_INTERNAL_DEFECT.
     */
    return t64_internal_defect(machine);
  }
  if (has_v26_state && machine->rdp_depth_hidden == NULL) {
    /* The hidden depth table is owned from creation on every descriptor 13
     * machine, and the v26 walker must not stream a missing table. */
    return t64_internal_defect(machine);
  }

  if (has_v26_state) {
    digest_bytes_class(sink, domain_v26, sizeof domain_v26 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v25_state) {
    digest_bytes_class(sink, domain_v25, sizeof domain_v25 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v24_state) {
    digest_bytes_class(sink, domain_v24, sizeof domain_v24 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v23_state) {
    digest_bytes_class(sink, domain_v23, sizeof domain_v23 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v22_state) {
    digest_bytes_class(sink, domain_v22, sizeof domain_v22 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v21_state) {
    digest_bytes_class(sink, domain_v21, sizeof domain_v21 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v20_state) {
    digest_bytes_class(sink, domain_v20, sizeof domain_v20 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v19_state) {
    digest_bytes_class(sink, domain_v19, sizeof domain_v19 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v18_state) {
    digest_bytes_class(sink, domain_v18, sizeof domain_v18 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v17_state) {
    digest_bytes_class(sink, domain_v17, sizeof domain_v17 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v16_state) {
    digest_bytes_class(sink, domain_v16, sizeof domain_v16 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v13_state) {
    digest_bytes_class(sink, domain_v13, sizeof domain_v13 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v12_state) {
    digest_bytes_class(sink, domain_v12, sizeof domain_v12 - 1u,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v11_state) {
    digest_bytes_class(sink, domain_v11, sizeof domain_v11,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v10_state) {
    digest_bytes_class(sink, domain_v10, sizeof domain_v10,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v9_state) {
    digest_bytes_class(sink, domain_v9, sizeof domain_v9,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v8_state) {
    digest_bytes_class(sink, domain_v8, sizeof domain_v8,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v7_state) {
    digest_bytes_class(sink, domain_v7, sizeof domain_v7,
                       T64_DIGEST_CLASS_IDENTITY);
  } else if (has_v6_state) {
    digest_bytes_class(sink, domain_v6, sizeof domain_v6,
                       T64_DIGEST_CLASS_IDENTITY);
  } else {
    digest_bytes_class(sink, domain_v5, sizeof domain_v5,
                       T64_DIGEST_CLASS_IDENTITY);
  }
  section_marker(sink, has_v25_framing, T64_STATE_SECTION_IDENTITY);
  hash_u32_class(sink, machine->profile.state_serialization_version,
                 T64_DIGEST_CLASS_IDENTITY);
  digest_bytes_class(sink, machine->profile_digest,
                     sizeof machine->profile_digest,
                     T64_DIGEST_CLASS_IDENTITY);
  digest_bytes(sink, machine->content_digest,
               sizeof machine->content_digest);
  hash_u32(sink, machine->has_image ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, (uint32_t)machine->image_length);
  digest_bytes(sink, machine->rom_header,
               sizeof machine->rom_header);
  digest_bytes(sink, machine->rom_title,
               sizeof machine->rom_title);
  digest_bytes(sink, machine->rom_game_code,
               sizeof machine->rom_game_code);
  hash_u32(sink, machine->rom_version);
  section_marker(sink, has_v25_framing, T64_STATE_SECTION_MACHINE_CORE);
  hash_u64(sink, machine->tick);
  hash_u64(sink, machine->instruction_count);
  hash_u64(sink, machine->frame_event_count);
  hash_u32(sink, machine->sticky_error_status);
  hash_u32(sink, machine->halted ? UINT32_C(1) : UINT32_C(0));
  section_marker(sink, has_v25_framing, T64_STATE_SECTION_RDRAM);
  hash_u32(sink, machine->profile.rdram_bytes);
  digest_bytes(sink, machine->ram, machine->profile.rdram_bytes);
  section_marker(sink, has_v25_framing, T64_STATE_SECTION_SP_MEMORY);
  hash_u32(sink, (uint32_t)sizeof machine->sp_dmem);
  digest_bytes(sink, machine->sp_dmem, sizeof machine->sp_dmem);
  hash_u32(sink, (uint32_t)sizeof machine->sp_imem);
  digest_bytes(sink, machine->sp_imem, sizeof machine->sp_imem);
  section_marker(sink, has_v25_framing, T64_STATE_SECTION_INTERRUPT_PI_SP);
  hash_u32(sink, machine->mi_pending & T64_MI_INTERRUPT_BITS);
  hash_u32(sink, machine->mi_mask & T64_MI_INTERRUPT_BITS);
  hash_u32(sink, machine->mi_irq_line ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, machine->pi.dram_address);
  hash_u32(sink, machine->pi.cart_address);
  hash_u32(sink, machine->pi.read_length);
  hash_u32(sink, machine->pi.write_length);
  hash_u32(sink, machine->pi.domain1_latency);
  hash_u32(sink, machine->pi.domain1_pulse_width);
  hash_u32(sink, machine->pi.domain1_page_size);
  hash_u32(sink, machine->pi.domain1_release_duration);
  hash_u32(sink, machine->pi.domain2_latency);
  hash_u32(sink, machine->pi.domain2_pulse_width);
  hash_u32(sink, machine->pi.domain2_page_size);
  hash_u32(sink, machine->pi.domain2_release_duration);
  hash_u32(sink, machine->pi.error ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, machine->pi.active_valid ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink,
           machine->pi.active_canceled ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, machine->pi.active_dram_address);
  hash_u32(sink, machine->pi.active_cart_address);
  hash_u32(sink, machine->pi.active_length);
  hash_u64(sink, machine->pi.active_token);
  hash_u64(sink, machine->pi.next_token);
  hash_u32(sink,
           machine->pi.token_exhausted ? UINT32_C(1) : UINT32_C(0));
  hash_u32(sink, machine->sp.mem_address);
  hash_u32(sink, machine->sp.dram_address);
  hash_u32(sink, machine->sp.read_length);
  hash_u32(sink, machine->sp.write_length);
  hash_sp_descriptor(sink, &machine->sp.active);
  hash_sp_descriptor(sink, &machine->sp.pending);
  hash_u64(sink, machine->sp.next_token);
  hash_u32(sink,
           machine->sp.token_exhausted ? UINT32_C(1) : UINT32_C(0));
  section_marker(sink, has_v25_framing, T64_STATE_SECTION_SCHEDULER);
  hash_u32(sink, (uint32_t)machine->queue_count);
  hash_u64(sink, machine->next_sequence);
  hash_u32(sink,
           machine->sequence_exhausted ? UINT32_C(1) : UINT32_C(0));
  for (i = 0u; i < machine->queue_count; i++) {
    size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;

    hash_event(sink, &machine->events[index]);
  }
  section_marker(sink, has_v25_framing, T64_STATE_SECTION_WARM_DEVICES);
  if (has_warm_state) {
    for (i = 0u; i < sizeof machine->boot.asset_sha256; i++) {
      digest_bytes(sink, &machine->boot.asset_sha256[i], 1u);
    }
    for (i = 0u; i < sizeof machine->boot.payload_sha256; i++) {
      digest_bytes(sink, &machine->boot.payload_sha256[i], 1u);
    }
    for (i = 0u; i < sizeof machine->boot.policy_sha256; i++) {
      digest_bytes(sink, &machine->boot.policy_sha256[i], 1u);
    }
    hash_u32(sink, machine->boot.source_offset);
    hash_u32(sink, machine->boot.payload_length);
    hash_u32(sink, machine->boot.dmem_offset);
    hash_u32(sink, machine->boot.entry_pc);
    hash_u32(sink, machine->boot.seed_reset_version);
    hash_u32(sink, machine->boot.seed_s3_rom_type);
    hash_u32(sink, machine->boot.seed_s4_tv_type);
    hash_u32(sink, machine->boot.seed_s5_reset_type);
    hash_u32(sink, machine->boot.seed_s6_entropy);
    hash_u32(sink, machine->boot.seed_s7_boot_info_version);
    hash_u32(sink, machine->boot.prepared ? UINT32_C(1) : UINT32_C(0));
    for (i = 0u; i < 32u; i++) {
      hash_u64(sink, machine->cpu.gpr[i]);
    }
    hash_u64(sink, machine->cpu.hi);
    hash_u64(sink, machine->cpu.lo);
    for (i = 0u; i < 32u; i++) {
      hash_u64(sink, machine->cpu.cp0[i]);
    }
    hash_u64(sink, machine->cpu.retired_instructions);
    hash_u32(sink, machine->cpu.pc);
    hash_u32(sink, machine->cpu.last_pc);
    hash_u32(sink, machine->cpu.last_opcode);
    hash_u32(sink, machine->cpu.delay_slot_pc);
    hash_u32(sink, machine->cpu.delay_target);
    hash_u32(sink,
             machine->cpu.delay_slot_pending ? UINT32_C(1) : UINT32_C(0));
    hash_u32(sink, machine->cpu.stopped ? UINT32_C(1) : UINT32_C(0));
    hash_u32(sink, machine->cpu.stop_reason);
    hash_u32(sink, machine->cpu.stop_pc);
    hash_u32(sink, machine->cpu.stop_opcode);
    if (has_v9_state || has_v10_state || has_v11_state || has_rsp_state) {
      hash_vr4300_cpu_state(sink, &machine->cpu);
      if (has_v10_state || has_v11_state || has_rsp_state) {
        hash_u64(sink, machine->cpu.delay_origin_pc_full);
      }
      if (has_v11_state || has_rsp_state) {
        hash_u64(sink, machine->cpu.cp2_transfer_latch);
      }
    }
    hash_u32(sink, machine->cache.policy_version);
    hash_u32(sink, machine->cache.last_operation);
    hash_u32(sink, machine->cache.last_pc);
    hash_u32(sink, machine->cache.last_address);
    hash_u32(sink,
             machine->cache.operation_seen ? UINT32_C(1) : UINT32_C(0));
    hash_u32(sink, machine->guest_devices.policy_version);
    hash_u32(sink, machine->guest_devices.mi_version);
    hash_u32(sink, machine->guest_devices.ri_select);
    hash_u32(sink, machine->guest_devices.mi_version_known
             ? UINT32_C(1)
             : UINT32_C(0));
    hash_u32(sink, machine->guest_devices.ri_select_known
             ? UINT32_C(1)
             : UINT32_C(0));
    hash_u32(sink, machine->output.policy_version);
    hash_u32(sink, machine->output.video_output_ready ? UINT32_C(1)
             : UINT32_C(0));
    hash_u32(sink, machine->output.audio_output_ready ? UINT32_C(1)
             : UINT32_C(0));
    if (has_v7_state || has_v8_state || has_v9_state || has_v10_state ||
        has_v11_state || has_rsp_state) {
      /* Retained guest output is future state; the host drain cursor is not. */
      hash_u32(sink, machine->output.magic_register);
      hash_u32(sink, machine->output.byte_count);
      hash_u32(sink, T64_ISVIEWER_BUFFER_BYTES);
      digest_bytes(sink, machine->output.buffer,
                   sizeof machine->output.buffer);
      hash_u32(sink, machine->output.captured_bytes);
      hash_u32(sink, T64_ISVIEWER_CAPTURE_BYTES);
      digest_bytes(sink, machine->output.captured,
                   sizeof machine->output.captured);
      hash_u32(sink,
               machine->output.lost_output ? UINT32_C(1) : UINT32_C(0));
      if (has_v8_state || has_v9_state || has_v10_state || has_v11_state ||
          has_rsp_state) {
        hash_u64(sink, machine->output.total_attempted_bytes);
        hash_u32(sink,
                 machine->output.total_attempted_bytes_saturated
                 ? UINT32_C(1)
                 : UINT32_C(0));
      }
    }
    if (has_rsp_state) {
      const t64_rsp_state *rsp = &machine->rsp;

      for (i = 0u; i < 32u; i++) {
        hash_u32(sink, rsp->gpr[i]);
      }
      digest_bytes(sink, (const uint8_t *)rsp->vector,
                   sizeof rsp->vector);
      for (i = 0u; i < 8u; i++) {
        hash_u64(sink, rsp->accumulator[i] & UINT64_C(0x0000ffffffffffff));
      }
      hash_u32(sink, rsp->vco);
      hash_u32(sink, rsp->vcc);
      hash_u32(sink, rsp->vce);
      hash_u32(sink, rsp->pc);
      hash_u32(sink, rsp->delay_target);
      hash_u32(sink, rsp->delay_slot_pending ? 1u : 0u);
      hash_u32(sink, rsp->branch_bubble_pending ? 1u : 0u);
      hash_u32(sink, rsp->divider_input);
      hash_u32(sink, rsp->divider_result);
      hash_u32(sink, rsp->divider_high_valid ? 1u : 0u);
      for (i = 0u; i < 32u; i++) {
        hash_u64(sink, rsp->scalar_ready_tick[i]);
        hash_u64(sink, rsp->vector_ready_tick[i]);
      }
      for (i = 0u; i < 8u; i++) {
        hash_u64(sink, rsp->accumulator_ready_tick[i]);
      }
      hash_u64(sink, rsp->memory_access_ready_tick);
      hash_u64(sink, rsp->next_issue_tick);
      hash_u64(sink, rsp->issue_sequence);
      hash_u64(sink, rsp->retired_instructions);
      hash_u64(sink, rsp->stall_slots);
      hash_u32(sink, rsp->sp_status);
      hash_u32(sink, rsp->sp_semaphore ? 1u : 0u);
      hash_u64(sink, rsp->dma_beat_cursor);
      hash_u64(sink, rsp->dma_setup_remaining);
      hash_u64(sink, rsp->active_dma_token);
      hash_u64(sink, rsp->pending_dma_token);
      hash_u64(sink, rsp->dma_event_sequence);
    }
    if (has_v13_state) {
      hash_rdram_protocol_state(
          sink, &machine->guest_devices.rdram_protocol);
    }
    if (has_rdp_state) {
      const t64_rdp_state *rdp = &machine->rdp;
      const t64_vi_state *vi = &machine->vi;
      size_t logical;

      section_marker(sink, has_v25_framing, T64_STATE_SECTION_RDP_VI);
      hash_u32(sink, rdp->dpc_start);
      hash_u32(sink, rdp->dpc_end);
      hash_u32(sink, rdp->dpc_current);
      hash_u32(sink, rdp->dpc_status);
      hash_u32(sink, rdp->dpc_clock_counter);
      hash_u32(sink, rdp->dpc_buffer_busy_counter);
      hash_u32(sink, rdp->dpc_pipe_busy_counter);
      hash_u32(sink, rdp->dpc_tmem_counter);
      hash_u32(sink, rdp->pending_command_address);
      hash_u32(sink, rdp->pending_command_byte);
      hash_u32(sink, rdp->pending_opcode_field);
      hash_u64(sink, rdp->pending_command_word);
      hash_u64(sink, rdp->fence_generation);
      hash_u32(sink, rdp->color_image_address);
      hash_u32(sink, rdp->color_image_width);
      hash_u32(sink, rdp->color_image_format);
      hash_u32(sink, rdp->color_image_size);
      hash_u32(sink, rdp->scissor_xh);
      hash_u32(sink, rdp->scissor_yh);
      hash_u32(sink, rdp->scissor_xl);
      hash_u32(sink, rdp->scissor_yl);
      hash_u64(sink, rdp->other_modes);
      hash_u32(sink, rdp->fill_color);
      hash_u32(sink, rdp->texture_image_address);
      hash_u32(sink, rdp->texture_image_width);
      hash_u32(sink, rdp->texture_image_format);
      hash_u32(sink, rdp->texture_image_size);
      hash_u32(sink, rdp->texture_image_valid);
      hash_u32(sink, rdp->tlut_format);
      hash_u32(sink, rdp->tlut_valid ? 1u : 0u);
      hash_u32(sink, rdp->render_kind);
      hash_u32(sink, rdp->render_texture_tile);
      hash_u32(sink, (uint32_t)rdp->render_texture_s);
      hash_u32(sink, (uint32_t)rdp->render_texture_t);
      hash_u32(sink, (uint32_t)rdp->render_texture_dsdx);
      hash_u32(sink, (uint32_t)rdp->render_texture_dtdy);
      hash_u64(sink, rdp->copy_count);
      for (i = 0u; i < 6u; i++)
        hash_u32(sink, rdp->convert_coefficients[i]);
      hash_u64(sink, rdp->pending_texrect_word);
      if (has_v17_state || has_v18_state || has_v19_state || has_v20_state ||
          has_v21_state) {
        size_t packet_index;
        hash_u32(sink, rdp->pending_triangle_word_count);
        for (packet_index = 0u;
             packet_index < rdp->pending_triangle_word_count &&
             packet_index < T64_RDP_TRIANGLE_MAX_WORDS;
             packet_index++) {
          hash_u64(sink, rdp->pending_triangle_words[packet_index]);
        }
        hash_u32(sink, rdp->primitive_color);
        hash_u32(sink, rdp->primitive_color_valid ? 1u : 0u);
        hash_u32(sink, rdp->triangle_count);
        hash_u32(sink, rdp->render_triangle ? 1u : 0u);
        hash_u32(sink, rdp->shade_triangle ? 1u : 0u);
        hash_u32(sink, T64_RDP_TRIANGLE_COVERAGE_WIDTH);
        hash_u32(sink, T64_RDP_TRIANGLE_COVERAGE_HEIGHT);
        digest_bytes(sink, machine->rdp_triangle_coverage,
                     T64_RDP_TRIANGLE_COVERAGE_BYTES);
      }
      if (has_v20_state || has_v21_body) {
        hash_u32(sink,
                 (uint32_t)T64_RDP_MEMORY_COVERAGE_BYTES(
                 machine->profile.rdram_bytes));
        digest_bytes(sink, machine->rdp_memory_coverage,
                     T64_RDP_MEMORY_COVERAGE_BYTES(
                     machine->profile.rdram_bytes));
      }
      if (has_v19_state || has_v20_state || has_v21_body) {
        hash_u32(sink, rdp->primitive_lod_fraction);
        hash_u32(sink, rdp->environment_color);
        hash_u32(sink, rdp->fog_color);
        hash_u32(sink, rdp->blend_color);
        hash_u64(sink, rdp->combine_mode);
        hash_u32(sink, rdp->combine_mode_valid ? 1u : 0u);
        hash_u32(sink, rdp->environment_color_valid ? 1u : 0u);
        hash_u32(sink, rdp->fog_color_valid ? 1u : 0u);
        hash_u32(sink, rdp->blend_color_valid ? 1u : 0u);
        hash_u32(sink, rdp->key_center_valid ? 1u : 0u);
        hash_u32(sink, rdp->key_scale_valid ? 1u : 0u);
        hash_u32(sink, rdp->key_width_valid ? 1u : 0u);
        for (i = 0u; i < 3u; i++) {
          hash_u32(sink, rdp->key_center[i]);
          hash_u32(sink, rdp->key_scale[i]);
          hash_u32(sink, rdp->key_width[i]);
        }
      }
      if (has_v21_body) {
        hash_u32(sink, rdp->depth_image_address);
        hash_u32(sink, rdp->depth_image_valid ? 1u : 0u);
        hash_u32(sink, rdp->primitive_z);
        hash_u32(sink, rdp->primitive_dz);
        hash_u32(sink, rdp->primitive_depth_valid ? 1u : 0u);
        hash_u32(sink,
                 (uint32_t)T64_RDP_DEPTH_HIDDEN_BYTES(
                 machine->profile.rdram_bytes));
        digest_bytes(sink, machine->rdp_depth_hidden,
                     T64_RDP_DEPTH_HIDDEN_BYTES(
                     machine->profile.rdram_bytes));
      } else if (has_v26_state) {
        /*
         * State 26 (plan 10-09): the five depth scalars stay inside the RDP
         * block at the fixed v21 position, before work_policy_version, so an
         * importer proves a Z triangle in flight with them before the tail.
         * They are new-span members: the stripped v26 stream equals the
         * stripped v25 stream.
         */
        hash_u32_new(sink, rdp->depth_image_address);
        hash_u32_new(sink, rdp->depth_image_valid ? 1u : 0u);
        hash_u32_new(sink, rdp->primitive_z);
        hash_u32_new(sink, rdp->primitive_dz);
        hash_u32_new(sink, rdp->primitive_depth_valid ? 1u : 0u);
      }
      hash_u32(sink, rdp->work_policy_version);
      hash_u32(sink, rdp->random_state);
      hash_u32(sink, rdp->refusal_reason);
      hash_u64(sink, rdp->command_count);
      hash_u64(sink, rdp->fill_count);
      hash_u64(sink, rdp->fullsync_count);
      hash_u64(sink, rdp->last_original_word);
      hash_u32(sink, rdp->last_command_byte);
      hash_u32(sink, rdp->last_opcode_field);
      hash_u32(sink, rdp->render_x_begin);
      hash_u32(sink, rdp->render_x_end);
      hash_u32(sink, rdp->render_y_begin);
      hash_u32(sink, rdp->render_y_end);
      hash_u32(sink, rdp->render_y);
      hash_u32(sink, rdp->color_image_valid ? 1u : 0u);
      hash_u32(sink, rdp->scissor_valid ? 1u : 0u);
      hash_u32(sink, rdp->other_modes_valid ? 1u : 0u);
      hash_u32(sink, rdp->fill_color_valid ? 1u : 0u);
      hash_u32(sink, rdp->render_active ? 1u : 0u);
      hash_u32(sink, rdp->fence_pending ? 1u : 0u);
      hash_u32(sink, rdp->busy ? 1u : 0u);
      hash_u32(sink, rdp->dpc_xbus ? 1u : 0u);
      hash_u32(sink, rdp->dpc_freeze ? 1u : 0u);
      hash_u32(sink, rdp->dpc_flush ? 1u : 0u);
      hash_u32(sink, rdp->dpc_start_valid ? 1u : 0u);
      hash_u32(sink, rdp->dpc_end_valid ? 1u : 0u);
      hash_u32(sink, rdp->command_pending ? 1u : 0u);
      hash_u32(sink, rdp->texrect_pending ? 1u : 0u);
      hash_u32(sink, rdp->render_texture_flip ? 1u : 0u);

      digest_bytes(sink, rdp->tmem, sizeof rdp->tmem);
      for (i = 0u; i < T64_RDP_TILE_COUNT; i++) {
        const t64_rdp_tile *tile = &rdp->tiles[i];
        hash_u32(sink, tile->format);
        hash_u32(sink, tile->size);
        hash_u32(sink, tile->line_words);
        hash_u32(sink, tile->tmem_word);
        hash_u32(sink, tile->palette);
        hash_u32(sink, tile->mask_s);
        hash_u32(sink, tile->mask_t);
        hash_u32(sink, tile->shift_s);
        hash_u32(sink, tile->shift_t);
        hash_u32(sink, tile->uls);
        hash_u32(sink, tile->ult);
        hash_u32(sink, tile->lrs);
        hash_u32(sink, tile->lrt);
        hash_u32(sink, tile->clamp_s ? 1u : 0u);
        hash_u32(sink, tile->mirror_s ? 1u : 0u);
        hash_u32(sink, tile->clamp_t ? 1u : 0u);
        hash_u32(sink, tile->mirror_t ? 1u : 0u);
        hash_u32(sink, tile->valid ? 1u : 0u);
        hash_u32(sink, tile->size_valid ? 1u : 0u);
      }

      for (i = 0u; i < sizeof vi->registers / sizeof vi->registers[0];
           i++) {
        hash_u32(sink, vi->registers[i]);
      }
      hash_u64(sink, vi->phase_remainder);
      hash_u64(sink, vi->phase_cycles);
      hash_u64(sink, vi->last_guest_tick);
      hash_u64(sink, vi->next_sequence);
      hash_u64(sink, vi->event_generation);
      hash_u32(sink, vi->field);
      hash_u32(sink, vi->refusal_reason);
      hash_u32(sink, vi->enabled ? 1u : 0u);
      hash_u32(sink, vi->sequence_exhausted ? 1u : 0u);
      if (has_v22_state || has_v23_state) {
        for (i = 0u; i < sizeof vi->active_registers /
                            sizeof vi->active_registers[0];
             i++) {
          hash_u32(sink, vi->active_registers[i]);
        }
        hash_u64(sink, vi->phase_units);
        hash_u64(sink, vi->next_boundary_units);
        hash_u64(sink, vi->field_start_tick);
        hash_u32(sink, vi->current_readback);
        hash_u32(sink, vi->latch_pending ? 1u : 0u);
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

        hash_u64(sink, length);
        if (machine->video_packets != NULL &&
            length <= T64_VIDEO_MAX_PACKET_BYTES) {
          digest_bytes(sink,
                       machine->video_packets +
                       slot * T64_VIDEO_MAX_PACKET_BYTES,
                       length);
        }
        hash_u32(sink, info->width);
        hash_u32(sink, info->height);
        hash_u32(sink, info->stride_bytes);
        hash_u32(sink, info->source_address);
        hash_u32(sink, info->source_format);
        hash_u32(sink, info->output_encoding);
        hash_u32(sink, info->field);
        hash_u32(sink, info->parity);
        hash_u32(sink, info->control);
        hash_u32(sink, info->burst);
        hash_u32(sink, info->h_sync);
        hash_u32(sink, info->v_sync);
        hash_u32(sink, info->h_start);
        hash_u32(sink, info->v_start);
        hash_u32(sink, info->x_scale);
        hash_u32(sink, info->y_scale);
        hash_u64(sink, info->start_tick);
        hash_u64(sink, info->end_tick);
        hash_u64(sink, info->sequence);
        hash_u64(sink, info->commands_completed);
        hash_u64(sink, info->fullsync_completed);
        hash_u64(sink, info->last_command_word);
        hash_u32(sink, info->last_command_byte);
        hash_u32(sink, info->last_opcode_field);
        hash_u64(sink, info->source_bytes);
        hash_u64(sink, info->packet_bytes);
        if (!has_v25_framing) {
          digest_bytes_class(sink, (const uint8_t *)info->profile_id,
                             sizeof info->profile_id,
                             T64_DIGEST_CLASS_IDENTITY);
          digest_bytes_class(sink, (const uint8_t *)info->build_id,
                             sizeof info->build_id,
                             T64_DIGEST_CLASS_IDENTITY);
        }
        digest_bytes(sink, (const uint8_t *)info->vi_clock_policy,
                     sizeof info->vi_clock_policy);
        if (has_v22_state || has_v23_state) {
          hash_u32(sink, info->h_end);
          hash_u32(sink, info->v_end);
          hash_u32(sink, info->source_width);
          hash_u32(sink, info->source_pitch_bytes);
          hash_u32(sink, info->source_x_offset);
          hash_u32(sink, info->source_y_offset);
          hash_u32(sink, info->filter);
          digest_bytes(sink,
                       (const uint8_t *)info->vi_output_policy,
                       sizeof info->vi_output_policy);
        }
      }
      if (has_v25_framing) {
        /*
         * The RDP members the composite family computes with and the older
         * v24 walker never hashed (plan 09-10, STATE-CONTENT-F1): the combiner
         * block (the members the v19 to v21 walkers hash) and the memory
         * coverage table. They end section 8 in the new-span class, so the
         * stripped v25 stream still equals the v24 stream (decision D-05) and
         * the state of the 17 older profiles is untouched. The depth members
         * are excluded on descriptor 12 (K1: depth is inactive there) and
         * streamed on descriptor 13 (the two v26 blocks); the diagnostic
         * last-coverage members are test observation state.
         */
        hash_u32_new(sink, rdp->primitive_lod_fraction);
        hash_u32_new(sink, rdp->environment_color);
        hash_u32_new(sink, rdp->fog_color);
        hash_u32_new(sink, rdp->blend_color);
        hash_u64_new(sink, rdp->combine_mode);
        hash_u32_new(sink, rdp->combine_mode_valid ? 1u : 0u);
        hash_u32_new(sink, rdp->environment_color_valid ? 1u : 0u);
        hash_u32_new(sink, rdp->fog_color_valid ? 1u : 0u);
        hash_u32_new(sink, rdp->blend_color_valid ? 1u : 0u);
        hash_u32_new(sink, rdp->key_center_valid ? 1u : 0u);
        hash_u32_new(sink, rdp->key_scale_valid ? 1u : 0u);
        hash_u32_new(sink, rdp->key_width_valid ? 1u : 0u);
        for (i = 0u; i < 3u; i++) {
          hash_u32_new(sink, rdp->key_center[i]);
          hash_u32_new(sink, rdp->key_scale[i]);
          hash_u32_new(sink, rdp->key_width[i]);
        }
        hash_u32_new(sink,
                     (uint32_t)T64_RDP_MEMORY_COVERAGE_BYTES(
                     machine->profile.rdram_bytes));
        digest_bytes_class(sink, machine->rdp_memory_coverage,
                           T64_RDP_MEMORY_COVERAGE_BYTES(
                           machine->profile.rdram_bytes),
                           T64_DIGEST_CLASS_NEW);
        if (has_v26_state) {
          /* The hidden depth table follows the coverage table (state 26). */
          hash_u32_new(sink,
                       (uint32_t)T64_RDP_DEPTH_HIDDEN_BYTES(
                       machine->profile.rdram_bytes));
          digest_bytes_class(sink, machine->rdp_depth_hidden,
                             T64_RDP_DEPTH_HIDDEN_BYTES(
                             machine->profile.rdram_bytes),
                             T64_DIGEST_CLASS_NEW);
        }
      }
    }
  }
  if (has_audio_state) {
    const t64_ai_state *ai = &machine->ai;
    size_t logical;

    section_marker(sink, has_v25_framing, T64_STATE_SECTION_AUDIO);
    hash_u32(sink, ai->staged_address);
    hash_u32(sink, ai->address_staged ? 1u : 0u);
    hash_u32(sink, ai->control);
    hash_u32(sink, ai->dacrate);
    hash_u32(sink, ai->bitrate);
    hash_u32(sink, ai->latched_dacrate);
    hash_u32(sink, ai->latched_bitrate);
    hash_u64(sink, ai->phase_remainder);
    hash_u32(sink, ai->active_byte_cursor);
    hash_u64(sink, ai->sample_index);
    hash_u64(sink, ai->descriptor_sequence);
    hash_u64(sink, ai->event_token);
    hash_u64(sink, ai->event_generation);
    hash_u64(sink, ai->packet_start_tick);
    hash_u64(sink, ai->packet_first_sample);
    hash_u64(sink, (uint64_t)ai->packet_length);
    hash_u32(sink, (uint32_t)ai->descriptor_count);
    hash_u32(sink, ai->active ? 1u : 0u);
    hash_u32(sink, ai->paused ? 1u : 0u);
    hash_u64(sink, ai->paused_remaining_ticks);
    hash_u32(sink, ai->packet_reserved ? 1u : 0u);
    hash_u32(sink, (uint32_t)ai->output_reservation_slot);
    hash_u32(sink, ai->sequence_exhausted ? 1u : 0u);
    for (logical = 0u; logical < ai->descriptor_count; logical++) {
      size_t slot = (ai->descriptor_head + logical) %
                    T64_AUDIO_QUEUE_CAPACITY;
      const t64_ai_dma_descriptor *descriptor = &ai->descriptors[slot];

      hash_u32(sink, descriptor->address);
      hash_u32(sink, descriptor->length);
      hash_u64(sink, descriptor->sequence);
      hash_u32(sink, descriptor->dacrate);
      hash_u32(sink, descriptor->bitrate);
    }
    hash_u64(sink, ai->packet_history_count);
    digest_bytes(sink, ai->packet_history_sha256,
                 sizeof ai->packet_history_sha256);
    for (logical = 0u; logical < T64_AUDIO_QUEUE_CAPACITY; logical++) {
      const t64_audio_packet_info *info = &ai->packet_info[logical];
      size_t length = ai->packet_lengths[logical];

      hash_u64(sink, (uint64_t)length);
      if (machine->audio_packets != NULL &&
          length <= T64_AUDIO_MAX_PACKET_BYTES) {
        digest_bytes(sink,
                     machine->audio_packets +
                     logical * T64_AUDIO_MAX_PACKET_BYTES,
                     length);
      }
      hash_u32(sink, info->struct_size);
      hash_u64(sink, info->sequence);
      hash_u64(sink, info->first_sample_index);
      hash_u64(sink, info->first_tick);
      hash_u64(sink, info->last_tick);
      hash_u32(sink, info->frame_count);
      hash_u64(sink, info->rate_numerator);
      hash_u64(sink, info->rate_denominator);
      hash_u32(sink, info->channels);
      hash_u32(sink, info->bits_per_sample);
      hash_u32(sink, info->encoding);
      hash_u64(sink, (uint64_t)info->byte_length);
      if (!has_v25_framing) {
        digest_bytes_class(sink, (const uint8_t *)info->profile_id,
                           sizeof info->profile_id,
                           T64_DIGEST_CLASS_IDENTITY);
        digest_bytes_class(sink, (const uint8_t *)info->build_id,
                           sizeof info->build_id,
                           T64_DIGEST_CLASS_IDENTITY);
      }
    }
  }
  if (has_v25_framing) {
    const t64_si_state *si = &machine->si;
    size_t index;

    section_marker(sink, true, T64_STATE_SECTION_SI_PIF);
    hash_u32_new(sink, si->dram_address);
    hash_u32_new(sink, si->error ? 1u : 0u);
    hash_u32_new(sink, si->active_valid ? 1u : 0u);
    hash_u32_new(sink, si->active_direction);
    hash_u32_new(sink, si->active_dram_address);
    hash_u64_new(sink, si->start_tick);
    hash_u64_new(sink, si->execute_tick);
    hash_u64_new(sink, si->complete_tick);
    hash_u64_new(sink, si->active_token);
    hash_u64_new(sink, si->next_token);
    hash_u32_new(sink, si->token_exhausted ? 1u : 0u);
    hash_u32_new(sink, si->execute_pending ? 1u : 0u);
    digest_bytes_class(sink, si->pif_ram, sizeof si->pif_ram,
                       T64_DIGEST_CLASS_NEW);
    hash_u32_new(sink, si->table.parsed ? 1u : 0u);
    hash_u32_new(sink, si->table.entry_count);
    for (index = 0u; index < T64_JOYBUS_CHANNELS; index++) {
      const t64_joybus_entry *entry = &si->table.entries[index];

      hash_u32_new(sink, entry->kind);
      hash_u32_new(sink, entry->channel);
      hash_u32_new(sink, entry->offset);
      hash_u32_new(sink, entry->tx_count);
      hash_u32_new(sink, entry->rx_count);
      hash_u32_new(sink, entry->flags);
    }
    for (index = 0u; index < T64_SI_PORTS; index++) {
      const t64_si_port_latch *latch = &si->latch[index];

      hash_u32_new(sink, latch->current_buttons);
      hash_u32_new(sink, latch->current_x);
      hash_u32_new(sink, latch->current_y);
      hash_u32_new(sink, latch->next_buttons);
      hash_u32_new(sink, latch->next_x);
      hash_u32_new(sink, latch->next_y);
      hash_u64_new(sink, latch->next_visible_from);
      hash_u32_new(sink, latch->next_valid ? 1u : 0u);
    }
    /* Section 11 exists for the three save ids only, so the stem stream keeps
     * markers 1 to 10. Every field and the raw storage are in the new-span
     * class. */
    if (machine->profile.save_kind != 0u) {
      const size_t save_size =
          t64_save_storage_bytes(machine->profile.save_kind);

      section_marker(sink, true, T64_STATE_SECTION_SAVE_MEDIA);
      hash_u32_new(sink, machine->profile.save_kind);
      hash_u32_new(sink, (uint32_t)save_size);
      hash_u64_new(sink, machine->save.busy_until);
      /* The active PI save direction: 0 when none or ROM to RDRAM, 1 SRAM to
       * RDRAM, 2 RDRAM to SRAM. */
      hash_u32_new(sink, machine->pi.active_save_direction);
      if (machine->save.bytes != NULL) {
        digest_bytes_class(sink, machine->save.bytes, save_size,
                           T64_DIGEST_CLASS_NEW);
      }
    }
  }
  return T64_STATUS_OK;
}

t64_status t64_machine_digest(const t64_machine *machine,
                              uint8_t digest_out[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;
  t64_digest_sink sink;
  t64_address_range output_range;
  t64_status status;

  T64_ENTRY_GUARD(machine);
  if (machine == NULL || digest_out == NULL ||
      !t64_internal_make_range(digest_out, T64_DIGEST_SIZE, &output_range) ||
      !t64_internal_observation_range_is_disjoint(machine, output_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  t64_sha256_init(&context);
  (void)memset(&sink, 0, sizeof sink);
  sink.hash = &context;
  status = t64_digest_stream(machine, &sink);
  if (status != T64_STATUS_OK) {
    return status;
  }
  t64_sha256_final(&context, digest_out);
  return T64_STATUS_OK;
}

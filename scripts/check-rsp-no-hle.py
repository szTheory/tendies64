#!/usr/bin/env python3
"""Check the canonical RSP fetched-instruction route and production objects."""

from __future__ import annotations

import argparse
import collections
import hashlib
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys


CANONICAL_CORE_SOURCES = frozenset(
    {
        "t64_bus.c",
        "t64_cache.c",
        "t64_cop1.c",
        "t64_core_stub.c",
        "t64_cp0.c",
        "t64_cpu.c",
        "t64_digest.c",
        "t64_exception.c",
        "t64_isviewer.c",
        "t64_machine.c",
        "t64_memory.c",
        "t64_mi.c",
        "t64_mmu.c",
        "t64_observe.c",
        "t64_pi.c",
        "t64_rdp.c",
        "t64_rdp_depth.c",
        "t64_rdp_pipeline.c",
        "t64_rdp_raster.c",
        "t64_rdp_tmem.c",
        "t64_rom.c",
        "t64_rdram.c",
        "t64_rsp.c",
        "t64_rsp_divider.c",
        "t64_scheduler.c",
        "t64_softfloat.c",
        "t64_sp.c",
        "t64_trace.c",
        "t64_vi.c",
        "t64_warm_devices.c",
    }
)
CANONICAL_SOFTFLOAT_SOURCES = frozenset(
    {
        "f32_add.c", "f32_div.c", "f32_eq.c", "f32_le.c", "f32_lt.c",
        "f32_mul.c", "f32_sqrt.c", "f32_sub.c", "f32_to_f64.c",
        "f32_to_i32.c", "f32_to_i64.c", "f64_add.c", "f64_div.c",
        "f64_eq.c", "f64_le.c", "f64_lt.c", "f64_mul.c", "f64_sqrt.c",
        "f64_sub.c", "f64_to_f32.c", "f64_to_i32.c", "f64_to_i64.c",
        "i32_to_f32.c", "i32_to_f64.c", "i64_to_f32.c", "i64_to_f64.c",
        "s_addMagsF32.c", "s_addMagsF64.c", "s_approxRecip32_1.c",
        "s_approxRecipSqrt32_1.c", "s_approxRecipSqrt_1Ks.c",
        "s_approxRecip_1Ks.c", "s_countLeadingZeros8.c",
        "s_countLeadingZeros32.c", "s_countLeadingZeros64.c",
        "s_mul64To128.c", "s_normRoundPackToF32.c", "s_normRoundPackToF64.c",
        "s_normSubnormalF32Sig.c", "s_normSubnormalF64Sig.c",
        "s_roundPackToF32.c", "s_roundPackToF64.c", "s_roundToI32.c",
        "s_roundToI64.c", "s_shiftRightJam32.c", "s_shiftRightJam64.c",
        "s_shiftRightJam64Extra.c", "s_shortShiftRightJam64.c",
        "s_subMagsF32.c", "s_subMagsF64.c",
    }
)


# Every file-scope static helper in t64_rsp.c is part of the execution inventory,
# including pure arithmetic and byte helpers whose outputs feed decode effects.
# This is a bounded normalized-source guard, not proof of arbitrary C behavior.
REVIEWED_RSP_STATIC_FUNCTIONS = frozenset(
    {
        "t64_rsp_issue_event",
        "t64_rsp_schedule_next",
        "rsp_read_imem_word",
        "rsp_read_vector_element",
        "rsp_write_vector_element",
        "rsp_signed16",
        "rsp_sign_extend16",
        "rsp_signed_less32",
        "rsp_arithmetic_shift_right32",
        "rsp_select_vector_lane",
        "rsp_vector_memory_add_address",
        "rsp_vector_memory_add_load_byte",
        "rsp_vector_memory_add_store_byte",
        "rsp_plan_vector_memory",
        "rsp_vector_memory_loaded_byte",
        "rsp_ready_tick_after",
        "rsp_decode_vadd",
        "rsp_commit_vadd",
        "rsp_decode_vector_basic",
        "rsp_commit_vector_effect",
        "rsp_decode_divider",
        "rsp_signed48",
        "rsp_clamp_signed_accumulator",
        "rsp_clamp_unsigned_accumulator",
        "rsp_decode_vector_multiply",
        "rsp_decode_vector_compare",
    }
)
REVIEWED_SP_DMA_API_FUNCTIONS = frozenset(
    {
        "t64_sp_rsp_dma_active",
        "t64_sp_rsp_dma_admission_preflight",
        "t64_sp_rsp_dma_admission_commit",
        "t64_sp_rsp_dma_admission_refuse",
        "t64_sp_rsp_dma_preflight",
        "t64_sp_rsp_dma_commit",
    }
)
REVIEWED_SP_DMA_HELPER_FUNCTIONS = frozenset(
    {
        "warm_zero_source_sp_transfer",
        "dma_busy",
        "dma_full",
        "trace_refusal",
        "refuse",
        "decode_descriptor",
        "rsp_pc_overlaps_dma_beat",
    }
)
REVIEWED_SP_STATUS_FUNCTIONS = frozenset(
    {"t64_rsp_validate_status_command", "t64_sp_rsp_guest_status_write"}
)
REVIEWED_SP_FUNCTIONS = (
    REVIEWED_SP_DMA_API_FUNCTIONS
    | REVIEWED_SP_DMA_HELPER_FUNCTIONS
    | REVIEWED_SP_STATUS_FUNCTIONS
    | frozenset(
        {
            "start_dma", "start_rsp_dma", "write_rsp_status",
            "t64_sp_complete", "t64_sp_peek_register", "t64_sp_read_register",
            "t64_sp_reset_machine_state", "t64_sp_write_register",
        }
    )
)

REVIEWED_RDP_FUNCTIONS = frozenset(
    {
        "apply_pending_command", "command_span_valid", "dpc_status_value",
        "expand5_u8",
        "fill_bounds", "fill_one_row", "finish_if_idle", "handle_row",
        "increment_counter", "rdp_event_queued", "read_be64", "record_command",
        "schedule_next_committed", "schedule_work_after_committed", "set_refusal",
        "supported_opcode", "t64_rdp_event", "t64_rdp_read_register",
        "t64_rdp_reset_machine_state", "t64_rdp_write_register",
        "texture_rect_bounds", "validate_fill_store", "write_be16", "write_be32",
    }
)
REVIEWED_RDP_TMEM_FUNCTIONS = frozenset(
    {
        "apply_load_block", "apply_load_tile", "apply_load_tlut",
        "average_four_texels", "bits_per_texel", "channel5", "clamp_u8",
        "coeff", "command_tile", "field12", "floor_div_i64",
        "floor_div_pow2", "format_size_supported", "load_source_span_valid",
        "average_four_rgba8", "expand5_rgba8", "map_triangle_index",
        "normalize_triangle_axis", "pack_rgba8", "positive_mod_i64",
        "read_tile_texel", "read_tile_texel_rgba8", "read_triangle_neighbor",
        "read_triangle_neighbor_rgba8", "rgba5551", "rgba8_channel",
        "sample_texel", "sign_extend16", "signed9", "source_byte_address", "source_texel",
        "three_point_rgba8", "three_point_texels",
        "t64_rdp_tmem_apply_command", "t64_rdp_tmem_begin_copy",
        "t64_rdp_tmem_convert_yuv", "t64_rdp_tmem_copy_pixel",
        "t64_rdp_tmem_handles_opcode", "t64_rdp_tmem_validate_command",
        "t64_rdp_tmem_triangle_pixel", "t64_rdp_tmem_triangle_pixel_rgba8",
        "t64_rdp_tmem_validate_copy",
        "t64_rdp_tmem_validate_triangle", "tmem_base", "tmem_read_bits",
        "tmem_store", "wrap_coordinate",
    }
)
REVIEWED_RDP_RASTER_FUNCTIONS = frozenset(
    {
        "alpha_dither_bias", "blend_texture_levels",
        "color_dither_bias", "decode_edges", "floor_div_i64",
        "magnitude_i64", "modulate_texture_shade", "pair_q16",
        "project_texture_q16", "rgba5551", "select_texture_lod",
        "t64_rdp_quantize_alpha5_v6", "t64_rdp_quantize_rgba5551_v6",
        "shade_channel", "shade_products_are_bounded", "texture_channel5",
        "texture_channel_q16", "texture_products_are_bounded",
        "texture_values_at",
        "sign_extend_u14", "signed_u32",
        "t64_rdp_triangle_bounds", "t64_rdp_triangle_render_row",
        "t64_rdp_triangle_word_count",
    }
)
REVIEWED_RDP_PIPELINE_FUNCTIONS = frozenset(
    {
        "alpha_source", "blend_alpha_factor", "blend_b_factor",
        "blend_coverage_destination", "blend_rgb_source", "clamp_u8",
        "color_channel", "combine_channel", "combine_cycle", "coverage_entry",
        "floor_div_255", "next_noise", "read_blend_memory", "rgb_source",
        "signed9", "t64_rdp_blend_pixel", "t64_rdp_combine_pixel",
        "t64_rdp_memory_coverage_invalidate", "t64_rdp_memory_coverage_read",
        "t64_rdp_memory_coverage_write",
    }
)
REVIEWED_RDP_DEPTH_FUNCTIONS = frozenset(
    {
        "depth_word_read", "floor_div_i64", "is_z_triangle", "magnitude_i64",
        "pair_from_word", "signed_i32", "signed_pair_q16", "z_compress",
        "z_decompress", "z_delta_decode", "z_delta_priority_code",
        "t64_rdp_depth_commit_pixel", "t64_rdp_depth_evaluate_pixel",
        "t64_rdp_depth_triangle_preflight",
    }
)
REVIEWED_VI_FUNCTIONS = frozenset(
    {
        "copy_text", "delay_for_cycles", "expand5", "geometry_for",
        "phase_at_tick", "vi_read_source_pixel", "vi_manhattan_distance",
        "vi_filter_source_pixel", "vi_divot_sample", "vi_median_channel",
        "vi_gamma_channel", "vi_gamma_adjust", "render_filter_packet",
        "render_packet", "t64_vi_event", "vi_v8_event",
        "t64_vi_read_register", "t64_vi_reset_machine_state",
        "t64_vi_write_register", "t64_video_copy", "t64_video_info",
    }
)

# Reviewed normalized function bodies. A changed body requires an explicit
# review and digest refresh; static definition additions/removals also fail the
# inventory check below.
REVIEWED_BODY_DIGESTS = {
    "core/t64_bus.c :: decode_region": "a7d3761fa6daa231b11e82c1c78f8e0f3968d629f7c094f6b6f3446d5f31f916",
    "core/t64_bus.c :: t64_bus_write": "c4981198e735c1125ced3607eaded936ace5919054d02834a52dee8881e98a1d",
    "core/t64_bus.c :: t64_bus_write_masked": "2c219bc6f9131c133dabee4f60d9e5a18bbcda8c8443bb6b7418b423f6ae2f82",
    "core/t64_bus.c :: t64_bus_writeback_cache_line": "00b198913012b79e8ff87fdc40822b2096e7d6f1ba9d9f8033f6e468e0621e80",
    "core/t64_pi.c :: t64_pi_complete": "7edd3c4b64276a9b6698cb13f16b91a183f400cee26347b350b49bccb69e36af",
    "core/t64_sp.c :: t64_sp_write_register": "3620ac7c6c379837ebdedcfcf21ca6c887e580880e314311633b08698f75ce09",
    "core/t64_sp.c :: t64_sp_reset_machine_state": "f410fa77785c8b063cde69306ced58702d70dae4c5bca62af2b8adf08529670a",
    "core/t64_sp.c :: t64_sp_read_register": "495be610b9df612b2637f74356824f1a4c6970981041d87dfb666a2f56946e5e",
    "core/t64_sp.c :: t64_sp_peek_register": "8e061feec47e6809739915b64f8e832cba96a8705521eb7b6e2a0cea2d7d4e5d",
    "core/t64_sp.c :: t64_sp_complete": "205cfdf05cdf127cbdf1a51c7dac2fc4a35972889260913a8b54768ef1ead9d9",
    "core/t64_sp.c :: start_dma": "e960624eb7510895ae8c1721620b8dc3aa948c0d1c4cc6298aa9c43f1f0f7fae",
    "core/t64_sp.c :: t64_sp_rsp_guest_status_write": "aa575856b41e3be20241aee8db88efd7d2def2007cad2cb9cd0424c11417beb6",
    "core/t64_sp.c :: t64_rsp_validate_status_command": "e750e5da69933b8e68350a243266836a41d259724c203e8e4b805dee49f2dc81",
    "core/t64_rsp.c :: rsp_arithmetic_shift_right32": "2ffdbfc144cc0d544303a2c9d6544d4f8a507c7f95ce5c6fb5a2c5f6cbedc8b2",
    "core/t64_rsp.c :: rsp_clamp_signed_accumulator": "5543a4680a99621460790929bbecd6338f6b0cb98f2ace605988b67dbbdb2faf",
    "core/t64_rsp.c :: rsp_clamp_unsigned_accumulator": "d203d306b5bb83931666bbbe8565bf2be28b0b14637c65bdd9da7d616461a7f7",
    "core/t64_rsp.c :: rsp_commit_vadd": "9fda6b6f2ec351237bb4f681a9e77cd0624e3ddf1f2dfae98315951a4d087025",
    "core/t64_rsp.c :: rsp_commit_vector_effect": "493558efd0852f44c6eccc9c683702db375699ab50f3ae44e7e9a982711f50af",
    "core/t64_rsp.c :: rsp_decode_divider": "a8cb7d31faaa548edba4be5cfc197bd557105f0b71809503215e21b35928ef0c",
    "core/t64_rsp.c :: rsp_decode_vadd": "fe08db8d3d2e49994398ee7d76de1dde81c0d9bebda3fe40be75be501feff9a0",
    "core/t64_rsp.c :: rsp_decode_vector_basic": "bc3f5a778d7ec0ebe7e0d81ca9e847f7f8f3febba8df3f28bd17d3aca65eb7b1",
    "core/t64_rsp.c :: rsp_decode_vector_compare": "047454f1508577f5521c8b9a80bb9eefbad5059600ff0b948af3b3b488a91b7f",
    "core/t64_rsp.c :: rsp_decode_vector_multiply": "a89194966324203bde8b8d30dee9c1843a9e31b21ecdbd3fe0e02c26afd7b2a1",
    "core/t64_rsp.c :: rsp_plan_vector_memory": "2c45cef2220b904e3715d57e40cd35d10939d834e9b3d749ecd4f38e58b620fe",
    "core/t64_rsp.c :: rsp_read_imem_word": "db7a2c1d21484afdd935008961692093d22515afe3bb383dbe19ec8fd68479e1",
    "core/t64_rsp.c :: rsp_read_vector_element": "44f09265b81c3e7d891389b338bf3869f05ed1c905ebbe2b813a58edf36622e7",
    "core/t64_rsp.c :: rsp_ready_tick_after": "d920721d8f0e1ecf83be81d167fb79557f72f64fbb8f927890e99015661980cd",
    "core/t64_rsp.c :: rsp_select_vector_lane": "702353a47c99b149e59087cc1dce99574b12fd3b30f2452383db133b38e0d193",
    "core/t64_rsp.c :: rsp_sign_extend16": "57ef376a78778052585f55ec04ce962cbf9b260749cfec3892f3a70b5607a6d7",
    "core/t64_rsp.c :: rsp_signed16": "2ad56dbb5f2ad65b4bac1e81206888862d6523d9e4717ed349549fa53407040b",
    "core/t64_rsp.c :: rsp_signed48": "06efd07d2b91c580875b5d483b05e62337ea765c4d86a1b14aa8f71bad5669d9",
    "core/t64_rsp.c :: rsp_signed_less32": "485d921a30b6b37fcba6602bb3980e1d4888620e9333f8e5195e389e4a2f0574",
    "core/t64_rsp.c :: rsp_vector_memory_add_address": "8025f869af91a4bad6b448f6de94cbeb88e7a30502ab89b0c98d3ca0197ea93f",
    "core/t64_rsp.c :: rsp_vector_memory_add_load_byte": "d44c46bff06098b1a67a15d94602a633b7443d8192e3da4aaa1e5feaee5e6970",
    "core/t64_rsp.c :: rsp_vector_memory_add_store_byte": "e2833f80313951cbe5f61aad8a23afe2e29a8a4d21223e46cdf4b0f8d81bd7e8",
    "core/t64_rsp.c :: rsp_vector_memory_loaded_byte": "530efea13ab67b3f5e92288e0ca372195613b5402593efaa0b57fd25af753073",
    "core/t64_rsp.c :: rsp_write_vector_element": "3d6b40d7c541286102647e6241ce0b6466937797d8844cb676f2f220ec255b73",
    "core/t64_rsp.c :: t64_rsp_ensure_opportunity": "d37ee11dced86400787fe5774fb85544f3eb2a625d0e9916e6a90f9de54c5bbb",
    "core/t64_rsp.c :: t64_rsp_issue_event": "a683dd23902b71f888b460f7954dd3420eb6ddea36d46efdf8a17fc437e61a57",
    "core/t64_rsp.c :: t64_rsp_schedule_next": "7bceb405fe698a622902fd136009ac272c585eb62288c1371db49b42fa1bc339",
    "core/t64_rsp.c :: t64_rsp_start": "844ab432a1a02296d0f0d768dab18a5c60d9ee167bc2959637214b304e6e1ef1",
    "core/t64_sp.c :: decode_descriptor": "bb8ebf0fa2a9c2161076806566f090449af9c20d7de8d74bba461d4503c53352",
    "core/t64_sp.c :: dma_busy": "ef1efda4327bde7a879ae04b7d7947457b10c13263c0f5cb40547fd02b203cfe",
    "core/t64_sp.c :: dma_full": "92dc417b13b19cfe01ce1c1bfdb61c35bb952b69f07950c34920518dddd50bba",
    "core/t64_sp.c :: refuse": "14c52d2fb1695e35a264c9face6e0ee3fce0a62d44e8fbf621a75584904cf475",
    "core/t64_sp.c :: rsp_pc_overlaps_dma_beat": "206b3bb9a2847981e36c6a146d64f34b10fe9cbb931a1b5b087ce1b49a8765ed",
    "core/t64_sp.c :: start_rsp_dma": "a789536986e683cdf3efa0762c2ee0365c52fd7b0cc1f90291ae484838b1ac1a",
    "core/t64_sp.c :: t64_sp_rsp_dma_active": "c5eb4df345f87ff3f73ac0f81b9a039dc3b6ca13e9a8bb4faf59ee26a4035bda",
    "core/t64_sp.c :: t64_sp_rsp_dma_admission_commit": "8e52fa80dd6ae74fc13d8d7a46d64b802ce68aa8c3744cd10eb79e0d7fab4eff",
    "core/t64_sp.c :: t64_sp_rsp_dma_admission_preflight": "63d6631c0e409082581aebb7778c0298ca12459b5e4f8e6fd71a8e829f6ba18e",
    "core/t64_sp.c :: t64_sp_rsp_dma_admission_refuse": "1e014801570acaa111ae5e932ea0260840aa6fb5d37f27bcf3018759550a5c3c",
    "core/t64_sp.c :: t64_sp_rsp_dma_commit": "c0b226ae170deb3c7ce4ab539b78d358165e3af447db5ca091e5945e96768231",
    "core/t64_sp.c :: t64_sp_rsp_dma_preflight": "e5b6d9024c565659e7b610795fd64dcea905238edc5f74b09b0495c401eb108f",
    "core/t64_sp.c :: trace_refusal": "d142faecf8d4f78e427aa40e7454d0a24cc05c20ca433e225bed118982517dfc",
    "core/t64_sp.c :: warm_zero_source_sp_transfer": "46b55d7252273228780a497b4f34180b53dc93676f6bac901aaff9fae6d86508",
    "core/t64_sp.c :: write_rsp_status": "8d1f00710b955fc530864d080ae8d99733711e73749c75b6800d7cc590dbf74c",
    "core/t64_rdp.c :: apply_pending_command": "dbd536261c345239cdc1887b05f54e848038fc5505339f0d69e6f063dab9295c",
    "core/t64_rdp.c :: expand5_u8": "ceac39882b1c7c81ee50b7bedf89cce86e3908b8c5d02fd0186769a093699142",
    "core/t64_rdp.c :: command_span_valid": "4f8a0ecc32f895ae4dfa8f6390d90e4e1a0c18477bbe8ebbb97d66136f882f3a",
    "core/t64_rdp.c :: dpc_status_value": "051e3e702318dd9975d0c0a41a990c733d78ca8e84c77aeeab67ef4da7d4be32",
    "core/t64_rdp.c :: fill_bounds": "05ce4e443ffa8fac9adc9503750953b1a9f8c0d7485c1c14a90601433d4c6540",
    "core/t64_rdp.c :: fill_one_row": "6673a9b4afcb8d884a8cde2c7d0117a638ec240a28db20d7416b517832896a4e",
    "core/t64_rdp.c :: finish_if_idle": "c5f6f068c0c3622f8ca67750f53032f9b2c2ec6324cfda9991d7420e06277a1a",
    "core/t64_rdp.c :: handle_row": "db4e661d1899a3b47aaa9e8eed28e7a4a6d9fc44fcaaf0c98cc49ad641074703",
    "core/t64_rdp.c :: increment_counter": "c7b52a4723395d1f114dd6c485c36e3c08ca7338afcd527d8e9fd0929dc86911",
    "core/t64_rdp.c :: rdp_event_queued": "f91aa69f7a194595cf8cc54d2aedc64132d4b0052494e3873b6c751aec0b4f81",
    "core/t64_rdp.c :: read_be64": "a3b364ca9b36969d9e21b3551356029e7a7956968b8a0191b964652f95d2f364",
    "core/t64_rdp.c :: record_command": "21228fdb1466ec9758bca4b1facce045c2107160d9d33b51d4aeccd8b77bb860",
    "core/t64_rdp.c :: schedule_next_committed": "a6201021b4e6d72278c6ee6717b26b4e904160d3001b514523cf7ccc9a54d5e9",
    "core/t64_rdp.c :: schedule_work_after_committed": "dd9806898e39a57176c4bbe00dfa4c8b898ec0fa830c761700a2d17c55169be5",
    "core/t64_rdp.c :: set_refusal": "e7931da07d8dba661d964c820715536da479388feb50f9de9f2471c51a5c6e1f",
    "core/t64_rdp.c :: supported_opcode": "6cb0de550773784a8160b84fc0646a9b0cea8edb4d8e667a634f3f569d680039",
    "core/t64_rdp.c :: t64_rdp_event": "b67580981748efee09c489010721f4c55c98d76d2f821e07b624814f38f9f3c8",
    "core/t64_rdp.c :: t64_rdp_read_register": "1d362cac600402a9115d698c9373ac1823e13f7422711cd9ad0b80540a115fda",
    "core/t64_rdp.c :: t64_rdp_reset_machine_state": "b18b1126b2bddb74b4ba5d0162dda1742d897c06d397972ed62adda0472be8cb",
    "core/t64_rdp.c :: t64_rdp_write_register": "53b44012b22a3f9e90d76a0b6794406375bde7f973f3ce3ea8cc894df24d8a8e",
    "core/t64_rdp.c :: texture_rect_bounds": "3e8cf320ead70529f470b0ecd7e7ee304e49b7f78465a143e6ca0127e9d4087f",
    "core/t64_rdp.c :: validate_fill_store": "646b842f994a7700d3d7b68bfd95c44a110fd43a613bfd070b07f3fb202a6910",
    "core/t64_rdp.c :: write_be16": "145a18aff31ba9937039922fe81436defa1f931f61f369bba1ba557a9bd4772a",
    "core/t64_rdp.c :: write_be32": "6e02de3ff9551fb14c3b1ef9d80324a02d3b8d76023c742c622174374da0380c",
    "core/t64_rdp_raster.c :: alpha_dither_bias": "b3625d1859f8dec2df039da3b9c72580613b1d248c4d7e6f6c0bf55aeb3edbfd",
    "core/t64_rdp_raster.c :: color_dither_bias": "5fb9876a0aa607a0407172d7075f9587f9d3402a4ff54b3b8efda9f57679931e",
    "core/t64_rdp_raster.c :: decode_edges": "5c88d5401deeb0f45be0f9228397c9a53043f2f80494bd021a793c2799c26c23",
    "core/t64_rdp_raster.c :: blend_texture_levels": "b8e13e9b792ba75e35850124762e33a206a106af120c91f04da1c0c91c334c1b",
    "core/t64_rdp_raster.c :: floor_div_i64": "7da65f0daca4a086ea1d8f889a0e006fc86a78dd674d556b735bafc8b2626ecd",
    "core/t64_rdp_raster.c :: magnitude_i64": "c2c067fb88a91c3dbda8cea9f8f3199759de7d5737eef061e0bd2290489735d7",
    "core/t64_rdp_raster.c :: modulate_texture_shade": "24ae068b1c7fe693fb9207abb7c8e220430dfac759c9b493432684cb2ea51326",
    "core/t64_rdp_raster.c :: pair_q16": "56024955567f7f18bb7163400af9d016d94c98ef3ee1757cbfc75ccca1ab0a5c",
    "core/t64_rdp_raster.c :: project_texture_q16": "10e2632526925b46889ad6cfaa377b410969d63ed57d0fff24fdd8ef034f1b19",
    "core/t64_rdp_raster.c :: rgba5551": "81db13760bb36f530b28c2a5740fc826f89a7abe4094bcd4cb98979cf21960cc",
    "core/t64_rdp_raster.c :: t64_rdp_quantize_alpha5_v6": "1b287c06c2af74b2fe197535cb39c57f9d6052a00d2e6ef1ee97f27648ce14ad",
    "core/t64_rdp_raster.c :: t64_rdp_quantize_rgba5551_v6": "8de298af195ebd7d04b510892a300aab0737a385956fb99e23674266c0136d74",
    "core/t64_rdp_raster.c :: select_texture_lod": "69683e8c2fd7968142b8746ce7a67bf853086b3525f25f7d2c6c846a1ec77247",
    "core/t64_rdp_raster.c :: shade_channel": "ffc805ab7a8a313f037ccecd916c12f6c05623fbe0904620a98824b218ce112d",
    "core/t64_rdp_raster.c :: shade_products_are_bounded": "dc43aa44161b59e8e18a5ea5abd0977e58fa250b8158e13c4ce4cf4901aa0346",
    "core/t64_rdp_raster.c :: sign_extend_u14": "4c60f2c84b607ddec9ccfa10319406dd75b180450d224ac17cc6fedc1b45dadb",
    "core/t64_rdp_raster.c :: signed_u32": "83179d57b15375b137c8e01de5374b4b027c396644745b06e8f4b984c817bebd",
    "core/t64_rdp_raster.c :: t64_rdp_triangle_bounds": "189c18cb74f4228f0e1223e8a927b0d82db36e0d32342c0e0916988d74844404",
    "core/t64_rdp_raster.c :: t64_rdp_triangle_render_row": "904343aed521eac46de207006632a5b95f94ac51270d96a0958681d84c8014fc",
    "core/t64_rdp_raster.c :: t64_rdp_triangle_word_count": "9c4ba2233607144ba272f851462678760aae1ad6e24cd2b2fa635d24ebdf272c",
    "core/t64_rdp_raster.c :: texture_channel5": "17a9184f0b0b54ee26aa50902b7d727c41f0bd05830b1713e519e3f8d4e8505a",
    "core/t64_rdp_raster.c :: texture_channel_q16": "fe71be8419580dd185832ca0cc82988f112bcb089aca42de74f718d9217f70fe",
    "core/t64_rdp_raster.c :: texture_products_are_bounded": "9be7a5fc721761af1e7e15e8fb8bd977be8eac747d2d25d1ceec1a06a76ecdbd",
    "core/t64_rdp_raster.c :: texture_values_at": "192537747cf88c057a5a0832d8a6126fa6321352c41347fbe65a62247d7e7632",
    "core/t64_rdp_pipeline.c :: alpha_source": "003b10ac38b9744bea2facbbf9ade2dac385961adda91ccdf5542c98a1f8c6e7",
    "core/t64_rdp_pipeline.c :: blend_alpha_factor": "20de162bba725c37ab3fdf4167b80fa86dbde39657f3c767d926d43f0c90a56f",
    "core/t64_rdp_pipeline.c :: blend_b_factor": "60a12d589b50a9ba7685cfadb051f87cf2b59d1abee3c872afb34f76118d009f",
    "core/t64_rdp_pipeline.c :: blend_coverage_destination": "0f8e9cbd055cd02e894a54027214f8dc274bcfc8a6b1d905e4726798afc2c938",
    "core/t64_rdp_pipeline.c :: blend_rgb_source": "e3a097010b908f137a802a972c5214c248126c580524be0d058e0bfe8bae337c",
    "core/t64_rdp_pipeline.c :: clamp_u8": "08dc4f5a1bb344129936543b0bf9c45941bcea865b568e10884ad5845006cdd8",
    "core/t64_rdp_pipeline.c :: color_channel": "66142787001041fc0a64049b3a8c38515a037fd823b9a2b59b08c6f7fc68224b",
    "core/t64_rdp_pipeline.c :: combine_channel": "ae37ee47401fcdbe53198602af5ff91a51dd173f1c772327caf8e81c5271ca97",
    "core/t64_rdp_pipeline.c :: combine_cycle": "cf5885b732dba7c76f439094094f54ab82b7593b29a50270cb907af38ded3562",
    "core/t64_rdp_pipeline.c :: coverage_entry": "1452db6fd6e70af153e403bf19c0ed7814fa8bfae8a1acf3c6aa8718e4a6bfaf",
    "core/t64_rdp_pipeline.c :: floor_div_255": "467f7c61a7d7eb981204c52a5a5977f33cd729be4f593a877a21cf3ab6152a36",
    "core/t64_rdp_pipeline.c :: next_noise": "ac21af7970e2f4618315b2878bae66c88e93b2508546625d15831cfb17f445d6",
    "core/t64_rdp_pipeline.c :: read_blend_memory": "3898501ea5bae18cbfffb0ff0c57d0d8bc098fdf6e58d3d0ccb1c7dbcf433dcd",
    "core/t64_rdp_pipeline.c :: rgb_source": "c23588569f95204ff49dfa9f6931b910df5880f3b22c3ad64e4efc983e968ec7",
    "core/t64_rdp_pipeline.c :: signed9": "9fe5122087d1b493f7380882126ce57c2cd0e1c94068dfad8d746452d87bb1e3",
    "core/t64_rdp_pipeline.c :: t64_rdp_blend_pixel": "68c4a727113409916c5d0c872e981ebdc0135053540b0876dadd8f2b065a09ce",
    "core/t64_rdp_pipeline.c :: t64_rdp_combine_pixel": "656b8bb98dc169b8e7ec8db460af4cf47fd020c8b133243f641999f86f2641db",
    "core/t64_rdp_pipeline.c :: t64_rdp_memory_coverage_invalidate": "b017ad6833a66247fb1ca2f15b6f52868fe76b046c0b5184c54b327dada0448a",
    "core/t64_rdp_pipeline.c :: t64_rdp_memory_coverage_read": "18dab858d34f120f7755b801f980cb6907acbe336cacacb10e01849f672e432f",
    "core/t64_rdp_pipeline.c :: t64_rdp_memory_coverage_write": "1840d1d9599a5003eb89ae9ea08cea862b19d18f193f2264a89ed463639b6f62",
    "core/t64_rdp_depth.c :: depth_word_read": "cd1981e4e38cc9c36ac9b0f2713e5250bf2f9437b56e02340a37633464fd9b9d",
    "core/t64_rdp_depth.c :: floor_div_i64": "e23d0cd59f6ef6dfc7c2fdade098af1f8f28edf649d362422bd99e320f4a6a17",
    "core/t64_rdp_depth.c :: is_z_triangle": "66e34a30e2d3b8a114224d67356fddf6aeca8aacfeb6f6574539bba4c84f5d68",
    "core/t64_rdp_depth.c :: magnitude_i64": "c2c067fb88a91c3dbda8cea9f8f3199759de7d5737eef061e0bd2290489735d7",
    "core/t64_rdp_depth.c :: pair_from_word": "2a47eefaca71b518ffa30ae70e39e022a94e93947a26f8c9c18c3b90e728231d",
    "core/t64_rdp_depth.c :: signed_i32": "83179d57b15375b137c8e01de5374b4b027c396644745b06e8f4b984c817bebd",
    "core/t64_rdp_depth.c :: signed_pair_q16": "2747147c9a0d25bd2897e28d76597370ead177fe5eec80b2a8b79401aa40e5ee",
    "core/t64_rdp_depth.c :: z_compress": "3194ade08fa753dfb0a6b3fe89b1b1b543fcc475353573de7440ab4f164eb1be",
    "core/t64_rdp_depth.c :: z_decompress": "d88a75f6014bb875c37c037e1a90b7956d0da34257649b50c63dff65cedffaf3",
    "core/t64_rdp_depth.c :: z_delta_decode": "16069d69e67a93c6e988a4d5dd7446f0843dd133a277ec248277ccf0cf1e0d5b",
    "core/t64_rdp_depth.c :: z_delta_priority_code": "a3f05bf184d69818975b5bbca317d31a03dc541a08381ac8397ad9180502c377",
    "core/t64_rdp_depth.c :: t64_rdp_depth_commit_pixel": "2dcd03e02e974b1cd918907419683f1ca8e86e1af6bf2094de45d80b9897b059",
    "core/t64_rdp_depth.c :: t64_rdp_depth_evaluate_pixel": "922895d516b4e36fbd0b54b6f7eff3222c567e527d4af6c7fa5cc82eb2258942",
    "core/t64_rdp_depth.c :: t64_rdp_depth_triangle_preflight": "d12436fa67cb53aaa3e019e287b6aa98200bd011efc7c98962f48058302ffaa3",
    "core/t64_rdp_tmem.c :: apply_load_block": "9a1e6035fe42bc1d6fe4fb9c98501d61b82894b6dc155fcc524c73f486547b7a",
    "core/t64_rdp_tmem.c :: apply_load_tile": "92d546a7231010976364a0eedf4b1005493cb1030985a31707b723a5bb2c91b5",
    "core/t64_rdp_tmem.c :: apply_load_tlut": "60fd5c460b216b64f4d04b36cb2960f33204c95398c04542a668948eb95f4ca7",
    "core/t64_rdp_tmem.c :: average_four_rgba8": "0dbdef464af27e23959e227141bf0ee37d81623bc7ec447773e7610b6a2bd891",
    "core/t64_rdp_tmem.c :: average_four_texels": "0545879b9d76c68b14a1f1e5a4bf89b86905ef5b437580757f539a5dcd7ed5d2",
    "core/t64_rdp_tmem.c :: bits_per_texel": "04908bfc768c99d1651c96692f85c23d63c5d9f3382f770b54d0172319ecd759",
    "core/t64_rdp_tmem.c :: channel5": "68eae710a4c3a4f4125bcba65a7e2c42a9dafde7050c8937a0014e8d1011fc3b",
    "core/t64_rdp_tmem.c :: clamp_u8": "a493572d8171ac194592ee17ca5ef4b5b8952b7ee37fab26032d63dbee93340c",
    "core/t64_rdp_tmem.c :: coeff": "76bebe5957b343cebe89e691598eadf9deec70619952d5b31681c62c2a8ff6ad",
    "core/t64_rdp_tmem.c :: command_tile": "abde80deab4254c300124c4c3a050a910fa9b43201c860c2ecc4a6bdb3220709",
    "core/t64_rdp_tmem.c :: field12": "ab67b691d417f175d1d94583c7301ad595d7e0579f16f1f59196ab1bf9f6edf4",
    "core/t64_rdp_tmem.c :: floor_div_i64": "7da65f0daca4a086ea1d8f889a0e006fc86a78dd674d556b735bafc8b2626ecd",
    "core/t64_rdp_tmem.c :: format_size_supported": "71adacb26c4798ec6ccd53ea5c01a9ec53a85d85f4bc1f9fd34a98803effb456",
    "core/t64_rdp_tmem.c :: expand5_rgba8": "ceac39882b1c7c81ee50b7bedf89cce86e3908b8c5d02fd0186769a093699142",
    "core/t64_rdp_tmem.c :: load_source_span_valid": "b07270aa22debd52a133db1310902526aa3f17ac3a8a857d4fb4a35d06cee53c",
    "core/t64_rdp_tmem.c :: map_triangle_index": "b8b0d5587fdc055bf62be564dd562980f1b8bef840a3ccff53889b30c24528a6",
    "core/t64_rdp_tmem.c :: pack_rgba8": "a719e6236847fd632cbfcc90b3373a2d01c25d5326a3b92df1ba93c0e263885d",
    "core/t64_rdp_tmem.c :: normalize_triangle_axis": "bccb7ced66128df66bd3d2c947c93d9bc3b8e6049bab7168f30c1cabb5eaf7e8",
    "core/t64_rdp_tmem.c :: positive_mod_i64": "98adfaab314ce936683310176359221c646ea5ebab1c1fe5c7b0b77ccfe02ed3",
    "core/t64_rdp_tmem.c :: read_tile_texel": "eef1214edce78c3d1694ba8a69450b86000d8505f6acfc5269688d86bd5c6471",
    "core/t64_rdp_tmem.c :: read_tile_texel_rgba8": "12363c2ae15782908a221e02f866d905fc46e4757343b352147cb2095c244af2",
    "core/t64_rdp_tmem.c :: read_triangle_neighbor": "83f0c8e7a2613bc456cab84fa0a02cd6f45f34c66d294491f8c60e141c4efb28",
    "core/t64_rdp_tmem.c :: read_triangle_neighbor_rgba8": "a0e65526145ef6c3c1dc6d9590eef27fb243b5da40376b0f95757ca70e068c45",
    "core/t64_rdp_tmem.c :: rgba5551": "326cf3a732fa8e0127680cf563a68ffb1d2fc2de18060bd01ddd577ca08ba15f",
    "core/t64_rdp_tmem.c :: rgba8_channel": "de9e9e78f3e9b5f3b01c125cf99b5565d83f80fdfdf0f68a38280cf5991f9bd6",
    "core/t64_rdp_tmem.c :: floor_div_pow2": "cb42f091dfb0b8dd678305dca8d54cf5648870d0f1efdf62da02673497c0367e",
    "core/t64_rdp_tmem.c :: sample_texel": "99a9eae10ae328b568ad592693328216cfc66df38ab3e2c1c0f598d428f1726b",
    "core/t64_rdp_tmem.c :: sign_extend16": "53b62a6c4435c39870489b1c13c4633a77c8796023c6bb624beaa2108602fd6a",
    "core/t64_rdp_tmem.c :: signed9": "a662c63aecded78017f7e1e2742de56a458fc84ae7585ef8e52e59c8ae172038",
    "core/t64_rdp_tmem.c :: source_byte_address": "7367926a15b13a1d4cee17396574c35685aeea71a10a5d86f394a65e4fd9fdba",
    "core/t64_rdp_tmem.c :: source_texel": "9d778dcc3d805c53523841d5ce80ed33dec06e5527daead5ed21d3beacbb6cc2",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_apply_command": "842ac4be4b28b37a80342a6dc8a20710e1b5b91e3667be7011633f4c559ccf7a",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_begin_copy": "94aa297821e682a17400a16d921a6d37bb97c4291606ad33aebab99128ee6b11",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_convert_yuv": "a5fa69ef69feed89f418e2641c0f8ef5263e0bdb5b7c8e7cc095fb6593dde0c7",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_copy_pixel": "2d60a7974fa5e49840b500b235beb5cf6da5468cd8e99989eaeb797c48761b34",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_handles_opcode": "ef56ea6f392c9004cd09c808c19849961ba16783cbfb5f052142a48432795fe6",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_validate_command": "2de348419f36cbc140773f9bc8ac345271d611f1e69968f946ff92f3394c75f7",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_validate_copy": "e2416dd2f5971012ec68721206e1286e45be3a69ca5335fbf536cee19f791581",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_triangle_pixel": "14bbcc1c091eb79eb7e8b149ff9aeaed21a51cec5eb99e0f0a3b9012709f4c8f",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_triangle_pixel_rgba8": "dc2ea2baf07afbf9fde6fe54807d9b4f1d57b16392b5579c34b830dc22e31ed5",
    "core/t64_rdp_tmem.c :: t64_rdp_tmem_validate_triangle": "344298b67c53f3e48f1519e1e0cf306f76aad68e7dc7046778517827dc98778c",
    "core/t64_rdp_tmem.c :: three_point_texels": "8126e6a1d50620052e0a0c38f5afd5c7815619c3c13e9e162bd1452170cda32c",
    "core/t64_rdp_tmem.c :: three_point_rgba8": "a4abfb4f4f16355ebd10a2bdcb4f5b5f8428e527825a743579a5a669725a5e5a",
    "core/t64_rdp_tmem.c :: tmem_base": "641c94f472515a2c3dd210d23dc989fe5fcbf2ee8a3cc306895228c70f69a6bb",
    "core/t64_rdp_tmem.c :: tmem_read_bits": "60ede2058b1483b54e7bcb9e87c5425ec698b84e2d9083fd8eee73ed11db4966",
    "core/t64_rdp_tmem.c :: tmem_store": "bef2f9bcbd59a7dcede834b8438c9ee3f69786ba7ba6f78b6a94aac35bdf5364",
    "core/t64_rdp_tmem.c :: wrap_coordinate": "b39f4d43714ffc658791ff10ee3f85f2bc2210cb879358b2b7c3a11bb2eb1a66",
    "core/t64_vi.c :: copy_text": "2f10ff25b6593ac813ad6619ef1b6658fc199ad4fb78bce2ee098c3fcda8e600",
    "core/t64_vi.c :: delay_for_cycles": "9c3dae70091562065968143a1a6a15e843905b6b5c675407e1204fa15714eab7",
    "core/t64_vi.c :: expand5": "ceac39882b1c7c81ee50b7bedf89cce86e3908b8c5d02fd0186769a093699142",
    "core/t64_vi.c :: geometry_for": "3cf20840d8d8f059086f5374ba6b978a2eabc449abcc964145b9e47ebf0b674c",
    "core/t64_vi.c :: phase_at_tick": "47456dc6873e26bf86feacb2c6b777dbf8aacdd464507a24b441f2a16a36c523",
    "core/t64_vi.c :: vi_read_source_pixel": "a4c1c5f2a595008ee64242fff6b4cf2b6dc4d6c6d7bc004883002b77a758f7e1",
    "core/t64_vi.c :: vi_manhattan_distance": "1b5137a9e1ef5b544b5c6434516a085537967bc2bacef792c21fa48980c36840",
    "core/t64_vi.c :: vi_filter_source_pixel": "a62e2b06020bc980a24994e6ef92db91f3e45fd33d5daf0d20b7cd84671760cf",
    "core/t64_vi.c :: vi_divot_sample": "a9837eb6efc0d7d4db95be193aeaee1b01ec1455d6adcb079aeee35c3cf76b36",
    "core/t64_vi.c :: vi_median_channel": "5ed783411de86616cdba74b655ce525458d5b83785cdff487ac4080c85cec5ce",
    "core/t64_vi.c :: vi_gamma_channel": "1093e82d6fa498335c5c42af84a46298b9b61ae6dd92ccc2e274edd7e7756384",
    "core/t64_vi.c :: vi_gamma_adjust": "64ab1921d07ef53a958e12989086d3a82b68412cc249ba49844d7586a4ea9702",
    "core/t64_vi.c :: render_filter_packet": "c20a8b3e4cfb9f18f9ef56b42145e7cc717023bf9ae9ddb4c7f24dfa582096c7",
    "core/t64_vi.c :: render_packet": "7141993af029b51528639c8a41c384976980a3dde39b1e225a5b581789f4352d",
    "core/t64_vi.c :: t64_vi_event": "04d5f84b84638fa54062d6335574534cc00dfdae539f9bae1f7717263aeb1dd8",
    "core/t64_vi.c :: vi_v8_event": "e63f65ac41e722727c251d9fc17c7939c4ad7db226cc4b25c1f3d37f591863b2",
    "core/t64_vi.c :: t64_vi_read_register": "dec8d91eaf14db9cd26becbcd4b99ae30d7e872995386299ae594bf96626c2c9",
    "core/t64_vi.c :: t64_vi_reset_machine_state": "4aed279f94c4064aec968cf30f14f90f138e7b9fe736e8d4bac549829ea15982",
    "core/t64_vi.c :: t64_vi_write_register": "58eb5261b7a609f47ac28a4aebf675404b27407a738e03f2e377a25927f9e7b0",
    "core/t64_vi.c :: t64_video_copy": "4de19a66521dee9c26a4e4598f4881aa45ab69a554669dc6320fd1cfe39d6a62",
    "core/t64_vi.c :: t64_video_info": "7552591d2934e6f2278089a9536d33e3aadba3ac2f6a3321f25d0be7dc76110c",
}


def strip_c_comments_and_literals(source: str) -> str:
    """Blank comments and literals while preserving newlines for diagnostics."""

    pattern = re.compile(
        r"/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"|"
        r"'(?:\\.|[^'\\])*'",
        re.DOTALL,
    )

    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if char == "\n" else " " for char in match.group(0))

    return pattern.sub(blank, source)


def function_body(source: str, name: str) -> str | None:
    """Return one C function body, ignoring declarations and call sites."""

    for match in re.finditer(r"\b" + re.escape(name) + r"\s*\(", source):
        open_paren = source.find("(", match.start())
        depth = 0
        close_paren = None
        for index in range(open_paren, len(source)):
            if source[index] == "(":
                depth += 1
            elif source[index] == ")":
                depth -= 1
                if depth == 0:
                    close_paren = index
                    break
        if close_paren is None:
            continue
        cursor = close_paren + 1
        while cursor < len(source) and source[cursor].isspace():
            cursor += 1
        if cursor >= len(source) or source[cursor] != "{":
            continue
        depth = 0
        for index in range(cursor, len(source)):
            if source[index] == "{":
                depth += 1
            elif source[index] == "}":
                depth -= 1
                if depth == 0:
                    return source[cursor + 1 : index]
    return None


def static_function_names(source: str) -> set[str]:
    """Return file-scope static function definitions, not just declarations."""

    pattern = re.compile(
        r"(?m)^static\b[^;{}]*?\b([A-Za-z_]\w*)\s*\("
    )
    names = set()
    for match in pattern.finditer(source):
        name = match.group(1)
        if function_body(source, name) is not None:
            names.add(name)
    return names


def file_function_names(source: str) -> set[str]:
    """Return file-scope function definitions regardless of storage class."""

    pattern = re.compile(
        r"(?m)^(?:static\s+)?[^;{}]*?\b([A-Za-z_]\w*)\s*\("
    )
    names = set()
    cursor = 0
    depth = 0
    for match in pattern.finditer(source):
        prefix = source[cursor:match.start()]
        depth += prefix.count("{") - prefix.count("}")
        cursor = match.start()
        name = match.group(1)
        if depth == 0 and name not in {"if", "for", "while", "switch"} and function_body(source, name) is not None:
            names.add(name)
    return names


def _softfloat_sources(root: pathlib.Path) -> tuple[set[str], list[str]]:
    findings: list[str] = []
    cmake_path = root / "core" / "softfloat" / "sources.cmake"
    if not cmake_path.is_file():
        return set(), ["core/softfloat/sources.cmake: explicit source ledger is missing"]
    text = re.sub(r"#[^\n]*", "", cmake_path.read_text(encoding="utf-8"))
    match = re.search(r"set\s*\(\s*T64_SOFTFLOAT_SOURCES\b(.*?)\)", text, re.S)
    if match is None:
        return set(), [
            "core/softfloat/sources.cmake: T64_SOFTFLOAT_SOURCES ledger is missing"
        ]
    names = set(re.findall(r"/([^/\s\"]+\.c)\s*\"", match.group(1)))
    if not names:
        findings.append(
            "core/softfloat/sources.cmake: compiled softfloat source count is zero"
        )
    missing = sorted(CANONICAL_SOFTFLOAT_SOURCES - names)
    unexpected = sorted(names - CANONICAL_SOFTFLOAT_SOURCES)
    if missing:
        findings.append(
            "core/softfloat/sources.cmake: reviewed source(s) missing: "
            + ", ".join(missing)
        )
    if unexpected:
        findings.append(
            "core/softfloat/sources.cmake: unreviewed source(s): "
            + ", ".join(unexpected)
        )
    for name in sorted(names):
        if not (root / "core" / "softfloat" / name).is_file():
            findings.append(f"core/softfloat/{name}: listed production source is missing")
    return names, findings


def _source_inventory(root: pathlib.Path) -> tuple[set[str], list[str]]:
    core = root / "core"
    actual = {path.name for path in core.glob("t64_*.c") if path.is_file()}
    findings: list[str] = []
    if not actual:
        findings.append("core: scanned production source count is zero")
    missing = sorted(CANONICAL_CORE_SOURCES - actual)
    unexpected = sorted(actual - CANONICAL_CORE_SOURCES)
    if missing:
        findings.append(
            "core: canonical production source(s) missing: " + ", ".join(missing)
        )
    if unexpected:
        findings.append(
            "core: unreviewed t64_*.c production source(s): " + ", ".join(unexpected)
        )
    return actual, findings


def _check_cmake_route(root: pathlib.Path) -> list[str]:
    path = root / "core" / "CMakeLists.txt"
    if not path.is_file():
        return ["core/CMakeLists.txt: production build route is missing"]
    text = re.sub(r"#[^\n]*", "", path.read_text(encoding="utf-8"))
    compact = re.sub(r"\s+", " ", text)
    findings = []
    if not re.search(
        r'file\s*\(\s*GLOB\s+T64_CORE_SOURCES\s+CONFIGURE_DEPENDS\s+'
        r'"\$\{CMAKE_CURRENT_SOURCE_DIR\}/t64_\*\.c"\s*\)',
        compact,
    ):
        findings.append(
            "core/CMakeLists.txt: reviewed top-level t64_*.c source inventory route changed"
        )
    library = re.search(r"add_library\s*\(\s*t64_core\s+STATIC\b(.*?)\)", text, re.S)
    if library is None:
        findings.append("core/CMakeLists.txt: t64_core static library route is missing")
    else:
        body = library.group(1)
        if "${T64_CORE_SOURCES}" not in body or "${T64_SOFTFLOAT_SOURCES}" not in body:
            findings.append(
                "core/CMakeLists.txt: t64_core omits the reviewed core or softfloat source ledger"
            )
    return findings


def _check_source_route(root: pathlib.Path) -> tuple[list[str], list[str], int]:
    findings: list[str] = []
    evidence: list[str] = []
    _, inventory_findings = _source_inventory(root)
    findings.extend(inventory_findings)
    softfloat, softfloat_findings = _softfloat_sources(root)
    findings.extend(softfloat_findings)
    findings.extend(_check_cmake_route(root))

    rsp_path = root / "core" / "t64_rsp.c"
    sp_path = root / "core" / "t64_sp.c"
    rdp_path = root / "core" / "t64_rdp.c"
    pipeline_path = root / "core" / "t64_rdp_pipeline.c"
    raster_path = root / "core" / "t64_rdp_raster.c"
    tmem_path = root / "core" / "t64_rdp_tmem.c"
    depth_path = root / "core" / "t64_rdp_depth.c"
    vi_path = root / "core" / "t64_vi.c"
    bus_path = root / "core" / "t64_bus.c"
    pi_path = root / "core" / "t64_pi.c"
    if not all(
        path.is_file()
        for path in (
            rsp_path, sp_path, rdp_path, pipeline_path, raster_path, tmem_path,
            depth_path, vi_path, bus_path, pi_path
        )
    ):
        findings.append(
            "canonical RSP/SP/RDP/depth/raster/TMEM/VI source is missing from the reviewed production set"
        )
        return findings, evidence, len(softfloat)

    rsp = strip_c_comments_and_literals(rsp_path.read_text(encoding="utf-8"))
    sp = strip_c_comments_and_literals(sp_path.read_text(encoding="utf-8"))
    rdp = strip_c_comments_and_literals(rdp_path.read_text(encoding="utf-8"))
    pipeline = strip_c_comments_and_literals(
        pipeline_path.read_text(encoding="utf-8")
    )
    raster = strip_c_comments_and_literals(raster_path.read_text(encoding="utf-8"))
    tmem = strip_c_comments_and_literals(tmem_path.read_text(encoding="utf-8"))
    depth = strip_c_comments_and_literals(depth_path.read_text(encoding="utf-8"))
    vi = strip_c_comments_and_literals(vi_path.read_text(encoding="utf-8"))
    bus = strip_c_comments_and_literals(bus_path.read_text(encoding="utf-8"))
    pi = strip_c_comments_and_literals(pi_path.read_text(encoding="utf-8"))
    rdp_function_names = file_function_names(rdp)
    pipeline_function_names = file_function_names(pipeline)
    vi_function_names = file_function_names(vi)
    if rdp_function_names != REVIEWED_RDP_FUNCTIONS:
        missing = sorted(REVIEWED_RDP_FUNCTIONS - rdp_function_names)
        unexpected = sorted(rdp_function_names - REVIEWED_RDP_FUNCTIONS)
        findings.append(
            "core/t64_rdp.c: reviewed RDP function inventory changed; "
            f"missing={missing!r} unexpected={unexpected!r}"
        )
    raster_function_names = file_function_names(raster)
    if raster_function_names != REVIEWED_RDP_RASTER_FUNCTIONS:
        missing = sorted(REVIEWED_RDP_RASTER_FUNCTIONS - raster_function_names)
        unexpected = sorted(raster_function_names - REVIEWED_RDP_RASTER_FUNCTIONS)
        findings.append(
            "core/t64_rdp_raster.c: reviewed triangle raster function inventory changed; "
            f"missing={missing!r} unexpected={unexpected!r}"
        )
    if pipeline_function_names != REVIEWED_RDP_PIPELINE_FUNCTIONS:
        missing = sorted(REVIEWED_RDP_PIPELINE_FUNCTIONS - pipeline_function_names)
        unexpected = sorted(pipeline_function_names - REVIEWED_RDP_PIPELINE_FUNCTIONS)
        findings.append(
            "core/t64_rdp_pipeline.c: reviewed combiner function inventory changed; "
            f"missing={missing!r} unexpected={unexpected!r}"
        )
    tmem_function_names = file_function_names(tmem)
    if tmem_function_names != REVIEWED_RDP_TMEM_FUNCTIONS:
        missing = sorted(REVIEWED_RDP_TMEM_FUNCTIONS - tmem_function_names)
        unexpected = sorted(tmem_function_names - REVIEWED_RDP_TMEM_FUNCTIONS)
        findings.append(
            "core/t64_rdp_tmem.c: reviewed TMEM function inventory changed; "
            f"missing={missing!r} unexpected={unexpected!r}"
        )
    depth_function_names = file_function_names(depth)
    if depth_function_names != REVIEWED_RDP_DEPTH_FUNCTIONS:
        missing = sorted(REVIEWED_RDP_DEPTH_FUNCTIONS - depth_function_names)
        unexpected = sorted(depth_function_names - REVIEWED_RDP_DEPTH_FUNCTIONS)
        findings.append(
            "core/t64_rdp_depth.c: reviewed depth function inventory changed; "
            f"missing={missing!r} unexpected={unexpected!r}"
        )
    if vi_function_names != REVIEWED_VI_FUNCTIONS:
        missing = sorted(REVIEWED_VI_FUNCTIONS - vi_function_names)
        unexpected = sorted(vi_function_names - REVIEWED_VI_FUNCTIONS)
        findings.append(
            "core/t64_vi.c: reviewed VI function inventory changed; "
            f"missing={missing!r} unexpected={unexpected!r}"
        )
    sp_function_names = file_function_names(sp)
    if sp_function_names != REVIEWED_SP_FUNCTIONS:
        findings.append("core/t64_sp.c: complete SP function inventory changed; route review is required")
    actual_sp_dma_apis = {
        name for name in sp_function_names if name.startswith("t64_sp_rsp_dma_")
    }
    if actual_sp_dma_apis != REVIEWED_SP_DMA_API_FUNCTIONS:
        missing = sorted(REVIEWED_SP_DMA_API_FUNCTIONS - actual_sp_dma_apis)
        unexpected = sorted(actual_sp_dma_apis - REVIEWED_SP_DMA_API_FUNCTIONS)
        if missing:
            findings.append(
                "core/t64_sp.c: reviewed RSP DMA helper definition(s) missing: "
                + ", ".join(missing)
            )
        if unexpected:
            findings.append(
                "core/t64_sp.c: unreviewed RSP DMA helper definition(s): "
                + ", ".join(unexpected)
            )
    routes = {
        "core/t64_bus.c :: decode_region": function_body(bus, "decode_region"),
        "core/t64_bus.c :: t64_bus_write": function_body(bus, "t64_bus_write"),
        "core/t64_bus.c :: t64_bus_write_masked": function_body(bus, "t64_bus_write_masked"),
        "core/t64_bus.c :: t64_bus_writeback_cache_line": function_body(bus, "t64_bus_writeback_cache_line"),
        "core/t64_pi.c :: t64_pi_complete": function_body(pi, "t64_pi_complete"),
        "core/t64_rsp.c :: rsp_read_imem_word": function_body(rsp, "rsp_read_imem_word"),
        "core/t64_rsp.c :: t64_rsp_schedule_next": function_body(
            rsp, "t64_rsp_schedule_next"
        ),
        "core/t64_rsp.c :: t64_rsp_ensure_opportunity": function_body(
            rsp, "t64_rsp_ensure_opportunity"
        ),
        "core/t64_rsp.c :: t64_rsp_start": function_body(rsp, "t64_rsp_start"),
        "core/t64_rsp.c :: t64_rsp_issue_event": function_body(
            rsp, "t64_rsp_issue_event"
        ),
        "core/t64_sp.c :: start_rsp_dma": function_body(sp, "start_rsp_dma"),
        "core/t64_sp.c :: write_rsp_status": function_body(sp, "write_rsp_status"),
    }
    static_names = static_function_names(rsp)
    if static_names != REVIEWED_RSP_STATIC_FUNCTIONS:
        missing = sorted(REVIEWED_RSP_STATIC_FUNCTIONS - static_names)
        unexpected = sorted(static_names - REVIEWED_RSP_STATIC_FUNCTIONS)
        if missing:
            findings.append(
                "core/t64_rsp.c: reviewed static helper definition(s) missing: "
                + ", ".join(missing)
            )
        if unexpected:
            findings.append(
                "core/t64_rsp.c: unreviewed static helper definition(s): "
                + ", ".join(unexpected)
            )
    for name in REVIEWED_RSP_STATIC_FUNCTIONS:
        routes[f"core/t64_rsp.c :: {name}"] = function_body(rsp, name)
    for name in REVIEWED_SP_FUNCTIONS:
        routes[f"core/t64_sp.c :: {name}"] = function_body(sp, name)
    for name in REVIEWED_RDP_FUNCTIONS:
        routes[f"core/t64_rdp.c :: {name}"] = function_body(rdp, name)
    for name in REVIEWED_RDP_PIPELINE_FUNCTIONS:
        routes[f"core/t64_rdp_pipeline.c :: {name}"] = function_body(
            pipeline, name
        )
    for name in REVIEWED_RDP_RASTER_FUNCTIONS:
        routes[f"core/t64_rdp_raster.c :: {name}"] = function_body(raster, name)
    for name in REVIEWED_RDP_TMEM_FUNCTIONS:
        routes[f"core/t64_rdp_tmem.c :: {name}"] = function_body(tmem, name)
    for name in REVIEWED_RDP_DEPTH_FUNCTIONS:
        routes[f"core/t64_rdp_depth.c :: {name}"] = function_body(depth, name)
    for name in REVIEWED_VI_FUNCTIONS:
        routes[f"core/t64_vi.c :: {name}"] = function_body(vi, name)
    for label, body in routes.items():
        if body is None:
            findings.append(f"{label}: required canonical route function is missing")
        else:
            expected_digest = REVIEWED_BODY_DIGESTS.get(label)
            if expected_digest is None:
                findings.append(f"{label}: reviewed normalized body digest is missing")
            elif hashlib.sha256(re.sub(r"\s+", "", body).encode()).hexdigest() != expected_digest:
                findings.append(
                    f"{label}: executable body changed; fetched-path review is required"
                )

    read_body = routes["core/t64_rsp.c :: rsp_read_imem_word"] or ""
    if "machine->sp_imem" not in read_body or "bytes[0]" not in read_body:
        findings.append(
            "core/t64_rsp.c :: rsp_read_imem_word: fetched instruction no longer reads SP IMEM bytes"
        )
    schedule_body = routes["core/t64_rsp.c :: t64_rsp_schedule_next"] or ""
    if not all(
        token in schedule_body
        for token in (
            "t64_schedule_reserve_after",
            "T64_SP_EVENT_RSP_ISSUE",
            "t64_rsp_issue_event",
        )
    ):
        findings.append(
            "core/t64_rsp.c :: t64_rsp_schedule_next: scheduled issue callback is not t64_rsp_issue_event"
        )
    ensure_body = routes["core/t64_rsp.c :: t64_rsp_ensure_opportunity"] or ""
    start_body = routes["core/t64_rsp.c :: t64_rsp_start"] or ""
    if "t64_rsp_schedule_next(machine)" not in ensure_body:
        findings.append(
            "core/t64_rsp.c :: t64_rsp_ensure_opportunity: no scheduled fetched issue route"
        )
    if "t64_rsp_ensure_opportunity(machine)" not in start_body:
        findings.append(
            "core/t64_rsp.c :: t64_rsp_start: public execution entry does not route to scheduled issue"
        )
    issue_body = routes["core/t64_rsp.c :: t64_rsp_issue_event"] or ""
    fetch_match = re.search(r"instruction\s*=\s*rsp_read_imem_word\s*\(\s*machine\s*,\s*pc\s*\)", issue_body)
    operation_match = re.search(r"operation\s*=\s*instruction\s*>>\s*26", issue_body)
    required_dispatch = (
        "rsp_decode_vadd",
        "rsp_decode_vector_basic",
        "rsp_decode_vector_multiply",
        "rsp_decode_vector_compare",
        "rsp_decode_divider",
        "rsp_plan_vector_memory",
        "T64_RSP_ATTEMPT_RETIRED",
        "T64_RSP_ATTEMPT_REFUSED",
        "T64_RSP_REFUSAL_UNSUPPORTED_OPCODE",
        "T64_RSP_REFUSAL_DP_UNAVAILABLE",
        "t64_has_rdp_vi_profile",
        "t64_rdp_read_register",
        "t64_rdp_write_register",
    )
    if fetch_match is None:
        findings.append(
            "core/t64_rsp.c :: t64_rsp_issue_event: event does not fetch instruction from current IMEM PC"
        )
    if operation_match is None:
        findings.append(
            "core/t64_rsp.c :: t64_rsp_issue_event: opcode is not derived from fetched instruction bits"
        )
    if fetch_match is not None and operation_match is not None and fetch_match.start() > operation_match.start():
        findings.append(
            "core/t64_rsp.c :: t64_rsp_issue_event: opcode derivation precedes fetched instruction"
        )
    for token in required_dispatch:
        if token not in issue_body:
            findings.append(
                f"core/t64_rsp.c :: t64_rsp_issue_event: required decode/refusal path {token} is absent"
            )

    pending_body = routes["core/t64_rdp.c :: apply_pending_command"] or ""
    if not all(
        token in pending_body
        for token in (
            "t64_rdp_tmem_handles_opcode",
            "t64_rdp_tmem_validate_command",
            "t64_rdp_tmem_apply_command",
        )
    ):
        findings.append(
            "core/t64_rdp.c :: apply_pending_command: fetched texture commands bypass TMEM validation/effect route"
        )
    if "t64_rdp_triangle_bounds" not in pending_body:
        findings.append(
            "core/t64_rdp.c :: apply_pending_command: triangle commands bypass preflight bounds"
        )
    triangle_bounds_body = routes["core/t64_rdp_raster.c :: t64_rdp_triangle_bounds"] or ""
    if "t64_rdp_tmem_validate_triangle" not in triangle_bounds_body:
        findings.append(
            "core/t64_rdp_raster.c :: t64_rdp_triangle_bounds: textured triangles bypass tile/TMEM preflight"
        )
    depth_preflight_body = routes[
        "core/t64_rdp_depth.c :: t64_rdp_depth_triangle_preflight"
    ] or ""
    if "t64_rdp_depth_triangle_preflight" not in triangle_bounds_body or not depth_preflight_body:
        findings.append(
            "core/t64_rdp_raster.c :: t64_rdp_triangle_bounds: Z triangles bypass depth preflight"
        )
    row_body = routes["core/t64_rdp.c :: handle_row"] or ""
    if "t64_rdp_triangle_render_row" not in row_body:
        findings.append(
            "core/t64_rdp.c :: handle_row: triangle row effects bypass the canonical raster module"
        )
    raster_row_body = routes["core/t64_rdp_raster.c :: t64_rdp_triangle_render_row"] or ""
    if "t64_rdp_tmem_triangle_pixel" not in raster_row_body:
        findings.append(
            "core/t64_rdp_raster.c :: t64_rdp_triangle_render_row: textured pixels bypass canonical TMEM sampling"
        )
    if "t64_rdp_combine_pixel" not in raster_row_body:
        findings.append(
            "core/t64_rdp_raster.c :: t64_rdp_triangle_render_row: covered pixels bypass the versioned color combiner"
        )
    if "t64_rdp_blend_pixel" not in raster_row_body or "t64_rdp_memory_coverage_write" not in raster_row_body:
        findings.append(
            "core/t64_rdp_raster.c :: t64_rdp_triangle_render_row: blender coverage/color ownership route is absent"
        )
    if not all(
        token in raster_row_body
        for token in (
            "t64_rdp_depth_evaluate_pixel",
            "t64_rdp_depth_commit_pixel",
        )
    ):
        findings.append(
            "core/t64_rdp_raster.c :: t64_rdp_triangle_render_row: depth comparison/update route is absent"
        )
    depth_commit_body = routes[
        "core/t64_rdp_depth.c :: t64_rdp_depth_commit_pixel"
    ] or ""
    if "t64_rdp_memory_coverage_invalidate" not in depth_commit_body:
        findings.append(
            "core/t64_rdp_depth.c :: t64_rdp_depth_commit_pixel: accepted depth write bypasses overlapping sidecar invalidation"
        )
    fetch_body = routes["core/t64_rdp.c :: t64_rdp_event"] or ""
    if "t64_rdp_triangle_word_count" not in fetch_body:
        findings.append(
            "core/t64_rdp.c :: t64_rdp_event: triangle packets bypass full-length fetch validation"
        )

    for label in (
        "core/t64_sp.c :: start_rsp_dma",
        "core/t64_sp.c :: write_rsp_status",
    ):
        body = routes[label] or ""
        if "t64_rsp_ensure_opportunity(machine)" not in body:
            findings.append(f"{label}: SP execution entry bypasses scheduled RSP opportunity")

    dispatch_sources = {"core/t64_rsp.c": rsp, "core/t64_sp.c": sp}
    for path, code in dispatch_sources.items():
        for token in re.findall(r"[A-Za-z_][A-Za-z_0-9]*", code):
            compact = token.casefold().replace("_", "")
            suspicious = (
                "microcode" in compact
                or "ucode" in compact
                or "highlevel" in compact
                or "hle" in compact
                or compact.startswith("task")
                or "signature" in compact
            )
            if suspicious:
                findings.append(
                    f"{path}: executable identifier {token} selects or names a task/signature/HLE route"
                )
        if path == "core/t64_rsp.c" and re.search(
            r"machine\s*->\s*(?:image_storage|image_length|content_digest|rom_header|rom_crc|image_digest)",
            code,
        ):
            findings.append(
                "core/t64_rsp.c :: t64_rsp_issue_event: dispatch reads content identity instead of fetched opcode"
            )

    if not findings:
        evidence.extend(
            [
                "core/t64_rsp.c :: t64_rsp_start -> t64_rsp_ensure_opportunity -> t64_rsp_schedule_next",
                "core/t64_rsp.c :: t64_rsp_schedule_next -> t64_schedule_reserve_after(T64_SP_EVENT_RSP_ISSUE, t64_rsp_issue_event)",
                "core/t64_rsp.c :: t64_rsp_issue_event -> rsp_read_imem_word(machine, pc) -> operation = instruction >> 26 -> decode/refuse/retire",
                "core/t64_rsp.c :: fetched COP0 DP registers 8-15 -> selected profile -> t64_rdp_read_register/t64_rdp_write_register",
                "core/t64_rdp.c :: apply_pending_command -> t64_rdp_tmem_validate_command -> t64_rdp_tmem_apply_command",
                "core/t64_rdp_tmem.c :: per-machine source validation -> bounded LoadTile/LoadBlock/LoadTLUT writes -> copy sampling",
                "core/t64_rdp.c :: texture triangle packet -> signed Q16 preflight -> row-event raster -> per-machine TMEM sampling",
                "core/t64_rdp_raster.c :: covered triangle pixel -> t64_rdp_combine_pixel -> t64_rdp_blend_pixel -> versioned color/coverage RDRAM output",
                "core/t64_rdp_raster.c :: Z triangle preflight -> pixel compare -> accepted color write -> visible depth plus logical hidden sidecar",
                "core/t64_bus.c / core/t64_pi.c / core/t64_sp.c :: accepted CPU/cache/PI/SP writes invalidate per-instance coverage sidecar",
                "core/t64_sp.c :: start_rsp_dma and write_rsp_status -> t64_rsp_ensure_opportunity",
            ]
        )
    return findings, evidence, len(CANONICAL_CORE_SOURCES) + len(softfloat)


def _expected_object_sources(root: pathlib.Path) -> dict[str, pathlib.Path]:
    names, findings = _softfloat_sources(root)
    if findings:
        return {}
    expected: dict[str, pathlib.Path] = {}
    for name in CANONICAL_CORE_SOURCES:
        target = f"core/CMakeFiles/t64_core.dir/{name}.o"
        expected[target] = root / "core" / name
    for name in names:
        target = f"core/CMakeFiles/t64_core.dir/softfloat/{name}.o"
        expected[target] = root / "core" / "softfloat" / name
    return expected


def _check_build_route(root: pathlib.Path, build: pathlib.Path) -> tuple[list[str], list[str]]:
    findings: list[str] = []
    evidence: list[str] = []
    expected = _expected_object_sources(root)
    if not expected:
        return ["build route: expected production source count is zero"], evidence
    ninja_path = build / "build.ninja"
    if not ninja_path.is_file():
        return ["build/build.ninja: active Ninja route is missing"], evidence

    object_edges: dict[str, str] = {}
    archive_objects: list[str] | None = None
    for line in ninja_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("build core/CMakeFiles/t64_core.dir/") and ".c.o:" in line:
            left, right = line.split(":", 1)
            target = left[len("build ") :]
            prerequisites = right.split(" ||", 1)[0].strip().split()
            if len(prerequisites) < 2:
                findings.append(f"build/build.ninja :: {target}: source edge is incomplete")
                continue
            source = prerequisites[1]
            object_edges[target] = source
        elif line.startswith("build core/libt64_core.a:"):
            tokens = shlex.split(line.split(":", 1)[1])
            archive_objects = [
                token
                for token in tokens
                if token.startswith("core/CMakeFiles/t64_core.dir/")
                and token.endswith(".c.o")
            ]

    if archive_objects is None:
        findings.append("build/build.ninja :: core/libt64_core.a: archive link edge is missing")
        archive_objects = []
    expected_targets = set(expected)
    actual_targets = set(object_edges)
    if actual_targets != expected_targets:
        missing = sorted(expected_targets - actual_targets)
        extra = sorted(actual_targets - expected_targets)
        if missing:
            findings.append("build/build.ninja: missing core object edge(s): " + ", ".join(missing))
        if extra:
            findings.append("build/build.ninja: unreviewed core object edge(s): " + ", ".join(extra))
    for target, source in object_edges.items():
        if target not in expected:
            continue
        expected_source = expected[target].resolve()
        actual_source = pathlib.Path(source).resolve()
        if actual_source != expected_source:
            findings.append(
                f"build/build.ninja :: {target}: source edge points to {source}, expected core-relative {expected[target].relative_to(root)}"
            )
    if collections.Counter(archive_objects) != collections.Counter(expected_targets):
        missing = sorted((collections.Counter(expected_targets) - collections.Counter(archive_objects)).elements())
        extra = sorted((collections.Counter(archive_objects) - collections.Counter(expected_targets)).elements())
        if missing:
            findings.append("build/build.ninja :: core/libt64_core.a: RSP/core object(s) omitted: " + ", ".join(missing))
        if extra:
            findings.append("build/build.ninja :: core/libt64_core.a: unreviewed object(s) linked: " + ", ".join(extra))

    object_root = build / "core" / "CMakeFiles" / "t64_core.dir"
    if object_root.is_dir():
        active_object_files = list(object_root.glob("*.c.o"))
        softfloat_object_root = object_root / "softfloat"
        if softfloat_object_root.is_dir():
            active_object_files.extend(softfloat_object_root.rglob("*.c.o"))
        disk_objects = {
            "core/CMakeFiles/t64_core.dir/" + path.relative_to(object_root).as_posix()
            for path in active_object_files
        }
    else:
        disk_objects = set()
    if disk_objects != expected_targets:
        missing = sorted(expected_targets - disk_objects)
        extra = sorted(disk_objects - expected_targets)
        if missing:
            findings.append("build/core/CMakeFiles/t64_core.dir: object file(s) missing: " + ", ".join(missing))
        if extra:
            findings.append("build/core/CMakeFiles/t64_core.dir: hidden object file(s): " + ", ".join(extra))

    archive_path = build / "core" / "libt64_core.a"
    if not archive_path.is_file():
        findings.append("build/core/libt64_core.a: canonical core archive is missing")
    else:
        ar = shutil.which("ar")
        if ar is None:
            findings.append("build route: `ar` unavailable for archive member inspection")
        else:
            result = subprocess.run(
                [ar, "-t", str(archive_path)],
                check=False,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode != 0:
                findings.append("build/core/libt64_core.a: archive member inspection failed")
            else:
                members = [
                    line.strip()
                    for line in result.stdout.splitlines()
                    if line.strip() and not line.strip().startswith("__.SYMDEF")
                ]
                expected_members = collections.Counter(
                    pathlib.PurePosixPath(target).name for target in expected_targets
                )
                if collections.Counter(members) != expected_members:
                    findings.append(
                        "build/core/libt64_core.a: actual archive members differ from reviewed production object inventory"
                    )

    rsp_object = build / "core" / "CMakeFiles" / "t64_core.dir" / "t64_rsp.c.o"
    raster_object = (
        build / "core" / "CMakeFiles" / "t64_core.dir" / "t64_rdp_raster.c.o"
    )
    nm = shutil.which("nm")
    if not rsp_object.is_file():
        findings.append("build/core/CMakeFiles/t64_core.dir/t64_rsp.c.o: canonical RSP object is missing")
    elif nm is None:
        findings.append("build route: `nm` unavailable for canonical RSP object inspection")
    else:
        result = subprocess.run(
            [nm, "-a", str(rsp_object)],
            check=False,
            capture_output=True,
            text=True,
            timeout=10,
        )
        symbols = set()
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                pieces = line.split()
                if pieces:
                    symbol = pieces[-1]
                    while symbol.startswith("_"):
                        symbol = symbol[1:]
                    symbols.add(symbol)
        else:
            findings.append("build/core/CMakeFiles/t64_core.dir/t64_rsp.c.o: nm inspection failed")
        for symbol in ("t64_rsp_issue_event", "t64_rsp_ensure_opportunity", "t64_rsp_start"):
            if symbol not in symbols:
                findings.append(
                    f"build/core/CMakeFiles/t64_core.dir/t64_rsp.c.o: required canonical symbol {symbol} is absent"
                )
    if not raster_object.is_file():
        findings.append(
            "build/core/CMakeFiles/t64_core.dir/t64_rdp_raster.c.o: canonical triangle raster object is missing"
        )
    elif nm is None:
        findings.append("build route: `nm` unavailable for triangle raster object inspection")
    else:
        result = subprocess.run(
            [nm, "-a", str(raster_object)],
            check=False,
            capture_output=True,
            text=True,
            timeout=10,
        )
        symbols = set()
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                pieces = line.split()
                if pieces:
                    symbol = pieces[-1]
                    while symbol.startswith("_"):
                        symbol = symbol[1:]
                    symbols.add(symbol)
        else:
            findings.append(
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_raster.c.o: nm inspection failed"
            )
        for symbol in (
            "t64_rdp_triangle_bounds",
            "t64_rdp_triangle_render_row",
            "t64_rdp_triangle_word_count",
        ):
            if symbol not in symbols:
                findings.append(
                    "build/core/CMakeFiles/t64_core.dir/t64_rdp_raster.c.o: "
                    f"required canonical symbol {symbol} is absent"
                )
    tmem_object = build / "core" / "CMakeFiles" / "t64_core.dir" / "t64_rdp_tmem.c.o"
    if not tmem_object.is_file():
        findings.append(
            "build/core/CMakeFiles/t64_core.dir/t64_rdp_tmem.c.o: canonical TMEM object is missing"
        )
    elif nm is None:
        findings.append("build route: `nm` unavailable for canonical TMEM object inspection")
    else:
        result = subprocess.run(
            [nm, "-a", str(tmem_object)],
            check=False,
            capture_output=True,
            text=True,
            timeout=10,
        )
        symbols = set()
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                pieces = line.split()
                if pieces:
                    symbol = pieces[-1]
                    while symbol.startswith("_"):
                        symbol = symbol[1:]
                    symbols.add(symbol)
        else:
            findings.append(
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_tmem.c.o: nm inspection failed"
            )
        for symbol in (
            "t64_rdp_tmem_triangle_pixel",
            "t64_rdp_tmem_validate_triangle",
        ):
            if symbol not in symbols:
                findings.append(
                    "build/core/CMakeFiles/t64_core.dir/t64_rdp_tmem.c.o: "
                    f"required canonical symbol {symbol} is absent"
                )
    pipeline_object = (
        build / "core" / "CMakeFiles" / "t64_core.dir" / "t64_rdp_pipeline.c.o"
    )
    if not pipeline_object.is_file():
        findings.append(
            "build/core/CMakeFiles/t64_core.dir/t64_rdp_pipeline.c.o: canonical combiner object is missing"
        )
    elif nm is None:
        findings.append("build route: `nm` unavailable for RDP combiner object inspection")
    else:
        result = subprocess.run(
            [nm, "-a", str(pipeline_object)],
            check=False,
            capture_output=True,
            text=True,
            timeout=10,
        )
        symbols = set()
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                pieces = line.split()
                if pieces:
                    symbol = pieces[-1]
                    while symbol.startswith("_"):
                        symbol = symbol[1:]
                    symbols.add(symbol)
        else:
            findings.append(
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_pipeline.c.o: nm inspection failed"
            )
        if "t64_rdp_combine_pixel" not in symbols:
            findings.append(
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_pipeline.c.o: "
                "required canonical symbol t64_rdp_combine_pixel is absent"
            )
    depth_object = build / "core" / "CMakeFiles" / "t64_core.dir" / "t64_rdp_depth.c.o"
    if not depth_object.is_file():
        findings.append(
            "build/core/CMakeFiles/t64_core.dir/t64_rdp_depth.c.o: canonical depth object is missing"
        )
    elif nm is None:
        findings.append("build route: `nm` unavailable for RDP depth object inspection")
    else:
        result = subprocess.run(
            [nm, "-a", str(depth_object)],
            check=False,
            capture_output=True,
            text=True,
            timeout=10,
        )
        symbols = set()
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                pieces = line.split()
                if pieces:
                    symbol = pieces[-1]
                    while symbol.startswith("_"):
                        symbol = symbol[1:]
                    symbols.add(symbol)
        else:
            findings.append(
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_depth.c.o: nm inspection failed"
            )
        for symbol in (
            "t64_rdp_depth_triangle_preflight",
            "t64_rdp_depth_evaluate_pixel",
            "t64_rdp_depth_commit_pixel",
        ):
            if symbol not in symbols:
                findings.append(
                    "build/core/CMakeFiles/t64_core.dir/t64_rdp_depth.c.o: "
                    f"required canonical symbol {symbol} is absent"
                )
    if not findings:
        evidence.extend(
            [
                f"build/build.ninja :: core/libt64_core.a contains {len(expected)} reviewed source-object edges",
                "build/core/libt64_core.a :: archive member list matches the source-object edges",
                "build/core/CMakeFiles/t64_core.dir/t64_rsp.c.o :: nm contains t64_rsp_issue_event, t64_rsp_ensure_opportunity, t64_rsp_start",
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_raster.c.o :: nm contains triangle bounds, row and packet-length entry points",
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_tmem.c.o :: nm contains triangle texture validation and pixel sampling entry points",
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_pipeline.c.o :: nm contains t64_rdp_combine_pixel",
                "build/core/CMakeFiles/t64_core.dir/t64_rdp_depth.c.o :: nm contains depth preflight, compare, and commit entry points",
            ]
        )
    return findings, evidence


def inspect_tree(root: pathlib.Path, build: pathlib.Path | None = None) -> tuple[list[str], list[str], int]:
    root = root.resolve()
    findings, evidence, scanned = _check_source_route(root)
    if build is not None:
        build_findings, build_evidence = _check_build_route(root, build.resolve())
        findings.extend(build_findings)
        evidence.extend(build_evidence)
    return findings, evidence, scanned


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--build",
        type=pathlib.Path,
        default=pathlib.Path(os.environ["T64_RSP_NO_HLE_BUILD_DIR"])
        if os.environ.get("T64_RSP_NO_HLE_BUILD_DIR")
        else None,
    )
    args = parser.parse_args(argv)
    findings, evidence, scanned = inspect_tree(args.root, args.build)
    if findings:
        for finding in findings:
            print(f"FAIL: {finding}", file=sys.stderr)
        print(f"rsp-no-hle: sources_scanned={scanned} findings={len(findings)}", file=sys.stderr)
        return 1
    print(f"PASS: rsp-no-hle sources_scanned={scanned}")
    for item in evidence:
        print(f"route: {item}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

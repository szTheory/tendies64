/*
 * The host pump (Phase 12, plan 12-07, API-05). See t64h_pump.h for the contract.
 *
 * Time enters only through the clock callback. The guest-tick bound of a step is the
 * time since the origin divided by 16 (carry kept from the origin, so steps of any
 * size reach the same tick), capped by the caller, and never behind the tick the
 * guest has; a reading behind the previous one is refused. The advance loop treats
 * RESOURCE_LIMIT by tick movement: moved is the per-advance event budget, not moved
 * is ring backpressure and gets a drain and one retry. The pump keeps a packet that
 * a sink refused (it is already out of the ring), so movement that only the held
 * copy made possible is not progress. After a terminal stop of the guest the pump
 * advances and queues nothing.
 */
#include "t64h_pump.h"

const char *t64h_status_name(t64h_status status) {
  switch (status) {
    case T64H_OK: return "ok";
    case T64H_CLOCK_BACKWARD: return "clock-backward";
    case T64H_STALL: return "stall";
    case T64H_TERMINAL: return "terminal";
    case T64H_API_ERROR: return "api-error";
    case T64H_INPUT_ERROR: return "input-error";
    case T64H_BAD_ARGUMENT: return "bad-argument";
    default: return "unknown";
  }
}

uint64_t t64h_ticks_from_ns(uint64_t elapsed_ns) {
  return elapsed_ns / T64H_TICK_NS;
}

static t64h_status api_error(t64h_step_result *result, t64_status status,
                             const char *message) {
  result->api_status = status;
  result->message = message;
  return T64H_API_ERROR;
}

/* ---- the drain: video, then audio, then text -------------------------------- */

/* Offer the video packet in the buffer to the sink; 0 when it was consumed. */
static int offer_video(t64h_pump *pump, const t64_video_packet_info *info, size_t length) {
  if (pump->config.sinks.video == NULL) {
    return 0;
  }
  return pump->config.sinks.video(pump->config.sinks.context, info,
                                  pump->config.video_buffer, length);
}

static int offer_audio(t64h_pump *pump, const t64_audio_packet_info *info, size_t length) {
  if (pump->config.sinks.audio == NULL) {
    return 0;
  }
  return pump->config.sinks.audio(pump->config.sinks.context, info,
                                  pump->config.audio_buffer, length);
}

static int offer_text(t64h_pump *pump, size_t length) {
  if (pump->config.sinks.text == NULL) {
    return 0;
  }
  return pump->config.sinks.text(pump->config.sinks.context, pump->config.text_buffer,
                                 length);
}

/* Every queued video packet, oldest first, or until the sink refuses one. */
static t64h_status drain_video(t64h_pump *pump, size_t *delivered,
                               t64h_step_result *result) {
  for (;;) {
    t64_output_status ring = T64_OUTPUT_STATUS_INIT;
    t64_video_packet_info info = T64_VIDEO_PACKET_INFO_INIT;
    size_t copied = 0u;
    t64_status status;

    if (pump->video_held) {
      if (offer_video(pump, &pump->video_held_info, pump->video_held_length) != 0) {
        pump->counters.sink_refusals++;
        return T64H_OK;
      }
      pump->video_held = 0;
      pump->counters.video_packets++;
      (*delivered)++;
      continue;
    }
    status = t64_machine_output_status(pump->machine, &ring);
    if (status != T64_STATUS_OK) {
      return api_error(result, status, "output status refused");
    }
    if (ring.video_queued == 0u) {
      return T64H_OK;
    }
    if (ring.video_next_bytes > (uint64_t)pump->config.video_capacity) {
      return api_error(result, T64_STATUS_BUFFER_TOO_SMALL,
                       "video packet larger than the host buffer");
    }
    status = t64_video_info(pump->machine, &info);
    if (status != T64_STATUS_OK) {
      return api_error(result, status, "video info refused");
    }
    status = t64_video_copy(pump->machine, pump->config.video_buffer,
                            pump->config.video_capacity, &copied);
    if (status != T64_STATUS_OK) {
      return api_error(result, status, "video copy refused");
    }
    if (offer_video(pump, &info, copied) != 0) {
      pump->video_held_info = info;
      pump->video_held_length = copied;
      pump->video_held = 1;
      pump->counters.sink_refusals++;
      return T64H_OK;
    }
    pump->counters.video_packets++;
    (*delivered)++;
  }
}

static t64h_status drain_audio(t64h_pump *pump, size_t *delivered,
                               t64h_step_result *result) {
  for (;;) {
    t64_output_status ring = T64_OUTPUT_STATUS_INIT;
    t64_audio_packet_info info = T64_AUDIO_PACKET_INFO_INIT;
    size_t copied = 0u;
    t64_status status;

    if (pump->audio_held) {
      if (offer_audio(pump, &pump->audio_held_info, pump->audio_held_length) != 0) {
        pump->counters.sink_refusals++;
        return T64H_OK;
      }
      pump->audio_held = 0;
      pump->counters.audio_packets++;
      (*delivered)++;
      continue;
    }
    status = t64_machine_output_status(pump->machine, &ring);
    if (status != T64_STATUS_OK) {
      return api_error(result, status, "output status refused");
    }
    if (ring.audio_queued == 0u) {
      return T64H_OK;
    }
    if (ring.audio_next_bytes > (uint64_t)pump->config.audio_capacity) {
      return api_error(result, T64_STATUS_BUFFER_TOO_SMALL,
                       "audio packet larger than the host buffer");
    }
    status = t64_audio_info(pump->machine, &info);
    if (status != T64_STATUS_OK) {
      return api_error(result, status, "audio info refused");
    }
    status = t64_audio_copy(pump->machine, pump->config.audio_buffer,
                            pump->config.audio_capacity, &copied);
    if (status != T64_STATUS_OK) {
      return api_error(result, status, "audio copy refused");
    }
    if (offer_audio(pump, &info, copied) != 0) {
      pump->audio_held_info = info;
      pump->audio_held_length = copied;
      pump->audio_held = 1;
      pump->counters.sink_refusals++;
      return T64H_OK;
    }
    pump->counters.audio_packets++;
    (*delivered)++;
  }
}

/* The ISViewer text the sink has not seen: one offer per pass (the capture keeps at
 * most 4096 bytes, so one drain takes it all when the buffer is that large). */
static t64h_status drain_text(t64h_pump *pump, size_t *delivered,
                              t64h_step_result *result) {
  t64_output_status ring = T64_OUTPUT_STATUS_INIT;
  t64_output_drain_result drained = T64_OUTPUT_DRAIN_RESULT_INIT;
  t64_status status;

  if (pump->text_held) {
    if (offer_text(pump, pump->text_held_length) != 0) {
      pump->counters.sink_refusals++;
      return T64H_OK;
    }
    pump->text_held = 0;
    pump->counters.text_bytes += (uint64_t)pump->text_held_length;
    (*delivered)++;
  }
  status = t64_machine_output_status(pump->machine, &ring);
  if (status != T64_STATUS_OK) {
    return api_error(result, status, "output status refused");
  }
  if (ring.text_pending_bytes == 0u) {
    return T64H_OK;
  }
  if (ring.text_pending_bytes > (uint64_t)pump->config.text_capacity) {
    return api_error(result, T64_STATUS_BUFFER_TOO_SMALL,
                     "ISViewer text larger than the host buffer");
  }
  status = t64_machine_output_drain(pump->machine, pump->config.text_buffer,
                                    pump->config.text_capacity, &drained);
  if (status != T64_STATUS_OK) {
    return api_error(result, status, "output drain refused");
  }
  if (drained.copied_bytes == 0u) {
    return T64H_OK;
  }
  if (offer_text(pump, (size_t)drained.copied_bytes) != 0) {
    pump->text_held_length = (size_t)drained.copied_bytes;
    pump->text_held = 1;
    pump->counters.sink_refusals++;
    return T64H_OK;
  }
  pump->counters.text_bytes += drained.copied_bytes;
  (*delivered)++;
  return T64H_OK;
}

/* One drain pass in host order. *delivered counts what the sinks consumed. */
static t64h_status drain_pass(t64h_pump *pump, size_t *delivered,
                              t64h_step_result *result) {
  t64h_status status;

  *delivered = 0u;
  pump->counters.drain_passes++;
  status = drain_video(pump, delivered, result);
  if (status != T64H_OK) {
    return status;
  }
  status = drain_audio(pump, delivered, result);
  if (status != T64H_OK) {
    return status;
  }
  return drain_text(pump, delivered, result);
}

/* ---- init and step ---------------------------------------------------------- */

/* A refused packet or text waits in the pump. */
static int anything_held(const t64h_pump *pump) {
  return pump->video_held || pump->audio_held || pump->text_held;
}

t64h_status t64h_pump_init(t64h_pump *pump, const t64h_config *config) {
  if (pump == NULL || config == NULL || config->machine == NULL ||
      config->clock == NULL || config->video_buffer == NULL ||
      config->video_capacity == 0u || config->audio_buffer == NULL ||
      config->audio_capacity == 0u || config->text_buffer == NULL ||
      config->text_capacity == 0u) {
    return T64H_BAD_ARGUMENT;
  }
  *pump = (t64h_pump){0};
  pump->machine = config->machine;
  pump->config = *config;
  pump->origin_ns = config->clock(config->clock_context);
  pump->last_ns = pump->origin_ns;
  return T64H_OK;
}

t64h_status t64h_pump_step(t64h_pump *pump, uint64_t max_ticks, t64h_step_result *result) {
  uint64_t now;
  uint64_t target;
  uint64_t bound;
  int armed = 0;
  size_t delivered = 0u;
  t64h_status status;

  if (result == NULL) {
    return T64H_BAD_ARGUMENT;
  }
  result->tick = 0u;
  result->stop_reason = 0u;
  result->api_status = T64_STATUS_OK;
  result->message = NULL;
  if (pump == NULL) {
    return T64H_BAD_ARGUMENT;
  }
  result->tick = pump->tick;
  result->stop_reason = pump->last_reason;
  pump->counters.steps++;
  if (pump->terminal) {
    result->message = "the guest reached a terminal stop";
    return T64H_TERMINAL;
  }
  now = pump->config.clock(pump->config.clock_context);
  if (now < pump->last_ns) {
    pump->counters.backward_clocks++;
    result->message = "the clock went backward";
    return T64H_CLOCK_BACKWARD;
  }
  pump->last_ns = now;
  /* The tick the origin-relative time allows, the caller's cap on this step, and never
   * a bound behind the tick the guest already has (the core would answer TIME_REVERSED). */
  target = t64h_ticks_from_ns(now - pump->origin_ns);
  bound = max_ticks > UINT64_MAX - pump->tick ? UINT64_MAX : pump->tick + max_ticks;
  if (target < bound) {
    bound = target;
  }
  if (bound < pump->tick) {
    bound = pump->tick;
  }

  for (;;) {
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    t64_status core;
    int moved;

    if (pump->config.input != NULL) {
      const uint64_t offered = bound;

      if (pump->config.input(pump->config.input_context, pump->machine, pump->tick, &bound) != 0) {
        result->message = "the input hook failed";
        return T64H_INPUT_ERROR;
      }
      /* The hook may only lower the bound (to an event it could not queue yet), never
       * past the guest tick the machine already has. */
      if (bound > offered) {
        bound = offered;
      }
      if (bound < pump->tick) {
        bound = pump->tick;
      }
    }
    core = t64_machine_advance(
        pump->machine, &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, bound),
        &stop);
    if (core != T64_STATUS_OK) {
      return api_error(result, core, "advance refused");
    }
    if (stop.tick < pump->tick || stop.tick > bound) {
      return api_error(result, T64_STATUS_INTERNAL_DEFECT, "advance went outside its bound");
    }
    pump->counters.advances++;
    moved = stop.tick > pump->tick;
    pump->tick = stop.tick;
    pump->last_reason = stop.reason;
    result->tick = stop.tick;
    result->stop_reason = stop.reason;
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION ||
        stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED ||
        stop.reason == T64_STOP_REASON_HALT) {
      pump->terminal = 1;
    }
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      break;
    }
    if (moved) {
      /* The per-advance event budget: go on. Movement clears the stall watch unless a
       * refused packet is held: then the copy into the pump freed the ring slot that
       * let the guest move, and the sink has still taken nothing. */
      pump->counters.budget_stops++;
      if (!anything_held(pump)) {
        armed = 0;
      }
      continue;
    }
    /* Ring backpressure: nothing can run until the host drains. Drain in host order and
     * retry once; a second stop that moved nothing after a pass that consumed nothing
     * is the permanent stall of docs/spec/api.md section 8.1. */
    pump->counters.backpressure_stops++;
    status = drain_pass(pump, &delivered, result);
    if (status != T64H_OK) {
      return status;
    }
    if (delivered > 0u) {
      armed = 0;
    } else if (armed) {
      result->message = "stall without progress";
      return T64H_STALL;
    } else {
      armed = 1;
    }
    pump->counters.stall_retries++;
  }
  return drain_pass(pump, &delivered, result);
}

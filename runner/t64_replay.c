/* t64.replay v1 module (plan 09-07, D-34). Closed bounded parser, canonical
 * writer, recorder over the real accepted input call, live identity, stepwise
 * player and in-process driver, report writer. Runner side only: nothing here
 * reaches the core except the public machine API. No API or ABI freeze. */
#include "t64_replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_artifact_publisher.h"
#include "t64_json.h"
#include "t64_jsonw.h"
#include "t64_sha256.h"

#define REPLAY_JSON_DEPTH 4u
#define REPLAY_JSON_STRING 160u
#define REPLAY_NODES_PER_OP 8u
#define REPLAY_NODES_BASE 64u
/* Largest canonical op (an input op with maximal fields) and header, used to
 * size the writer buffer. */
#define REPLAY_OP_BYTES 256u
#define REPLAY_HEADER_BYTES 1536u
#define REPLAY_VERSION_MAX UINT32_C(2147483647)
#define REPLAY_SAVE_IMAGE_MAX UINT64_C(1048576)

struct t64_replay {
  t64_replay_header header;
  t64_replay_op *ops;
  size_t op_count;
  size_t checkpoint_count;
  uint8_t *canonical;
  size_t canonical_length;
  uint8_t sha256[T64_SHA256_DIGEST_SIZE];
};

const char *t64_replay_status_name(t64_replay_status status) {
  switch (status) {
  case T64_REPLAY_OK: return "ok";
  case T64_REPLAY_INVALID_ARGUMENT: return "invalid-argument";
  case T64_REPLAY_SIZE_LIMIT: return "size-limit";
  case T64_REPLAY_INVALID_JSON: return "invalid-json";
  case T64_REPLAY_INVALID_SCHEMA: return "invalid-schema";
  case T64_REPLAY_INVALID_HEADER: return "invalid-header";
  case T64_REPLAY_BOOT_UNSUPPORTED: return "boot-unsupported";
  case T64_REPLAY_OP_LIMIT: return "op-limit";
  case T64_REPLAY_CHECKPOINT_LIMIT: return "checkpoint-limit";
  case T64_REPLAY_INVALID_OP: return "invalid-op";
  case T64_REPLAY_AT_REVERSED: return "at-reversed";
  case T64_REPLAY_EVENT_BEFORE_AT: return "event-before-at";
  case T64_REPLAY_END_MISSING: return "end-missing";
  case T64_REPLAY_END_NOT_LAST: return "end-not-last";
  case T64_REPLAY_ALLOCATION_FAILED: return "allocation-failed";
  case T64_REPLAY_CANONICALIZATION_FAILED: return "canonicalization-failed";
  }
  return "unknown";
}

/* ---- small helpers ------------------------------------------------------------- */

static t64_replay_status refuse(t64_replay_error *error,
                                t64_replay_status status, size_t offset) {
  if (error != NULL) {
    error->status = status;
    error->offset = offset;
  }
  return status;
}

static bool lower_hex(const char *text, size_t length) {
  size_t i;
  for (i = 0u; i < length; i++) {
    if (!((text[i] >= '0' && text[i] <= '9') ||
          (text[i] >= 'a' && text[i] <= 'f'))) {
      return false;
    }
  }
  return true;
}

static bool node_int(const t64_json_node *node, int64_t minimum,
                     int64_t maximum, int64_t *value_out) {
  if (node == NULL || node->type != T64_JSON_INT || node->num < minimum ||
      node->num > maximum) {
    return false;
  }
  *value_out = node->num;
  return true;
}

/* Exactly 64 lowercase hex characters into a NUL-terminated buffer. */
static bool node_hex64(const t64_json_node *node, char out[T64_REPLAY_HEX_SIZE]) {
  if (node == NULL || node->type != T64_JSON_STRING || node->len != 64u ||
      !lower_hex(node->str, 64u)) {
    return false;
  }
  memcpy(out, node->str, 64u);
  out[64] = '\0';
  return true;
}

static bool node_hex_digest(const t64_json_node *node,
                            uint8_t out[T64_DIGEST_SIZE]) {
  char text[T64_REPLAY_HEX_SIZE];
  size_t i;
  if (!node_hex64(node, text)) return false;
  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    unsigned value = 0u;
    size_t k;
    for (k = 0u; k < 2u; k++) {
      char c = text[2u * i + k];
      value = value * 16u +
              (unsigned)(c <= '9' ? c - '0' : c - 'a' + 10);
    }
    out[i] = (uint8_t)value;
  }
  return true;
}

static bool string_equals(const t64_json_node *node, const char *text) {
  size_t length = strlen(text);
  return node != NULL && node->type == T64_JSON_STRING && node->len == length &&
         memcmp(node->str, text, length) == 0;
}

/* Grammar of the build identity of the runner (valid_build_identity of
 * runner/t64_real.c, copied with provenance): the fixed prefix and 64
 * lowercase hex characters. */
static bool valid_build_identity(const t64_json_node *node, char *out,
                                 size_t capacity) {
  static const char prefix[] = "t64-build-v1-";
  const size_t total = sizeof prefix - 1u + 64u;
  if (node == NULL || node->type != T64_JSON_STRING || node->len != total ||
      memcmp(node->str, prefix, sizeof prefix - 1u) != 0 ||
      !lower_hex(node->str + sizeof prefix - 1u, 64u) || capacity <= total) {
    return false;
  }
  memcpy(out, node->str, total);
  out[total] = '\0';
  return true;
}

/* Grammar of the source revision (valid_source_revision of
 * runner/t64_real.c, copied with provenance): 40 lowercase hex characters. */
static bool valid_source_revision(const t64_json_node *node, char out[41]) {
  if (node == NULL || node->type != T64_JSON_STRING || node->len != 40u ||
      !lower_hex(node->str, 40u)) {
    return false;
  }
  memcpy(out, node->str, 40u);
  out[40] = '\0';
  return true;
}

/* A registered-id shaped profile id: 1 to 63 characters of a-z, 0-9 and '-'. */
static bool valid_profile_id(const t64_json_node *node, char *out,
                             size_t capacity) {
  size_t i;
  if (node == NULL || node->type != T64_JSON_STRING || node->len == 0u ||
      node->len >= 64u || node->len >= capacity) {
    return false;
  }
  for (i = 0u; i < node->len; i++) {
    char c = node->str[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
      return false;
    }
  }
  memcpy(out, node->str, node->len);
  out[node->len] = '\0';
  return true;
}

static void digest_to_hex(const uint8_t digest[T64_DIGEST_SIZE],
                          char out[T64_REPLAY_HEX_SIZE]) {
  t64_sha256_hex(digest, out);
}

/* ---- parse ----------------------------------------------------------------------- */

static const char *const root_keys[] = {
    "boot",           "build_id",           "content_sha256",
    "content_size",   "descriptor_version", "input_schema_version",
    "machine_model_version", "ops",         "profile_digest",
    "profile_id",     "save_init",          "schema",
    "schema_version", "source_revision",     "state_serialization_version"};
#define ROOT_KEY_COUNT (sizeof root_keys / sizeof root_keys[0])

static t64_replay_status parse_save_init(const t64_json_node *node,
                                         t64_replay_live_identity *id) {
  static const char *const blank_keys[] = {"kind"};
  static const char *const image_keys[] = {"kind", "length", "sha256"};
  const t64_json_node *kind;
  const char *bad = NULL;
  uint64_t length = 0u;
  if (node == NULL || node->type != T64_JSON_OBJECT) {
    return T64_REPLAY_INVALID_SCHEMA;
  }
  kind = t64_json_get(node, "kind");
  if (kind == NULL || kind->type != T64_JSON_STRING) {
    return T64_REPLAY_INVALID_SCHEMA;
  }
  if (string_equals(kind, "blank")) {
    if (t64_json_check_keys(node, blank_keys, 1u, blank_keys, 1u, &bad) !=
        T64_JSON_KEYS_OK) {
      return T64_REPLAY_INVALID_SCHEMA;
    }
    id->save_kind = T64_REPLAY_SAVE_BLANK;
    id->save_length = 0u;
    id->save_sha256[0] = '\0';
    return T64_REPLAY_OK;
  }
  if (string_equals(kind, "image")) {
    if (t64_json_check_keys(node, image_keys, 3u, image_keys, 3u, &bad) !=
        T64_JSON_KEYS_OK) {
      return T64_REPLAY_INVALID_SCHEMA;
    }
    if (!t64_json_u64_string(t64_json_get(node, "length"), &length) ||
        length == 0u || length > REPLAY_SAVE_IMAGE_MAX ||
        !node_hex64(t64_json_get(node, "sha256"), id->save_sha256)) {
      return T64_REPLAY_INVALID_HEADER;
    }
    id->save_kind = T64_REPLAY_SAVE_IMAGE;
    id->save_length = length;
    return T64_REPLAY_OK;
  }
  return T64_REPLAY_INVALID_HEADER;
}

static t64_replay_status parse_header(const t64_json_node *root,
                                      t64_replay_header *header) {
  const char *bad = NULL;
  const t64_json_node *node;
  int64_t value;
  t64_replay_live_identity *id = &header->identity;
  memset(header, 0, sizeof *header);
  if (t64_json_check_keys(root, root_keys, ROOT_KEY_COUNT, root_keys,
                          ROOT_KEY_COUNT, &bad) != T64_JSON_KEYS_OK) {
    return T64_REPLAY_INVALID_SCHEMA;
  }
  /* Types first (a wrong JSON type is a schema refusal). */
  node = t64_json_get(root, "schema");
  if (!string_equals(node, T64_REPLAY_FORMAT_NAME)) {
    return T64_REPLAY_INVALID_SCHEMA;
  }
  if (!node_int(t64_json_get(root, "schema_version"), 1, 1, &value) ||
      !node_int(t64_json_get(root, "input_schema_version"), 1, 1, &value)) {
    return T64_REPLAY_INVALID_SCHEMA;
  }
  {
    static const char *const string_keys[] = {
        "boot",         "build_id",       "content_sha256", "profile_digest",
        "profile_id",   "source_revision"};
    static const char *const int_keys[] = {
        "descriptor_version", "machine_model_version",
        "state_serialization_version"};
    size_t i;
    for (i = 0u; i < sizeof string_keys / sizeof string_keys[0]; i++) {
      node = t64_json_get(root, string_keys[i]);
      if (node == NULL || node->type != T64_JSON_STRING) {
        return T64_REPLAY_INVALID_SCHEMA;
      }
    }
    for (i = 0u; i < sizeof int_keys / sizeof int_keys[0]; i++) {
      node = t64_json_get(root, int_keys[i]);
      if (node == NULL || node->type != T64_JSON_INT) {
        return T64_REPLAY_INVALID_SCHEMA;
      }
    }
    node = t64_json_get(root, "ops");
    if (node == NULL || node->type != T64_JSON_ARRAY) {
      return T64_REPLAY_INVALID_SCHEMA;
    }
    node = t64_json_get(root, "save_init");
    if (node == NULL || node->type != T64_JSON_OBJECT) {
      return T64_REPLAY_INVALID_SCHEMA;
    }
  }
  /* Then values. */
  node = t64_json_get(root, "boot");
  if (string_equals(node, "state")) return T64_REPLAY_BOOT_UNSUPPORTED;
  if (!string_equals(node, "cold-admit")) return T64_REPLAY_INVALID_HEADER;
  if (!valid_profile_id(t64_json_get(root, "profile_id"), id->profile_id,
                        sizeof id->profile_id) ||
      !node_hex64(t64_json_get(root, "profile_digest"), id->profile_digest) ||
      !valid_build_identity(t64_json_get(root, "build_id"), id->build_id,
                            sizeof id->build_id) ||
      !node_hex64(t64_json_get(root, "content_sha256"), id->content_sha256) ||
      !t64_json_u64_string(t64_json_get(root, "content_size"),
                           &id->content_size) ||
      !valid_source_revision(t64_json_get(root, "source_revision"),
                             header->source_revision)) {
    return T64_REPLAY_INVALID_HEADER;
  }
  if (!node_int(t64_json_get(root, "descriptor_version"), 1, REPLAY_VERSION_MAX,
                &value)) {
    return T64_REPLAY_INVALID_HEADER;
  }
  id->descriptor_version = (uint32_t)value;
  if (!node_int(t64_json_get(root, "machine_model_version"), 1,
                REPLAY_VERSION_MAX, &value)) {
    return T64_REPLAY_INVALID_HEADER;
  }
  id->machine_model_version = (uint32_t)value;
  if (!node_int(t64_json_get(root, "state_serialization_version"), 1,
                REPLAY_VERSION_MAX, &value)) {
    return T64_REPLAY_INVALID_HEADER;
  }
  id->state_serialization_version = (uint32_t)value;
  return parse_save_init(t64_json_get(root, "save_init"), id);
}

static t64_replay_status parse_op(const t64_json_node *node,
                                  t64_replay_op *op) {
  static const char *const input_keys[] = {"at", "buttons", "event_tick", "op",
                                           "port", "x", "y"};
  static const char *const checkpoint_keys[] = {"at", "digest", "op"};
  static const char *const end_keys[] = {"at", "digest", "op", "stop"};
  const t64_json_node *kind;
  const char *bad = NULL;
  int64_t value;
  memset(op, 0, sizeof *op);
  if (node == NULL || node->type != T64_JSON_OBJECT) {
    return T64_REPLAY_INVALID_OP;
  }
  kind = t64_json_get(node, "op");
  if (kind == NULL || kind->type != T64_JSON_STRING) {
    return T64_REPLAY_INVALID_OP;
  }
  if (string_equals(kind, "input")) {
    op->kind = T64_REPLAY_OP_INPUT;
    if (t64_json_check_keys(node, input_keys, 7u, input_keys, 7u, &bad) !=
            T64_JSON_KEYS_OK ||
        !t64_json_u64_string(t64_json_get(node, "at"), &op->at) ||
        !t64_json_u64_string(t64_json_get(node, "event_tick"),
                             &op->event_tick)) {
      return T64_REPLAY_INVALID_OP;
    }
    /* K7: replay v1 restricts the port to 0. */
    if (!node_int(t64_json_get(node, "port"), 0, 0, &value)) {
      return T64_REPLAY_INVALID_OP;
    }
    op->port = T64_REPLAY_PORT;
    if (!node_int(t64_json_get(node, "buttons"), 0, UINT16_MAX, &value)) {
      return T64_REPLAY_INVALID_OP;
    }
    op->buttons = (uint16_t)value;
    if (!node_int(t64_json_get(node, "x"), T64_INPUT_AXIS_MIN, T64_INPUT_AXIS_MAX,
                  &value)) {
      return T64_REPLAY_INVALID_OP;
    }
    op->axis_x = (int32_t)value;
    if (!node_int(t64_json_get(node, "y"), T64_INPUT_AXIS_MIN, T64_INPUT_AXIS_MAX,
                  &value)) {
      return T64_REPLAY_INVALID_OP;
    }
    op->axis_y = (int32_t)value;
    return T64_REPLAY_OK;
  }
  if (string_equals(kind, "checkpoint")) {
    op->kind = T64_REPLAY_OP_CHECKPOINT;
    if (t64_json_check_keys(node, checkpoint_keys, 3u, checkpoint_keys, 3u,
                            &bad) != T64_JSON_KEYS_OK ||
        !t64_json_u64_string(t64_json_get(node, "at"), &op->at) ||
        !node_hex_digest(t64_json_get(node, "digest"), op->digest)) {
      return T64_REPLAY_INVALID_OP;
    }
    return T64_REPLAY_OK;
  }
  if (string_equals(kind, "end")) {
    const t64_json_node *stop = t64_json_get(node, "stop");
    op->kind = T64_REPLAY_OP_END;
    if (t64_json_check_keys(node, end_keys, 4u, end_keys, 4u, &bad) !=
            T64_JSON_KEYS_OK ||
        !t64_json_u64_string(t64_json_get(node, "at"), &op->at) ||
        !node_hex_digest(t64_json_get(node, "digest"), op->digest)) {
      return T64_REPLAY_INVALID_OP;
    }
    if (string_equals(stop, "guest-bound")) {
      op->stop = T64_REPLAY_STOP_GUEST_BOUND;
    } else if (string_equals(stop, "guest-completion")) {
      op->stop = T64_REPLAY_STOP_GUEST_COMPLETION;
    } else {
      return T64_REPLAY_INVALID_OP;
    }
    return T64_REPLAY_OK;
  }
  return T64_REPLAY_INVALID_OP;
}

/* ---- canonical writer ---------------------------------------------------------------- */

static size_t u64_decimal(uint64_t value, char output[20]) {
  char reverse[20];
  size_t count = 0u;
  size_t i;
  do {
    reverse[count++] = (char)('0' + (int)(value % UINT64_C(10)));
    value /= UINT64_C(10);
  } while (value != 0u);
  for (i = 0u; i < count; i++) output[i] = reverse[count - i - 1u];
  return count;
}

#define W_OK(call) ((call) == T64_JSONW_OK)

static bool write_key(t64_jsonw *w, const char *key) {
  return W_OK(t64_jsonw_key(w, key, strlen(key)));
}

static bool write_text(t64_jsonw *w, const char *key, const char *text) {
  return write_key(w, key) && W_OK(t64_jsonw_string(w, text, strlen(text)));
}

static bool write_u64_text(t64_jsonw *w, const char *key, uint64_t value) {
  char text[20];
  size_t length = u64_decimal(value, text);
  return write_key(w, key) && W_OK(t64_jsonw_string(w, text, length));
}

static bool write_int(t64_jsonw *w, const char *key, int64_t value) {
  return write_key(w, key) && W_OK(t64_jsonw_int(w, value));
}

static bool write_digest(t64_jsonw *w, const char *key,
                         const uint8_t digest[T64_DIGEST_SIZE]) {
  char hex[T64_REPLAY_HEX_SIZE];
  digest_to_hex(digest, hex);
  return write_text(w, key, hex);
}

static bool write_op(t64_jsonw *w, const t64_replay_op *op) {
  if (!W_OK(t64_jsonw_begin_object(w)) || !write_u64_text(w, "at", op->at)) {
    return false;
  }
  switch (op->kind) {
  case T64_REPLAY_OP_INPUT:
    if (!write_int(w, "buttons", (int64_t)op->buttons) ||
        !write_u64_text(w, "event_tick", op->event_tick) ||
        !write_text(w, "op", "input") || !write_int(w, "port", (int64_t)op->port) ||
        !write_int(w, "x", (int64_t)op->axis_x) ||
        !write_int(w, "y", (int64_t)op->axis_y)) {
      return false;
    }
    break;
  case T64_REPLAY_OP_CHECKPOINT:
    if (!write_digest(w, "digest", op->digest) ||
        !write_text(w, "op", "checkpoint")) {
      return false;
    }
    break;
  case T64_REPLAY_OP_END:
    if (!write_digest(w, "digest", op->digest) || !write_text(w, "op", "end") ||
        !write_text(w, "stop",
                    op->stop == T64_REPLAY_STOP_GUEST_COMPLETION
                        ? "guest-completion"
                        : "guest-bound")) {
      return false;
    }
    break;
  default:
    return false;
  }
  return W_OK(t64_jsonw_end_object(w));
}

t64_replay_status t64_replay_encode(const t64_replay_header *header,
                                    const t64_replay_op *ops, size_t op_count,
                                    uint8_t **bytes_out, size_t *length_out) {
  const t64_replay_live_identity *id;
  t64_jsonw writer;
  size_t capacity;
  size_t length = 0u;
  size_t i;
  uint8_t *buffer;
  if (bytes_out != NULL) *bytes_out = NULL;
  if (length_out != NULL) *length_out = 0u;
  if (header == NULL || (ops == NULL && op_count != 0u) || bytes_out == NULL ||
      length_out == NULL || op_count > SIZE_MAX / REPLAY_OP_BYTES - 1u) {
    return T64_REPLAY_INVALID_ARGUMENT;
  }
  id = &header->identity;
  capacity = REPLAY_HEADER_BYTES + op_count * REPLAY_OP_BYTES + 64u;
  buffer = (uint8_t *)malloc(capacity);
  if (buffer == NULL) return T64_REPLAY_ALLOCATION_FAILED;
  t64_jsonw_init(&writer, (char *)buffer, capacity);
  if (!W_OK(t64_jsonw_begin_object(&writer)) ||
      !write_text(&writer, "boot", "cold-admit") ||
      !write_text(&writer, "build_id", id->build_id) ||
      !write_text(&writer, "content_sha256", id->content_sha256) ||
      !write_u64_text(&writer, "content_size", id->content_size) ||
      !write_int(&writer, "descriptor_version", (int64_t)id->descriptor_version) ||
      !write_int(&writer, "input_schema_version", 1) ||
      !write_int(&writer, "machine_model_version",
                 (int64_t)id->machine_model_version) ||
      !write_key(&writer, "ops") || !W_OK(t64_jsonw_begin_array(&writer))) {
    goto failed;
  }
  for (i = 0u; i < op_count; i++) {
    if (!write_op(&writer, &ops[i])) goto failed;
  }
  if (!W_OK(t64_jsonw_end_array(&writer)) ||
      !write_text(&writer, "profile_digest", id->profile_digest) ||
      !write_text(&writer, "profile_id", id->profile_id) ||
      !write_key(&writer, "save_init") ||
      !W_OK(t64_jsonw_begin_object(&writer))) {
    goto failed;
  }
  if (id->save_kind == T64_REPLAY_SAVE_IMAGE) {
    if (!write_text(&writer, "kind", "image") ||
        !write_u64_text(&writer, "length", id->save_length) ||
        !write_text(&writer, "sha256", id->save_sha256)) {
      goto failed;
    }
  } else if (id->save_kind == T64_REPLAY_SAVE_BLANK) {
    if (!write_text(&writer, "kind", "blank")) goto failed;
  } else {
    goto failed;
  }
  if (!W_OK(t64_jsonw_end_object(&writer)) ||
      !write_text(&writer, "schema", T64_REPLAY_FORMAT_NAME) ||
      !write_int(&writer, "schema_version", (int64_t)T64_REPLAY_SCHEMA_VERSION) ||
      !write_text(&writer, "source_revision", header->source_revision) ||
      !write_int(&writer, "state_serialization_version",
                 (int64_t)id->state_serialization_version) ||
      !W_OK(t64_jsonw_end_object(&writer)) ||
      !W_OK(t64_jsonw_finish(&writer, &length))) {
    goto failed;
  }
  *bytes_out = buffer;
  *length_out = length;
  return T64_REPLAY_OK;
failed:
  free(buffer);
  return T64_REPLAY_CANONICALIZATION_FAILED;
}

/* ---- parse (public) ------------------------------------------------------------------ */

void t64_replay_destroy(t64_replay *replay) {
  if (replay != NULL) {
    free(replay->ops);
    free(replay->canonical);
    free(replay);
  }
}

t64_replay_status t64_replay_parse_limits(const uint8_t *bytes, size_t length,
                                          const t64_replay_limits *limits,
                                          t64_replay **replay_out,
                                          t64_replay_error *error_out) {
  t64_json_limits json_limits;
  t64_json_doc doc = {0};
  t64_json_error json_error = {T64_JSON_OK, 0u};
  t64_replay_header header;
  const t64_json_node *ops_node;
  const t64_json_node *node;
  t64_replay_op *ops = NULL;
  t64_replay *replay = NULL;
  t64_replay_status status;
  size_t count;
  size_t i;
  if (replay_out == NULL || bytes == NULL || limits == NULL) {
    return refuse(error_out, T64_REPLAY_INVALID_ARGUMENT, SIZE_MAX);
  }
  if (length > limits->max_bytes) {
    return refuse(error_out, T64_REPLAY_SIZE_LIMIT, SIZE_MAX);
  }
  json_limits.max_input = limits->max_bytes;
  json_limits.max_depth = REPLAY_JSON_DEPTH;
  json_limits.max_string = REPLAY_JSON_STRING;
  json_limits.max_nodes = REPLAY_NODES_PER_OP * limits->max_ops + REPLAY_NODES_BASE;
  if (t64_json_parse(bytes, length, &json_limits, &doc, &json_error) != 0) {
    return refuse(error_out, T64_REPLAY_INVALID_JSON, json_error.offset);
  }
  status = parse_header(doc.root, &header);
  if (status != T64_REPLAY_OK) {
    t64_json_free(&doc);
    return refuse(error_out, status, SIZE_MAX);
  }
  ops_node = t64_json_get(doc.root, "ops");
  count = ops_node->count;
  /* The caps are taken from the node counts and checked before any decoded op
   * array exists (D-41). */
  if (count > limits->max_ops) {
    t64_json_free(&doc);
    return refuse(error_out, T64_REPLAY_OP_LIMIT, count);
  }
  {
    size_t checkpoints = 0u;
    for (node = ops_node->first; node != NULL; node = node->next) {
      if (node->type == T64_JSON_OBJECT &&
          string_equals(t64_json_get(node, "op"), "checkpoint")) {
        checkpoints++;
      }
    }
    if (checkpoints > limits->max_checkpoints) {
      t64_json_free(&doc);
      return refuse(error_out, T64_REPLAY_CHECKPOINT_LIMIT, checkpoints);
    }
  }
  if (count > 0u) {
    if (count > SIZE_MAX / sizeof *ops) {
      t64_json_free(&doc);
      return refuse(error_out, T64_REPLAY_ALLOCATION_FAILED, SIZE_MAX);
    }
    ops = (t64_replay_op *)calloc(count, sizeof *ops);
    if (ops == NULL) {
      t64_json_free(&doc);
      return refuse(error_out, T64_REPLAY_ALLOCATION_FAILED, SIZE_MAX);
    }
  }
  i = 0u;
  {
    bool seen_end = false;
    for (node = ops_node->first; node != NULL; node = node->next, i++) {
      status = parse_op(node, &ops[i]);
      /* Ordering in array order: nothing after the end op, `at` never
       * decreasing, an event stamped no earlier than its submission tick. */
      if (status == T64_REPLAY_OK && seen_end) {
        status = T64_REPLAY_END_NOT_LAST;
      } else if (status == T64_REPLAY_OK && i > 0u && ops[i].at < ops[i - 1u].at) {
        status = T64_REPLAY_AT_REVERSED;
      } else if (status == T64_REPLAY_OK && ops[i].kind == T64_REPLAY_OP_INPUT &&
                 ops[i].event_tick < ops[i].at) {
        status = T64_REPLAY_EVENT_BEFORE_AT;
      }
      if (status != T64_REPLAY_OK) {
        free(ops);
        t64_json_free(&doc);
        return refuse(error_out, status, i);
      }
      if (ops[i].kind == T64_REPLAY_OP_END) seen_end = true;
    }
    if (!seen_end) {
      free(ops);
      t64_json_free(&doc);
      return refuse(error_out, T64_REPLAY_END_MISSING, count);
    }
  }
  t64_json_free(&doc);
  replay = (t64_replay *)calloc(1u, sizeof *replay);
  if (replay == NULL) {
    free(ops);
    return refuse(error_out, T64_REPLAY_ALLOCATION_FAILED, SIZE_MAX);
  }
  replay->header = header;
  replay->ops = ops;
  replay->op_count = count;
  for (i = 0u; i < count; i++) {
    if (ops[i].kind == T64_REPLAY_OP_CHECKPOINT) replay->checkpoint_count++;
  }
  if (t64_replay_encode(&header, ops, count, &replay->canonical,
                        &replay->canonical_length) != T64_REPLAY_OK) {
    t64_replay_destroy(replay);
    return refuse(error_out, T64_REPLAY_CANONICALIZATION_FAILED, SIZE_MAX);
  }
  {
    t64_sha256_ctx ctx;
    t64_sha256_init(&ctx);
    t64_sha256_update(&ctx, replay->canonical, replay->canonical_length);
    t64_sha256_final(&ctx, replay->sha256);
  }
  *replay_out = replay;
  return refuse(error_out, T64_REPLAY_OK, SIZE_MAX);
}

t64_replay_status t64_replay_parse(const uint8_t *bytes, size_t length,
                                   t64_replay **replay_out,
                                   t64_replay_error *error_out) {
  t64_replay_limits limits = {T64_REPLAY_MAX_BYTES, T64_REPLAY_MAX_OPS,
                              T64_REPLAY_MAX_CHECKPOINTS};
  return t64_replay_parse_limits(bytes, length, &limits, replay_out, error_out);
}

const t64_replay_header *t64_replay_header_get(const t64_replay *replay) {
  return replay == NULL ? NULL : &replay->header;
}

size_t t64_replay_op_count(const t64_replay *replay) {
  return replay == NULL ? 0u : replay->op_count;
}

bool t64_replay_op_get(const t64_replay *replay, size_t index,
                       t64_replay_op *op_out) {
  if (replay == NULL || op_out == NULL || index >= replay->op_count) {
    return false;
  }
  *op_out = replay->ops[index];
  return true;
}

const uint8_t *t64_replay_canonical(const t64_replay *replay,
                                    size_t *length_out) {
  if (replay == NULL || length_out == NULL) return NULL;
  *length_out = replay->canonical_length;
  return replay->canonical;
}

const uint8_t *t64_replay_sha256(const t64_replay *replay) {
  return replay == NULL ? NULL : replay->sha256;
}

/* ---- recorder ------------------------------------------------------------------------ */

struct t64_replay_recorder {
  t64_replay_header header;
  t64_replay_limits limits;
  t64_replay_op *ops;
  size_t count;
  size_t capacity;
  size_t checkpoints;
  bool ended;
  t64_replay_status error;
};

t64_replay_recorder *t64_replay_recorder_create(
    const t64_replay_header *header, const t64_replay_limits *limits) {
  t64_replay_recorder *recorder;
  if (header == NULL) return NULL;
  recorder = (t64_replay_recorder *)calloc(1u, sizeof *recorder);
  if (recorder == NULL) return NULL;
  recorder->header = *header;
  if (limits != NULL) {
    recorder->limits = *limits;
  } else {
    recorder->limits.max_bytes = T64_REPLAY_MAX_BYTES;
    recorder->limits.max_ops = T64_REPLAY_MAX_OPS;
    recorder->limits.max_checkpoints = T64_REPLAY_MAX_CHECKPOINTS;
  }
  return recorder;
}

void t64_replay_recorder_destroy(t64_replay_recorder *recorder) {
  if (recorder != NULL) {
    free(recorder->ops);
    free(recorder);
  }
}

static t64_replay_op *recorder_append(t64_replay_recorder *recorder) {
  if (recorder->count == recorder->capacity) {
    size_t grown = recorder->capacity == 0u ? 32u : recorder->capacity * 2u;
    t64_replay_op *bigger;
    if (grown < recorder->capacity || grown > SIZE_MAX / sizeof *bigger) {
      recorder->error = T64_REPLAY_ALLOCATION_FAILED;
      return NULL;
    }
    bigger = (t64_replay_op *)realloc(recorder->ops, grown * sizeof *bigger);
    if (bigger == NULL) {
      recorder->error = T64_REPLAY_ALLOCATION_FAILED;
      return NULL;
    }
    recorder->ops = bigger;
    recorder->capacity = grown;
  }
  memset(&recorder->ops[recorder->count], 0, sizeof recorder->ops[0]);
  return &recorder->ops[recorder->count];
}

t64_status t64_replay_recorder_queue_input(t64_replay_recorder *recorder,
                                           t64_machine *machine, uint64_t at,
                                           uint64_t event_tick, uint32_t port,
                                           uint16_t buttons, int32_t axis_x,
                                           int32_t axis_y) {
  t64_status status;
  t64_replay_op *op;
  if (recorder == NULL || machine == NULL) return T64_STATUS_INVALID_ARGUMENT;
  if (recorder->error != T64_REPLAY_OK || recorder->ended) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  /* An append that would pass the op cap (one slot stays free for the end op)
   * is refused before the machine call: the replay is never published. */
  if (recorder->count + 2u > recorder->limits.max_ops) {
    recorder->error = T64_REPLAY_OP_LIMIT;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  op = recorder_append(recorder);
  if (op == NULL) return T64_STATUS_RESOURCE_EXHAUSTED;
  status = t64_machine_queue_input(machine, event_tick, port, buttons, axis_x,
                                   axis_y);
  if (status != T64_STATUS_OK) return status;
  op->kind = T64_REPLAY_OP_INPUT;
  op->at = at;
  op->event_tick = event_tick;
  op->port = port;
  op->buttons = buttons;
  op->axis_x = axis_x;
  op->axis_y = axis_y;
  recorder->count++;
  return T64_STATUS_OK;
}

t64_replay_status t64_replay_recorder_checkpoint(
    t64_replay_recorder *recorder, const t64_machine *machine, uint64_t at) {
  t64_replay_op *op;
  if (recorder == NULL || machine == NULL) return T64_REPLAY_INVALID_ARGUMENT;
  if (recorder->error != T64_REPLAY_OK) return recorder->error;
  if (recorder->ended) return T64_REPLAY_END_NOT_LAST;
  if (recorder->count + 2u > recorder->limits.max_ops) {
    recorder->error = T64_REPLAY_OP_LIMIT;
    return recorder->error;
  }
  if (recorder->checkpoints + 1u > recorder->limits.max_checkpoints) {
    recorder->error = T64_REPLAY_CHECKPOINT_LIMIT;
    return recorder->error;
  }
  op = recorder_append(recorder);
  if (op == NULL) return recorder->error;
  op->kind = T64_REPLAY_OP_CHECKPOINT;
  op->at = at;
  if (t64_machine_digest(machine, op->digest) != T64_STATUS_OK) {
    recorder->error = T64_REPLAY_INVALID_ARGUMENT;
    return recorder->error;
  }
  recorder->count++;
  recorder->checkpoints++;
  return T64_REPLAY_OK;
}

t64_replay_status t64_replay_recorder_end(t64_replay_recorder *recorder,
                                          const t64_machine *machine,
                                          uint64_t at, t64_replay_stop stop) {
  t64_replay_op *op;
  if (recorder == NULL || machine == NULL ||
      (stop != T64_REPLAY_STOP_GUEST_BOUND &&
       stop != T64_REPLAY_STOP_GUEST_COMPLETION)) {
    return T64_REPLAY_INVALID_ARGUMENT;
  }
  if (recorder->error != T64_REPLAY_OK) return recorder->error;
  if (recorder->ended) return T64_REPLAY_END_NOT_LAST;
  if (recorder->count + 1u > recorder->limits.max_ops) {
    recorder->error = T64_REPLAY_OP_LIMIT;
    return recorder->error;
  }
  op = recorder_append(recorder);
  if (op == NULL) return recorder->error;
  op->kind = T64_REPLAY_OP_END;
  op->at = at;
  op->stop = stop;
  if (t64_machine_digest(machine, op->digest) != T64_STATUS_OK) {
    recorder->error = T64_REPLAY_INVALID_ARGUMENT;
    return recorder->error;
  }
  recorder->count++;
  recorder->ended = true;
  return T64_REPLAY_OK;
}

t64_replay_status t64_replay_recorder_error(
    const t64_replay_recorder *recorder) {
  return recorder == NULL ? T64_REPLAY_INVALID_ARGUMENT : recorder->error;
}

t64_replay_status t64_replay_recorder_finish(t64_replay_recorder *recorder,
                                             t64_replay **replay_out,
                                             t64_replay_error *error_out) {
  uint8_t *bytes = NULL;
  size_t length = 0u;
  t64_replay_status status;
  if (recorder == NULL || replay_out == NULL) {
    return refuse(error_out, T64_REPLAY_INVALID_ARGUMENT, SIZE_MAX);
  }
  if (recorder->error != T64_REPLAY_OK) {
    return refuse(error_out, recorder->error, SIZE_MAX);
  }
  if (!recorder->ended) return refuse(error_out, T64_REPLAY_END_MISSING, SIZE_MAX);
  status = t64_replay_encode(&recorder->header, recorder->ops, recorder->count,
                             &bytes, &length);
  if (status != T64_REPLAY_OK) return refuse(error_out, status, SIZE_MAX);
  /* The recorder re-parses its own output as a self-check. */
  status = t64_replay_parse_limits(bytes, length, &recorder->limits,
                                   replay_out, error_out);
  free(bytes);
  return status;
}

/* ---- live identity -------------------------------------------------------------------- */

/* Blank content of each device (the policy values of core/t64_save.c): EEPROM
 * bytes read 0xff and SRAM bytes read 0x00 before any write. */
bool t64_replay_live_save_init(const t64_machine *machine,
                               t64_replay_live_identity *identity) {
  const t64_profile_descriptor *profile = NULL;
  t64_save_info info;
  uint8_t *bytes;
  size_t length = 0u;
  uint8_t blank;
  size_t i;
  bool is_blank = true;
  if (machine == NULL || identity == NULL ||
      t64_machine_profile(machine, &profile) != T64_STATUS_OK ||
      profile == NULL) {
    return false;
  }
  identity->save_kind = T64_REPLAY_SAVE_BLANK;
  identity->save_length = 0u;
  identity->save_sha256[0] = '\0';
  if (profile->save_kind == T64_SAVE_NONE) return true;
  if (t64_machine_save_info(machine, profile->save_kind, &info) !=
          T64_STATUS_OK ||
      info.size_bytes == 0u) {
    return false;
  }
  bytes = (uint8_t *)malloc(info.size_bytes);
  if (bytes == NULL) return false;
  if (t64_machine_save_export(machine, profile->save_kind, T64_SAVE_FORMAT_RAW,
                              bytes, info.size_bytes, &length) !=
          T64_STATUS_OK ||
      length != info.size_bytes) {
    free(bytes);
    return false;
  }
  blank = profile->save_kind == T64_SAVE_SRAM_32K ? UINT8_C(0x00)
                                                  : UINT8_C(0xff);
  for (i = 0u; i < length; i++) {
    if (bytes[i] != blank) {
      is_blank = false;
      break;
    }
  }
  if (!is_blank) {
    uint8_t digest[T64_SHA256_DIGEST_SIZE];
    t64_sha256_ctx ctx;
    t64_sha256_init(&ctx);
    t64_sha256_update(&ctx, bytes, length);
    t64_sha256_final(&ctx, digest);
    identity->save_kind = T64_REPLAY_SAVE_IMAGE;
    identity->save_length = (uint64_t)length;
    t64_sha256_hex(digest, identity->save_sha256);
  }
  free(bytes);
  return true;
}

bool t64_replay_live_identity_compose(const t64_machine *machine,
                                      t64_replay_live_identity *identity) {
  const t64_profile_descriptor *profile = NULL;
  uint8_t digest[T64_DIGEST_SIZE];
  size_t size = 0u;
  const char *build_id = t64_build_identity();
  if (machine == NULL || identity == NULL || build_id == NULL ||
      strlen(build_id) >= sizeof identity->build_id ||
      t64_machine_profile(machine, &profile) != T64_STATUS_OK ||
      profile == NULL || strlen(profile->profile_id) >= sizeof identity->profile_id ||
      t64_profile_hash(profile, digest) != T64_STATUS_OK) {
    return false;
  }
  memset(identity, 0, sizeof *identity);
  (void)strcpy(identity->profile_id, profile->profile_id);
  digest_to_hex(digest, identity->profile_digest);
  identity->descriptor_version = profile->descriptor_version;
  identity->machine_model_version = profile->machine_model_version;
  identity->state_serialization_version = profile->state_serialization_version;
  (void)strcpy(identity->build_id, build_id);
  if (t64_machine_content_digest(machine, digest) != T64_STATUS_OK ||
      t64_machine_rom_size(machine, &size) != T64_STATUS_OK) {
    return false;
  }
  digest_to_hex(digest, identity->content_sha256);
  identity->content_size = (uint64_t)size;
  return t64_replay_live_save_init(machine, identity);
}

/* Compares in the fixed order profile, build, content, save_init (D-31): the
 * three families are separate statuses and the field names the first mismatch.
 * Every comparison is against the live machine; nothing is bound to a host
 * path, time or user name. */
t64_replay_bind_status t64_replay_bind(const t64_replay_header *recorded,
                                       const t64_replay_live_identity *live,
                                       const char **field_out) {
  const t64_replay_live_identity *a;
  if (field_out != NULL) *field_out = T64_REPLAY_FIELD_NONE;
  if (recorded == NULL || live == NULL) return T64_REPLAY_BIND_PROFILE;
  a = &recorded->identity;
#define BIND_MISMATCH(status, field)                 \
  do {                                               \
    if (field_out != NULL) *field_out = (field);     \
    return (status);                                 \
  } while (0)
  if (strcmp(a->profile_id, live->profile_id) != 0) {
    BIND_MISMATCH(T64_REPLAY_BIND_PROFILE, T64_REPLAY_FIELD_PROFILE_ID);
  }
  if (strcmp(a->profile_digest, live->profile_digest) != 0) {
    BIND_MISMATCH(T64_REPLAY_BIND_PROFILE, T64_REPLAY_FIELD_PROFILE_DIGEST);
  }
  if (a->descriptor_version != live->descriptor_version) {
    BIND_MISMATCH(T64_REPLAY_BIND_PROFILE, T64_REPLAY_FIELD_DESCRIPTOR_VERSION);
  }
  if (a->machine_model_version != live->machine_model_version) {
    BIND_MISMATCH(T64_REPLAY_BIND_PROFILE,
                  T64_REPLAY_FIELD_MACHINE_MODEL_VERSION);
  }
  if (a->state_serialization_version != live->state_serialization_version) {
    BIND_MISMATCH(T64_REPLAY_BIND_PROFILE,
                  T64_REPLAY_FIELD_STATE_SERIALIZATION_VERSION);
  }
  if (strcmp(a->build_id, live->build_id) != 0) {
    BIND_MISMATCH(T64_REPLAY_BIND_BUILD, T64_REPLAY_FIELD_BUILD_ID);
  }
  if (strcmp(a->content_sha256, live->content_sha256) != 0) {
    BIND_MISMATCH(T64_REPLAY_BIND_CONTENT, T64_REPLAY_FIELD_CONTENT_SHA256);
  }
  if (a->content_size != live->content_size) {
    BIND_MISMATCH(T64_REPLAY_BIND_CONTENT, T64_REPLAY_FIELD_CONTENT_SIZE);
  }
  if (a->save_kind != live->save_kind ||
      (a->save_kind == T64_REPLAY_SAVE_IMAGE &&
       (a->save_length != live->save_length ||
        strcmp(a->save_sha256, live->save_sha256) != 0))) {
    BIND_MISMATCH(T64_REPLAY_BIND_SAVE_INIT, T64_REPLAY_FIELD_SAVE_INIT);
  }
#undef BIND_MISMATCH
  return T64_REPLAY_BIND_OK;
}

/* ---- report ----------------------------------------------------------------------------- */

const char *t64_replay_result_name(t64_replay_result result) {
  switch (result) {
  case T64_REPLAY_RESULT_MATCH: return "match";
  case T64_REPLAY_RESULT_DIVERGED: return "diverged";
  case T64_REPLAY_RESULT_IDENTITY_MISMATCH: return "identity-mismatch";
  case T64_REPLAY_RESULT_REFUSED: return "refused";
  }
  return "unknown";
}

const char *t64_replay_detail_name(t64_replay_detail detail) {
  switch (detail) {
  case T64_REPLAY_DETAIL_NONE: return "none";
  case T64_REPLAY_DETAIL_ARTIFACT_INVALID: return "artifact-invalid";
  case T64_REPLAY_DETAIL_INPUT_REFUSED: return "input-refused";
  case T64_REPLAY_DETAIL_QUEUE_FULL: return "queue-full";
  case T64_REPLAY_DETAIL_ADVANCE_INVALID: return "advance-invalid";
  case T64_REPLAY_DETAIL_STOP_EARLY: return "stop-early";
  case T64_REPLAY_DETAIL_STOP_KIND: return "stop-kind";
  case T64_REPLAY_DETAIL_DIGEST: return "digest";
  }
  return "unknown";
}

const char *t64_replay_report_status_name(t64_replay_report_status status) {
  switch (status) {
  case T64_REPLAY_REPORT_OK: return "ok";
  case T64_REPLAY_REPORT_INVALID_ARGUMENT: return "invalid-argument";
  case T64_REPLAY_REPORT_SIZE_LIMIT: return "size-limit";
  case T64_REPLAY_REPORT_INVALID_JSON: return "invalid-json";
  case T64_REPLAY_REPORT_INVALID_SCHEMA: return "invalid-schema";
  case T64_REPLAY_REPORT_BAD_RESULT: return "bad-result";
  case T64_REPLAY_REPORT_BAD_DETAIL: return "bad-detail";
  case T64_REPLAY_REPORT_BAD_FIELD: return "bad-field";
  case T64_REPLAY_REPORT_BAD_VALUE: return "bad-value";
  case T64_REPLAY_REPORT_MATCH_SHAPE: return "match-shape";
  case T64_REPLAY_REPORT_DIVERGED_SHAPE: return "diverged-shape";
  case T64_REPLAY_REPORT_IDENTITY_SHAPE: return "identity-shape";
  case T64_REPLAY_REPORT_REFUSED_SHAPE: return "refused-shape";
  case T64_REPLAY_REPORT_COUNT_ORDER: return "count-order";
  }
  return "unknown";
}

/* The closed mismatch field set (plus none). */
static const char *const report_fields[] = {
    T64_REPLAY_FIELD_NONE,
    T64_REPLAY_FIELD_PROFILE_ID,
    T64_REPLAY_FIELD_PROFILE_DIGEST,
    T64_REPLAY_FIELD_DESCRIPTOR_VERSION,
    T64_REPLAY_FIELD_MACHINE_MODEL_VERSION,
    T64_REPLAY_FIELD_STATE_SERIALIZATION_VERSION,
    T64_REPLAY_FIELD_BUILD_ID,
    T64_REPLAY_FIELD_CONTENT_SHA256,
    T64_REPLAY_FIELD_CONTENT_SIZE,
    T64_REPLAY_FIELD_SAVE_INIT};
#define REPORT_FIELD_COUNT (sizeof report_fields / sizeof report_fields[0])

static bool report_field_known(const char *field) {
  size_t i;
  for (i = 0u; i < REPORT_FIELD_COUNT; i++) {
    if (strcmp(field, report_fields[i]) == 0) return true;
  }
  return false;
}

/* The cross-field rules shared by the writer and the checker. */
static t64_replay_report_status validate_report(const t64_replay_report *r) {
  const bool digests = r->has_actual_digest || r->has_expected_digest;
  const bool none_field = strcmp(r->field, T64_REPLAY_FIELD_NONE) == 0;
  if (r->result < T64_REPLAY_RESULT_MATCH ||
      r->result > T64_REPLAY_RESULT_REFUSED) {
    return T64_REPLAY_REPORT_BAD_RESULT;
  }
  if (r->detail < T64_REPLAY_DETAIL_NONE ||
      r->detail > T64_REPLAY_DETAIL_DIGEST) {
    return T64_REPLAY_REPORT_BAD_DETAIL;
  }
  if (!report_field_known(r->field)) return T64_REPLAY_REPORT_BAD_FIELD;
  if (r->checkpoints_matched > r->ops_applied) {
    return T64_REPLAY_REPORT_COUNT_ORDER;
  }
  switch (r->result) {
  case T64_REPLAY_RESULT_MATCH:
    if (!none_field || r->detail != T64_REPLAY_DETAIL_NONE || r->has_op_index ||
        r->has_at || digests) {
      return T64_REPLAY_REPORT_MATCH_SHAPE;
    }
    break;
  case T64_REPLAY_RESULT_DIVERGED:
    if (!none_field ||
        (r->detail != T64_REPLAY_DETAIL_DIGEST &&
         r->detail != T64_REPLAY_DETAIL_STOP_EARLY &&
         r->detail != T64_REPLAY_DETAIL_STOP_KIND) ||
        !r->has_op_index || !r->has_at || !r->has_expected_digest ||
        !r->has_actual_digest) {
      return T64_REPLAY_REPORT_DIVERGED_SHAPE;
    }
    break;
  case T64_REPLAY_RESULT_IDENTITY_MISMATCH:
    if (none_field || r->detail != T64_REPLAY_DETAIL_NONE || r->has_op_index ||
        r->has_at || digests) {
      return T64_REPLAY_REPORT_IDENTITY_SHAPE;
    }
    break;
  case T64_REPLAY_RESULT_REFUSED:
    if (!none_field ||
        (r->detail != T64_REPLAY_DETAIL_ARTIFACT_INVALID &&
         r->detail != T64_REPLAY_DETAIL_INPUT_REFUSED &&
         r->detail != T64_REPLAY_DETAIL_QUEUE_FULL &&
         r->detail != T64_REPLAY_DETAIL_ADVANCE_INVALID) ||
        digests) {
      return T64_REPLAY_REPORT_REFUSED_SHAPE;
    }
    break;
  }
  return T64_REPLAY_REPORT_OK;
}

static bool write_nullable_digest(t64_jsonw *w, const char *key, bool present,
                                  const uint8_t digest[T64_DIGEST_SIZE]) {
  if (present) return write_digest(w, key, digest);
  return write_key(w, key) && W_OK(t64_jsonw_null(w));
}

t64_replay_report_status t64_replay_report_encode(
    const t64_replay_report *report, uint8_t *bytes_out, size_t capacity,
    size_t *length_out) {
  t64_jsonw writer;
  size_t length = 0u;
  t64_replay_report_status status;
  if (report == NULL || bytes_out == NULL || length_out == NULL) {
    return T64_REPLAY_REPORT_INVALID_ARGUMENT;
  }
  status = validate_report(report);
  if (status != T64_REPLAY_REPORT_OK) return status;
  t64_jsonw_init(&writer, (char *)bytes_out, capacity);
  if (!W_OK(t64_jsonw_begin_object(&writer)) ||
      !write_nullable_digest(&writer, "actual_digest",
                             report->has_actual_digest, report->actual_digest)) {
    goto failed;
  }
  if (report->has_at) {
    if (!write_u64_text(&writer, "at", report->at)) goto failed;
  } else if (!write_key(&writer, "at") || !W_OK(t64_jsonw_null(&writer))) {
    goto failed;
  }
  if (!write_int(&writer, "checkpoints_matched",
                 (int64_t)report->checkpoints_matched) ||
      !write_text(&writer, "detail", t64_replay_detail_name(report->detail)) ||
      !write_nullable_digest(&writer, "expected_digest",
                             report->has_expected_digest,
                             report->expected_digest) ||
      !write_text(&writer, "field", report->field) ||
      !write_u64_text(&writer, "live_tick", report->live_tick)) {
    goto failed;
  }
  if (report->has_op_index) {
    if (!write_int(&writer, "op_index", (int64_t)report->op_index)) goto failed;
  } else if (!write_key(&writer, "op_index") || !W_OK(t64_jsonw_null(&writer))) {
    goto failed;
  }
  if (!write_int(&writer, "ops_applied", (int64_t)report->ops_applied) ||
      !write_nullable_digest(&writer, "replay_sha256",
                             report->has_replay_sha256,
                             report->replay_sha256) ||
      !write_text(&writer, "result", t64_replay_result_name(report->result)) ||
      !write_text(&writer, "schema", T64_REPLAY_REPORT_FORMAT_NAME) ||
      !write_int(&writer, "schema_version",
                 (int64_t)T64_REPLAY_REPORT_SCHEMA_VERSION) ||
      !W_OK(t64_jsonw_end_object(&writer)) ||
      !W_OK(t64_jsonw_finish(&writer, &length)) ||
      length > T64_REPLAY_REPORT_MAX_BYTES) {
    goto failed;
  }
  *length_out = length;
  return T64_REPLAY_REPORT_OK;
failed:
  return T64_REPLAY_REPORT_INVALID_ARGUMENT;
}

/* A nullable member: JSON null or a node of the wanted type. */
static bool node_is_null(const t64_json_node *node) {
  return node != NULL && node->type == T64_JSON_NULL;
}

t64_replay_report_status t64_replay_report_check(const uint8_t *bytes,
                                                 size_t length,
                                                 t64_replay_report *report_out) {
  static const char *const keys[] = {
      "actual_digest", "at",      "checkpoints_matched", "detail",
      "expected_digest", "field", "live_tick",           "op_index",
      "ops_applied",   "replay_sha256", "result",        "schema",
      "schema_version"};
  const t64_json_limits limits = {T64_REPLAY_REPORT_MAX_BYTES, 2u, 64u, 64u};
  t64_json_doc doc = {0};
  t64_replay_report r;
  const t64_json_node *node;
  const char *bad = NULL;
  int64_t value;
  size_t i;
  if (bytes == NULL) return T64_REPLAY_REPORT_INVALID_ARGUMENT;
  if (length > T64_REPLAY_REPORT_MAX_BYTES) return T64_REPLAY_REPORT_SIZE_LIMIT;
  if (t64_json_parse(bytes, length, &limits, &doc, NULL) != 0) {
    return T64_REPLAY_REPORT_INVALID_JSON;
  }
  memset(&r, 0, sizeof r);
  {
    t64_replay_report_status status = T64_REPLAY_REPORT_OK;
    if (t64_json_check_keys(doc.root, keys, 13u, keys, 13u, &bad) !=
        T64_JSON_KEYS_OK) {
      status = T64_REPLAY_REPORT_INVALID_SCHEMA;
    } else if (!string_equals(t64_json_get(doc.root, "schema"),
                              T64_REPLAY_REPORT_FORMAT_NAME) ||
               !node_int(t64_json_get(doc.root, "schema_version"), 1, 1,
                         &value)) {
      status = T64_REPLAY_REPORT_INVALID_SCHEMA;
    } else {
      static const char *const string_keys[] = {"detail", "field", "live_tick",
                                                "result"};
      static const char *const nullable_strings[] = {
          "actual_digest", "at", "expected_digest", "replay_sha256"};
      for (i = 0u; i < sizeof string_keys / sizeof string_keys[0]; i++) {
        node = t64_json_get(doc.root, string_keys[i]);
        if (node == NULL || node->type != T64_JSON_STRING) {
          status = T64_REPLAY_REPORT_INVALID_SCHEMA;
        }
      }
      for (i = 0u; i < sizeof nullable_strings / sizeof nullable_strings[0]; i++) {
        node = t64_json_get(doc.root, nullable_strings[i]);
        if (node == NULL ||
            (node->type != T64_JSON_STRING && node->type != T64_JSON_NULL)) {
          status = T64_REPLAY_REPORT_INVALID_SCHEMA;
        }
      }
      node = t64_json_get(doc.root, "op_index");
      if (node == NULL ||
          (node->type != T64_JSON_INT && node->type != T64_JSON_NULL)) {
        status = T64_REPLAY_REPORT_INVALID_SCHEMA;
      }
      node = t64_json_get(doc.root, "checkpoints_matched");
      if (node == NULL || node->type != T64_JSON_INT) {
        status = T64_REPLAY_REPORT_INVALID_SCHEMA;
      }
      node = t64_json_get(doc.root, "ops_applied");
      if (node == NULL || node->type != T64_JSON_INT) {
        status = T64_REPLAY_REPORT_INVALID_SCHEMA;
      }
    }
    if (status != T64_REPLAY_REPORT_OK) {
      t64_json_free(&doc);
      return status;
    }
  }
  /* Closed enums. */
  {
    const t64_json_node *result = t64_json_get(doc.root, "result");
    const t64_json_node *detail = t64_json_get(doc.root, "detail");
    const t64_json_node *field = t64_json_get(doc.root, "field");
    t64_replay_result found_result = (t64_replay_result)0;
    t64_replay_detail found_detail = (t64_replay_detail)-1;
    int k;
    for (k = T64_REPLAY_RESULT_MATCH; k <= T64_REPLAY_RESULT_REFUSED; k++) {
      if (string_equals(result, t64_replay_result_name((t64_replay_result)k))) {
        found_result = (t64_replay_result)k;
      }
    }
    for (k = T64_REPLAY_DETAIL_NONE; k <= T64_REPLAY_DETAIL_DIGEST; k++) {
      if (string_equals(detail, t64_replay_detail_name((t64_replay_detail)k))) {
        found_detail = (t64_replay_detail)k;
      }
    }
    if ((int)found_result == 0) {
      t64_json_free(&doc);
      return T64_REPLAY_REPORT_BAD_RESULT;
    }
    if ((int)found_detail < 0) {
      t64_json_free(&doc);
      return T64_REPLAY_REPORT_BAD_DETAIL;
    }
    if (field->len >= sizeof r.field) {
      t64_json_free(&doc);
      return T64_REPLAY_REPORT_BAD_FIELD;
    }
    memcpy(r.field, field->str, field->len);
    r.field[field->len] = '\0';
    if (!report_field_known(r.field)) {
      t64_json_free(&doc);
      return T64_REPLAY_REPORT_BAD_FIELD;
    }
    r.result = found_result;
    r.detail = found_detail;
  }
  /* Values. */
  {
    const t64_json_node *actual = t64_json_get(doc.root, "actual_digest");
    const t64_json_node *expected = t64_json_get(doc.root, "expected_digest");
    const t64_json_node *sha = t64_json_get(doc.root, "replay_sha256");
    const t64_json_node *at = t64_json_get(doc.root, "at");
    const t64_json_node *op_index = t64_json_get(doc.root, "op_index");
    bool ok = true;
    r.has_actual_digest = !node_is_null(actual);
    r.has_expected_digest = !node_is_null(expected);
    r.has_replay_sha256 = !node_is_null(sha);
    r.has_at = !node_is_null(at);
    r.has_op_index = !node_is_null(op_index);
    ok = ok && (!r.has_actual_digest || node_hex_digest(actual, r.actual_digest));
    ok = ok && (!r.has_expected_digest ||
                node_hex_digest(expected, r.expected_digest));
    ok = ok && (!r.has_replay_sha256 || node_hex_digest(sha, r.replay_sha256));
    ok = ok && (!r.has_at || t64_json_u64_string(at, &r.at));
    ok = ok && t64_json_u64_string(t64_json_get(doc.root, "live_tick"),
                                   &r.live_tick);
    if (r.has_op_index) {
      ok = ok && op_index->num >= 0;
      r.op_index = (uint64_t)op_index->num;
    }
    node = t64_json_get(doc.root, "checkpoints_matched");
    ok = ok && node->num >= 0;
    r.checkpoints_matched = (uint64_t)node->num;
    node = t64_json_get(doc.root, "ops_applied");
    ok = ok && node->num >= 0;
    r.ops_applied = (uint64_t)node->num;
    if (!ok) {
      t64_json_free(&doc);
      return T64_REPLAY_REPORT_BAD_VALUE;
    }
  }
  t64_json_free(&doc);
  {
    t64_replay_report_status status = validate_report(&r);
    if (status == T64_REPLAY_REPORT_OK && report_out != NULL) *report_out = r;
    return status;
  }
}

/* ---- player ------------------------------------------------------------------------------- */

struct t64_replay_player {
  const t64_replay *replay;
  size_t index;
  uint64_t checkpoints_matched;
};

t64_replay_player *t64_replay_player_create(const t64_replay *replay) {
  t64_replay_player *player;
  if (replay == NULL || replay->op_count == 0u) return NULL;
  player = (t64_replay_player *)calloc(1u, sizeof *player);
  if (player != NULL) player->replay = replay;
  return player;
}

void t64_replay_player_destroy(t64_replay_player *player) { free(player); }

uint64_t t64_replay_player_next_at(const t64_replay_player *player) {
  if (player == NULL || player->index >= player->replay->op_count) {
    return UINT64_MAX;
  }
  return player->replay->ops[player->index].at;
}

uint64_t t64_replay_player_end_at(const t64_replay_player *player) {
  if (player == NULL) return UINT64_MAX;
  return player->replay->ops[player->replay->op_count - 1u].at;
}

static void report_begin(const t64_replay_player *player, uint64_t tick,
                         t64_replay_result result, t64_replay_detail detail,
                         t64_replay_report *report) {
  memset(report, 0, sizeof *report);
  report->result = result;
  report->detail = detail;
  (void)strcpy(report->field, T64_REPLAY_FIELD_NONE);
  report->checkpoints_matched = player->checkpoints_matched;
  report->ops_applied = (uint64_t)player->index;
  report->live_tick = tick;
  report->has_replay_sha256 = true;
  memcpy(report->replay_sha256, player->replay->sha256, T64_SHA256_DIGEST_SIZE);
}

void t64_replay_player_refuse(const t64_replay_player *player, uint64_t tick,
                              t64_replay_detail detail,
                              t64_replay_report *report) {
  if (player == NULL || report == NULL) return;
  report_begin(player, tick, T64_REPLAY_RESULT_REFUSED, detail, report);
}

static t64_replay_step_result fail_with_digests(
    const t64_replay_player *player, const t64_machine *machine, uint64_t tick,
    t64_replay_detail detail, size_t op_index, const t64_replay_op *op,
    t64_replay_report *report) {
  report_begin(player, tick, T64_REPLAY_RESULT_DIVERGED, detail, report);
  report->has_op_index = true;
  report->op_index = (uint64_t)op_index;
  report->has_at = true;
  report->at = op->at;
  report->has_expected_digest = true;
  memcpy(report->expected_digest, op->digest, T64_DIGEST_SIZE);
  report->has_actual_digest =
      t64_machine_digest(machine, report->actual_digest) == T64_STATUS_OK;
  return T64_REPLAY_STEP_FAILED;
}

t64_replay_step_result t64_replay_player_step(t64_replay_player *player,
                                              t64_machine *machine,
                                              uint64_t tick,
                                              t64_replay_stop terminal,
                                              t64_replay_report *report) {
  const t64_replay *replay;
  if (player == NULL || machine == NULL || report == NULL) {
    return T64_REPLAY_STEP_FAILED;
  }
  replay = player->replay;
  while (player->index < replay->op_count) {
    const t64_replay_op *op = &replay->ops[player->index];
    uint8_t actual[T64_DIGEST_SIZE];
    if (op->at < tick) {
      report_begin(player, tick, T64_REPLAY_RESULT_REFUSED,
                   T64_REPLAY_DETAIL_ADVANCE_INVALID, report);
      report->has_op_index = true;
      report->op_index = (uint64_t)player->index;
      return T64_REPLAY_STEP_FAILED;
    }
    if (op->at > tick) break;
    if (op->kind == T64_REPLAY_OP_INPUT) {
      t64_status status = t64_machine_queue_input(
          machine, op->event_tick, op->port, op->buttons, op->axis_x,
          op->axis_y);
      if (status != T64_STATUS_OK) {
        /* A refusal by the machine ends the play at the op: QUEUE_FULL is its
         * own detail, any other refusal is input-refused. */
        report_begin(player, tick, T64_REPLAY_RESULT_REFUSED,
                     status == T64_STATUS_QUEUE_FULL
                         ? T64_REPLAY_DETAIL_QUEUE_FULL
                         : T64_REPLAY_DETAIL_INPUT_REFUSED,
                     report);
        report->has_op_index = true;
        report->op_index = (uint64_t)player->index;
        report->has_at = true;
        report->at = op->at;
        return T64_REPLAY_STEP_FAILED;
      }
      player->index++;
    } else if (op->kind == T64_REPLAY_OP_CHECKPOINT) {
      if (t64_machine_digest(machine, actual) != T64_STATUS_OK ||
          memcmp(actual, op->digest, T64_DIGEST_SIZE) != 0) {
        return fail_with_digests(player, machine, tick,
                                 T64_REPLAY_DETAIL_DIGEST, player->index, op,
                                 report);
      }
      player->checkpoints_matched++;
      player->index++;
    } else {
      /* The end op: the terminal stop kind and the final digest. */
      if (terminal != op->stop) {
        return fail_with_digests(player, machine, tick,
                                 T64_REPLAY_DETAIL_STOP_KIND, player->index, op,
                                 report);
      }
      if (t64_machine_digest(machine, actual) != T64_STATUS_OK ||
          memcmp(actual, op->digest, T64_DIGEST_SIZE) != 0) {
        return fail_with_digests(player, machine, tick,
                                 T64_REPLAY_DETAIL_DIGEST, player->index, op,
                                 report);
      }
      player->index++;
      report_begin(player, tick, T64_REPLAY_RESULT_MATCH,
                   T64_REPLAY_DETAIL_NONE, report);
      return T64_REPLAY_STEP_MATCH;
    }
  }
  if (terminal != T64_REPLAY_STOP_NONE) {
    /* The machine stopped before the recorded end. */
    return fail_with_digests(player, machine, tick,
                             T64_REPLAY_DETAIL_STOP_EARLY,
                             replay->op_count - 1u,
                             &replay->ops[replay->op_count - 1u], report);
  }
  return T64_REPLAY_STEP_CONTINUE;
}

/* ---- in-process driver ---------------------------------------------------------------------- */

#define REPLAY_MAX_YIELDS_WITHOUT_PROGRESS 64u

t64_replay_step_result t64_replay_play(t64_machine *machine,
                                       const t64_replay *replay,
                                       t64_replay_play_options *options,
                                       t64_replay_report *report) {
  t64_replay_player *player;
  t64_replay_step_result result;
  uint64_t tick = 0u;
  uint64_t step;
  if (machine == NULL || replay == NULL || options == NULL || report == NULL ||
      options->advance_step == 0u) {
    return T64_REPLAY_STEP_FAILED;
  }
  step = options->advance_step;
  options->advance_calls = 0u;
  player = t64_replay_player_create(replay);
  if (player == NULL) return T64_REPLAY_STEP_FAILED;
  {
    /* The binding comes first: a mismatch ends the play before any guest
     * advance (zero advance calls, live tick 0). */
    t64_replay_live_identity live;
    const char *field = T64_REPLAY_FIELD_NONE;
    if (!t64_replay_live_identity_compose(machine, &live)) {
      report_begin(player, 0u, T64_REPLAY_RESULT_REFUSED,
                   T64_REPLAY_DETAIL_ADVANCE_INVALID, report);
      t64_replay_player_destroy(player);
      return T64_REPLAY_STEP_FAILED;
    }
    if (t64_replay_bind(t64_replay_header_get(replay), &live, &field) !=
        T64_REPLAY_BIND_OK) {
      report_begin(player, 0u, T64_REPLAY_RESULT_IDENTITY_MISMATCH,
                   T64_REPLAY_DETAIL_NONE, report);
      (void)snprintf(report->field, sizeof report->field, "%s", field);
      t64_replay_player_destroy(player);
      return T64_REPLAY_STEP_FAILED;
    }
  }
  result = t64_replay_player_step(player, machine, tick, T64_REPLAY_STOP_NONE,
                                  report);
  while (result == T64_REPLAY_STEP_CONTINUE) {
    const uint64_t next_at = t64_replay_player_next_at(player);
    const uint64_t end_at = t64_replay_player_end_at(player);
    uint64_t target = next_at < tick + step ? next_at : tick + step;
    t64_replay_stop terminal = T64_REPLAY_STOP_NONE;
    t64_stop_result stop = {0u, 0u};
    unsigned stalled = 0u;
    if (target <= tick) {
      report_begin(player, tick, T64_REPLAY_RESULT_REFUSED,
                   T64_REPLAY_DETAIL_ADVANCE_INVALID, report);
      result = T64_REPLAY_STEP_FAILED;
      break;
    }
    for (;;) {
      t64_stop_request request = {T64_STOP_GUEST_TICK, target};
      const uint64_t before = tick;
      if (t64_machine_advance(machine, &request, &stop) != T64_STATUS_OK ||
          stop.tick < before || stop.tick > target) {
        report_begin(player, tick, T64_REPLAY_RESULT_REFUSED,
                     T64_REPLAY_DETAIL_ADVANCE_INVALID, report);
        result = T64_REPLAY_STEP_FAILED;
        goto done;
      }
      options->advance_calls++;
      tick = stop.tick;
      if (options->after_advance != NULL) {
        options->after_advance(options->context, tick);
      }
      if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
        stalled = tick > before ? 0u : stalled + 1u;
        if (stalled > REPLAY_MAX_YIELDS_WITHOUT_PROGRESS) {
          report_begin(player, tick, T64_REPLAY_RESULT_REFUSED,
                       T64_REPLAY_DETAIL_ADVANCE_INVALID, report);
          result = T64_REPLAY_STEP_FAILED;
          goto done;
        }
        continue;
      }
      break;
    }
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
      terminal = T64_REPLAY_STOP_GUEST_COMPLETION;
    } else if (stop.reason == T64_STOP_REASON_BOUND) {
      if (tick == end_at) terminal = T64_REPLAY_STOP_GUEST_BOUND;
    } else {
      report_begin(player, tick, T64_REPLAY_RESULT_REFUSED,
                   T64_REPLAY_DETAIL_ADVANCE_INVALID, report);
      result = T64_REPLAY_STEP_FAILED;
      break;
    }
    result = t64_replay_player_step(player, machine, tick, terminal, report);
  }
done:
  t64_replay_player_destroy(player);
  return result;
}

/* ---- publication ------------------------------------------------------------------------------ */

bool t64_replay_publish(const char *directory, const char *leaf,
                        uint64_t byte_cap, const uint8_t *bytes, size_t length,
                        const char **error_out) {
  t64_artifact_publisher *publisher;
  const char *error = NULL;
  bool ok;
  publisher = t64_artifact_publisher_open(directory, leaf, byte_cap, &error);
  if (publisher == NULL) {
    if (error_out != NULL) *error_out = error;
    return false;
  }
  ok = t64_artifact_publisher_write(publisher, bytes, length) &&
       t64_artifact_publisher_finalize(publisher);
  if (!ok) {
    if (error_out != NULL) *error_out = t64_artifact_publisher_error(publisher);
    t64_artifact_publisher_abort(publisher);
  }
  t64_artifact_publisher_destroy(publisher);
  return ok;
}

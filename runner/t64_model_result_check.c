/* Strict consumer for native-model-probe observations. */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "t64_json.h"
#include "t64_jsonw.h"

#define MODEL_RECORD_MAX_BYTES 65536u
#define MODEL_STRING_MAX_BYTES 256u
#define MODEL_NODE_MAX 64u
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

enum { EXIT_OK = 0, EXIT_INVALID = 1, EXIT_USAGE = 2 };

static const char *const model_keys[] = {
    "assertions",  "build_id",      "content_digest", "evidence_tier",
    "guest_tick",  "model_scope",   "outcome",        "profile_hash",
    "profile_id",  "schema",        "schema_version", "state_digest"};
static const char *const assertion_keys[] = {"checked", "failed"};

static int str_is(const t64_json_node *node, const char *value) {
  size_t len = strlen(value);
  return node != NULL && node->type == T64_JSON_STRING && node->len == len &&
         memcmp(node->str, value, len) == 0;
}

static int digest_ok(const t64_json_node *node) {
  size_t i;
  if (node == NULL || node->type != T64_JSON_STRING || node->len != 64u) {
    return 0;
  }
  for (i = 0; i < node->len; i++) {
    char c = node->str[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

static int build_id_ok(const t64_json_node *node) {
  static const char prefix[] = "t64-build-v1-";
  size_t i;
  if (node == NULL || node->type != T64_JSON_STRING ||
      node->len != sizeof prefix - 1u + 64u ||
      memcmp(node->str, prefix, sizeof prefix - 1u) != 0) {
    return 0;
  }
  for (i = sizeof prefix - 1u; i < node->len; i++) {
    char c = node->str[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

static int exact_keys(const t64_json_node *object, const char *const *keys,
                      size_t key_count) {
  return t64_json_check_keys(object, keys, key_count, keys, key_count, NULL) ==
         T64_JSON_KEYS_OK;
}

static int read_bounded_file(const char *path, uint8_t **out, size_t *out_len) {
  struct stat st;
  uint8_t *buf;
  size_t used = 0u;
  int fd = open(path, O_RDONLY);

  if (fd < 0) {
    return 0;
  }
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
      (uintmax_t)st.st_size > MODEL_RECORD_MAX_BYTES) {
    (void)close(fd);
    return 0;
  }
  buf = (uint8_t *)malloc(MODEL_RECORD_MAX_BYTES + 1u);
  if (buf == NULL) {
    (void)close(fd);
    return 0;
  }
  while (used < MODEL_RECORD_MAX_BYTES + 1u) {
    ssize_t got = read(fd, buf + used, MODEL_RECORD_MAX_BYTES + 1u - used);
    if (got < 0 && errno == EINTR) {
      continue;
    }
    if (got < 0) {
      free(buf);
      (void)close(fd);
      return 0;
    }
    if (got == 0) {
      break;
    }
    used += (size_t)got;
  }
  (void)close(fd);
  if (used > MODEL_RECORD_MAX_BYTES) {
    free(buf);
    return 0;
  }
  *out = buf;
  *out_len = used;
  return 1;
}

static int validate(const t64_json_node *root, const char **outcome,
                    const t64_json_node **profile_hash,
                    const t64_json_node **build_id,
                    const t64_json_node **content_digest,
                    const t64_json_node **state_digest,
                    const t64_json_node **guest_tick, int64_t *checked,
                    int64_t *failed) {
  const t64_json_node *assertions;
  const t64_json_node *node;

  if (root == NULL || root->type != T64_JSON_OBJECT ||
      !exact_keys(root, model_keys, ARRAY_LEN(model_keys))) {
    return 0;
  }
  node = t64_json_get(root, "schema");
  if (!str_is(node, "t64.model-result")) {
    return 0;
  }
  node = t64_json_get(root, "schema_version");
  if (node == NULL || node->type != T64_JSON_INT || node->num != 1) {
    return 0;
  }
  if (!str_is(t64_json_get(root, "model_scope"), "native-model-probe") ||
      !str_is(t64_json_get(root, "evidence_tier"), "unit-tested") ||
      !str_is(t64_json_get(root, "profile_id"),
              "ntsc-cart-4m-skeleton")) {
    return 0;
  }
  *profile_hash = t64_json_get(root, "profile_hash");
  *build_id = t64_json_get(root, "build_id");
  *content_digest = t64_json_get(root, "content_digest");
  *state_digest = t64_json_get(root, "state_digest");
  *guest_tick = t64_json_get(root, "guest_tick");
  if (!digest_ok(*profile_hash) || !build_id_ok(*build_id) ||
      !digest_ok(*content_digest) || !digest_ok(*state_digest)) {
    return 0;
  }
  {
    uint64_t tick;
    if (!t64_json_u64_string(*guest_tick, &tick)) {
      return 0;
    }
  }

  assertions = t64_json_get(root, "assertions");
  if (!exact_keys(assertions, assertion_keys, ARRAY_LEN(assertion_keys))) {
    return 0;
  }
  node = t64_json_get(assertions, "checked");
  if (node == NULL || node->type != T64_JSON_INT || node->num <= 0) {
    return 0;
  }
  *checked = node->num;
  node = t64_json_get(assertions, "failed");
  if (node == NULL || node->type != T64_JSON_INT || node->num < 0 ||
      node->num > *checked) {
    return 0;
  }
  *failed = node->num;
  *outcome = *failed == 0 ? "model-match" : "model-mismatch";
  return str_is(t64_json_get(root, "outcome"), *outcome);
}

static int canonical_bytes_match(const uint8_t *input, size_t input_len,
                                 const t64_json_node *root) {
  char *canonical = (char *)malloc(MODEL_RECORD_MAX_BYTES + 1u);
  t64_jsonw writer;
  size_t canonical_len = 0u;
  int matches = 0;

  if (canonical == NULL) {
    return 0;
  }
  t64_jsonw_init(&writer, canonical, MODEL_RECORD_MAX_BYTES + 1u);
  if (t64_jsonw_node(&writer, root) == T64_JSONW_OK &&
      t64_jsonw_finish(&writer, &canonical_len) == T64_JSONW_OK &&
      canonical_len == input_len &&
      memcmp(canonical, input, input_len) == 0) {
    matches = 1;
  }
  free(canonical);
  return matches;
}

static int check_record(const char *path) {
  t64_json_limits limits;
  t64_json_doc doc = {0};
  t64_json_error error;
  uint8_t *bytes = NULL;
  size_t length = 0u;
  const char *outcome = NULL;
  const t64_json_node *profile_hash = NULL;
  const t64_json_node *build_id = NULL;
  const t64_json_node *content_digest = NULL;
  const t64_json_node *state_digest = NULL;
  const t64_json_node *guest_tick = NULL;
  int64_t checked = 0;
  int64_t failed = 0;
  int result = EXIT_INVALID;

  if (!read_bounded_file(path, &bytes, &length)) {
    (void)fprintf(stderr, "M-INPUT: input is unreadable or exceeds 65536 bytes\n");
    return EXIT_USAGE;
  }
  t64_json_limits_default(&limits);
  limits.max_input = MODEL_RECORD_MAX_BYTES;
  limits.max_depth = 2u;
  limits.max_string = MODEL_STRING_MAX_BYTES;
  limits.max_nodes = MODEL_NODE_MAX;
  if (t64_json_parse(bytes, length, &limits, &doc, &error) != 0) {
    (void)fprintf(stderr, "M-JSON: %s at byte %lu\n",
                  t64_json_category_name(error.category),
                  (unsigned long)error.offset);
    goto done;
  }
  if (!validate(doc.root, &outcome, &profile_hash, &build_id,
                &content_digest, &state_digest, &guest_tick, &checked,
                &failed)) {
    (void)fprintf(stderr,
                  "M-SCHEMA: invalid t64.model-result v1 scope, fields, values, or counts\n");
    goto done;
  }
  if (!canonical_bytes_match(bytes, length, doc.root)) {
    (void)fprintf(stderr, "M-CANONICAL: record is not canonical JSON with one LF\n");
    goto done;
  }
  (void)printf(
      "valid schema=t64.model-result/1 model_scope=native-model-probe "
      "outcome=%s evidence_tier=unit-tested profile_id=ntsc-cart-4m-skeleton "
      "profile_hash=%.*s build_id=%.*s content_digest=%.*s "
      "state_digest=%.*s guest_tick=%.*s assertions_checked=%" PRId64
      " assertions_failed=%" PRId64 "\n",
      outcome, (int)profile_hash->len, profile_hash->str, (int)build_id->len,
      build_id->str, (int)content_digest->len, content_digest->str,
      (int)state_digest->len, state_digest->str, (int)guest_tick->len,
      guest_tick->str, checked, failed);
  result = EXIT_OK;

done:
  t64_json_free(&doc);
  free(bytes);
  return result;
}

int main(int argc, char **argv) {
  if (argc != 3 || strcmp(argv[1], "result") != 0) {
    (void)fprintf(stderr, "usage: t64-model-result-check result FILE\n");
    return EXIT_USAGE;
  }
  return check_record(argv[2]);
}

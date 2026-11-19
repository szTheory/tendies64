#ifndef T64_ARTIFACT_PUBLISHER_H
#define T64_ARTIFACT_PUBLISHER_H

/* Single-file private no-clobber publisher (Phase 9, D-38). It is a copy, with
 * the delta below, of the transaction in runner/t64_raw_capture.c: parent
 * directory opened by descriptor walk with O_NOFOLLOW and no dot components, a
 * private 0700 stage inside the artifact directory with recorded identity, a
 * real no-replace rename probe, a 0600 staged file created with O_EXCL and
 * O_NOFOLLOW, fsync of the file and the directory, and identity-checked
 * cleanup. The delta is one published file instead of a bundle directory, a
 * leaf-name grammar, and a hard cap checked before every write.
 *
 * No API or ABI freeze is implied; names are provisional until Phase 10. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Upper bound of --artifact-max-bytes in the supervisor and the runner. */
#define T64_ARTIFACT_MAX_BYTES UINT64_C(33554432)

typedef struct t64_artifact_publisher t64_artifact_publisher;

/* Stable error names (the string returned by error_out and _error):
 *   artifact-dir-unsafe, artifact-leaf-name, artifact-stage,
 *   artifact-no-replace-unsupported, artifact-cap, artifact-write,
 *   artifact-exists, artifact-substitution, artifact-rename */

/* Requires an owner-only directory (not group- or world-writable, owned by the
 * caller, every component opened without following symlinks), probes the
 * host's real no-replace rename inside a private stage and removes the stage,
 * leaving the directory as it was. Run before guest start. */
bool t64_artifact_preflight(const char *directory, const char **error_out);

/* Leaf grammar: a lowercase stem of 1 to 32 characters from a-z0-9- starting
 * alphanumeric, one dot, a lowercase three-letter extension. Validated before
 * any filesystem effect. byte_cap is 1 to T64_ARTIFACT_MAX_BYTES. */
t64_artifact_publisher *t64_artifact_publisher_open(const char *directory,
                                                    const char *leaf,
                                                    uint64_t byte_cap,
                                                    const char **error_out);

/* A write that would pass byte_cap is refused (artifact-cap) and nothing is
 * published. */
bool t64_artifact_publisher_write(t64_artifact_publisher *publisher,
                                  const uint8_t *bytes, size_t length);

/* fsync, identity re-check of the held staged descriptor against the stage
 * entry, then no-replace rename through the held parent descriptor. */
bool t64_artifact_publisher_finalize(t64_artifact_publisher *publisher);

/* Removes the stage after identity checks; a no-op once committed. */
void t64_artifact_publisher_abort(t64_artifact_publisher *publisher);
void t64_artifact_publisher_destroy(t64_artifact_publisher *publisher);
const char *t64_artifact_publisher_error(
    const t64_artifact_publisher *publisher);
uint64_t t64_artifact_publisher_written(
    const t64_artifact_publisher *publisher);

#ifdef T64_REAL_RUNNER_TESTING
/* Called once between the last write and the rename so a test can substitute
 * the staged file, the stage or the target through the held parent descriptor
 * (paths are relative to parent_fd). */
typedef void (*t64_artifact_precommit_hook)(void *context, int parent_fd,
                                            const char *stage_name,
                                            const char *leaf);
int t64_artifact_publisher_stage_fd_for_test(
    const t64_artifact_publisher *publisher);
void t64_artifact_publisher_set_precommit_hook_for_test(
    t64_artifact_publisher *publisher, t64_artifact_precommit_hook hook,
    void *context);
/* Keeps the private stage in place on abort so a harness can inspect it. */
void t64_artifact_publisher_preserve_stage_for_test(
    t64_artifact_publisher *publisher);
#endif

#endif /* T64_ARTIFACT_PUBLISHER_H */

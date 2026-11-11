# Descriptor handoff boundaries for plan 03-09

The real producer does not exist until plan 03-11. Recognize the planned real
mode but refuse it explicitly until then; this is intentional staging, not a
missing owner prerequisite. Keep v1 synthetic bytes/goldens intact. The new
helper/descriptor consumer proves the preparation seam, not final SC6 or a
real guest-pass; actual CPU repetition remains required in 03-11.

Use the warm profile's 1 MiB fixture ceiling for this profile, never exceed the
existing 64 MiB harness maximum, and bound input schedules to 65,536 raw bytes.
Keep raw descriptor byte length/SHA separate from the parser's canonical input
identity: formatting can change raw JSON bytes while preserving canonical
schedule bytes. Both identities must describe the same owned snapshot.

Prepare via no-follow, nonblocking regular-file opening and bounded copying,
verify expected bytes, finish writes, remove write permission, close the writer,
reopen read-only, verify inode/length/digest, unlink and rewind. Return only the
owned read-only descriptor; child arguments contain descriptor numbers and
identities, never original input paths. Close owned descriptors on every exit,
keep close-on-exec by default, and clear it only for the specific child inputs.
Do not close unrelated caller descriptors or touch unrelated files/processes.

Tests must copy generated fixtures to their own temporary source paths before
replacement; do not rename or mutate shared `build/fixtures` outputs. Use pipe
barriers and real descriptor reads/admission rather than timing sleeps. Cover
invalid/closed/writable/named descriptors and all owned cleanup failures.

Code inspection also found a bounded legacy hardening fix in the owned
`runner/t64_supervisor.c`: `check_fixture` does `lstat` then opens with
`O_RDONLY | O_NOFOLLOW`, omitting `O_NONBLOCK`. A replacement FIFO can block
that open before `fstat` and the child wall backstop. Add `O_NONBLOCK` and retain
the post-open regular-file check. This does not change valid v1 results and
does not close its separate original-path handoff limitation; document only
the implemented descriptor route as resistant to replacement.

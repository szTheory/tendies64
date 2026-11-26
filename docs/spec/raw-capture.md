# Raw A/V capture bundle

Raw capture is an opt-in output for an authored headless runner invocation. It
stores the exact video and audio packets copied from the machine's general
observer APIs so callers can inspect guest output without a presentation or
codec layer. The bundle is local output and may contain rights-bearing content;
do not commit or upload it without the applicable rights review.

## CLI

Pass `--raw-capture-dir <target-path>` to a real-mode invocation. The option is
forwarded by the supervisor to the real runner. `--raw-capture-max-bytes
<bytes>` optionally sets a total bundle limit, including `manifest.json`.
Without that option the limit is 67,108,864 bytes (64 MiB). Explicit limits
must be from 1 through 268,435,456 bytes (256 MiB). Duplicate or malformed
options, a limit without a target path, zero, and values above the maximum are
rejected before guest execution.

The target path names a new directory. Its parent must already exist. Parent
components are opened one at a time as directory file descriptors; `.`/`..`,
symlink components, empty or invalid basenames, and non-directory parents are
refused. Existing files, directories, and symlinks at the target name are left
untouched. The target's parent must support the host's atomic no-replace
directory rename before the guest starts.

## Bundle layout

```text
<target>/
  manifest.json
  video/
    frame-000000.rgba8
    ...
  audio/
    packet-000000.s16be
    ...
```

Every file contains the bytes copied from the corresponding actual guest
observer packet. Video files contain tightly bounded RGBA8 rows with the
packet's reported stride; `byte_length` is `stride_bytes * height`. Audio files
contain signed 16-bit big-endian, interleaved stereo samples;
`byte_length` is `frame_count * channels * 2`, with two channels. The file
sequence is local to its stream. Video and audio timing is related by guest
tick ranges in the manifest, not by an invented cross-stream sequence.

`manifest.json` is UTF-8 JSON with schema `t64.raw-av-capture`, version `1`.
Its identity fields record the profile ID and hash, descriptor/model/state
serialization versions, core and runner build IDs, source revision, content
and fixture hashes and sizes, and input file hash, canonical digest, and size.
Terminal fields record the guest tick, outcome, stop kind, video/audio counts,
stream data byte total, manifest byte length, and total bundle byte count.

Each `video` entry records the relative file name, sequence, first and last
guest ticks, width, height, stride, `rgba8` encoding, and byte length. Each
`audio` entry records the relative file name, sequence, first sample index,
frame count, sample-rate numerator and denominator, first and last guest
ticks, `s16be-interleaved-stereo` encoding, channel count, bit depth, and byte
length. Counters, sizes, ticks and sample indexes are written as exact decimal
strings (for example `"16581"`) so 64-bit values cannot lose precision in a JSON
reader; only `schema_version` is a JSON number. File names are generated from
bounded counters and are relative to the bundle.

Both streams must contain nonzero actual data. Missing metadata, incomplete
DMA data, empty files, or files whose observed length differs from their
metadata prevent finalization. Output is never reconstructed from expected
pixels or a fixed observation record.

## Publication and errors

The runner opens and retains the target parent directory FD, then creates a
unique mode-0700 staging parent on that same filesystem. It opens and validates
the staging-parent FD's owner, mode, device and inode before using it, then
creates the actual bundle as its `bundle` child. Stream directories and files
are created beneath verified FDs. Stream files and the manifest are mode 0600.
Writes handle short writes and `EINTR`; files are validated and closed, the
manifest is written last, and required file and directory flushes finish
before commit.

The actual bundle is published with an atomic no-replace directory rename from
the retained staging-parent FD into the retained target-parent FD: macOS
`renameatx_np(..., RENAME_EXCL)` or Linux `renameat2(..., RENAME_NOREPLACE)`.
The staging-parent entry is visible in the destination parent and may itself
be renamed or replaced there, but that cannot redirect the source directory
bound to the retained FD. Unsupported platforms fail closed before guest
startup. After the target rename succeeds, removal of the now-empty staging
parent is best-effort and cannot change the successful capture result. Cleanup
removes only entries whose owner and recorded identity match this attempt; it
never follows a replacement or searches for a renamed entry. The final removal
is an identity-checked `rmdir` of the staging parent, which can only remove an
empty directory and never follows a symlink, so a principal that swaps the
visible entry cannot redirect it. In an ordinary destination parent, including
one that is group-writable and not sticky, a completed capture leaves exactly
the target and a host-side failure before commit leaves nothing. The empty
mode-0700 staging parent remains only if another principal renamed or replaced
that entry first. A destination filesystem that cannot represent the required
mode 0700 (for example FAT or exFAT) fails closed with a host-side `capture-*`
error before guest execution.

Before the rename, failures return a host-side harness error and do not publish
this attempt's final target. A competing writer cannot replace a completed
target; its loser receives a host error and the winner remains intact. Cleanup
uses only the current attempt's verified stage and does not follow links or
remove another contender's stage. Abrupt process termination before commit may
leave a private stage directory, but never a final bundle. The rename provides
atomic process-visible publication; the preceding flushes do not promise
power-loss durability.

Capture details are a closed set of host-error result details (`capture-*` in
the result checker). A completed capture error retains the actual guest tick
and state digest reached before that host error. When the runner is killed
before it can write its own report, the supervisor reports `runner-signalled`
with unknown guest state instead of inventing a rollback or success. Test-only
I/O faults and the commit barrier are available only in test runner builds and
are rejected by the production CLI.

The macOS primitive and race are exercised by the local focused suite. The Linux
primitive ran in the existing hosted Linux job of the Phase 08 qualification
(see [the evidence](../evidence/phase08-audio.md)); this document does not claim
hardware compatibility.

Known limit: the supervisor starts the real runner under an `RLIMIT_FSIZE` of the
result-size bound plus 64 KiB (about 68 KiB). The runner writes the capture
files, so a single video or audio packet file, or the `manifest.json` itself
(about 174 bytes per frame, so roughly 390 frames), larger than that limit ends
the run with `SIGXFSZ`. It fails closed and publishes nothing, but a larger frame
(for example a 320x240 RGBA frame is 307,200 bytes) is not capturable until that
limit is raised for the capture directory. The accepted authored case is
16,581 bytes in total and is unaffected; the current real runner accepts only
that authored guest.

Artifact pair (Phase 9; runner-internal, outside the ABI promise): with `--artifact-dir` and
`--artifact-max-bytes` (both required together, no default, at most 33,554,432
bytes, refused together with `--raw-capture-dir`) the child limit is exactly the
declared maximum plus 65,536 bytes; without the pair it stays at 69,632 bytes as
described above, and the report and the capture stay bounded by their own caps in
both configurations. A child ended at the limit leaves a private (mode 0700)
stage directory inside the artifact directory and publishes nothing; the
supervisor reports `runner-signalled`. The artifact publisher is a second,
single-file copy of the transaction above (descriptor walk, private stage, real
no-replace rename probe, identity-checked cleanup); `runner/t64_raw_capture.c` is
unchanged. The evidence for the boundary is the test twin of the supervisor and
runner, with a write of exactly the limit (not signalled) and one byte more
(signalled) in each configuration; it is not a hardware or timing claim.

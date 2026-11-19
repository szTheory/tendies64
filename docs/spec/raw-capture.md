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
length. JSON integer fields are exact integers; file names are generated from
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
never follows a replacement or searches for a renamed entry. If another
principal can replace entries in a non-sticky destination parent, an empty
mode-0700 staging parent may remain after completion or a host-side failure.

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

The macOS primitive and race are exercised by the local focused suite. Linux
primitive execution remains pending the existing hosted Linux job; this
document does not claim Linux runtime evidence or hardware compatibility.

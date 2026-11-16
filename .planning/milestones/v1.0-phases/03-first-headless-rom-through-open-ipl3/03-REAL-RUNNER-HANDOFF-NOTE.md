# Real runner integration boundaries

Parent preparation for 03-11, after 03-10 terminal handoff.

- `profile_digest` means the complete `t64_profile_hash` result, currently
  `6e6c4f86beb81c08023dfdf3a7b04e5d74ab0d4038ef5a95ea1a83f3eb8bba97`.
  The nested boot-policy SHA starts `08634481` and is a different identity.
  A demonstrated follow-up regression corrected the checker and sample.
- The core currently emits CPU_UNSUPPORTED at the real authored BREAK:
  tick 530438, PC 0x80000478, opcode 0x0000000d, 530437 retired instructions.
  Exact text alone cannot turn this unsupported stop into pass. Implement a
  narrowly documented software debug-stop convention for this BREAK encoding,
  with no instruction retirement or hardware exception claim. The existing
  GUEST_COMPLETION stop code can carry that convention; the runner alone
  qualifies authored completion by the manifest fixture digest, PC and opcode.
  Do not put fixture digests, transcript comparisons or guest verdicts in CPU
  execution. Unrecognized encodings and delay-slot cases remain explicit stops.
  Add independent stop checks, bind the changed convention into the active
  boot-policy/profile hash and update independently checked vectors. Existing
  serialized stop fields can represent the convention without a format bump
  if the layout is unchanged. Preserve historical qualification snapshots.
- This necessary seam correction extends 03-11 ownership to the CPU, its public
  trace/stop definitions, boot-policy identity and directly affected tests/docs;
  it does not authorize general BREAK exceptions or broader CPU implementation.
  Reproduce the unsupported-vs-completion defect using actual existing core
  execution for meaningful RED, rather than a missing executable or a false
  producer stub. The runner must still reject arbitrary or misplaced BREAKs.
- Trace kinds are scoped by device. Drain at most every 16 guest ticks, as
  verified previously, and reject trace loss. Bound processing must include
  scheduled input events at the bound. Resource-budget yields require bounded
  monotonic continuation, not an invented guest completion.
- 03-09's descriptor replacement helper is foundation evidence only. Repeat
  both fixture and logical-input replacement through actual CPU admission;
  compare admitted content identity, canonical input identity and literal guest
  output. Preserve legacy helper coverage and synthetic v1 bytes.
- Do not fabricate a final state digest when a child dies or its report cannot
  be trusted. The initial v2 all-null prelaunch rule does not yet specify that
  postlaunch uncertainty. Report this concrete contract conflict to the parent
  before choosing its representation; any narrow reconciliation needs its own
  independent checker case and explicit documentation. A correct captured text
  cannot override a process/report/capture failure.
- Reuse the healthy pinned toolchain and generated fixtures. Configure/build
  narrowly first; broad tests once after final source changes, relevant focused
  sanitizers, then prompt summary/state closeout. Retain and poll tool session
  identifiers until terminal output is captured. No outcome from stale logs.

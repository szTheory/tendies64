# Tendies64 project instructions

## Start here

Read `.planning/bootstrap/START-HERE.md`, `PROJECT-SEED.md` and
`EMULATOR-DNA.md` before OpenGSD initialization. Read `.planning/AUTONOMY.md`
for the owner's autonomous workflow and explicit stops. These are dated
research and policy inputs, not generated GSD project artifacts. Let the
installed `$gsd-new-project` workflow create and validate `.planning/PROJECT.md`,
`REQUIREMENTS.md`, `ROADMAP.md` and `STATE.md`.

The owner selected **Tendies64** as the working name for a Nintendo 64
emulator. Use it consistently. The name is not trademark clearance; do not
reopen the choice without materially new evidence.

## Product and engineering direction

- Aim for a high-quality, accurate, useful and performant N64 emulator. Keep
  the long-term goal ambitious and the first profile small enough to verify.
  A narrow first profile is delivery order, not the project's permanent limit.
- Identify Tendies64's concrete value beside mature existing N64 emulators.
  Historical reach and Switch-session data are context; they do not establish an
  unmet need or guarantee users will switch.
- Keep one canonical deterministic machine implementation separate from host
  clocks, files, physical input mapping, presentation, and durable storage.
  Define content/profile/input/time/output/lifecycle/error ownership and
  refused-operation effects before promising an API/ABI.
- Keep CPU, RSP, RDP, VI, audio, memory, PIF/boot and peripheral behavior
  observable at meaningful boundaries. Separate guest progress, guest time,
  raw video, raw audio and host presentation.
- Prefer a small, flat dependency tree. A small understandable copied helper
  can be better than a dependency if its provenance and maintenance owner are
  clear; add dependencies only for a concrete capability with justified
  lifetime and supply-chain cost.
- Treat ROMs, IPL/PIF/boot data, 64DD disks, saves, firmware, manuals, diagnostic
  assets and archive parsers as rights-bearing or untrusted inputs. Do not
  commit, use in public CI, or redistribute protected material without
  documented permission. A hash proves identity, not rights.
- Tie support statements to exact machine/region/boot/expansion/peripheral
  profiles, build identity and evidence tier. A boot, screenshot, test score,
  output hash, or agreement with another emulator alone does not establish
  broad compatibility or hardware accuracy.

## Autonomous GSD development

Follow `.planning/AUTONOMY.md` for every phase and milestone. Discuss phases
must use decision-focused research fan-out and one synthesized, adversarially
checked recommendation. Routine reversible decisions, phase execution,
verification and roadmap maintenance proceed autonomously under the configured
GSD workflow. Preserve the policy's explicit rights, authority, physical,
publication and destructive-action stops.

At every milestone close, keep the main branch green, triage open PRs, update
lessons and the rolling roadmap, and target a release from the exact tested
artifact when the selected license, visibility, rights, qualification and
release channel permit. Do not ask for routine approvals or repeat a question
already answered in project-owned records.

## Evidence and current-state rules

Distinguish owner decisions, hardware-document facts, code observations,
historical reports, measurements, inferences, recommendations and unknowns.
Verify mutable facts and active project state when they could change a decision.
An emulator source tree or planned test is not proof that its behavior passed.
Do not transfer another project's hardware assumptions, active blockers, test
rights, license or API as if they applied here.

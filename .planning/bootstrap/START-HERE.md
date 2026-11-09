# Tendies64 bootstrap

Prepared 2026-11-09 as a pre-GSD setup for a new Nintendo 64 emulator. The
target was initially empty. A minimal `.planning/config.json` appeared while
the pack was being prepared; its existing values were preserved and the
requested autonomous workflow settings were added. The target still has no source code, Git repository, or generated OpenGSD
artifacts; project instructions and bootstrap files are now installed.
Preserve any user work added after this inspection.

## Initialize the project

In a fresh LLM session with this repository as its working directory, read:

1. `AGENTS.md` and `.planning/AUTONOMY.md`.
2. `.planning/bootstrap/PROJECT-SEED.md` and `EMULATOR-DNA.md`.
3. The current `.planning/config.json`.

Then run:

    $gsd-new-project --auto @.planning/bootstrap/PROJECT-SEED.md

Use the installed GSD syntax if it differs. Let GSD create and validate
`.planning/PROJECT.md`, `REQUIREMENTS.md`, `ROADMAP.md`, and `STATE.md`. The
roadmap should remain rolling: preserve the North Star, make the first
milestones verifiable, and revise later sequencing as evidence arrives.

## Continue autonomously

After project initialization creates the first roadmap, run `$gsd-autonomous`
to discuss, plan, execute and verify every remaining phase in the active
milestone. At each milestone end, audit, keep main green, triage PRs, update
lessons/roadmap, and target a release when its project decisions and evidence
allow. Continue with the installed GSD new-milestone workflow and
`$gsd-autonomous` for subsequent milestones. The autonomy policy preserves
specific rights, publication, physical-hardware, paid-service and destructive
operation stops; routine reversible choices proceed without approval prompts.

This is only a bootstrap pack. Do not hand-write the GSD-generated project,
requirements, roadmap or state artifacts in this setup step. No code, ROM,
PIF/BIOS data, 64DD image, save, build, test, Git initialization, release or
hardware work was performed here. The owner is next to initialize Tendies64 in
a fresh LLM session.

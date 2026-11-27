# Phase 12 minimal host: owner checklist (physical controller)

Status: human-needed, non-blocking, owner-attested; not run.

Label: maintainer observation, not reproducible, no latency or sync claim.

This is a template for the owner. An agent never fills, attests or marks it passed. It is not a
must-have of Phase 12 and not evidence of any claim: the claims are in `docs/claims/phase12.md`
and rest on the scripted, fake-clock runs of the gate. A completed checklist records what one
person saw on one machine with one controller; it says nothing about rendering correctness, audio
fidelity, timing, latency, A/V sync, speed or any other machine. The optional SDL3 shell plays the
authored title (`fixtures/title/title.S`, built in this repository) through the host pump with
the real clock; on the planning host the unthrottled pump ran below real time (PIN-IMPACT row
H-09, information only), so slow movement or sound is expected and is not a failure of this list.

## Commands

The shell needs a system SDL3 of 3.2.0 or newer (link-only; nothing is installed by these
commands) and the gate build of the repository (it provides the moved install prefix and the
title ROM).

```sh
sh scripts/check-host.sh sdl
build/host-sdl/t64h_sdl --rom build/fixtures/phase12-title-s32k.z64
```

The first command builds the shell into `build/host-sdl` and runs its dummy-driver smoke; its last
line must be the lane's pass line before the second is worth running. The second opens the window and plays
the title with a connected controller (`--scale N`, 1 to 8, sets the integer window scale; the
default is 4). Start saves the position and ends the title; closing the window also ends it.

## Run record

Qualified build id, for comparison: `t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203` (record `20261127T033336673294Z-40b7d2d9`).

| Field | Value |
|-------|-------|
| Date | |
| OS and version | |
| SDL version (the first line of the lane) | |
| Controller model | |
| Build id (`strings build/host-sdl/t64h_sdl \| grep t64-build-v1-`) | |
| Observer | |

## Checks

Write pass or fail on each line, with a note when it fails.

| Check | Pass or fail | Note |
|-------|--------------|------|
| The D-pad moves the square | | |
| The left stick moves the square | | |
| A plays one tone | | |
| B resets the square to its start position | | |
| Start saves and quits | | |
| A relaunch restores the saved position | | |
| The window title reads exactly "t64 minimal host (development)" | | |

About the relaunch line: the shell keeps no host-side save file (decision DR-19 omitted it), so
each launch starts a fresh instance with an empty save slot, and the title's boot restore has
nothing to load across a relaunch. Write down what the window shows. The restored position is
proven headlessly by `installed.consumer.title-persist` (the long run's save export imported
into a fresh instance), not by this list.

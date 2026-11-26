# SDL3 host dependency audit (link-only system package)

Audited 2026-11-26 for Phase 12, plan 12-09, threat T-12-SC and decision DR-17. Maintenance
owner: Tendies64 maintainers. This audit is written before the first line of the optional shell
(`host/sdl`) is built or run, so the pre-use order that AUTONOMY section 2 asks for holds here
(contrast `docs/evidence/zlib-host-prerequisite-audit.md`, which is retrospective).

## Identity and need

| Field | Value |
|-------|-------|
| Package | SDL3, the Simple DirectMedia Layer, library `libSDL3` with CMake package `SDL3` |
| Provider on macOS | Homebrew core formula `sdl3` (bottle), formula page `Formula/s/sdl3.rb` of `homebrew-core` |
| Provider on Linux | Ubuntu archive package `libsdl3-dev`; research measured no candidate on ubuntu-24.04 (the only Linux label the hosted workflow allows), 3.2.20 on 25.10 and 3.4.2 on 26.04 |
| Official source | `github.com/libsdl-org/SDL`, release archive `SDL3-3.4.10.tar.gz` named by the installed package's SBOM; the project site is `libsdl.org` |
| License | zlib license (SPDX `Zlib`), recorded by the formula and by the installed `LICENSE.txt`, whose copyright line reads "Copyright (C) 1997-2026 Sam Lantinga" followed by the maintainer's contact address (not repeated here) |
| Version found on the planning host | 3.4.10, found by a scratch CMake probe of `find_package(SDL3 3.2.0 CONFIG)` that installed nothing and was deleted afterwards; the host is macOS (Homebrew prefix) |
| Version floor the host asks for | 3.2.0, the first stable SDL3 API; the host uses only calls that exist since 3.2.0 |

The name is the library's own name and matches the upstream project, the Homebrew formula and the
CMake package it installs (`SDL3Config.cmake`, target `SDL3::SDL3`); it is not a look-alike of
another package. No npm, pip, cargo, PPA, downloaded script or source tarball is involved, and
this plan installs nothing: SDL3 is used only where it is already present, and a missing SDL3 is a
recorded skip of the local lane (`[host-sdl] SKIP`).

## Acquisition trust

On macOS the package comes from the Homebrew core tap through its signed bottle; the install
receipt on the planning host shows a bottle poured from `homebrew/core` at 3.4.10 as a dependency
of another formula, and the formula metadata lists the license as Zlib. The installed SBOM names
the upstream release archive and its SHA-256 (`12b34280...2785`); this audit records that value as
the package's own statement and does not re-derive it, so it is an observation of the installed
metadata and not an independent verification of the upstream archive. On Ubuntu the trust chain is
APT's signed archive metadata, as the zlib audit describes. Neither chain proves the absence of
vulnerabilities or malicious upstream code, and no historical digest of a binary is claimed. The
version moves with the package manager (the formula page shows a newer stable release than the one
installed), so the lane records the SDL version it ran with every time.

## Purpose and lifetime

Purpose: the optional development shell of the minimal host (window, audio device and gamepad
around the pure-C pump), and its local smoke lane `sh scripts/check-host.sh sdl`. SDL3 is never
part of the required gate, the core library, the installed package, the runner or the tests: the
gate's build does not build `host/`, `scripts/check-host-isolation.sh` fails if SDL3 reaches the
core, the runner, the tools, the tests or the root build file, and the shell is off unless
`-DT64H_SDL=ON` is given. Lifetime: until an owner decision changes the host (a different
windowing library, a hosted lane, or no shell).

## Use boundary

- Link-only: `find_package(SDL3 3.2.0 CONFIG REQUIRED)` against a system installation. SDL3 source
  is never vendored, fetched (no FetchContent, no ExternalProject, no submodule), copied or
  patched, and nothing is added to `provenance/copies.json`. `scripts/check-host-policy.sh`
  (CTest `repo.host-policy`, with a seeded selftest) enforces these rules over `host/`.
- Copying SDL3 source into the repository would be a D4 owner stop: the zlib license is listed in
  `docs/policy/copy-provenance.md` under "Pre-vetted, not yet allowed" and is not on the allowlist,
  so the gate rejects a copy under it.
- No binary redistribution (D6): the shell is built locally, has no install rule, bundle,
  installer or icon, and is not shipped, so no notice duty of the zlib license's third condition
  arises; a future redistribution decision needs a new assessment before any SDL binary leaves
  the build tree.
- No local path, user name or host name from a lane log is committed: raw logs stay under the
  ignored `build/host-sdl` and only sanitized lines are recorded.

## Conclusion

The official package identity, provider, license, purpose and the system package manager fit a
bounded, optional, link-only development dependency. This is a legitimacy assessment of a system
package, not a complete vulnerability audit. A provider change, a vendored or fetched copy,
linkage from the core or the gate, a hosted lane that installs the package, or any redistribution
requires a new assessment before use.

Pre-use statement, 2026-11-26: this audit was committed before any SDL3 build of the host project
existed in the repository; the first build of `host/sdl` followed it.

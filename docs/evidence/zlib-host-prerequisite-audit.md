# Ubuntu zlib build prerequisite legitimacy audit

Audited 2026-11-11 for Phase 3, threat T-03-SC. Maintenance owner: Tendies64
maintainers. This is a retrospective audit: the prerequisite was introduced and
used before this record existed. It does not establish that the planned
pre-install audit occurred, and does not erase that process miss.

## Identity and need

The exact requested package is `zlib1g-dev`, the Ubuntu Noble development
package from source `zlib`. Ubuntu's official [package record](https://packages.ubuntu.com/noble/zlib1g-dev)
identifies Ubuntu Developers as maintainer, publication in main, the upstream
homepage, source archives and dependencies. The official [amd64 file list](https://packages.ubuntu.com/noble/amd64/zlib1g-dev/filelist)
includes `zlib.h`, `zconf.h` and the host linker library. These match the missing
header/library required to build pinned GNU Binutils, rather than a similarly
named registry package. No npm, pip, cargo, PPA or downloaded install script is
involved. The official listing currently shows amd64 version
`1:1.3.dfsg-3.1ubuntu2.3`; this is an observation of the listing, not an assertion
about the version selected by an earlier hosted run.

## Acquisition boundary

`scripts/build-boot-fixtures.sh` first checks whether the compiler can include
the system header. Installation is allowed only with both CI flags set on
Linux, with sudo and apt-get present. It runs `apt-get update` and requests only
`zlib1g-dev` with recommendations disabled. Required dependencies remain APT's
responsibility; this is not a claim that only one binary package is installed.
The script adds no repository/key and supplies no unauthenticated or insecure
repository override. Local missing-header checks fail without installing.

Ubuntu's [APT authentication documentation](https://manpages.ubuntu.com/manpages/noble/man8/apt-secure.8.html)
describes signed archive metadata and package checksum verification, with
unauthenticated archives rejected by default. The hosted-image repository and
key configuration remain a trusted environment input; the script does not
independently attest them. Archive authentication establishes the distribution
trust chain, not absence of vulnerabilities or malicious upstream code. No
historical package digest, version receipt or independent package signature
verification is claimed here.

## Lifetime, rights and conclusion

This is a host toolchain prerequisite. Guest fixtures do not execute it and the
core does not acquire a zlib dependency. Binutils host binaries may link libz;
neither those binaries nor the package are uploaded by the scoped JSON evidence
workflow. The upstream [zlib license](https://zlib.net/zlib_license.html) permits
use and redistribution subject to its notice/origin requirements. This audit
authorizes the existing build-only use, not a new redistribution artifact or a
legal clearance for all files in a future package.

The official package identity, provider, purpose and authenticated package
manager fit the bounded prerequisite. Retain the current distribution-managed
dependency rather than vendoring another compression implementation. This is a
legitimacy assessment, not a complete vulnerability audit; package versions
remain mutable with Ubuntu security updates. A repository/provider change,
disabled authentication, new package, guest/runtime linkage or redistribution
requires a new assessment before use. The missed pre-use audit is retained as a
workflow lesson: a guarded install is not its legitimacy audit.

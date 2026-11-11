# Qualified flat-loader layout note

The pinned software source [`boot/loader_compat.c`](https://github.com/n64brew/libdragon/blob/98154dafd5ac50e74ee09a5de1035d24a53f54f0/boot/loader_compat.c)
was inspected directly after the entry slice. Its stage2 reads the entrypoint
from cartridge header offset 0x08 and a load length from offset 0x10, then
requests PI DMA from cartridge offset 0x1000. A zero or out-of-range length
selects its 1 MiB fallback. This is this upstream loader's software contract,
not a claim about Nintendo boot/checksum behavior.

The authored packer must therefore write a nonzero checked load length matching
the actual padded flat payload, keep that complete region in the cartridge,
and record this header transformation. It must not inadvertently substitute a
classic CRC value or zero and invoke the larger fallback. Keep the existing
1 MiB whole-cartridge ceiling; a small explicit payload length fits it.

Source artifact identity (4096 bytes), copied IPL3 payload identity (offset0x40,
length4032), authored header transformations and complete authored cartridge
identity stay separate. Exact transcript bytes are authored independently.

Integration prerequisite: the first tracer deliberately admits only the exact
upstream source asset. Later CPU/warm-path integration must replace that
staged restriction with the qualified embedded-payload/header policy for the
authored ROM, update its normative policy identity and test transformed headers
and wrong payloads. Packing a valid fixture alone does not establish that it
boots; do not silently bypass the current admission guard to claim execution.

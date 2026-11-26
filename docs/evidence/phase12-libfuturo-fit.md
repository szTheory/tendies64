# LibFuturo fit evaluation (API-07)

Written 2026-11-26 for Phase 12, plan 12-10, requirement API-07 and decision DR-22. Maintenance
owner: Tendies64 maintainers.

Evidence tier: a written design comparison of two contracts, read from committed text. There is
no adapter, no test, no measurement and no compatibility, accuracy or hardware claim in this
document. It is not a statement about what any other project promises beyond the words quoted
from its committed files.

## The pin

Pinned commit: 4177a43e67312e2db88ab21cba13aa7350063105

- Commit date and subject: 2026-11-23T06:27:54-05:00, "docs: finalize browser-host v2.4 state".
- Published status: local and unpublished. The checkout the commit was read from has no configured
  remote, so no public URL is known to hold this SHA. The public repository of the same name had
  a different history of two commits when the planning lens looked at it on 2026-11-23; it was not
  asked again for this document, because a request would carry the project's identifiers to an
  outside service. This evaluation therefore pins a local SHA and says so: a
  reader without that checkout can verify none of what follows against the pin.
- Relation to the earlier read: the Phase 11 paper read took the same SHA and no commit follows it,
  so the pin did not move between the two reads.

## Method

Committed objects only. Four files were read by name at the pinned commit, through `git show` of
that commit and never from a working tree: the public header (347 lines), the shared contract
document (430 lines), the project README and the LICENSE (the Apache License, Version 2.0). Nothing
was written to the other checkout, nothing was fetched, and no filesystem path of it is recorded
here. Quotations are limited to short identifiers.

## Conclusion

no adapter now.

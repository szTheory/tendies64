---
phase: 07-rdp-reference-renderer-and-vi-scanout
source_plan: "07-06"
review_type: orchestrator_primary_recheck
status: issues_found
findings:
  critical: 1
  warning: 0
  info: 0
  total: 1
---

# Additional primary-register finding

BLOCKER: core/t64_rdp.c read primitive LOD fraction from raw bits47:40, the
adjacent minimum-level byte. The test literal had been moved to mirror that
incorrect decoder. The [SDK producer declaration](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm)
places its LOD parameter in upper-word bits7:0, hence raw64 bits39:32. Literal
FA0012A7636E7984 must produce fraction167 rather than18. This is an input-layout
fact, not an authorized software precision choice. Earlier green fetched-LOD
coverage is withdrawn; the original malformed-literal and later false-green
records remain preserved. The source repair changes the shift to32 and a
focused regression must discriminate the two bytes before qualification.

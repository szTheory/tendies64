# Plan07-11 artifact download bound

The focused independent recheck resolved the four initial findings and found
one remaining warning: the response was length-checked only after its complete
write. The collector now streams through a one-megabyte-plus-one-byte sink
before checking length and digest. Its pipeline waits for the downloader and
retains the downloader exit status independently of the sink's status. A failed
API command cannot become success merely because the bounded sink exited zero.
Bounded rejected bytes and API records remain in private ignored storage.

The final actual-producer collector selftest exited zero in 7.47 seconds with
34 rejection controls. A mock two-megabyte response with unchanged small API
metadata retained exactly 1,048,577 bytes, then refused before extraction; its
downloader exit record was present before return. A separately valid archive
followed by a failing API process also refused. Both-host resealed bytes,
digest mismatch and prior archive/member controls remain included. This uses
mock transport and establishes no hosted result.

Original command, collector hash, process outcome and logs are retained at
`build/gate/07-11-review/collector-stream-bound-attempt02/`. The complete upload
control is `build/gate/07-qualification/upload-layout-control.t1xPYy/`.
The previous attempt reached its controls but failed while copying a missing
test-only retention fixture; that attempt remains at
`build/gate/07-11-review/collector-stream-bound/`. The missing fixture was added
before the fresh complete pass. No full native, sanitizer, GCC or hosted run
was performed for this correction.

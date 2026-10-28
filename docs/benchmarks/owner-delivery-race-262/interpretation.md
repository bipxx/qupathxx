# Raw frontend owner-delivery repair

Original candidate260 has all eleven authenticated archives. Its engine run
40448479178 is not accepted: Linux sanitizer viewer_session fails the streamed preview
assertion at line 81 (49/50 suites); all four other engine profiles pass. Both
Mac-native architectures and portable builders/fresh consumers pass their full
applicable audits at f9ad28f. The exact original engine run is 40448479178.

The raw frontend polled a job, then separately observed busy=false. A worker can
finish between those calls, before its future has been collected on the owner.
Reading result() then returns no result and can leave a misleading terminal
frontend state. This affects direct and managed previews and related raw jobs.

The controlled diagnostic waits for the stream worker after an owner poll that
returned running. It records worker_busy=0 and delivered=0. Old code then fails
the direct preview assertion at line 72; the repair passes the entire native viewer
suite with the same transition, including preview/apply/undo/redo, stale edits,
cancellation, retry, quota refusal and shutdown. Equivalence proof authenticates
both instrumented source versions against their commits after removing only the
identical diagnostic block/include. This proves the delivery defect; the original
hosted schedule was not instrumented.

Final e2282b8 source contains no diagnostic wait or logging. Once a worker is no
longer busy, the frontend polls again to collect its future before consuming its
result and terminal status. The refresh also handles newly started replay jobs
and cancelled/stale completions. One compiler input changes; no public API,
schema, numerical algorithm, source/model guard or overwrite policy changes.

Final local validation passes all 51 native suites, 897 controls, 386 installed
passes/six declared skips across 392 identities, stubs and strict typing. The
installed 33 package members, complete unchanged SDK trees, 55 final products and
all 490 raw source paths authenticate. Local Windows shares test dependencies and
has OpenSlide OFF. It is not clean-machine, Linux-sanitizer or platform acceptance.
Original executable captures and prior products remain local under
build/owner-delivery-race-262 and build/root-locator-fix-260/products.

Successor262 is frozen at e2282b8 and dispatched once for complete engine,
Mac-native and portable qualification. Earlier source-bound successes do not
transfer. All M0-M7 and independent science/performance/hardware/UI/support/
license/release requirements retain their stated boundaries.

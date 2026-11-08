# Selected gap-closure evidence

Current native tree2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e.
These checks extend named cases, not universal qualification.

## G02: representative coarse whole-slide workflow

CMU-1.svs CC0 fixture,46000x32914 source pixels, canonical level2 origin-zero
snapshot. Tile256/512 both produce5,913,875 supported samples,1,083,798 foreground
samples and688 components. Exact input/label hashes and independent threshold
agree. Cancellation produces no result/retained record. [Probe](whole-slide.json)
records native allowances102.1/105.3MB and observer peak272.3MB; the process peak
includes Python/oracle/decoder caches, not just native work.

[32 repeats](repeat-tissue.json) preserve every input/label identity and have zero
private-memory range after eight warmups. This is bounded repeat evidence, not a
long-duration leak guarantee. [Viewer navigation](viewer.json): two240-frame
zoom/overview replays settle all tiles with no pending work; CPU p95s17.22/17.07ms,
rendered-loop wall p95s17.31/17.22ms. GPU retained tile payload peaks13.27/10MiB
are native payload accounting, not measured total driver/process GPU residency.
No full-resolution whole-slide cell, dense-object stress, arbitrary-size memory,
GPU completion/photon latency or other-machine performance claim.

Reuse existing canonical-pyramid implementation and probe.py; no production
change/build. Raw profiles/logs stay under build/gap-closure. New bounded driver:
`repeat-tissue.py <CMU-1.svs> <new-report.json>`.

## G03 / G05: additional NDPI crop and current supported reader route

New offset crop[11392,17792,256,256] of the retained CC0 Hamamatsu CMU-1.ndpi
compares native QUPATH_070_V2/affine-inverse with actual QuPath0.7.0 output.
All558 cells, RGB bytes, canonical nucleus/body partitions and anisotropic
calibration match exactly. Native compartment pixel counts/RGB means also match
independent source-pixel arithmetic at fixed1e-10 absolute tolerance.
[Checks](ndpi-checks.json), [native report](ndpi-native-report.json),
[upstream report](ndpi-qupath-report.json).
One archive/mask capture per engine, outside timing claims. Original13+9 cell
cases stay accepted; this is an additional vendor/crop case, not new pathologist
ground truth, independent-specimen validation or context-halo qualification.

Observer failure: the first comparison unnecessarily imported a SciPy-based
assignment metric. Exact canonical-label equality only needs the existing NumPy
oracle. Removed the unused import; reused both successful captures, without
installing SciPy or rerunning engines. No assertion/tolerance was weakened.

## G04: combined Windows candidate6

[Run/support guide](RUN.md). package.py freezes the already-tested installed
bytes, regenerates wheel RECORD/build tag6, and creates native/source archives.
It does not rebuild/relabel native bytes or borrow candidate5 qualification.
The historical embedded +dirty revision remains disclosed; recomputed exact
compiler-input source hash is authoritative. Reuse the authenticated12 dependency
archives; the added pinned Zlib-licensed LodePNG is vendored in the568-input
application source. Published candidate5 remains unchanged.

Installation, package closure, changed consumer-route checks and publication
results will be attached here after they finish. Broader G02/G03/G05 claims remain
explicit limits; this release is a local Windows CPython3.12 engineering route.

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

[Installation](installation.json) verifies69 immutable files and actual package
DLL origins. Static package closures: [wheel](wheel-runtime.json)70 files/7
binaries, [native](native-runtime.json)59/6; native EXEs have no Python dependency.
[Installed routes](installed-routes.json) retain8 exact density rows, native/Python
CSV agreement, bidirectional resume and32/28 NuCLS objects/pixels/calibration/
properties. Standalone app opens the imported project and standalone batch runs
the selected density manifest successfully. [Removal](removal.json) proves package
absence and all29 generated user output files unchanged. Native bytes were not
rebuilt. [Candidate receipt](candidate.json) binds packages, source and licenses.

G05 actual upstream NDPI saved-data import reveals an explicitly refused invalid
ring; [compatibility limit](qpdata-limit.json) remains open. Supported installed
saved-data checks give2 passes plus the repaired loss test's1 pass. Its older
assertion expected classification hierarchy flattening; current native ancestry
is retained. The repaired test asserts parent Tumor and intact nested class after
reopen, and still requires workflow loss review/explicit admission. No product
behavior or assertion tolerance changed. Initial test-selection typo and missing
optional pytest in the consumer environment caused no product rerun; development
pytest was pointed explicitly at the installed package. Log renderings retain the original
failure and focused repair (trailing spaces normalized; raw logs stay local). [Known limits](KNOWN-LIMITS.md) define remaining scope.

[Candidate6 is public](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.6).
[Publication receipt](publication.json): all14 asset sizes and server SHA256
digests agree before/after publication; tag targets source commitf1bb522c.
One source-only refresh includes the corrected test/known limits and retains
all568 exact compiler inputs; wheel/native bytes stayed identical. Draft lookup
by tag returned404; release-ID lookup reused the existing uploaded draft. No
second upload, hosted/full-platform campaign or native rebuild was needed.
G01/G04 close their scoped checks; G02/G03/G05 remain partial as recorded in the
[checklist](../../remaining-gaps-checklist.md).

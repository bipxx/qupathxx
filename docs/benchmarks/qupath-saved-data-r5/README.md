# R5-QPDATA1 native QuPath saved-data import

Native source `cc85a848c1b81407822cca435487b954407f66ef` adds direct data-file
version3 import for the pinned QuPath0.7.0 saved-data representation. Observer
`e88858419bb0e933686923cf98c1233daa32a2b7` corrects two test assumptions and
requires the actual decoder success marker. [Validation](validation.json) binds
442 compiler inputs, native/wheel identities, all62 installed members/RECORD and
retained products with explicit cache paths. This is local Windows engineering
verification. All M0-M7 and wider R5 remain Partial.

The [contract](../../annotation-migration.md#native-saved-data-decoding) and
[pinned synthetic corpus](../../../tests/fixtures/qupath-saved-data-0.7.0/manifest.json)
define support. Actual QuPath [capture](capture.log) saves eight objects in the
geometry case, plus separate loss-bearing and non-default-plane cases. It covers
nested parent links, UUID mappings, supplementary Unicode in stored names,
classes/colors, rectangle/ellipse/polygon/holes/multipart/line/point geometry,
detection and cell/nucleus, XY calibration and native save/reopen. The old two-image
corpus independently compares direct geometry with the prior assisted exchange.
No patient data or biological accuracy claim is involved.

The reader interprets Java serialization framing and pinned QuPath records as data.
It never loads Java classes or runs constructors, readObject/readResolve, workflow
scripts or a JVM. The [Oracle framing specification](https://docs.oracle.com/en/java/javase/21/docs/specs/serialization/protocol.html)
and pinned [PathIO](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/io/PathIO.java),
[PathObject](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/objects/PathObject.java)
and ROI serializers supply the contract; fetched source digests are retained in
the fixture manifest. Bounded handles/nesting/arrays/strings and modified-UTF8
conversion precede semantic decoding; geometry and publication reuse the existing
native authorities. Unsupported versions, planes, records or corrupt framing
are fatal. Supported-but-unrepresented settings produce explicit loss reports.

Evidence:

- [Before](before.xml): both selected direct-import workflows fail on the prior
  installed importer. This verifies the previous assisted-only gap.
- [Initial model](model.xml):251 passes, five declared skips and two failed test
  assumptions. [Corrected cases](model-final.xml):two passes on the same product.
  The first used mixed-case rather than the existing uppercase ObjectKind names.
  The second demanded identical GeoJSON bytes across retained analytic rectangles
  and assisted polygons. It now verifies exact IDs/hierarchy/name/class/kind,
  areas and vertex sets, plus every pixel-center containment result for both images.
  Native code and the original assisted importer were unchanged by these fixes.
- [Desktop](desktop.xml):direct saved-data preview/apply/reopen and existing
  project-switch/history both pass. The [inspected capture](direct-import.png)
  shows all eight outlines, including hole, multipart, ellipse and nucleus.
  Stored Unicode is verified; supplementary glyph rendering is not qualified.
- [Native](native.xml):qupath_project, project and interchange pass. The new marker
  covers actual binary geometry/calibration, truncation, trailing bytes and
  cancellation. [Observer controls](observer.xml) add missing/duplicate marker
  refusals and exercise all four project/GEOS capability combinations.
- By named case identity, all12 new direct workflows pass. The union of selected
  model/corrected/desktop/observer files has257 passing identities and four declared
  skips. These include two unchanged assisted desktop opt-ins and two unavailable
  slide cases; exact names remain in validation.json. They are not silently promoted.
- [Installed stub check](stubs.log) passes; all public headers, binding sources and
  Python implementation inputs are byte-identical to the preceding importer.
  Its positive/negative typing results are reused within that unchanged signature
  scope. The wheel includes unchanged public stubs; no regeneration/repackaging or
  intermediate install was needed. Prior JPEG/TIFF static archives remain identical.

The archived [proof](prove_qpdata_r5.py) ran from the root checkout's
`build/ci/prove_qpdata_r5.py` using
`build/ci/qupath-saved-data-r5-env/Scripts/python.exe -I -X utf8`.
Its paths describe the retained local layout; no runtime bundle or compiled product
is distributed here. Originals/products remain under `build/qupath-saved-data-r5`.
[Validation commands](validate_qpdata_r5.py) and per-mode JSON preserve arguments.
The corrected pytest invocation selected the two named model-final cases with
QUPATHXX_TEST_GUI=0; the observer invocation selected the two named control families
in observer.xml. Each wrote separate output paths without repeating passing suites.

Waste review: one cached seven-target build and one final install; no shared
public-header/main-binding edit or extra decoder per frontend. Build elapsed was
not captured. Reuse source TIFFs, existing hierarchy/import/snapshot authorities
and the pinned runtime. Keep both original test failures and run only corrected
cases. Compare representation-independent geometry when the storage forms differ;
do not require reserialization identity across those forms. Retain the existing
signature checks instead of regenerating stubs for implementation-only work.
No reader campaign, platform campaign or overall speed claim follows this delivery.

Next is R5-RELINK1: Project::relink verifies one image reader, discards it, then
creates a lazy LinkedServer that opens the path again on first use. Transfer the
verified reader into the link, preserving metadata/calibration, source generation
and external-owner lifetime. This removes duplicate work and closes the path
replacement interval without adding another relink API or fingerprint framework.
Broader persisted fingerprint/replacement policy remains a separate R5 gap.

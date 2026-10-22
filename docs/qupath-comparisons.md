# Pinned QuPath runtime comparisons

The optional comparison harness runs QuPath **v0.7.0**, source revision
`04ccfa4fb7d43e9b566393e08e83690b72248d44`, independently of the native engine.
It verifies the official Windows ZIP's size/SHA256 and the identities of all
496 extracted files before launching the bundled Java runtime. A fresh download,
extraction and live capture have been exercised. No Java installation is needed.
The runtime, logs and intermediate captures stay under ignored `build/` paths.
The QuPath binary is not redistributed in the repository or wheel.

The [contract](../tests/fixtures/qupath-0.7.0/contract.json) pins the bundle,
synthetic image, scope and tolerances. The [Groovy generator](../tools/upstream/qupath_reference.groovy)
calls QuPath APIs; it does not invoke qupathxx or compute expected outputs using
the native implementation. It creates ten objects, exports GeoJSON, decodes
that export in QuPath, measures the decoded objects, and produces OD/stain
values and pixel-center masks/means. A second QuPath process imports a real
native export and emits another reference set.

The [capture manifest](../tests/fixtures/qupath-0.7.0/capture-manifest.json)
identifies the generator, contract, runtime and native exporter, and hashes
every captured file. Original UUIDs are deterministic; new UUIDs assigned when
QuPath imports native numeric IDs are intentionally not deterministic. Geometry,
class/name and quantitative comparisons use unique fixture names, while a test
checks the identity remapping itself. Fixtures are never updated implicitly.
Git attributes preserve captured bytes and enforce LF for the hashed tooling;
checksums remain valid across Windows/Linux checkout line-ending settings.

## Reproduce

Use an installed qupathxx environment with NumPy. Fetching the runtime uses only
the Python standard library. Live generation currently uses the Windows x64
distribution; captured-reference checks do not require Java or Windows.

```powershell
python -I tools/fetch_qupath.py
python -I tools/compare_qupath.py --output build/qupath-comparison-new
python -I tools/compare_qupath.py --reference-root tests/fixtures/qupath-0.7.0 --output build/qupath-offline-new
```

Each output directory must be new. The checker writes a structured report and
returns **2** when the report is `partial`; unexpected reference/numerical
failures return 1. Argument errors can also return 2, so automation must inspect
the report, not infer acceptance from the exit code alone. A successful exit
0 is reserved for a comparison with no unresolved gates. The recorded corpus now passes using the explicit matching policy for foreign
ellipse hints. Default import still preserves raw bounds; the policy and any
coordinate changes are included in the report.

```powershell
$env:QUPATHXX_COMPARE_QUPATH = '1'
python -I -m pytest tests/test_qupath_reference.py -q
Remove-Item Env:QUPATHXX_COMPARE_QUPATH
```

Without that opt-in, the captured-reference tests still run and only the live
regeneration case skips. `QUPATHXX_QUPATH_CACHE` can select a different verified
runtime cache. ZIP and extracted-file validation reject changed caches. Captured
file validation rejects a modified fixture before comparisons run.

## Covered comparisons

| Scope | Evidence and acceptance |
| --- | --- |
| Interchange | Rectangle/polygon, ellipse, hole, multipart area, line, multipoint, detection, cell/nucleus, names including Unicode, flat classes/RGB and real numeric-ID-to-UUID remapping |
| Shape measurements | Bounds, area, length and anisotropic physical features against QuPath-decoded objects; `1e-6` absolute / `1e-10` relative tolerance for coordinates, areas and linear lengths |
| Ellipse lengths | Separate method budget of `0.05%`, with every actual difference reported; independent dense trapezoidal quadrature checks the native result. Fixture aspect ratios include 1.5/3, 100/200 and approximately 600/1200 after calibration |
| OD/stains | 260 RGB triples, covering every byte value in every channel, black/white and mixed colors; two explicit white models and a supplied nonorthogonal two-vector basis; `1e-6` float absolute/relative tolerance and `1e-14` basis absolute tolerance |
| Sampling | Exact SHA256 of the complete 256x256 binary eligibility mask, including a separate half-pixel hole case; zero mismatched pixels or sample-count tolerance |
| ROI analysis | Nine body/nucleus compartments, OD and stain means, three native tile sizes (16/64/256), in each direction: 108 native jobs against independently generated QuPath means |
| Preserved edge evidence | A quantized subpixel rectangle, a very thin ellipse and half-pixel hole boundaries remain in the corpus rather than disappearing from the acceptance checks |

The sampling check exposes the native analysis domain using threshold zero,
no morphology and opaque pixels. It is not the closed-boundary picking predicate.
Comparing a full-image `Roi.contains` mask would conflate picking/containment with
the analysis grid's half-open maximum bounds. This is a domain check, not an
upstream threshold-tissue plugin comparison.

The QuPath script uses the pinned
[ROI serializer](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/io/ROITypeAdapters.java),
[PathIO](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/io/PathIO.java)
and [ColorTransformer](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/color/ColorTransformer.java)
APIs. It does not claim to reproduce all QuPath measurement plugins, image-server
decoding, arbitrary axes, hierarchy, derived classes, direct Java project formats
or tissue/cell segmentation pipelines.

## Differences retained in the result

**Ellipse length methods.** Native length uses adaptive quadrature, while the
pinned [EllipseROI implementation](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/roi/EllipseROI.java)
uses Ramanujan's second approximation. An exploratory `1e-6` common absolute
length gate rejected an anisotropic ellipse (about `1.1e-6` difference). The
contract therefore distinguishes the methods explicitly; it does not loosen
coordinate, area or linear-geometry checks. The primary eccentric case measures about `0.03045%` length difference and
independently agrees with native dense quadrature. The thin migration case
meets the same method budget, at about `0.03823%` maximum relative difference. This method budget is not a claim that the algorithms are
identical or a universal bound for all possible inputs.

**Upstream export quantization.** The rectangle starting at `(150.499,150.499)`
with size `(20.002,20.002)` is decoded by QuPath from its own export as
`(150.5,150.5,20,20)`. Its sampling domain changes from 441 to 400 pixel centers.
Both pre-export and decoded geometry/masks are retained. Native import and the
native-to-QuPath round trip match the decoded reference; that does not restore
information lost before the native importer receives the file.

**Very thin ellipse: explicit matching policy.** A separate 60-by-0.1 ellipse
exports a raw analytic envelope that default native import continues to preserve.
The pinned QuPath decoder reduces the polygon at 0.01-pixel precision, removes
collapsed pieces and stores float coordinates before deriving its envelope.
It produces width `59.8800048828125` and area `4.7032516321`, instead of the raw
width 60 and area `4.7123889804`.

The optional `qupath_ellipse_precision=True` import/load argument reproduces
that path through the native GEOS precision reducer and separately rounded float
extrema. The recorded matching bounds and area now agree exactly. Changed bounds
produce `qupath_ellipse_precision_changed` and require `allow_losses=True`;
otherwise import fails before publishing any objects/classes or advancing IDs.
Collapsed/unsupported envelopes also reject atomically. This operation requires
GEOS. Raw/default import remains available without it.

```python
report = q.import_geojson(doc, text, assume_pixels=True, allow_losses=True,
                         qupath_ellipse_precision=True)
```

Only foreign `isEllipse` hints use this policy. Native `qupathxx` analytic hints
remain exact, and general polygons are not rounded by this option. The GUI's
**Match QuPath ellipse bounds** checkbox shares the same import path. Scripts
may preconfigure `GuiOptions.interchange_assume_pixels`,
`interchange_allow_losses` and `qupath_ellipse_precision`; all default to false.
The imported canonical geometry survives Undo/Redo and native project storage.

The checker retains both the default raw envelope and the matched result. The
named thin-ellipse gate now passes with the explicit policy; its strict expected
failure has become an ordinary passing regression. Coordinate/area tolerances
were not widened. This closes the captured decoder case, not a universal QuPath
format or workflow parity claim.

The half-pixel hole case agrees: geometric area 1850, but 1874 eligible centers
under both implementations' boundary rules. Its exact mask and means pass.
Area and the number of covered pixel centers are separate quantities.

## Recorded result and limits

The [recorded run](benchmarks/qupath-runtime-windows.json) identifies the updated installed native artifact and source for the explicit
precision-matching implementation. Primary bounds,
areas and linear lengths have zero observed discrepancy. OD error is at most
`5.85e-8`, stain error at most `1.09e-7`, all 18 primary masks agree and all 108
tiled mean results agree exactly in this capture. Four additional edge mean
checks agree; the thin-ellipse geometry gate now passes under the explicit
matching policy while preserving the default raw envelope.

The reported native timing covers only 54 small warmed ROI jobs per direction.
It excludes setup, mask checks, QuPath startup/execution, exports and file I/O.
It is not a cross-language speed comparison, whole-slide throughput benchmark
or new viewer frame-rate claim. Large/vendor images, segmentation counts and
split/merge features, longer runs and all outstanding M0-M7 gates remain open.


The one-time import benchmark uses 1000 copies of the foreign thin-ellipse
fixture (1.835 MB JSON), history disabled and five fresh documents per mode.
Median import is 84.10 ms raw and 342.22 ms matching; the matching maximum is
358.16 ms. Parsing, native validation and publication are included. Document/input construction and post-import checks are outside those timers.
The [raw record](benchmarks/qupath-precision-windows.json) identifies the native
artifact and source. Precision reduction adds work to an explicit import; it is
not applied per frame and this benchmark makes no viewer latency or universal
throughput claim.


Final precision-matching validation passes 1233 installed-wheel Python cases
with three expected skips and no expected failures (272.23 s). Native suites
pass 33/32/32, plus the separate interchange-enabled/GEOS-disabled probe. Public
stubs match, strict typing passes, and the GUI conversion controls/notice are
visually inspected. The updated captured source/native identities match the
implementation validated here; broader M0-M7 acceptance remains open.

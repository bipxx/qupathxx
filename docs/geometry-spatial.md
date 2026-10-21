# Geometry and spatial queries

The authoritative annotation shape is an immutable, shared native `Roi`.
`Annotation.geometry` retains its shape across document edits and history;
the legacy `Annotation.roi` property returns a copy of its bounding rectangle.
Rectangle APIs remain available. Use `Document.add_roi`, `update_roi` and
`ObjectEdit.create_geometry` / `replace_geometry` for general shapes.
`polygon_parts` returns copied shells and holes with multipart grouping intact.

Supported shapes are rectangles, ellipses, polygons with holes, multipolygons,
polylines and point collections. Invalid topology, overlapping multipart areas,
nonfinite coordinates and unrepresentable extents are rejected without repair.
Hierarchy checks use the actual geometry, including holes, even when an edit
leaves its bounding box unchanged. Geometry and history snapshots share storage.
Payload accounting includes shape coordinates and WKT conservatively, excluding
GEOS caches, allocator overhead and spare capacity; it is not a process-RAM cap.

## Dependency and packaging

Polygon construction and general topology use the stable GEOS C API (3.12+).
The validated SDK is GEOS 3.15.0; source URL and SHA256 are recorded in the
[fixture manifest](../tests/fixtures/manifest.json). Build it with:

```powershell
.venv/Scripts/python tools/build_geos_sdk.py
uv pip install -e . --config-setting "cmake.define.QUPATHXX_BUILD_GEOS=ON" --config-setting "cmake.define.GEOS_ROOT=C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk"
```

Adjust the SDK path for your checkout. CMake defaults GEOS off; native rectangle,
ellipse, line and point operations remain available without it. Polygon creation
then raises a clear error. Windows builds copy both GEOS DLLs beside binaries;
wheels require and install the SDK's COPYING license. The SDK build tool installs
that license and preserves MSVC exception handling. Unix runtime libraries must
be discoverable by the loader. `has_geos` and `geos_version` expose capability.

Each polygon owns a reentrant context, geometry and prepared geometry. A mutex
serializes all published context access, including queries that populate caches.
This follows the [C API context contract](https://libgeos.org/doxygen/geos__c_8h.html),
which restricts a context to one thread at a time. Immutable Python predicate
calls release the GIL; mutable document operations remain single-threaded.

## Numerical and sampling semantics

Rectangle point containment preserves the original half-open convention.
Ellipse containment is closed and analytic. Polygon containment includes outer
and hole boundaries, while excluding hole interiors. Picking uses distance to
the represented shape, with zero distance throughout an area's interior.

Ellipse area is analytic; perimeter uses adaptive quadrature at a radius-scaled
1e-12 tolerance. Outside distance solves the closest-point equation. Containment
between ellipses and polygon/ellipse boundaries uses analytic tests, with a
64-machine-epsilon normalized allowance for ellipse tangency. Ellipse `paths`
and WKT use 512 segments for drawing/export only; measurements and masks use
the analytic shape. GEOS provides linear polygon area, length and predicates.

Mean RGB samples pixel centers inside the ROI and excludes transparent pixels.
Lines and points have zero area and reject mean-intensity requests. Physical
calibration scales area; it does not change raw geometry or RGB sampling.
Independent analytic tests pass; QuPath numerical comparison fixtures remain open.

## Index and editing

A native median-split bounding-volume tree indexes object bounds. Region queries
return sorted candidate IDs based on inclusive bounding-box intersection;
`pick` refines candidates against exact geometry and a nonnegative tolerance.
Up to 256 changed bounds are merged as overrides before a lazy tree rebuild.
Metadata-only changes reuse the index. Delta preparation precedes atomic commit;
undo/redo follows the same invalidation path. GUI picking and overlay culling use
this index rather than scanning every stored annotation.

Select the tool in the toolbar or press R/E/P/L/T for rectangle, ellipse,
polygon, polyline or points. Drag rectangles/ellipses; click vertices for the
other tools, then Enter or double-click to finish. Escape cancels. Inspector
translation preserves topology; holes/multipart shapes can be supplied through
the API. Vertex/hole editing and Boolean operations are implemented; see geometry-editing.md.
Shared classes/colors are documented in classes.md; multi-selection is in selection.md.

## Validation and performance scope

The current suite includes 129 Python tests and eleven native suites, exercising
validity, hole masks, multipart topology, analytic ellipse metrics, immutable
snapshots, concurrent predicates, spatial deltas, failed edits and GUI drawing
with undo/redo. Native tests also run with optional readers and GEOS disabled.
Installed-wheel checks exercise packaged DLLs and licenses.

The [spatial benchmark](benchmarks/spatial-100000-windows.json) stores 100,000
rectangles and measures 1,000 region queries plus edit/undo reuse. Its GUI case
shows a narrow cached viewport with 828 visible objects on a 2560 x 1440
framebuffer. It does not establish performance for displaying all 100,000
overlays simultaneously or for complex polygon workloads. Those gates remain.


## Revision-keyed shape measurements

`Document.shape_measurements(id)` returns a native `ShapeMeasurements` value.
Its `roi` and optional `nucleus` each contain `area_px2`, `length_px` and optional
`area_um2`/`length_um`. Length means boundary length for area ROIs, open path
length for polylines, and zero for point sets; hole and multipart boundaries are
included. Cell and nucleus geometry are measured separately. Physical fields
are `None`/`std::nullopt` until both document calibration axes are known. These
are numeric snapshots; they survive later edits, eviction and document release.

```python
features = document.shape_measurements(object_id)
print(features.roi.area_px2, features.roi.length_px)
if features.roi.area_um2 is not None:
    print(features.roi.area_um2, features.roi.length_um)
if features.nucleus is not None:
    print(features.nucleus.area_px2)
```

Anisotropic length applies the document's X/Y scales to each boundary/path
segment before Euclidean length. Ellipse lengths use numerical integration of
the scaled semiaxes. Equal calibration axes reuse pixel length times the scale;
the cache avoids a second ellipse integration or polygon traversal in this case.
Ellipse integration normalizes the axes before adaptive Simpson evaluation to
avoid overflow/underflow in intermediate sums. Physical area preserves ordinary
rounding and uses exponent decomposition if a reciprocal-axis product would
otherwise overflow or underflow partway through evaluation. Scalar
`area_um2`/`nucleus_area_um2` share that arithmetic. Unrepresentable physical
results raise an overflow error; failed bundles are not published into the cache.
Binary64 underflow can still round a quantity to zero. These are floating-point
measurements, not arbitrary-precision geometry.

Each document owns an LRU cache keyed by object ID, the object's revision and
`Document.calibration_revision`. The calibration stamp starts at zero for a
freshly loaded document and changes on successful calibration changes, including
Undo/Redo. It is a runtime stamp, not a persisted identity. Object edits invalidate
that object's bundle; unrelated objects and class-color changes do not invalidate
geometry results. A deleted ID is rejected before consulting resident entries.
Calibration changes invalidate lazily through the key, without traversing all
objects. Repeating the same calibration is a no-op.

The default capacity is 4096 resident object bundles per document.
`set_feature_cache_limit(entries)` accepts 0..65536; zero disables retention.
`feature_cache_stats()` returns resident entries, capacity, hits and misses.
Resident entries can include stale revisions or deleted objects until reuse or
eviction. Misses count attempted computations, including ones that fail.
`clear_feature_cache()` removes entries and resets counters while preserving the
configured capacity. Cache operations are derived state: they do not create edits,
consume object IDs, dirty projects or enter undo history. No pixel, geometry or
Python-owner references are retained by entries. Native copies start cold and
preserve the configured capacity; moves retain valid LRU links. Save snapshots
and restored projects have cold caches, and cache contents/configuration are not
part of the SQLite schema.

Document/cache access follows the model's owner-thread contract, like spatial
queries. Workers capture immutable ROI values and calibration inputs rather than
sharing this mutable cache. The feature bundles described here contain geometry
measurements; pixel intensity caching still needs an image-source/version contract
and is not implemented by this cache. Raw RGB measurement/display behavior is
unchanged, and CSV columns retain their existing format.

ImGui reads the same native bundles for pixel/physical area and length, including
nucleus fields. Multiple selection explicitly labels these as primary-object
measurements. Calibration drafts refresh on the calibration stamp; the object-ID
list refreshes on membership revision, so unrelated metadata edits no longer
rebuild that list. A captured cell/nucleus inspector was checked after scrolling
its measurement fields into view.

`tests/test_shape_measurements.py` covers invalidation, calibration/no-op/Undo,
LRU pressure/disable, independent result lifetimes, closed pixels, persistence,
holes, multipart boundaries, open paths and numerical range. Ellipse fixtures use
an independent composite 64-node Gauss-Legendre reference over 32 subintervals,
with a relative length tolerance of 2e-11; areas use analytic fixtures. The
reference uses [NumPy's documented quadrature API](https://numpy.org/doc/2.3/reference/generated/numpy.polynomial.legendre.leggauss.html),
not the native adaptive Simpson implementation. NumPy is a test extra rather
than a runtime engine dependency. Native tests also exercise copy/move/assignment
and isotropic/anisotropic lengths without Python. These are independent fixture
checks; upstream QuPath numerical parity and wider geometry workloads remain open.

The [anisotropic benchmark](benchmarks/shape-feature-cache-100000-windows.json)
measures 65536 repeated native/Python queries over a 4096-object hot set in a
100000-ellipse document. It records 1240.7 ms with cache retention disabled versus
21.25 ms warm, with matching calibrated-area/length checksums. A traversal of all
100000 objects takes 1884.4 ms and retains 4096 entries. The
[isotropic case](benchmarks/shape-feature-cache-isotropic-100000-windows.json)
records 491.8 ms disabled versus 21.15 ms warm; its calibration and checksum differ
from the anisotropic case, so these are separate workloads.
The [selected viewer run](benchmarks/shape-feature-cache-view-100000-windows.json)
records 16.79 ms frame p95 and 17.12 ms maximum sampled frame work, 100000 selected
objects, 789 visible overlays and 3 MiB GPU payload. Its primary measurements use
176 hits and zero recomputations. Normal viewer sampling excludes the first ten
frames and swap wait; these results do not resolve the recorded initial rendering
stalls. Fixture creation/warm fill, intensity reads, persisted analysis tables,
cold storage and broader hardware/complex shapes are outside this timing scope.


Final verification records 411 passing installed-wheel Python tests with two
unavailable Windows file-symlink cases skipped. Seventeen new measurement/cache
checks and the extended native geometry/core tests cover this packet; all
fourteen native suites pass in full, core and minimal configurations. The
geometry cache contract is implemented; M3 acceptance still needs its remaining
comparison audit, and image/analysis feature versioning remains future work.

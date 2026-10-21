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

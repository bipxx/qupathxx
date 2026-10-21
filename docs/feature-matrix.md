# QuPath workflow inventory

Baseline: [QuPath v0.7.0](https://github.com/qupath/qupath/tree/v0.7.0), revision
`04ccfa4fb7d43e9b566393e08e83690b72248d44`. Source areas below refer to modules
under that revision. Each row names an implementation responsibility, not a
claim of upstream parity. Acceptance applies only to the listed fixture scope.

| Workflow | QuPath source area | Native owner | State | Required evidence |
| --- | --- | --- | --- | --- |
| Dense image regions | qupath-core | io | Partial | Exact pixels, padding and lifetime pass; reader variety missing |
| OpenSlide regions | qupath-extension-openslide | io | Partial | Synthetic pyramid and small Aperio comparisons; vendor multilevel/errors remain |
| Metadata and calibration | qupath-core, qupath-extension-openslide | core/io | Partial | Unknown and anisotropic calibration; broader axes/ICC missing |
| Whole-slide navigation | qupath-gui-fx | render/gui | Missing | Tiled rendering, latency, memory and DPI fixtures |
| Background tile reads and cache | qupath-core, qupath-gui-fx | jobs/io | Partial | Native scheduling, generation rejection and cache budgets pass; viewer integration pending |
| Image display and channels | qupath-gui-fx | render/gui | Partial | Dense RGB only; fluorescence, LUTs and composites missing |
| Rectangle annotation | qupath-core, qupath-gui-fx | geometry/gui | Partial | Bounds and sampling pass; interactive editing and upstream fixtures missing |
| Polygon and ellipse tools | qupath-core, qupath-gui-fx | geometry/gui | Missing | Holes, overlaps, validity, edits and reference area/perimeter |
| Object hierarchy and classes | qupath-core | core | Missing | Stable identities, containment, reparenting and selection |
| Undo and redo | qupath-gui-fx | core/gui | Missing | Compound operations restore geometry and classes |
| Projects and relinking | qupath-core, qupath-gui-fx | project | Missing | Transactional round trip and moved/missing images |
| Annotation interchange | qupath-core, qupath-gui-fx | project | Missing | GeoJSON/CSV fixtures and loss reporting |
| Region measurements | qupath-core, qupath-core-processing | analysis | Partial | Rectangle RGB mean and area; upstream and general shapes missing |
| Stain separation | qupath-core, qupath-core-processing | analysis | Missing | Optical-density and stain-vector fixtures |
| Tissue detection | qupath-core-processing, qupath-extension-processing | analysis | Missing | Masks and tiled-boundary comparisons |
| Cell detection and features | qupath-core-processing, qupath-extension-processing | analysis | Missing | Nucleus/cell split/merge and feature tolerances |
| Pixel and object classifiers | qupath-core-processing, qupath-gui-fx | analysis/gui | Missing | Training, prediction, model persistence and provenance |
| Tissue microarrays | qupath-core, qupath-gui-fx | core/analysis/gui | Missing | Grid edits, missing cores and per-core analysis |
| Batch workflows | qupath-core, qupath-gui-fx | jobs | Missing | Restartable per-image results and failure policy |
| Python scripting | New API replacing Groovy workflow surface | bindings | Partial | Shared document and region API pass; arrays/jobs remain |
| Bio-Formats and multiplex data | qupath-extension-bioformats | io | Deferred | Format/axis support matrix and raw dtype preservation |
| External model integrations | Extensions outside the core inventory | bindings/jobs | Deferred | Versioned Cellpose/StarDist results and publication contract |
| Java extensions and Groovy | qupath-extension-script-editor, qupath-gui-fx | compatibility | Deferred | No Java runtime compatibility promised in first release |

## Current packet and remaining decisions

The image-server contract is recorded in [image-server-contract.md](image-server-contract.md).
Reader fixture identities and retrieval are committed; large binary fixtures
and SDK downloads stay under the ignored build directory.

M0 remains open: select and probe the geometry dependency and SQLite schema,
choose the project license, obtain broader slide/damage fixtures, and freeze
benchmark hardware plus workflow tolerances. A bounded native read scheduler,
cancellation and decoded caching are implemented; see the
[scheduler contract](tile-scheduler-contract.md). M1 remains open for thumbnail
policy and broader reader validation. M2 remains open for tiled rendering and
viewport integration. Keep region-read and scheduler acceptance tests passing.

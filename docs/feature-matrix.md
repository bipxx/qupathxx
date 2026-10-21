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
| Whole-slide navigation | qupath-gui-fx | render/gui | Partial | Real slide rendering, overview, ImGui pointer replay and large synthetic navigation pass; OS/DPI and vendor stress remain |
| Background tile reads and cache | qupath-core, qupath-gui-fx | jobs/io | Partial | Native scheduling, generation rejection, cache budgets and viewer integration pass; broader workloads remain |
| Image display and channels | qupath-gui-fx | render/gui | Partial | RGB/gray/channel LUT, window and gamma framebuffer checks pass; fluorescence/ICC remain |
| Rectangle annotation | qupath-core, qupath-gui-fx | geometry/gui | Partial | Atomic inspector edits, sampling and drawing/history replay pass; upstream fixtures missing |
| Polygon and ellipse tools | qupath-core, qupath-gui-fx | geometry/gui | Partial | Holes, multipart validity, analytic ellipses and drawing/history pass; vertex/hole editing and Boolean overlay pass; QuPath comparisons remain |
| Object hierarchy and classes | qupath-core | core | Partial | General ROI parent containment, classes, cycles and depth pass; cell/nucleus snapshots, conversion and history pass; class definitions/colors/counts, rename and batch classification pass; multi-selection gestures and group delete pass; vertex editing passes; upstream workflow comparisons remain |
| Undo and redo | qupath-gui-fx | core/gui | Partial | Compound ROI/class/hierarchy/calibration restore and drawing/history replay pass; multi-selection gestures and group delete pass; vertex editing passes; upstream workflow comparisons remain |
| Projects and relinking | qupath-core, qupath-gui-fx | project | Partial | Multi-image SQLite with validated bulk restoration, rollback, stale writers, relative moves, browser/add/rename/relink, persistent views, v1-to-v2 migration, async project/recovery opening, explicit saves/autosave, save-baseline reconciliation, hot-journal recovery, staged replacement, empty workspaces, new/remove flows, reviewed checkpoint deletion, identity-based moved recovery, bounded native retirement, marked cross-session work cleanup with non-overwriting snapshot rescue, opt-in age/count retention with idle session protection, published/progress polling, safe maintenance cancellation, bounded marker prefetch, deferred initial file loading with counts/cancellation and dirty-close replay pass; legacy orphan/obsolete lock cleanup, source fingerprints and broader fault campaigns remain |
| Annotation interchange | qupath-core, qupath-gui-fx | project | Partial | Atomic GeoJSON, deterministic CSV, explicit losses, protocol fixtures and GUI replay; QuPath runtime round trip remains |
| Region measurements | qupath-core, qupath-core-processing | analysis | Partial | ROI-masked RGB mean and area pass; upstream and stain features remain |
| Stain separation | qupath-core, qupath-core-processing | analysis | Missing | Optical-density and stain-vector fixtures |
| Tissue detection | qupath-core-processing, qupath-extension-processing | analysis | Missing | Masks and tiled-boundary comparisons |
| Cell detection and features | qupath-core-processing, qupath-extension-processing | analysis | Partial | Cell/nucleus storage and masked RGB means pass; detection algorithms, split/merge and feature tolerances remain |
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

M0 remains open: geometry and SQLite probes are recorded in geometry-spatial.md and project-storage.md;
choose the project license, obtain broader slide/damage fixtures, and freeze
benchmark hardware plus workflow tolerances. A bounded native read scheduler,
cancellation and decoded caching are implemented; see the
[scheduler contract](tile-scheduler-contract.md). M1 remains open for thumbnail
policy and broader reader validation. Tiled rendering and viewport integration
are implemented, including overview navigation and RGB display controls.
M2 remains open for docking/layout, fluorescence/ICC, OS/DPI interactions and
vendor stress benchmarks. Keep reader, scheduler, planner and OpenGL checks passing.

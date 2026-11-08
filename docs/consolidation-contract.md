# Consolidation compatibility contract

NC01 baseline before implementation, parent cb87a56e. Source/rule fingerprints and
retained accepted outputs are in [legacy-baseline.sha256](benchmarks/consolidation/legacy-baseline.sha256).
Historical receipts prove the former workflows in their own source scope; they
are frozen comparators, not qualification of this migration. Read the current
implementations/tests when completing each contract below.

| Operation | Preserved contract | Native owner / evidence |
| --- | --- | --- |
| Manifest load | qpx.project-density/1; root schema/project/images/options;16MiB;1..1000 explicit distinct images; windows/tissue1..10000 IDs, excluded0..10000, cells0..100000; positive uint64 IDs; canonical ordering; optional positive record_id; exact known options; strict types/duplicate keys/nonfinite/unknown fields; project path relative to manifest | Native manifest loader/selection; existing test_project_density_runner setup cases |
| Class manifest | Exact case-sensitive names, no descendant expansion; required window/tissue classes; cell classes XOR all_cells; missing class fails each image; empty image selection means all saved images;0 Cells legitimate; no exclusion names means empty set; preview no computation/output; publish manifest only if every image admitted | Native class-selection factory over shared preview; test_density_manifest and prior workflow |
| GUI preview/run | Explicit selected images; class selectors; preview validates all rows and Run requires successful preview. Source closed/saved, no journal/WAL/SHM; preserve calibration, geometry/planes/curve policy. Completed-image results survive partial/cancel; source unchanged | Headless density service + thin GUI controller; density_panel/density_resume tests and frozen GUI JSON/CSV |
| Python manifest run | Existing CLI/programmatic options resume_from, recompute, stop_on_error, csv/project/checkpoint bytes (1..1GiB), cancellation and owner-thread image notifications; create missing output parents but never overwrite; protect source/manifest/nonembedded image paths; per-image failures remain errors, never zero rows; skip rest on cancel/fatal guards/budget or requested stop_on_error | Shared run service with explicit admission/publication policy; same Python control signature except internal observation hooks retired |
| Reuse | Capture IDs/revisions/geometry/centroids, algorithm/policy, calibration/extent, six-field engine and resource admission; unrelated metadata edits/larger limits may reuse; changed captured inputs/engine or smaller limits recompute. Hints replay source records. Checkpoint reuse refers directly to prior file, no copies. Recheck source/checkpoints through final publication | Existing density_reuse_compatible + native guards; tests cover mutation/corruption/engine/limits/unrelated changes |
| Receipt/report formats | Python qpx.project-density-checkpoint/1 and qpx.project-density-report/1 versus GUI qpx.gui-density-checkpoint/1 and qpx.gui-project-density/1. Retain distinct schema adapters. Python reuse tags computed/source_record/checkpoint, per-image complete/failed/cancelled and counts not_run; GUI tags computed/reused, row saved/reused/failed/not_run. Main states completed/partial/cancelled; retain each existing report key and error semantics | Native profile serializers normalize one internal result; preserve existing routes, not silent cross-format acceptance |
| Combined CSV | Seven context columns + unchanged20 native columns, same ordering. Python context uses csv.writer minimal quoting and explicit LF; GUI quotes string context fields with LF. Numeric/native scientific text remains exact. Whole-image CSV atomicity on limits/cancel; completed rows only in density.partial.csv; no replacing checkpoint/receipt/report/output | Shared native CSV assembly + explicit legacy format adapters; frozen GUI CSV and Python tests/accepted workflow |
| NuCLS | Exact case-sensitive .png/.csv stem union by default; explicit selection validates names; max100 images,16M aggregate pixels,100k rows,1M vertices,10k per polygon; positive explicit XY micrometers/pixel; strict RGB PNG without conversion. Rectangle/polyline-closed outline, bounds/finite checks, accurate local-frame centroid; outlines or1px centroid Cell marker; classes/source columns retained; preview no output; any issue blocks entire save-new project | Native import service/decoder/geometry/project; test_nucls_import and frozen previews/workflow. Existing CLI0 success,1 input issues,2 setup/integrity/publication failure |
| Training/example boundary | Raw classifier class/area eligibility native; training science already native. Keep arguments/options/job control/display in Python; diagnostics/fixtures/oracles in tests/tools | Existing native training frontend and thin examples; selected cohorts/model input compared before replacement |

Python density CLI status remains0 completed,1 partial/failure,2 setup failure,
130 cancelled; manifest CLI0 valid,1 invalid preview,2 setup failure. Cancellation
control callbacks run on the caller thread after native image-progress publication;
workers must never borrow Python callbacks or objects. Preserve meaningful behavior
coverage when replacing monkeypatches of Python internals with native counters.

At the initial extraction checkpoint, explicit-ID selection was additive and
manifest execution/adapters were still pending. Their native and installed
[completion evidence](benchmarks/consolidation/README.md) is now attached to
NC05-NC26. This baseline retains historical compatibility expectations; it does
not imply a binary release or full parity target.

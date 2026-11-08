# Shipped Python boundary review

The native-first follow-up is complete: [audit/build/runtime evidence](benchmarks/native-optional/README.md).
Plain CMake defaults Python OFF; native GUI, CLI and services run without Python.
Wheel builds explicitly opt in. This is the product boundary, not a requirement
to rewrite independent development oracles/automation in C++.

NC27 source review and evidence map. The [single consolidation
checklist](consolidation-coverage.md) owns status. Python may parse arguments,
construct caller-selected options, submit/poll/cancel native operations and display
native reports. Native services must validate and own domain interpretation,
selection, scientific values, reuse, persistence and authoritative export.

| Surface | Native authority / remaining action | Verification |
| --- | --- | --- |
| project_density.py | ProjectDensityManifest + ProjectDensityJob own run/reuse/guards/receipts/CSV; Python forwards events and caller cancellation | [Installed mapping/cancel/callback/legacy/source-change evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |
| density_manifest.py | ProjectDensityRecipe + native generation own class resolution/admission/publication | Same installed checkpoint proves class selection, invalid cohorts and save-new behavior |
| nucls_import.py | NuclsImportOptions + NuclsImportJob own pairing/CSV/RGB8/geometry/publication | Same installed checkpoint proves interpretation, limits, mutation and atomic save |
| raw_classifier_workflow.py | One run_raw_classifier_workflow call; C++ owns lattice/tiles/features, capture/checkpoint/train/predict/retention/resource lifetime and JSON report. C++ CLI calls the same service | Exact baseline/interface/native-CLI/Python-OFF hashes; positive/no-overwrite/typed boundary and -O cases pass in the follow-up |
| src/qupathxx/_workflow_control.py | Controller-thread polling, event JSON display/callbacks, cancel/acknowledge and typed error delivery only | Source reviewed; native workflow state/receipts/science are not duplicated |
| src/qupathxx/__init__.py | Native reexports and capability flag | Matching generated/installed stubs; optional workflows explicitly absent without a Python fallback |
| src/qupathxx/__main__.py | Forward argv/return code to shared C++ run_application; no Python startup policy remains | Native and optional Python launchers share C++;8 actual GUI regressions and headless/native entry tests pass |
| live_workflow.py | ViewerSession ticket submit/wait/take/close; caller-selected first image/default bounded ROI and stain options | Source reviewed; UI/default input choices are controls, while object edits, measurements, export and save execute natively |
| region_workflow.py | Native requests/results/save/replay/export; explicit/default ROI/tissue-level/quota choices; native values displayed in JSON | RGB/moment/CSV oracle is tools/validate_region_workflow.py and remains in installed region integration/handoff checks |
| neighborhood_workflow.py | Native request/result/replay/export; explicit source/target IDs and radius options | CSV oracle moved to tools/validate_neighborhood_workflow.py; installed CLI/oracle check passes |
| pixel_classifier.py / pixel_workflow.py | Explicit saved project, labelled/target ROI IDs and native options; native training/model save/load/prediction jobs | Installed -O training, native model/result agreement and no-overwrite check pass; original synthetic/lifetime demos retained and run in tools |
| rgb_nominal_preview.py | Native make_rgb_nominal_preview owns RGB extraction/new source/calibration; native open_rgb_nominal_source owns reader dispatch | Native ownership/calibration/overflow/cancel/reader tests and installed exact pixels/calibration/source-lifetime checks pass |
| density_workflow.py | Explicit caller IDs/options, native density request/result/keep/replay/export/save_new | Installed control checks native count/area/export/replay; it has no manifest selection, receipt/reuse adapter or competing batch state machine |

[Boundary closeout evidence](benchmarks/consolidation/README.md#shipped-python-boundary-closeout)
records the source scope, native test, ten passing installed cases and retained
independent tools. All11 shipped examples and three Python package modules were
reviewed. Python may choose explicit defaults, output locations and quotas, forward
native terminal failures and display summaries; it does not interpret cohorts,
compute per-pixel/object scientific values, decide reuse, parse input data or encode
authoritative scientific CSV/receipts. Native APIs validate at the actual trust
boundary even when argparse also constrains syntax.

Region/neighborhood/density examples use native save_new for new scientific
projects. The native raw-classifier workflow creates its output directory exclusively
and completes its checkpoints without forced partial/resume IO; save/retention/
coverage decisions remain native. Explicit native resume APIs remain available. UI/default source/ROI choices do
not create a second scientific selection/reuse authority. No compatibility fallback
or frontend-only scientific policy remains in these shipped Python workflows.
Independent NumPy/reference/fixture operations are retained in tests/tools and
are not imported by product controls.

Historical benchmark programs/fixtures are independent development evidence and
retain their original source scope. Changing a shipped control does not retroactively
qualify it with an older script's receipt. No optional parity feature, broad speed
campaign or binary release is added by this review.


## Native-only acceptance and remaining caller code

The [four-way comparison](benchmarks/native-optional/README.md) uses one retained
input: original Python workflow, migrated Python interface, native C++ CLI and
Python-disabled installed native CLI all preserve the same model/prediction
hashes,36 rows and four complete windows. The normal C++ workflow avoids the
old demonstration's artificial first-window/resume IO; explicit native checkpoint
resume remains available. No scientific algorithm was reimplemented.

Python/Python3/nanobind discovery was disabled in the existing cache. The target
graph excludes bindings, the fresh native install has no Python files, and all6
PE dependency closures exclude Python runtimes. With interpreter directories off
PATH, native GUI and train/save/stream-verify/project-reopen succeed. The new
application entry also links against headless core without a GUI/Python target.
The optional development interface was restored afterward;36/38 objects reused
for the two new APIs, and all38 preserved through configuration tests.

Remaining examples are optional callers of public C++ APIs. They may choose
explicit/documented input defaults, sequence native calls, wait/cancel and display
reports. They contain no per-pixel/object science, native-format parsing,
cohort/reuse implementation or authoritative export codec. In particular the live
producer illustrates ViewerSession tickets, whose C++ owner/worker implementation
is available to native producers too. Region/spatial/pixel examples demonstrate
native requests/services already callable by C++ or the native GUI/CLI.

Independent tools/tests may use Python/NumPy as separate oracles or fixture
producers. Those are not a native application dependency or product fallback.
No product module imports them. Existing NC01-NC28 receipts remain historical;
this follow-up does not reopen them or imply a new binary/platform release.

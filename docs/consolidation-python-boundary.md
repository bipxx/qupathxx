# Shipped Python boundary review

Working inventory for NC27, not acceptance evidence. The [single consolidation
checklist](consolidation-coverage.md) owns status. Python may parse arguments,
construct caller-selected options, submit/poll/cancel native operations and display
native reports. Native services must validate and own domain interpretation,
selection, scientific values, reuse, persistence and authoritative export.

| Surface | Native authority / remaining action | Verification |
| --- | --- | --- |
| project_density.py | ProjectDensityManifest + ProjectDensityJob own run/reuse/guards/receipts/CSV; Python forwards events and caller cancellation | Installed mapping, callback failure, cancellation, legacy resume and source-change tests pending |
| density_manifest.py | ProjectDensityRecipe + native generation own class resolution/admission/publication | Installed class selection, invalid cohorts and save-new tests pending |
| nucls_import.py | NuclsImportOptions + NuclsImportJob own pairing/CSV/RGB8/geometry/publication | Installed interpretation, limit, mutation and atomic-save tests pending |
| raw_classifier_workflow.py | Native labelled-training capture replaces class/area filtering; remaining calls construct options and control native checkpoint/training jobs | Installed training cohort agreement pending |
| src/qupathxx/_workflow_control.py | Controller-thread polling, event JSON decoding for display/callbacks, cancel/acknowledge and typed error delivery only | No per-object/pixel analysis or native workflow state duplicated |
| src/qupathxx/__init__.py | Conditional native reexports and capability flag | Check feature-unavailable build/stubs; no Python fallback |
| src/qupathxx/__main__.py and live_workflow.py | Argument/option/launch and ViewerSession request control | Retain native validation; audit source-opening choices and output control before NC27 closure |
| region_workflow.py | Requests/jobs/persistence/export stay native; NumPy RGB/moment and CSV accounting moved to tools/validate_region_workflow.py | Product and saved-output oracle integration checks pending |
| neighborhood_workflow.py | Native neighborhood request/result/replay/export; Python independently rechecks CSV counts | Move CSV oracle to tests/tools without losing retained-count coverage |
| pixel_classifier.py / pixel_workflow.py | Native feature/training/prediction; synthetic image/labels and lifecycle assertions remain embedded in demonstrations | Move synthetic/reference checks to tests/tools; make shipped controls consume caller-supplied native inputs/options |
| rgb_nominal_preview.py | Sampling is native, but Python slices RGBA to RGB and scales calibration | Reuse/add native preview-document construction so pixels/calibration have one authority; retain explicit caller sampling options |
| density_workflow.py | Native density request/result/replay/export, explicit caller IDs | Review output orchestration against the shared workflow boundary; avoid a second receipt/reuse/state machine |

The additional examples above must be addressed or justified as pure controls
before claiming every shipped Python workflow compliant. Do not hide them by
counting only the four primary migrations. Independent oracles can remain Python
under tests/tools; they must not become product fallbacks. Keep their accepted
scientific checks rather than deleting them to obtain a clean boundary report.

Historical benchmark programs/fixtures are independent development evidence and
retain their original source scope. Changing a shipped control does not retroactively
qualify it with an older script's receipt. No optional parity feature, broad speed
campaign or binary release is added by this review.

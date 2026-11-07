# NuCLS importer coverage

Selected outcome: standalone multi-pair RGB PNG/NuCLS CSV import with explicit
representation/calibration, preview and new-project publication. All14 selected
requirements are closed; arbitrary annotation formats remain outside this phase.

| Requirement | Status / evidence |
| --- | --- |
| Multiple exact-stem pairs, missing counterpart preview | Closed; test_missing_pairs_and_exact_stems |
| Explicit subset, duplicate/path names rejected | Closed; test_class_column_and_subset; test_invalid_options |
| Positive finite anisotropic X/Y calibration | Closed; invalid option cases; reopen test |
| RGB PNG only, source pixels unchanged | Closed; format cases; installed workflow exact RGB comparison |
| Explicit outlines preserve original geometry | Closed; test_preview_save_reopen_and_metadata[outlines] |
| Explicit centroid markers preserve coordinates | Closed; centroids reopen/native capture case |
| No clipping; outside geometry diagnostic | Closed; test_outside_outline_is_not_silently_clipped; public outlines-preview.json |
| Polygon topology, coordinate pairing and extent validation | Closed; test_polygon_validation |
| Chosen classes and source classification/row provenance | Closed; reopen and alternate class-column cases |
| Malformed CSV and per-row errors block all publication | Closed; bad-row/format cases |
| Input/aggregate limits and bounded diagnostics | Closed; test_limits_and_bounded_diagnostics; decoder size refusal |
| Read-only preview, changed source and existing output protection | Closed; preview/reopen; test_existing_output_and_changed_input |
| Isolated CLI preview/write/error exits | Closed; test_isolated_cli |
| Installed public import to existing density consumers | Closed; workflow.json:2 cases/60 annotations, original pixels exact, density31/27 |

[Focused tests](../tests/test_nucls_import.py):29 passed in1.57s against unchanged
installed candidate4. [Log](benchmarks/nucls-import/tests.log) and
[source receipt](benchmarks/nucls-import/receipt.json) pin evidence.
[Workflow script](benchmarks/nucls-import/workflow.py) takes the preserved NuCLS
downloads directory and a new output directory. It reuses the prior pilot's
source hashes/FOV/count-area oracles rather than repeating downloads or numerical
qualification. [Public source inventory](benchmarks/research-density-pilot/sources.json).

Actual importer [centroid preview](benchmarks/nucls-import/preview.json) and
[outline rejection preview](benchmarks/nucls-import/outlines-preview.json) show
the known one-pixel source-boundary issue. This expected rejection is preserved,
not a product failure. The selected importer makes no new tissue/whole-cell
segmentation, biological, platform or speed acceptance claim. No native build,
wheel, install, GUI or additional dataset campaign was needed.

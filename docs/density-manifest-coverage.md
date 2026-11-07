# Class-based manifest coverage

Selected outcome: resolve saved classes into explicit per-image density cohorts,
preview before calculation and write a new manifest for the delivered runner.
All12 selected requirements are closed; this is not QuPath feature parity.

| Requirement | Status | Evidence |
| --- | --- | --- |
| All images or explicit IDs; unknown/duplicate IDs rejected | Closed | test_image_selection_and_explicit_all_cells; test_bad_selectors |
| Exact names; multiple selectors; missing names reported per image | Closed | test_repeated_classes_and_policy_options; test_exact_names_and_missing_classes_block_publication |
| Explicit Cell classes or all actual Cells | Closed | test_image_selection_and_explicit_all_cells; test_bad_selectors |
| Legitimate zero Cells versus missing named cohort | Closed | test_zero_cells_with_explicit_all_cells |
| Counts and anisotropic calibration preview, no kernel work | Closed | test_preview_and_roundtrip_to_runner |
| Native invalid calibration/kind errors visible | Closed | test_native_capture_errors_are_visible |
| Explicit native curve policy and body/nucleus option mapping | Closed | curve case; test_repeated_classes_and_policy_options |
| One invalid image blocks whole manifest publication | Closed | native error cases; missing class cases |
| Sorted explicit IDs and existing runner schema | Closed | roundtrip/load_manifest test; installed workflow |
| Source changes and existing outputs/scratch preserved | Closed | test_changed_source_blocks_publication; test_existing_output_and_scratch_are_preserved |
| Isolated CLI preview/write/errors | Closed | test_isolated_cli_preview_and_generation |
| Installed generated-manifest to combined CSV, source unchanged | Closed | [workflow.json](benchmarks/density-manifest/workflow.json):2 images/8 rows |

Tests: [test_density_manifest.py](../tests/test_density_manifest.py),22 passed
in1.42s against installed candidate4; [log](benchmarks/density-manifest/tests.log).
Source/engine identities: [receipt](benchmarks/density-manifest/receipt.json).
Reproduction: [workflow.py](benchmarks/density-manifest/workflow.py) takes the
existing project and a new output directory. It reuses the prior same-crop
two-image fixture; no segmentation or native acceptance campaign was repeated.
Raw failed logs remain in main build/density-manifest. Fixture-only color,
unsupported plane and ellipse accessor mistakes are recorded in current-task.md.

Plane behavior remains native capture authority; this addition makes no new
quantitative-TIFF plane claim. Native capture preview does not execute the density
kernel, so the runner still reports runtime geometry/quota/export errors.

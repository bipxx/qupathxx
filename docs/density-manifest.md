# Class-based density manifests

For external NuCLS PNG/CSV pairs, first use the [NuCLS importer](nucls-import.md)
to create a saved project with explicit calibration and annotation representation.

[density_manifest.py](../examples/density_manifest.py) selects saved project
objects by exact class name and creates the explicit-ID manifest used by the
[project-density runner](project-density.md). Both standalone scripts must stay
together. Use Python with candidate4 installed; no native rebuild is required.

Preview all project images:

```powershell
python -I examples/density_manifest.py study.qpxproj --window-class "Density windows" --tissue-class "Density tissue" --exclusion-class "Density exclusion" --all-cells
```

The JSON preview lists image IDs/names, counts for each cohort, X/Y calibration
in micrometers, and issues reported by native request capture. It performs no
density calculation and saves no project. Exit0 means valid; exit1 means at
least one image has selection/capture issues; exit2 means setup/publication failed.
Fix issues before generating a manifest. A valid preview is not a guarantee
that later area/count work will fit runtime quotas or complete successfully.

Add `--output selections.json` to publish a **new** manifest after validation.
If any selected image has an issue, nothing is published. Existing outputs and
scratch files are preserved. The source must be closed/saved with no SQLite
sidecars. Source changes during selection abort publication. Source hashing is
at setup/final publication, with stat checks between images.

Run the resulting manifest:

```powershell
python -I examples/project_density.py selections.json new-results
```

Selection rules:

- Repeat `--image ID` to select particular images; omission selects all (1..1000).
- Repeat each class flag to combine multiple exact, case-sensitive names.
  Names include spaces/Unicode/punctuation as stored. Descendants are not
  automatically included; list their complete names explicitly.
- Windows and tissue classes are mandatory. Omit `--exclusion-class` for no
  exclusions. Every requested class must match objects in every selected image;
  missing/empty classes are issues, preventing unnoticed misspellings or omissions.
- Choose `--cell-class "Tumor"` (repeatable) or `--all-cells`. All-Cells selects
  actual native Cell objects, including unclassified Cells, and may legitimately
  yield zero. Class-based Cell selection containing a non-Cell fails native
  capture. Other cohorts use native geometry eligibility rules.
- Default centroids prefer nucleus then body. `--body-centroids` selects body.
  `--polygonize-curves` explicitly accepts the native512-segment ellipse policy.
  Native request capture owns geometry, calibration, plane and resource admission.

The manifest records sorted explicit IDs and an absolute source-project path,
plus the two native policy options. Edit that path if moving the project. It
does not contain live class queries: later class edits do not reselect its IDs.
Regenerate a new manifest when changing cohort membership. Each role is bounded
by the runner's existing limits (10000 area objects,100000 Cells); new manifest
publication is limited to16MiB. Default native budgets apply; advanced budget
options can be edited using the runner's documented manifest contract.

Python callers can load the standalone script and call `generate(...)` with
window_classes/tissue_classes, optional exclusion_classes, and either cell_classes
or all_cells=True. Optional image_ids, prefer_nucleus, polygonize_curves and output
match the CLI. The returned dict contains valid/images/manifest/written; per-image
issues are returned, while setup/integrity/publication errors raise exceptions.

[Coverage](density-manifest-coverage.md) records the selected outcome.
The [installed demonstration](benchmarks/density-manifest/workflow.json) reuses
the same506-cell crop twice to test image namespaces, not independent specimens.
Candidate4's native results and the batch runner are unchanged. No new GUI,
reader, segmentation, platform or speed claim is part of this addition.

A subsequent [public NuCLS pilot](benchmarks/research-density-pilot/README.md)
also passes on two different cases using original annotation centroids and
annotated FOV area, with explicit representation/denominator limits.

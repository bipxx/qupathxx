# Import NuCLS annotations

[nucls_import.py](../examples/nucls_import.py) creates a **new** embedded-image
QPX project from matching RGB PNG/CSV pairs. It is a standalone Python example
over unchanged candidate4, using Pillow and numpy. Keep project_density.py beside
it: the importer reuses that script's source-file integrity guards. No native
build, wheel update or reader plugin is needed. Pillow's installed license is
MIT-CMU; numpy's open license expression is captured in the
[installed workflow receipt](benchmarks/nucls-import/workflow.json).

Preview before writing:

```powershell
python -I examples/nucls_import.py path/to/rgb path/to/csv --mpp-x .2 --mpp-y .2 --representation outlines
```

Add `--output imported.qpxproj` to write after every selected pair passes.
Existing output files are refused, and any image/row issue blocks the whole
project. The output parent directory must exist. Preview constructs native
documents in memory but writes no project.
Exit0 means valid/saved as requested; exit1 means image/row issues; exit2 means
setup, integrity or publication failed. Stdout is the JSON preview/result.

Selection and coordinates:

- Matching uses exact, case-sensitive stems and lowercase `.png`/`.csv`
  extensions. Default selection is the union of both directories, so missing
  counterparts are reported rather than silently skipped. Repeat `--name STEM`
  to select specific pairs; this also avoids treating metadata CSVs or mask PNGs
  as data when they share a directory. Selected names cannot contain paths.
- Both positive finite X/Y scales are mandatory, in micrometers/pixel. They
  apply to every selected image in that import. No calibration is guessed from
  magnification or filenames. Use separate imports for different calibrations.
- Only RGB PNG is accepted: pixels are embedded exactly, with no color conversion,
  resampling, clipping, padding or source-file modification. Coordinates remain
  in the source PNG pixel frame.
- NuCLS `rectangle` uses xmin/ymin/xmax/ymax. NuCLS `polyline` means a **closed
  polygon boundary** for this format, using paired coords_x/coords_y lists;
  bounds must agree with those coordinates. Native ROI construction validates
  polygon topology. These are not generic open polylines.

Representation must be explicit:

| Mode | Saved native Cell body/nucleus geometry | Invalid/outside data |
| --- | --- | --- |
| `outlines` | Original rectangle or polygon in both slots | Any out-of-image or invalid geometry is an issue; no automatic repair |
| `centroids` | 1px square marker at the original geometry's area centroid, in both slots | Invalid source geometry still fails. Out-of-image source bounds are counted in source_bounds_outside; a marker must fit completely inside the image |

Centroid coordinates use box centers or signed-area polygon moments. Centroid
mode intentionally discards outlines from the saved geometry; source CSV path/
SHA remain in metadata. Marker area/shape is **not** nucleus area or shape.
Both modes represent supplied nuclear annotations; copying a nuclear ROI to
native Cell slots does not establish a whole-cell boundary or run segmentation.

Class/provenance behavior:

- `--class-column` chooses raw_classification, main_classification (default),
  or super_classification as the native object class. Nonempty names are kept
  exactly; surrounding whitespace is rejected. A default green display color
  is assigned because NuCLS CSV does not provide one here.
- All available source classification columns, source data-row ordinal, source
  type, chosen class column and representation are saved in object metadata.
  Image metadata records source paths/hashes, explicit calibration provenance,
  representation and nuclear-object semantics. It does not assign a dataset
  license on the caller's behalf; respect the source dataset's terms.
- Required CSV headers are type/xmin/ymin/xmax/ymax/coords_x/coords_y and the
  selected classification column. Additional columns may be present. Duplicate
  headers, inconsistent row widths, nonfinite/degenerate coordinates and native
  validation errors are issues. A valid header with zero rows is allowed.

The preview reports per-image dimensions/calibration, row/prepared-object counts,
class counts, hashes, outside-bounds count and row/physical-line diagnostics.
`imported` counts are provisional until top-level written=true. Up to50 detailed
issues per image are retained with the full issue_count. Resource exhaustion
stops further inspection; inspected_pairs then identifies how far preview got.

Admission limits:100 pairs,16M aggregate pixels,100000 aggregate annotation rows,
1M aggregate vertices,10000 vertices per polygon,32MiB per PNG and16MiB per CSV.
CSV fields use the parser's field limit (128KiB in the tested interpreter).
Native new-file save uses
512MiB snapshot and1GiB file limits. These are input/payload admission limits,
not a process-RSS guarantee. Source hashes are checked at setup and before
publication; changed/disappeared inputs abort. No partial project is published
for invalid data. Publication uses native save_new's no-replace behavior.

The importer does not infer tissue, FOV support, analysis windows or exclusions.
Add the appropriate regions using existing APIs/editor before generating a
[density manifest](density-manifest.md). The
[two-case installed demonstration](benchmarks/nucls-import/workflow.json) imports
the preserved public NuCLS subset, verifies exact saved RGB, classes/calibration,
adds the previously validated FOV support to a separate analysis project, and
reproduces31/27 nuclei within the selected FOVs. No new downloads or segmentation.
[Coverage checklist](nucls-import-coverage.md) defines this delivered scope.

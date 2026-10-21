# Tissue contours and annotation publication

`trace_tissue(mask, options)` converts an immutable `TissueResult` into native
polygon geometry using optional GEOS. The nanobind call releases the GIL.
`TissueJob.request_contours()` runs the same kernel asynchronously; poll or wait
with the current document and selected source object. Failed conversion retains
the mask for retry. Cancellation or stale source/selection discards both results.

```python
assert job.request_contours(q.TissueContourOptions())
assert job.wait(document, source_id).state == "ready"
created_ids = job.keep_contours(document, source_id,
                                name_prefix="Tissue", class_name="Tissue")
```

The default conversion clips sampled pixel footprints to the captured source ROI
and image bounds. Publication creates children of that source. Setting
`clip_to_roi=False` retains image-clipped footprints and publishes at the root.
Coordinates use the selected level's nominal downsample in level-zero space.
`pixels` remains the original component sample count; it does not measure the
clipped polygon area. Ellipse clipping uses the existing inscribed 512-segment
polygon approximation. Ellipse coverage permits normalized squared-distance
roundoff of 64 machine epsilons; pixel-center sampling remains unchanged.

The kernel scans the global label raster, merges collinear edges with identical
adjacent labels, and polygonizes fully noded linework once. Interior points map
faces back to their labels. Before clipping, integral face areas must exactly
reproduce every retained component's pixel count. Holes and disconnected parts
are preserved, including diagonal contacts under eight-connectivity. Removing
straight intermediate vertices preserves the footprint exactly. No smoothing or
approximate simplification is applied. The algorithm identity is
`qpx.pixel-contours/1`; the bundle carries its input mask digest and object ID.

Default limits are 10,000 components, 250,000 merged edges, 250,000 extracted
vertices and 64 MiB logical work. Counts accept 1..1,000,000; work accepts
1 byte..512 MiB. Edge counting precedes GEOS allocation. Extracted vertex limits
apply before straight-run compression; reported vertices count final geometry.
The estimate includes a 64 KiB base, component and per-edge allowances, and
captured clipping geometry. It is not exact RSS or a hard bound on GEOS's opaque
allocator. Input mask, source reader/cache and shared source ROI storage are
separate. Mask `retained_bytes` excludes the shared ROI payload; contour retained
bytes include owned output geometry. Cancellation is checked throughout scans
and extraction, with a context-local GEOS interrupt callback on GEOS >=3.14.
Older supported GEOS versions can delay cancellation during backend calls.

Publication validates the live source and selection again, then executes one
atomic object-edit batch. Invalid names/classes, limits and stale work create no
partial objects. One Undo removes the entire batch; Redo restores it. Each
bundle can be published once, even after Undo; explicitly tracing again creates
a new publishable bundle. An empty result changes no document/history state.
Returned geometry snapshots survive their job, document and reader owners.

The inspector exposes Trace contours (Ctrl+J) and Add annotations
(Ctrl+Shift+J). It retains the source selection. Replay supports `TISSUE_READY`,
`TISSUE_CONTOURS_READY` and `TISSUE_CONTOURS_PUBLISHED` readiness conditions.
The publication flag records the action, including after Undo. Report counts
and timing describe the current contour bundle; job elapsed time describes the
latest operation, while the mask retains its own detection timing.

Created annotations roundtrip through existing native projects and GeoJSON.
[Persisted masks](tissue-archive.md) now retain label rasters and captured inputs.
Complete source/environment provenance remains unfinished, as do
watershed/cell detection and comparisons against pinned QuPath workflows.

Validation includes all 512 three-by-three masks under both connectivity
choices, independent random-raster area/perimeter/membership checks, fractional
and pyramid coordinates, ellipse error, atomic history, resource failure,
cancellation, stale results, owner teardown and desktop replay. Reproduce the
synthetic performance recording with:

```powershell
build/check-install/Scripts/python.exe -I tools/benchmark_tissue_contours.py
# Add --objects 100000 --gui for the selected-document/viewer case.
```

The [4194304-pixel recording](benchmarks/tissue-contours-4194304-windows.json)
contains 200700 foreground samples in 285 rectangular components. Straight-run
compression produces 1140 edges/vertices, with 795440 estimated work bytes and
95435 retained bytes. Conversion records 47.40 ms, submission 0.015 ms,
maximum sampled polling 0.021 ms and atomic publication 0.35 ms.

The [100001-object recording](benchmarks/tissue-contours-view-100001-windows.json)
and [repeat](benchmarks/tissue-contours-view-repeat-100001-windows.json) publish
the same 285 children among 100001 existing objects in 0.38 ms. Native conversion
records 44.94/38.82 ms. The viewer preserves 100001 selected objects, draws 876
visible objects and retains 16 MiB each of image/mask GPU payload. Sampled frame
p95 records 29.20/17.30 ms, with 42.83/45.32 ms maxima. The
[mask-only comparison](benchmarks/tissue-contours-mask-baseline-100001-windows.json)
records 17.30 ms p95 and 18.19 ms maximum on the same wheel/environment. The
recordings include variability and frame spikes; these synthetic examples do
not establish hardware-wide latency, complex-boundary scaling or QuPath parity.

The final installed wheel passes 665 Python tests (45 new contour/desktop
cases), with two unavailable Windows file-symlink cases skipped. All twenty
native suites pass in full/core/minimal builds. A rendered demo additionally
verifies contour outlines and the visible inspector actions.

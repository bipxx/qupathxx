# Physical tissue component area limits

`TissueRequest.with_component_area_limits(document, object_id, transform,
minimum_um2=None, maximum_um2=None, parameters=TissueParameters(), nucleus=False)`
adds optional inclusive component area limits in square micrometers. The existing
constructor and pixel-count filters retain their behavior.

The factory first selects the same sampling level and raster as an ordinary
TissueRequest. A component with N sampled pixels has physical sampled area
N * downsample * downsample * captured_pixel_width_um * captured_pixel_height_um.
Both calibration axes are required when either physical bound is supplied.
Bounds must be finite and nonnegative, with minimum no greater than maximum.
An explicit zero maximum keeps no components, including when the numeric area
would underflow. Disable the maximum with None to make it unlimited.

Physical bounds intersect the existing min_pixels/max_pixels filters. Conversion
uses the shared calibrated count resolver, preserving inclusive comparisons and
handling intermediate overflow and underflow. An impossible interval resolves to
a minimum one greater than the sampling raster count and an unlimited maximum;
this remains a valid empty-result pixel recipe. Morphology and connected-component
labeling use their existing implementation; no extra decoding or labeling pass is
introduced. These are sampled component areas, not polygon contour areas.

A physical request captures the calibration revision. Changing calibration before
publication invalidates that request, so keep_tissue and asynchronous jobs refuse
stale results. With both bounds absent, ordinary request matching is preserved.

Retained results store the resolved count limits and capture-time provenance.
Save/reopen and TissueRequest.replay reproduce that frozen count recipe; replay
does not reinterpret old bounds using current calibration. Physical invocation
arguments are not separately persisted, following the cell-area factory contract.
No tissue archive version or table schema changes. Existing tissue CSV reports
the retained sampled component area and captured calibration.

The managed viewer exposes tissue_with_component_area_limits with the same
bounds and existing retain_result option. GuiOptions and ProjectBatchOptions
expose tissue_min_component_area_um2 and tissue_max_component_area_um2. ImGui
shows Physical tissue component area controls; the project batch dialog can use
image-inspector settings or explicit batch settings. The native batch executable
accepts --tissue-min-area-um2 and --tissue-max-area-um2 for --kind tissue. Batch
recipe identity includes supplied bounds, so changed physical limits cannot reuse
a checkpoint from a different recipe. Non-tissue batches reject these options.

Validation scope: native tiny masks independently establish 1/2/4-pixel component
areas, inclusive and empty limits, anisotropic/noninteger-level conversion,
unknown/invalid/extreme calibration, stale publication and count-recipe replay.
SDK/CLI/desktop checks live in tests/test_tissue_area_limits.py; their execution
status is recorded in current-task.md until workflow closeout. This does not
establish specimen accuracy, additional reader support or a speed advantage.
